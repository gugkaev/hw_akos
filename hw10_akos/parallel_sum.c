#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    int hours;
    int minutes;
    int seconds;
    int milliseconds;
} Timestamp;

typedef struct {
    int *data;
    int capacity;
    int size;
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Buffer;

typedef struct {
    int id;
    int number;
} ThreadData;

Buffer buffer;
int numbers_processed = 0;
int total_numbers = 100;
int results_generated = 0;
bool all_processed = false;
pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

Timestamp get_current_time() {
    Timestamp ts;
    struct timespec spec;
    struct tm *tm_info;

    clock_gettime(CLOCK_REALTIME, &spec);
    tm_info = localtime(&spec.tv_sec);

    ts.hours = tm_info->tm_hour;
    ts.minutes = tm_info->tm_min;
    ts.seconds = tm_info->tm_sec;
    ts.milliseconds = spec.tv_nsec / 1000000;

    return ts;
}

void print_timestamp(Timestamp ts) {
    printf("[%02d:%02d:%02d.%03d] ",
           ts.hours, ts.minutes, ts.seconds, ts.milliseconds);
}

void log_message(const char *message) {
    pthread_mutex_lock(&global_mutex);

    Timestamp ts = get_current_time();
    print_timestamp(ts);
    printf("%s\n", message);

    pthread_mutex_unlock(&global_mutex);
}

int random_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

void buffer_init(Buffer *buf, int capacity) {
    buf->data = (int *)malloc(capacity * sizeof(int));
    buf->capacity = capacity;
    buf->size = 0;
    buf->head = 0;
    buf->tail = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->cond, NULL);
}

void buffer_destroy(Buffer *buf) {
    free(buf->data);
    pthread_mutex_destroy(&buf->mutex);
    pthread_cond_destroy(&buf->cond);
}

void buffer_add_number(Buffer *buf, int number, int source_id) {
    pthread_mutex_lock(&buf->mutex);

    while (buf->size >= buf->capacity) {
        pthread_cond_wait(&buf->cond, &buf->mutex);
    }

    buf->data[buf->tail] = number;
    buf->tail = (buf->tail + 1) % buf->capacity;
    buf->size++;

    pthread_mutex_unlock(&global_mutex);
    numbers_processed++;
    pthread_mutex_unlock(&global_mutex);

    char message[256];
    snprintf(message, sizeof(message),
             "Источник %d добавил число %d в буфер. В буфере: %d элементов",
             source_id, number, buf->size);
    log_message(message);

    pthread_cond_signal(&buf->cond);
    pthread_mutex_unlock(&buf->mutex);
}

void buffer_add_result(Buffer *buf, int result) {
    pthread_mutex_lock(&buf->mutex);

    while (buf->size >= buf->capacity) {
        pthread_cond_wait(&buf->cond, &buf->mutex);
    }

    buf->data[buf->tail] = result;
    buf->tail = (buf->tail + 1) % buf->capacity;
    buf->size++;

    pthread_mutex_lock(&global_mutex);
    results_generated++;

    if (numbers_processed >= total_numbers && buf->size == 1 &&
        results_generated == total_numbers - 1) {
        all_processed = true;
        char final_msg[256];
        snprintf(final_msg, sizeof(final_msg),
                 "ПОЛУЧЕН ФИНАЛЬНЫЙ РЕЗУЛЬТАТ: %d", result);
        log_message(final_msg);
    }
    pthread_mutex_unlock(&global_mutex);

    char message[256];
    snprintf(message, sizeof(message),
             "Сумматор добавил результат %d в буфер. В буфере: %d элементов",
             result, buf->size);
    log_message(message);

    pthread_cond_signal(&buf->cond);
    pthread_mutex_unlock(&buf->mutex);
}

bool buffer_get_two_numbers(Buffer *buf, int *num1, int *num2) {
    pthread_mutex_lock(&buf->mutex);

    while (buf->size < 2 && !all_processed) {
        pthread_cond_wait(&buf->cond, &buf->mutex);
    }

    if (all_processed && buf->size < 2) {
        pthread_mutex_unlock(&buf->mutex);
        return false;
    }

    *num1 = buf->data[buf->head];
    buf->head = (buf->head + 1) % buf->capacity;
    buf->size--;

    *num2 = buf->data[buf->head];
    buf->head = (buf->head + 1) % buf->capacity;
    buf->size--;

    char message[256];
    snprintf(message, sizeof(message),
             "Отслеживающий поток взял числа %d и %d для суммирования. В буфере осталось: %d элементов",
             *num1, *num2, buf->size);
    log_message(message);

    pthread_cond_signal(&buf->cond);
    pthread_mutex_unlock(&buf->mutex);
    return true;
}

void print_buffer_state(Buffer *buf) {
    pthread_mutex_lock(&buf->mutex);

    char state[1024] = "Состояние буфера (";
    char temp[50];

    snprintf(temp, sizeof(temp), "%d элементов): [", buf->size);
    strcat(state, temp);

    for (int i = 0; i < buf->size; i++) {
        int index = (buf->head + i) % buf->capacity;
        snprintf(temp, sizeof(temp), "%d", buf->data[index]);
        strcat(state, temp);

        if (i < buf->size - 1) {
            strcat(state, ", ");
        }
    }

    strcat(state, "]");
    log_message(state);

    pthread_mutex_unlock(&buf->mutex);
}

void* source_thread(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int source_id = data->id;

    char message[256];
    snprintf(message, sizeof(message), "Источник %d запущен", source_id);
    log_message(message);

    int sleep_time = random_range(1, 7);
    snprintf(message, sizeof(message),
             "Источник %d заснет на %d секунд", source_id, sleep_time);
    log_message(message);

    sleep(sleep_time);

    int number = random_range(1, 100);
    snprintf(message, sizeof(message),
             "Источник %d сгенерировал число %d", source_id, number);
    log_message(message);

    buffer_add_number(&buffer, number, source_id);

    snprintf(message, sizeof(message), "Источник %d завершил работу", source_id);
    log_message(message);

    free(data);
    return NULL;
}

void* adder_thread(void *arg) {
    int *nums = (int *)arg;
    int num1 = nums[0];
    int num2 = nums[1];

    char message[256];
    snprintf(message, sizeof(message),
             "Сумматор запущен для чисел %d и %d", num1, num2);
    log_message(message);

    int sum_time = random_range(3, 6);
    snprintf(message, sizeof(message),
             "Сумматор будет суммировать %d секунд", sum_time);
    log_message(message);

    sleep(sum_time);

    int result = num1 + num2;
    snprintf(message, sizeof(message),
             "Сумматор завершил вычисление: %d + %d = %d", num1, num2, result);
    log_message(message);

    buffer_add_result(&buffer, result);

    free(nums);
    return NULL;
}

void* tracker_thread(void *arg) {
    (void)arg;

    log_message("Отслеживающий поток запущен");

    int check_count = 0;

    while (!all_processed) {
        int num1, num2;

        if (buffer_get_two_numbers(&buffer, &num1, &num2)) {
            int *nums = (int *)malloc(2 * sizeof(int));
            nums[0] = num1;
            nums[1] = num2;

            pthread_t adder_tid;
            pthread_create(&adder_tid, NULL, adder_thread, nums);
            pthread_detach(adder_tid);

            char message[256];
            snprintf(message, sizeof(message),
                     "Отслеживающий поток создал сумматор для чисел %d и %d",
                     num1, num2);
            log_message(message);

            if (++check_count % 5 == 0) {
                print_buffer_state(&buffer);
            }
        }

        usleep(100000);
    }

    log_message("Отслеживающий поток завершил работу");
    return NULL;
}

int main() {
    srand(time(NULL));

    buffer_init(&buffer, 200);

    log_message("Начало вычислений");
    log_message("Создание 100 источников данных...");

    pthread_t tracker_tid;
    pthread_create(&tracker_tid, NULL, tracker_thread, NULL);

    pthread_t source_tids[100];

    for (int i = 0; i < 100; i++) {
        ThreadData *data = (ThreadData *)malloc(sizeof(ThreadData));
        data->id = i + 1;

        pthread_create(&source_tids[i], NULL, source_thread, data);
    }

    for (int i = 0; i < 100; i++) {
        pthread_join(source_tids[i], NULL);
    }

    log_message("Все источники завершили работу");

    while (!all_processed) {
        sleep(1);
        print_buffer_state(&buffer);
    }

    pthread_join(tracker_tid, NULL);

    char stats[256];
    snprintf(stats, sizeof(stats),
             "Статистика: обработано чисел: %d, сгенерировано результатов: %d",
             numbers_processed, results_generated);
    log_message(stats);

    log_message("Вычисления завершены");

    buffer_destroy(&buffer);
    pthread_mutex_destroy(&global_mutex);

    return 0;
}