#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#define MAX_BOOKS 100
#define MAX_READERS 50
#define MAX_MESSAGE_SIZE 256
#define FIFO_BASE_PATH "/tmp/library_fifo"
#define SHM_NAME "/library_shm"
#define MAX_OBSERVERS 10
#define FIFO_DIR "/tmp/library_observers"

typedef enum {
    BOOK_AVAILABLE = 0,
    BOOK_BORROWED = 1,
    BOOK_REQUESTED = 2
} book_status_t;

typedef struct {
    int id;
    book_status_t status;
    pid_t reader_pid;  
    int request_count; 
} book_t;

typedef struct {
    int num_books;
    book_t books[MAX_BOOKS];
    pid_t librarian_pid;
    int active_readers;
    int total_operations;
} library_state_t;

library_state_t* create_shared_memory();
library_state_t* attach_shared_memory();
void detach_shared_memory(library_state_t* shm);
void destroy_shared_memory();

sem_t* create_book_semaphore(int book_id);
sem_t* open_book_semaphore(int book_id);
void close_semaphore(sem_t* sem, int book_id);
void destroy_all_semaphores(int num_books);

int create_observer_fifo(int observer_id);
int open_observer_fifo(int observer_id);
void close_fifo(int fd);
void destroy_observer_fifo(int observer_id);
void destroy_all_observer_fifos();
int ensure_fifo_directory();
int open_observer_fifo_write(int observer_id);

void send_message_to_observers(const char* message);

volatile sig_atomic_t running;
void signal_handler(int sig);


void init_signal_handlers();
char* get_timestamp();

#endif 

