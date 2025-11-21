#include "common.h"

int main(int argc, char* argv[]) {
    int observer_id = 1;
    
    if (argc >= 2) {
        observer_id = atoi(argv[1]);
    }
    
    init_signal_handlers();
    
    pid_t my_pid = getpid();
    printf("[НАБЛЮДАТЕЛЬ %d (PID: %d)] Запущен. Нажмите Ctrl+C для завершения\n", 
           observer_id, my_pid);
    
    if (create_observer_fifo(observer_id) == -1) {
        fprintf(stderr, "[НАБЛЮДАТЕЛЬ %d] Ошибка создания FIFO\n", observer_id);
        exit(1);
    }
    
    printf("[НАБЛЮДАТЕЛЬ %d] Подключен к библиотеке. Ожидание событий...\n\n", observer_id);
    
    int fifo_fd = -1;
    int attempts = 0;
    while (fifo_fd == -1 && attempts < 10 && running) {
        fifo_fd = open_observer_fifo(observer_id);
        if (fifo_fd == -1) {
            if (errno == ENXIO || errno == ENOENT) {
                usleep(500000); 
                attempts++;
            } else {
                perror("open observer fifo");
                break;
            }
        }
    }
    
    if (fifo_fd == -1) {
        char fifo_path[256];
        snprintf(fifo_path, sizeof(fifo_path), "%s/observer_%d", FIFO_DIR, observer_id);
        fifo_fd = open(fifo_path, O_RDONLY);
        if (fifo_fd == -1) {
            perror("open observer fifo (blocking)");
            exit(1);
        }
    }
    
    char buffer[MAX_MESSAGE_SIZE + 1];
    ssize_t bytes_read;
    
    while (running) {
        bytes_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            if (buffer[bytes_read - 1] == '\n') {
                buffer[bytes_read - 1] = '\0';
            }
            
            if (strlen(buffer) > 0) {
                printf("[НАБЛЮДАТЕЛЬ %d] %s\n", observer_id, buffer);
                fflush(stdout);
            }
        } else if (bytes_read == 0) {
            usleep(100000); 
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000); 
            } else {
                perror("read fifo");
                usleep(500000); 
            }
        }
    }
    
    printf("\n[НАБЛЮДАТЕЛЬ %d] Завершение работы...\n", observer_id);
    close_fifo(fifo_fd);
    destroy_observer_fifo(observer_id);
    
    printf("[НАБЛЮДАТЕЛЬ %d] Завершен\n", observer_id);
    return 0;
}

