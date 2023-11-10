#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <ucontext.h>
#include <fcntl.h>
#include "queue.h"
#include "sut.h"

#define THREAD_STACK_SIZE (1024 * 1024)

struct sut_tcb {
    int id;
    char *stack;
    sut_task_f function;
    ucontext_t context;
    u_int8_t status; // 0 = expected to finish, 1 = terminated , 2 = yielded, 3 = wait on I/O
} *current_task, *waiting_task;

pthread_t *t_compute, *t_io;
pthread_mutex_t mtx_q_ready, mtx_q_wait, mtx_shutdown;
struct timespec t_sleep_time = {0, 100000};
bool shutdown;

struct queue q_ready, q_wait;

ucontext_t *ucp_c_exec, *ucp_i_exec;
//struct sut_tcb *current_task, *waiting_task;
int task_count;


void *c_exec();
void *i_exec();

/// @brief Initializes the SUT library. It should be called before making any other API calls.
void sut_init() {
    t_compute = (pthread_t*) malloc(sizeof(pthread_t));
    t_io = (pthread_t*) malloc(sizeof(pthread_t));
    ucp_c_exec = (ucontext_t *) malloc(sizeof(ucontext_t));
    ucp_i_exec = (ucontext_t *) malloc(sizeof(ucontext_t));
    shutdown = false;

    // Init kernel threads (C-EXEC and I-EXEC)
    pthread_create(t_compute, NULL, c_exec, NULL);
    pthread_create(t_io, NULL, i_exec, NULL);
    
    // Init mutex locks
    pthread_mutex_init(&mtx_q_ready, NULL);
    pthread_mutex_init(&mtx_q_wait, NULL);
    pthread_mutex_init(&mtx_shutdown, NULL);

    // Init task queues
    q_ready = queue_create();
    q_wait = queue_create();
    queue_init(&q_ready);
    queue_init(&q_wait);
    
    current_task = NULL;
    waiting_task = NULL;
    task_count = 0;
}

/// @brief Creates a task with the given function `fn` as its main body. 
/// @param fn The task function to create a task for.
/// @return true (1) on success, false (0) otherwise.
bool sut_create(sut_task_f fn) {
    struct sut_tcb *task;
    task = (struct sut_tcb *) malloc(sizeof(struct sut_tcb));
    if (task == NULL) {
        fprintf(stderr, "Failed to allocate memory for task.\n");
        return false;
    }

    if (getcontext(&(task->context)) == -1) {
        fprintf(stderr, "getcontext() failed.\n");
        return false;
    }
    
    task->stack = (char *) malloc(sizeof(char) * THREAD_STACK_SIZE);
    if (task->stack == NULL) {
        fprintf(stderr, "Failed to allocate memory for task->stack.\n");
        return false;
    }

    task->id = task_count;
    task->context.uc_stack.ss_sp = task->stack;
    task->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    task->context.uc_stack.ss_flags = 0;
    task->context.uc_link = ucp_c_exec;
    task->function = fn;
    task->status = 0; // 0 = expected to finish

    struct queue_entry *node = queue_new_node(task);
    pthread_mutex_lock(&mtx_q_ready); // LOCK READY QUEUE
    queue_insert_tail(&q_ready, node);
    ++task_count;
    pthread_mutex_unlock(&mtx_q_ready); // UNLOCK READY QUEUE
	return true;
}

/// @brief The start routine for the C-EXEC (compute) thread.
void *c_exec() {
    struct queue_entry *q_ready_entry;
    while (1) {
        pthread_mutex_lock(&mtx_q_ready); // LOCK READY QUEUE
        q_ready_entry = queue_pop_head(&q_ready);
        pthread_mutex_unlock(&mtx_q_ready); // UNLOCK READY QUEUE
        
        if (q_ready_entry == NULL) {
            // Check for shutdown
            pthread_mutex_lock(&mtx_shutdown); // LOCK SHUTDOWN FLAG
            if (shutdown) {
                pthread_mutex_unlock(&mtx_shutdown); // UNLOCK SHUTDOWN FLAG
                // Need to ensure that BOTH queues empty, and C-EXEC is not executing a task
                pthread_mutex_lock(&mtx_q_wait); // LOCK WAIT QUEUE
                struct queue_entry *q_wait_entry = queue_peek_front(&q_wait);
                if (q_wait_entry == NULL && waiting_task == NULL) {
                    pthread_mutex_unlock(&mtx_q_wait); // UNLOCK WAIT QUEUE
                    break;
                }
                pthread_mutex_unlock(&mtx_q_wait); // UNLOCK WAIT QUEUE
            }
            pthread_mutex_unlock(&mtx_shutdown); // UNLOCK SHUTDOWN FLAG
            
            nanosleep(&t_sleep_time, NULL);
            continue;
        }

        // There was a task in q_ready, so execute it.
        current_task = (struct sut_tcb *)q_ready_entry->data;
        if (current_task->status == 0) {
            makecontext(&(current_task->context), current_task->function, 1, current_task);
        } else {
            current_task->status = 0; // Reset task status to 'expected to finish'.
        }
        
        // Execute the task ...
        swapcontext(ucp_c_exec, &(current_task->context));
        // ... and return here
        
        // Check the task status
        if (current_task->status <= 1) { // finished (0) OR terminated (1)
            free(current_task->stack);
            free(current_task);
        } else if (current_task->status == 2) { // yielded --> back into q_ready
            struct queue_entry *node = queue_new_node(current_task);
            pthread_mutex_lock(&mtx_q_ready); // LOCK READY QUEUE
            queue_insert_tail(&q_ready, node);
            pthread_mutex_unlock(&mtx_q_ready); // UNLOCK READY QUEUE
        } else if (current_task->status == 3) { // waiting on I/O --> into q_wait
            struct queue_entry *node = queue_new_node(current_task);
            pthread_mutex_lock(&mtx_q_wait); // LOCK WAIT QUEUE
            queue_insert_tail(&q_wait, node);
            pthread_mutex_unlock(&mtx_q_wait); // UNLOCK WAIT QUEUE
        }
        current_task = NULL;
    }
}

/// @brief The start routine for the I-EXEC (I/O) thread.
void *i_exec() {
    struct queue_entry *q_wait_entry;
    while (1) {
        pthread_mutex_lock(&mtx_q_wait); // LOCK WAIT QUEUE
        q_wait_entry = queue_pop_head(&q_wait);
        pthread_mutex_unlock(&mtx_q_wait); // UNLOCK WAIT QUEUE

        if (q_wait_entry == NULL) {
            // Check for shutdown
            pthread_mutex_lock(&mtx_shutdown); // LOCK SHUTDOWN FLAG
            if (shutdown) {
                pthread_mutex_unlock(&mtx_shutdown); // UNLOCK SHUTDOWN FLAG
                // Need to ensure that BOTH queues empty, and C-EXEC is not executing a task
                pthread_mutex_lock(&mtx_q_ready); // LOCK READY QUEUE
                struct queue_entry *q_ready_entry = queue_peek_front(&q_ready);
                if (q_ready_entry == NULL && current_task == NULL) {
                    pthread_mutex_unlock(&mtx_q_ready); // UNLOCK READY QUEUE
                    break;
                }
                pthread_mutex_unlock(&mtx_q_ready); // UNLOCK READY QUEUE
            }
            pthread_mutex_unlock(&mtx_shutdown); // UNLOCK SHUTDOWN FLAG
            
            nanosleep(&t_sleep_time, NULL);
            continue;
        }
        
        // There was a task in q_wait, so handle the I/O, then put it back in q_ready.
        waiting_task = (struct sut_tcb *)q_wait_entry->data;
        // Switch back to the context of the waiting task to do execute I/O operation (on the I-EXEC thread)
        swapcontext(ucp_i_exec, &(waiting_task->context));
        
        // Done with I/O operation, now put the task back in the ready queue
        pthread_mutex_lock(&mtx_q_ready); // LOCK READY QUEUE
        queue_insert_tail(&q_ready, q_wait_entry);
        pthread_mutex_unlock(&mtx_q_ready); // UNLOCK READY QUEUE
        waiting_task = NULL;
    }
}

/// @brief Allows a running task to yield execution before completing its function.
void sut_yield() {
    current_task->status = 2;
    swapcontext(&(current_task->context), ucp_c_exec);
}

/// @brief Terminates the execution of a task. If there are multiple tasks running, calling this function will only terminate the current task.
void sut_exit() {
    current_task->status = 1;
    setcontext(ucp_c_exec);
}

/// @brief Requests the system to open the file specified by `file_name`.
/// @param file_name The name of the file to open.
/// @return -1 if the file does not exist, otherwise the file descriptor of the opened file (non-negative) 
int sut_open(char *file_name) {
    current_task->status = 3;
    // Yield control of the C-EXEC thread (returning to c_exec())
    swapcontext(&(current_task->context), ucp_c_exec);
    
    // Continue in the I-EXEC thread (coming from i_exec())
    int fd = open(file_name, O_RDWR, 0644);
    
    // I/O operation done --> switch back to i_exec(), to get put back into q_ready 
    swapcontext(&(waiting_task->context), ucp_i_exec);
    // Resume here from the C-EXEC thread (popped from q_ready)
    return fd;
}

/// @brief Closes the file pointed to by the file descriptor `fd`.
/// @param fd The file descriptor for the file to close.
void sut_close(int fd) {
    current_task->status = 3;
    // Yield control of the C-EXEC thread (returning to c_exec())
    swapcontext(&(current_task->context), ucp_c_exec);

    // Continue in the I-EXEC thread (coming from i_exec())
    close(fd);

    // I/O operation done --> switch back to i_exec(), to get put back into q_ready 
    swapcontext(&(waiting_task->context), ucp_i_exec);
    // Resume here from the C-EXEC thread (popped from q_ready)
}

/// @brief Writes the bytes in `buf` to the file belonging to `fd`. Write errors are not considered in this call.
/// @param fd The file descriptor for the file to write to.
/// @param buf The buffer of contents to write to the file.
/// @param size The size of the buffer. 
void sut_write(int fd, char *buf, int size) {
    current_task->status = 3;
    // Yield control of the C-EXEC thread (returning to c_exec())
    swapcontext(&(current_task->context), ucp_c_exec);

    // Continue in the I-EXEC thread (coming from i_exec())
    write(fd, buf, size);

    // I/O operation done --> switch back to i_exec(), to get put back into q_ready 
    swapcontext(&(waiting_task->context), ucp_i_exec);
    // Resume here from the C-EXEC thread (popped from q_ready)
}

/// @brief Reads up to `size` bytes into `buf`, from the file belonging to `fd`.
/// @param fd The file descriptor for the file to read to.
/// @param buf The buffer of contents to write to the file.
/// @param size The size of the buffer.
/// @return 
char *sut_read(int fd, char *buf, int size) {
    current_task->status = 3;
    // Yield control of the C-EXEC thread (returning to c_exec())
    swapcontext(&(current_task->context), ucp_c_exec);

    // Continue in the I-EXEC thread (coming from i_exec())
    size_t bytes_read = read(fd, buf, size);

    // I/O operation done --> switch back to i_exec(), to get put back into q_ready 
    swapcontext(&(waiting_task->context), ucp_i_exec);
    // Resume here from the C-EXEC thread (popped from q_ready)
    return bytes_read > 0 ? buf : NULL;
}

/// @brief Completely shuts down the executors and terminates the program.
void sut_shutdown() {
    pthread_mutex_lock(&mtx_shutdown); // LOCK SHUTDOWN FLAG
    shutdown = 1;
    pthread_mutex_unlock(&mtx_shutdown); // UNLOCK SHUTDOWN FLAG
    
    pthread_join(*t_compute, NULL);
    pthread_join(*t_io, NULL);
    free(t_compute);
    free(t_io);
    free(ucp_c_exec);
    free(ucp_i_exec);
    printf("\nSUT stopped.\n");
}
