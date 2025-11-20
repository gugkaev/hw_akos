#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>

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

int main() {
    signal(SIGINT, cleanup_handler);

    do {
        fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (fd == -1) {
            perror("shm_open client: trying again in 1 sec");
            sleep(1);
        } else {
            break;
        }
    } while (1);

    shm_ptr = (shared_data_t *)mmap(NULL, DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    shm_ptr->client_pid = getpid();
    printf("Client PID: %d\n", shm_ptr->client_pid);

    srand(time(NULL));

    while (1) {
        if (shm_ptr->should_exit) {
            break;
        }
        int random_data = rand() % 100;
        shm_ptr->data = random_data;
        shm_ptr->is_updated = 1;
        sleep(1);
    }

    shm_ptr->should_exit = 1;

    munmap(shm_ptr, DATA_SIZE);
    close(fd);
    return 0;
}