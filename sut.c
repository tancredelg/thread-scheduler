#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sut.h"


/// @brief Initializes the SUT library. It should be called before making any other API calls.
void sut_init() {
    
}

/// @brief Creates a task with the given function `fn` as its main body. 
/// @param fn The task function to create a task for.
/// @return true (1) on success, false (0) otherwise.
bool sut_create(sut_task_f fn) {
	return 0;
}

/// @brief Allows a running task to yield execution before completing its function.
void sut_yield() {

}

/// @brief Terminates the execution of a task. If there are multiple tasks running, calling this function will only terminate the current task.
void sut_exit() {

}

/// @brief Requests the system to open the file specified by `file_name`.
/// @param file_name The name of the file to open.
/// @return -1 if the file does not exist, 0 otherwise.
int sut_open(char *file_name) {
    return 0;
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
