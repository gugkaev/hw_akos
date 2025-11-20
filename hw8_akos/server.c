#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>

#define SHM_NAME "/shm_example"
#define DATA_SIZE sizeof(shared_data_t)

typedef struct {
    int data;
    int is_updated;
    int should_exit;
    pid_t client_pid;
} shared_data_t;

int fd;
shared_data_t *shm_ptr;

void cleanup_handler(int sig) {
    if (shm_ptr) {
        shm_ptr->should_exit = 1;
    }
    exit(0);
}

void cleanup_atexit() {
    shm_unlink(SHM_NAME);
}

int main() {
    signal(SIGINT, cleanup_handler);

    shm_unlink(SHM_NAME);

    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open server");
        exit(1);
    }

    if (ftruncate(fd, DATA_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    shm_ptr = (shared_data_t *)mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    atexit(cleanup_atexit);

    printf("Server waiting for client PID...\n");

    while (shm_ptr->client_pid == 0) {
        usleep(100000);
    }

    printf("Server: client PID is %d\n", shm_ptr->client_pid);

    while (1) {
        if (shm_ptr->is_updated) {
            printf("Server: received data = %d\n", shm_ptr->data);
            shm_ptr->is_updated = 0;
        }
        if (shm_ptr->should_exit) {
            break;
        }
        if (kill(shm_ptr->client_pid, 0) == -1) {
            if (errno == ESRCH) {
                printf("Client died (PID %d), exiting server.\n", shm_ptr->client_pid);
                break;
            }
        }
        usleep(100000);
    }

    munmap(shm_ptr, DATA_SIZE);
    close(fd);
    return 0;
}