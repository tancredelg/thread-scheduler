#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "sut.h"

// Can randomly yield or terminate during execution
void slow_func() {
    struct timespec sleep_time = {0, 250000000};
    printf("slow_func: [");
    nanosleep(&sleep_time, NULL);
    fflush(stdout);
    int rng;
    for (int i = 0; i < 6; ++i) {
        rng = rand() % 23;
        if (rng == 0) {
            printf("TERMINATE\n");
            sut_exit();
        }
        if (rng == 1) {
            printf("YIELD\n");
            sut_yield();   
            printf("RESUME");
        }
        
        printf("--");
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
    printf("]\n");
}

int main() {
    sut_init();

    for (int i = 0; i < 15; ++i) {
        if (i == 7) sleep(12);
        sut_create(slow_func);
    }

    sut_shutdown();
}