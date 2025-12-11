#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>

typedef struct {
    int id;
    bool is_available;
    pthread_cond_t cond;
    int waiting_count;
} book_t;

typedef struct {
    int id;
    int num_books;
    int *book_ids;
} reader_t;

static book_t *books = NULL;
static int num_books = 0;
static int num_readers = 0;
static pthread_mutex_t library_mutex = PTHREAD_MUTEX_INITIALIZER;
static int active_readers = 0;
static pthread_mutex_t active_readers_mutex = PTHREAD_MUTEX_INITIALIZER;
static int event_counter = 0;

void log_event_unlocked(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    event_counter++;
    printf("[Событие %d] ", event_counter);
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    
    va_end(args);
}

void log_event(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    pthread_mutex_lock(&library_mutex);
    event_counter++;
    printf("[Событие %d] ", event_counter);
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&library_mutex);
    
    va_end(args);
}

int init_books(int n) {
    books = (book_t *)malloc(n * sizeof(book_t));
    if (books == NULL) {
        fprintf(stderr, "Ошибка: не удалось выделить память для книг\n");
        return -1;
    }
    
    for (int i = 0; i < n; i++) {
        books[i].id = i;
        books[i].is_available = true;
        books[i].waiting_count = 0;
        
        if (pthread_cond_init(&books[i].cond, NULL) != 0) {
            fprintf(stderr, "Ошибка: не удалось инициализировать условную переменную для книги %d\n", i);
            for (int j = 0; j < i; j++) {
                pthread_cond_destroy(&books[j].cond);
            }
            free(books);
            return -1;
        }
    }
    
    return 0;
}

void cleanup_books(void) {
    if (books != NULL) {
        for (int i = 0; i < num_books; i++) {
            pthread_cond_destroy(&books[i].cond);
        }
        free(books);
        books = NULL;
    }
}

void take_book(int reader_id, int book_id) {
    pthread_mutex_lock(&library_mutex);
    
    log_event_unlocked("Читатель %d пытается взять книгу %d", reader_id, book_id);
    
    while (!books[book_id].is_available) {
        books[book_id].waiting_count++;
        log_event_unlocked("Читатель %d ждет освобождения книги %d (ожидающих: %d)", 
                  reader_id, book_id, books[book_id].waiting_count);
        
        pthread_cond_wait(&books[book_id].cond, &library_mutex);
        
        books[book_id].waiting_count--;
    }
    
    books[book_id].is_available = false;
    log_event_unlocked("Читатель %d взял книгу %d", reader_id, book_id);
    
    pthread_mutex_unlock(&library_mutex);
}

void return_book(int reader_id, int book_id) {
    pthread_mutex_lock(&library_mutex);
    
    books[book_id].is_available = true;
    log_event_unlocked("Читатель %d вернул книгу %d", reader_id, book_id);
    
    if (books[book_id].waiting_count > 0) {
        log_event_unlocked("Уведомление %d читателей о доступности книги %d", 
                  books[book_id].waiting_count, book_id);
        pthread_cond_broadcast(&books[book_id].cond);
    }
    
    pthread_mutex_unlock(&library_mutex);
}

void *reader_thread(void *arg) {
    reader_t *reader = (reader_t *)arg;
    int reader_id = reader->id;
    
    log_event("Читатель %d начал работу (хочет взять %d книг)", reader_id, reader->num_books);
    
    unsigned int seed = (unsigned int)(time(NULL) ^ (unsigned long)pthread_self());
    
    bool *selected = (bool *)calloc(num_books, sizeof(bool));
    if (selected == NULL) {
        fprintf(stderr, "Ошибка: не удалось выделить память для читателя %d\n", reader_id);
        pthread_exit(NULL);
    }
    
    for (int i = 0; i < reader->num_books; i++) {
        int book_id;
        do {
            book_id = rand_r(&seed) % num_books;
        } while (selected[book_id]);
        
        selected[book_id] = true;
        reader->book_ids[i] = book_id;
    }
    
    free(selected);
    
    for (int i = 0; i < reader->num_books - 1; i++) {
        for (int j = i + 1; j < reader->num_books; j++) {
            if (reader->book_ids[i] > reader->book_ids[j]) {
                int temp = reader->book_ids[i];
                reader->book_ids[i] = reader->book_ids[j];
                reader->book_ids[j] = temp;
            }
        }
    }
    
    pthread_mutex_lock(&library_mutex);
    log_event_unlocked("Читатель %d выбрал книги (отсортированы по ID): ", reader_id);
    for (int i = 0; i < reader->num_books; i++) {
        printf("%d", reader->book_ids[i]);
        if (i < reader->num_books - 1) printf(", ");
    }
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&library_mutex);
    
    for (int i = 0; i < reader->num_books; i++) {
        take_book(reader_id, reader->book_ids[i]);
    }
    
    log_event("Читатель %d получил все книги и начал чтение", reader_id);
    
    int reading_time = 1 + (rand_r(&seed) % 3);
    sleep(reading_time);
    
    log_event("Читатель %d закончил чтение (читал %d секунд)", reader_id, reading_time);
    
    for (int i = 0; i < reader->num_books; i++) {
        return_book(reader_id, reader->book_ids[i]);
    }
    
    log_event("Читатель %d завершил работу", reader_id);
    
    pthread_mutex_lock(&active_readers_mutex);
    active_readers--;
    pthread_mutex_unlock(&active_readers_mutex);
    
    return NULL;
}

void cleanup_reader(reader_t *reader) {
    if (reader != NULL) {
        if (reader->book_ids != NULL) {
            free(reader->book_ids);
            reader->book_ids = NULL;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <число_книг> <число_читателей>\n", argv[0]);
        return 1;
    }
    
    num_books = atoi(argv[1]);
    num_readers = atoi(argv[2]);
    
    if (num_books <= 0 || num_readers <= 0) {
        fprintf(stderr, "Ошибка: число книг и читателей должно быть положительным\n");
        return 1;
    }
    
    if (num_books > 1000 || num_readers > 1000) {
        fprintf(stderr, "Ошибка: слишком большие значения (максимум 1000)\n");
        return 1;
    }
    
    printf("=== Библиотека: %d книг, %d читателей ===\n\n", num_books, num_readers);
    fflush(stdout);
    
    srand((unsigned int)time(NULL));
    
    if (init_books(num_books) != 0) {
        return 1;
    }
    
    pthread_t *threads = (pthread_t *)malloc(num_readers * sizeof(pthread_t));
    reader_t *readers = (reader_t *)malloc(num_readers * sizeof(reader_t));
    
    if (threads == NULL || readers == NULL) {
        fprintf(stderr, "Ошибка: не удалось выделить память для потоков/читателей\n");
        cleanup_books();
        if (threads) free(threads);
        if (readers) free(readers);
        return 1;
    }
    
    active_readers = num_readers;
    
    for (int i = 0; i < num_readers; i++) {
        readers[i].id = i;
        readers[i].num_books = 1 + (rand() % 3);
        readers[i].book_ids = (int *)malloc(readers[i].num_books * sizeof(int));
        
        if (readers[i].book_ids == NULL) {
            fprintf(stderr, "Ошибка: не удалось выделить память для книг читателя %d\n", i);
            for (int j = 0; j < i; j++) {
                cleanup_reader(&readers[j]);
            }
            cleanup_books();
            free(threads);
            free(readers);
            return 1;
        }
        
        if (pthread_create(&threads[i], NULL, reader_thread, &readers[i]) != 0) {
            fprintf(stderr, "Ошибка: не удалось создать поток для читателя %d: %s\n", 
                    i, strerror(errno));
            for (int j = 0; j < i; j++) {
                cleanup_reader(&readers[j]);
                pthread_join(threads[j], NULL);
            }
            cleanup_reader(&readers[i]);
            cleanup_books();
            free(threads);
            free(readers);
            return 1;
        }
    }
    
    for (int i = 0; i < num_readers; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Предупреждение: ошибка при ожидании потока читателя %d\n", i);
        }
    }
    
    printf("\nВсе читатели завершили работуn");
    
    for (int i = 0; i < num_readers; i++) {
        cleanup_reader(&readers[i]);
    }
    
    free(threads);
    free(readers);
    
    pthread_mutex_destroy(&library_mutex);
    pthread_mutex_destroy(&active_readers_mutex);
    
    cleanup_books();
    
    return 0;
}