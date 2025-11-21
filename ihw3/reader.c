#include "common.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <reader_id> <num_books>\n", argv[0]);
        fprintf(stderr, "  reader_id: уникальный идентификатор читателя\n");
        fprintf(stderr, "  num_books: количество книг в библиотеке\n");
        exit(1);
    }
    
    int reader_id = atoi(argv[1]);
    int num_books = atoi(argv[2]);
    
    if (num_books <= 0 || num_books > MAX_BOOKS) {
        fprintf(stderr, "Invalid number of books (1-%d)\n", MAX_BOOKS);
        exit(1);
    }
    
    init_signal_handlers();
    
    pid_t my_pid = getpid();
    printf("[ЧИТАТЕЛЬ %d-%d] Запущен (PID: %d)\n", reader_id, my_pid, my_pid);
    
    library_state_t* shm = attach_shared_memory();
    if (shm == NULL) {
        fprintf(stderr, "[ЧИТАТЕЛЬ %d] Ошибка подключения к разделяемой памяти\n", reader_id);
        exit(1);
    }
    
    shm->active_readers++;
    
    sem_t* semaphores[MAX_BOOKS];
    for (int i = 0; i < num_books; i++) {
        semaphores[i] = open_book_semaphore(i);
        if (semaphores[i] == NULL || semaphores[i] == SEM_FAILED) {
            fprintf(stderr, "[ЧИТАТЕЛЬ %d] Ошибка открытия семафора для книги %d\n", reader_id, i);
            for (int j = 0; j < i; j++) {
                close_semaphore(semaphores[j], j);
            }
            shm->active_readers--;
            detach_shared_memory(shm);
            exit(1);
        }
    }
    
    char msg[MAX_MESSAGE_SIZE];
    snprintf(msg, sizeof(msg), "[%s] [ЧИТАТЕЛЬ %d] Пришел в библиотеку", 
             get_timestamp(), reader_id);
    printf("%s\n", msg);
    send_message_to_observers(msg);
    
    srand(time(NULL) ^ my_pid); 
    
    int books_to_read = 1 + (rand() % 3); 
    int requested_books[MAX_BOOKS];
    int books_borrowed[MAX_BOOKS];
    int num_borrowed = 0;
    
    snprintf(msg, sizeof(msg), "[%s] [ЧИТАТЕЛЬ %d] Хочет прочитать %d книг", 
             get_timestamp(), reader_id, books_to_read);
    printf("%s\n", msg);
    send_message_to_observers(msg);
    
    for (int i = 0; i < books_to_read; i++) {
        int book_id = rand() % num_books;
        requested_books[i] = book_id;
    }
    
    for (int attempt = 0; attempt < books_to_read && running; attempt++) {
        int book_id = requested_books[attempt];
        
        if (sem_wait(semaphores[book_id]) == -1) {
            perror("sem_wait");
            continue;
        }
        
        if (shm->books[book_id].status == BOOK_AVAILABLE) {
            shm->books[book_id].status = BOOK_BORROWED;
            shm->books[book_id].reader_pid = my_pid;
            shm->books[book_id].request_count = 0;
            shm->total_operations++;
            books_borrowed[num_borrowed++] = book_id;
            
            snprintf(msg, sizeof(msg), 
                     "[%s] [ЧИТАТЕЛЬ %d] Взял книгу %d", 
                     get_timestamp(), reader_id, book_id);
            printf("%s\n", msg);
            send_message_to_observers(msg);
        } else {
            shm->books[book_id].request_count++;
            snprintf(msg, sizeof(msg), 
                     "[%s] [ЧИТАТЕЛЬ %d] Книга %d занята. Добавлен в очередь. Запросов: %d", 
                     get_timestamp(), reader_id, book_id, shm->books[book_id].request_count);
            printf("%s\n", msg);
            send_message_to_observers(msg);
        }
        
        sem_post(semaphores[book_id]);
        
        if (shm->books[book_id].status != BOOK_BORROWED) {
            int waiting = 1;
            while (waiting && running) {
                sleep(1);
                
                if (sem_wait(semaphores[book_id]) == -1) {
                    perror("sem_wait (waiting)");
                    break;
                }
                
                if (shm->books[book_id].status == BOOK_AVAILABLE) {
                    shm->books[book_id].status = BOOK_BORROWED;
                    shm->books[book_id].reader_pid = my_pid;
                    shm->books[book_id].request_count--;
                    shm->total_operations++;
                    books_borrowed[num_borrowed++] = book_id;
                    
                    snprintf(msg, sizeof(msg), 
                             "[%s] [ЧИТАТЕЛЬ %d] Получил ожидаемую книгу %d!", 
                             get_timestamp(), reader_id, book_id);
                    printf("%s\n", msg);
                    send_message_to_observers(msg);
                    waiting = 0;
                }
                
                sem_post(semaphores[book_id]);
            }
        }
        
        usleep(500000);
    }
    
    if (num_borrowed > 0 && running) {
        int reading_days = 2 + (rand() % 5); 
        snprintf(msg, sizeof(msg), 
                 "[%s] [ЧИТАТЕЛЬ %d] Читает %d книг в течение %d дней", 
                 get_timestamp(), reader_id, num_borrowed, reading_days);
        printf("%s\n", msg);
        send_message_to_observers(msg);
        
        for (int day = 0; day < reading_days && running; day++) {
            sleep(3); 
            
            if ((day + 1) % 2 == 0) {
                snprintf(msg, sizeof(msg), 
                         "[%s] [ЧИТАТЕЛЬ %d] Читает... День %d/%d", 
                         get_timestamp(), reader_id, day + 1, reading_days);
                printf("%s\n", msg);
                send_message_to_observers(msg);
            }
        }
        
        if (running) {
            for (int i = 0; i < num_borrowed; i++) {
                int book_id = books_borrowed[i];
                
                if (sem_wait(semaphores[book_id]) == -1) {
                    perror("sem_wait (return)");
                    continue;
                }
                
                if (shm->books[book_id].reader_pid == my_pid) {
                    shm->books[book_id].status = BOOK_AVAILABLE;
                    shm->books[book_id].reader_pid = 0;
                    shm->total_operations++;
                    
                    snprintf(msg, sizeof(msg), 
                             "[%s] [ЧИТАТЕЛЬ %d] Вернул книгу %d", 
                             get_timestamp(), reader_id, book_id);
                    printf("%s\n", msg);
                    send_message_to_observers(msg);
                }
                
                sem_post(semaphores[book_id]);
            }
        }
    }
    
    snprintf(msg, sizeof(msg), "[%s] [ЧИТАТЕЛЬ %d] Уходит из библиотеки", 
             get_timestamp(), reader_id);
    printf("%s\n", msg);
    send_message_to_observers(msg);
    
    shm->active_readers--;
    
    for (int i = 0; i < num_books; i++) {
        close_semaphore(semaphores[i], i);
    }
    
    detach_shared_memory(shm);
    
    printf("[ЧИТАТЕЛЬ %d] Завершен\n", reader_id);
    return 0;
}

