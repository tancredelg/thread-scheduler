#ifndef __SUT_H__
#define __SUT_H__

#include <stdbool.h>


#define THREAD_STACK_SIZE (1024*1024)
#define SLEEP_TIME_NS 250000000


extern pthread_t t_compute, t_io;
extern pthread_mutex_t mutex_q_ready, mutex_q_wait;
extern struct timespec t_sleep_time;

extern struct queue q_ready, q_wait;

extern ucontext_t *ucp_c_exec;
extern struct sut_tcb *current_task;
extern int task_count;

typedef void (*sut_task_f)();

struct sut_tcb {
	int id;
	char *stack;
    sut_task_f function;
	ucontext_t context;
    // 0 = finished, 1 = terminated , 2 = yielded,
    // 3 = wait for open, 4 = wait for close, 5 = wait for read, 6 = wait for write
    u_int8_t exit_status;
};

void *c_exec();
void *i_exec();

void sut_init();
bool sut_create(sut_task_f fn);

void sut_yield();
void sut_exit();

int sut_open(char *file_name);
void sut_close(int fd);

void sut_write(int fd, char *buf, int size);
char* sut_read(int fd, char *buf, int size);

void sut_shutdown();

#endif
