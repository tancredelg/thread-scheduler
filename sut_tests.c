#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "sut.c"


void func1() {
    struct timespec sleep_time = {0, 250000000};
    printf("func1: [");
    nanosleep(&sleep_time, NULL);
    fflush(stdout);
    int rng;
    for (int i = 0; i < 3; ++i) {
        printf("--");
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
    printf("]\n");
}

void func2() {
    struct timespec sleep_time = {0, 250000000};
    printf("func2: [");
    nanosleep(&sleep_time, NULL);
    fflush(stdout);
    int rng;
    for (int i = 0; i < 6; ++i) {
        rng = rand() % 23;
        if (rng == 0) sut_exit();
        if (rng == 1) sut_yield();
        printf("--");
        fflush(stdout);
        nanosleep(&sleep_time, NULL);
    }
    printf("]\n");
}

void openfile() {
    printf("openfile: going to open file.txt\n");
    int fd = sut_open("file.txt");
    printf("openfile: the fd = %d\n", fd);
}

void hello1() {
    int i, fd;
    char sbuf[128];
    fd = sut_open("./test4.txt");
    if (fd < 0)
        printf("Error: sut_open() failed\n");
    else {
        for (i = 0; i < 100; i++) {
            sprintf(sbuf, "Hello world!, message from SUT-One i = %d \n", i);
            sut_write(fd, sbuf, strlen(sbuf));
            sut_yield();
        }
        sut_close(fd);
    }
    sut_exit();
}

void hello2() {
    int i;
    for (i = 0; i < 100; i++) {
        printf("Hello world!, this is SUT-Two \n");
        sut_yield();
    }
    sut_exit();
}

void test4() {
    sut_init();
    sut_create(hello1);
    sut_create(hello2);
    sut_shutdown();
}

void test6() {
    sut_init();

    for (int i = 0; i < 15; ++i) {
        if (i == 7) sleep(12);
        sut_create(func2);
    }

    sut_shutdown();
}

void test7() {
    sut_init();
    sut_create(func1);
    sut_create(openfile);
    sut_create(func1);
    sleep(3);
    sut_create(func1);
    sleep(2);
    sut_shutdown();
}

int main() {
    //test4();
    //test6();
    test7();
    return 0;
}