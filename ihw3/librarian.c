#include "common.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num_books>\n", argv[0]);
        exit(1);
    }
    
    int num_books = atoi(argv[1]);
    if (num_books <= 0 || num_books > MAX_BOOKS) {
        fprintf(stderr, "Invalid number of books (1-%d)\n", MAX_BOOKS);
        exit(1);
    }
    
    init_signal_handlers();
    
    printf("[БИБЛИОТЕКАРЬ %d] Запуск библиотеки с %d книгами\n", getpid(), num_books);
    printf("[БИБЛИОТЕКАРЬ %d] Нажмите Ctrl+C для завершения\n", getpid());
    
    library_state_t* shm = create_shared_memory();
    if (shm == NULL) {
        fprintf(stderr, "[БИБЛИОТЕКАРЬ] Ошибка создания разделяемой памяти\n");
        exit(1);
    }
    
    shm->num_books = num_books;
    shm->librarian_pid = getpid();
    shm->active_readers = 0;
    shm->total_operations = 0;
    
    for (int i = 0; i < num_books; i++) {
        shm->books[i].id = i;
        shm->books[i].status = BOOK_AVAILABLE;
        shm->books[i].reader_pid = 0;
        shm->books[i].request_count = 0;
    }
    
    sem_t* semaphores[MAX_BOOKS];
    for (int i = 0; i < num_books; i++) {
        semaphores[i] = create_book_semaphore(i);
        if (semaphores[i] == NULL || semaphores[i] == SEM_FAILED) {
            fprintf(stderr, "[БИБЛИОТЕКАРЬ] Ошибка создания семафора для книги %d\n", i);
            destroy_all_semaphores(i);
            destroy_shared_memory();
            exit(1);
        }
    }
    
    if (ensure_fifo_directory() == -1) {
        fprintf(stderr, "[БИБЛИОТЕКАРЬ] Ошибка создания директории для FIFO\n");
        destroy_all_semaphores(num_books);
        destroy_shared_memory();
        exit(1);
    }
    
    char msg[MAX_MESSAGE_SIZE];
    snprintf(msg, sizeof(msg), "[%s] [БИБЛИОТЕКАРЬ] Библиотека открыта. Книг: %d", 
             get_timestamp(), num_books);
    printf("%s\n", msg);
    send_message_to_observers(msg);
    
    while (running) {
        sleep(2);
        
        for (int i = 0; i < num_books; i++) {
            if (shm->books[i].request_count > 0 && 
                shm->books[i].status == BOOK_AVAILABLE) {
                
                snprintf(msg, sizeof(msg), "[%s] [БИБЛИОТЕКАРЬ] Книга %d доступна! Ожидающих: %d", 
                         get_timestamp(), i, shm->books[i].request_count);
                printf("%s\n", msg);
                send_message_to_observers(msg);
            }
        }
        
        static int report_counter = 0;
        if (++report_counter >= 10) {
            report_counter = 0;
            int available = 0;
            int borrowed = 0;
            int requested = 0;
            
            for (int i = 0; i < num_books; i++) {
                if (shm->books[i].status == BOOK_AVAILABLE) available++;
                else if (shm->books[i].status == BOOK_BORROWED) borrowed++;
                else if (shm->books[i].status == BOOK_REQUESTED) requested++;
            }
            
            snprintf(msg, sizeof(msg), 
                     "[%s] [БИБЛИОТЕКАРЬ] Статистика: Доступно: %d, Взято: %d, Запрошено: %d, Читателей: %d", 
                     get_timestamp(), available, borrowed, requested, shm->active_readers);
            printf("%s\n", msg);
            send_message_to_observers(msg);
        }
    }
    
    printf("\n[БИБЛИОТЕКАРЬ] Завершение работы...\n");
    snprintf(msg, sizeof(msg), "[%s] [БИБЛИОТЕКАРЬ] Библиотека закрывается", get_timestamp());
    send_message_to_observers(msg);
    
    for (int i = 0; i < num_books; i++) {
        close_semaphore(semaphores[i], i);
    }
    
    sleep(1);
    
    destroy_all_semaphores(num_books);
    destroy_shared_memory();
    destroy_all_observer_fifos();
    
    printf("[БИБЛИОТЕКАРЬ] Завершен\n");
    return 0;
}

