#include "common.h"

volatile sig_atomic_t running = 1;

library_state_t* create_shared_memory() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return NULL;
    }
    
    if (ftruncate(shm_fd, sizeof(library_state_t)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return NULL;
    }
    
    library_state_t* shm = (library_state_t*)mmap(NULL, sizeof(library_state_t),
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    
    if (shm == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME);
        return NULL;
    }
    
    return shm;
}

library_state_t* attach_shared_memory() {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return NULL;
    }
    
    library_state_t* shm = (library_state_t*)mmap(NULL, sizeof(library_state_t),
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    
    if (shm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    
    return shm;
}

void detach_shared_memory(library_state_t* shm) {
    if (shm != NULL && shm != MAP_FAILED) {
        munmap(shm, sizeof(library_state_t));
    }
}

void destroy_shared_memory() {
    shm_unlink(SHM_NAME);
}

sem_t* create_book_semaphore(int book_id) {
    char sem_name[64];
    snprintf(sem_name, sizeof(sem_name), "/book_sem_%d", book_id);
    
    sem_t* sem = sem_open(sem_name, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open (create)");
        return NULL;
    }
    
    return sem;
}

sem_t* open_book_semaphore(int book_id) {
    char sem_name[64];
    snprintf(sem_name, sizeof(sem_name), "/book_sem_%d", book_id);
    
    sem_t* sem = sem_open(sem_name, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        return NULL;
    }
    
    return sem;
}

void close_semaphore(sem_t* sem, int book_id) {
    (void)book_id;
    if (sem != NULL && sem != SEM_FAILED) {
        sem_close(sem);
    }
}

void destroy_all_semaphores(int num_books) {
    for (int i = 0; i < num_books; i++) {
        char sem_name[64];
        snprintf(sem_name, sizeof(sem_name), "/book_sem_%d", i);
        sem_unlink(sem_name);
    }
}

int ensure_fifo_directory() {
    struct stat st;
    if (stat(FIFO_DIR, &st) == -1) {
        if (mkdir(FIFO_DIR, 0777) == -1) {
            perror("mkdir fifo directory");
            return -1;
        }
    }
    return 0;
}

int create_observer_fifo(int observer_id) {
    if (ensure_fifo_directory() == -1) {
        return -1;
    }
    
    char fifo_path[256];
    snprintf(fifo_path, sizeof(fifo_path), "%s/observer_%d", FIFO_DIR, observer_id);
    
    unlink(fifo_path);
    
    if (mkfifo(fifo_path, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            return -1;
        }
    }
    
    return 0;
}

int open_observer_fifo(int observer_id) {
    char fifo_path[256];
    snprintf(fifo_path, sizeof(fifo_path), "%s/observer_%d", FIFO_DIR, observer_id);
    
    int fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if (fd == -1 && errno != ENXIO && errno != ENOENT) {
        perror("open observer fifo");
    }
    return fd;
}

int open_observer_fifo_write(int observer_id) {
    char fifo_path[256];
    snprintf(fifo_path, sizeof(fifo_path), "%s/observer_%d", FIFO_DIR, observer_id);
    
    int fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if (fd == -1 && errno != ENXIO && errno != ENOENT) {
        return -1;
    }
    return fd;
}

void close_fifo(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

void destroy_observer_fifo(int observer_id) {
    char fifo_path[256];
    snprintf(fifo_path, sizeof(fifo_path), "%s/observer_%d", FIFO_DIR, observer_id);
    unlink(fifo_path);
}

void destroy_all_observer_fifos() {
    DIR* dir = opendir(FIFO_DIR);
    if (dir != NULL) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, "observer_") == entry->d_name) {
                char fifo_path[256];
                snprintf(fifo_path, sizeof(fifo_path), "%s/%s", FIFO_DIR, entry->d_name);
                unlink(fifo_path);
            }
        }
        closedir(dir);
    }
    rmdir(FIFO_DIR);
}

void send_message_to_observers(const char* message) {
    char msg_with_newline[MAX_MESSAGE_SIZE + 2];
    snprintf(msg_with_newline, sizeof(msg_with_newline), "%s\n", message);
    size_t msg_len = strlen(msg_with_newline);
    
    for (int i = 1; i <= MAX_OBSERVERS; i++) {
        int fd = open_observer_fifo_write(i);
        if (fd >= 0) {
            write(fd, msg_with_newline, msg_len);
            close(fd);
        }
    }
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
    }
}

void init_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}


char* get_timestamp() {
    static char buffer[64];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
    return buffer;
}

