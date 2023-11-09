#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <ucontext.h>
#include "queue.h"
#include "sut.h"


pthread_t t_compute, t_io;
pthread_mutex_t mutex_q_ready;
struct queue q_ready, q_wait;
ucontext_t *ucp_c_exec;
struct sut_tcb *current_task;
int task_count;


/// @brief Initializes the SUT library. It should be called before making any other API calls.
void sut_init() {
    t_compute = (pthread_t) malloc(sizeof(pthread_t));
    t_io = (pthread_t) malloc(sizeof(pthread_t));
    ucp_c_exec = (ucontext_t *) malloc(sizeof(ucontext_t));

    // Init kernel threads (C-EXEC and I-EXEC)
    pthread_create(&t_compute, NULL, c_exec, NULL);
    pthread_create(&t_io, NULL, i_exec, NULL);

    // Init task queues
    q_ready = queue_create();
    q_wait = queue_create();
    queue_init(&q_ready);
    queue_init(&q_wait);
    
    current_task = NULL;
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
        return 0;
    }

    if (getcontext(&(task->context)) == -1) {
        fprintf(stderr, "getcontext() failed.\n");
        return 0;
    }
    
    task->stack = (char *) malloc(sizeof(char) * THREAD_STACK_SIZE);
    if (task->stack == NULL) {
        fprintf(stderr, "Failed to allocate memory for task->stack.\n");
        return 0;
    }

    task->id = task_count;
    task->context.uc_stack.ss_sp = task->stack;
    task->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    task->context.uc_stack.ss_flags = 0;
    task->function = fn;
    task->exit_status = 0;

    struct queue_entry *node = queue_new_node(task);
    queue_insert_tail(&q_ready, node);
    ++task_count;
	return 1;
}

void *c_exec() {
    struct timespec sleep_time = {0, SLEEP_TIME_NS};
    struct queue_entry *next_task_entry;

    while (1) {
        next_task_entry = queue_pop_head(&q_ready);

        if (next_task_entry == NULL) {
            nanosleep(&sleep_time, NULL);
            continue;
        }

        // There is a task in q_ready, prepare it for execution
        pthread_mutex_lock(&mutex_q_ready); // ENTER CRITICAL SECTION
        
        current_task = (struct sut_tcb *)next_task_entry->data;
        printf("[Task %d]\t", current_task->id);
        current_task->exit_status = 0; // 0 = finished (default)
        current_task->context.uc_link = ucp_c_exec; // Link to c_exec()
        makecontext(&(current_task->context), current_task->function, 1, current_task);
        
        pthread_mutex_unlock(&mutex_q_ready); // EXIT CRITICAL SECTION

        // Execute the task
        swapcontext(ucp_c_exec, &(current_task->context));

        // Back from task, check its exit_status to see how it returned.
        pthread_mutex_lock(&mutex_q_ready); // ENTER CRITICAL SECTION

        if (current_task->exit_status == 0) { // finished
            printf("[Task %d]\tFinished.\n", current_task->id);
            free(current_task->stack);
            free(current_task);
            current_task = NULL;
        } else if (current_task->exit_status == 1) { // yielded
            // Put the task back in q_ready
            struct queue_entry *node = queue_new_node(current_task);
            queue_insert_tail(&q_ready, node);
            current_task = NULL;
        } else if (current_task->exit_status == 2) { // terminated
            printf("[Task %d]\tTerminated.\n", current_task->id);
            free(current_task->stack);
            free(current_task);
            current_task = NULL;
        }

        pthread_mutex_unlock(&mutex_q_ready); // EXIT CRITICAL SECTION
    }
}

void *i_exec() {

}

/// @brief Allows a running task to yield execution before completing its function.
void sut_yield() {
    // Called by the task itself
    if (current_task == NULL) return;
    current_task->exit_status = 1; // 1 = yielded
    swapcontext(&(current_task->context), ucp_c_exec);
}

/// @brief Terminates the execution of a task. If there are multiple tasks running, calling this function will only terminate the current task.
void sut_exit() {
    // Called by the task itself
    if (current_task == NULL) return;
    current_task->exit_status = 2; // 2 = terminated
    setcontext(ucp_c_exec);
}

/// @brief Requests the system to open the file specified by `file_name`.
/// @param file_name The name of the file to open.
/// @return -1 if the file does not exist, 0 otherwise.
int sut_open(char *file_name) {

}

/// @brief Closes the file pointed to by the file descriptor `fd`.
/// @param fd The file descriptor for the file to close.
void sut_close(int fd) {
    
}

/// @brief Writes the bytes in `buf` to the file belonging to `fd`. Write errors are not considered in this call.
/// @param fd The file descriptor for the file to write to.
/// @param buf The buffer of contents to write to the file.
/// @param size The size of the buffer. 
void sut_write(int fd, char *buf, int size) {

}

/// @brief Reads up to `size` bytes into `buf`, from the file belonging to `fd`.
/// @param fd The file descriptor for the file to read to.
/// @param buf The buffer of contents to write to the file.
/// @param size The size of the buffer.
/// @return Like fgets, returns buf on success, and NULL on error or when end of file occurs while no characters have been read.
char *sut_read(int fd, char *buf, int size) {

}

/// @brief Completely shuts down the executors and terminates the program.
void sut_shutdown() {

}
