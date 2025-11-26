#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

volatile sig_atomic_t ack = 0;
volatile sig_atomic_t receiver_pid = 0;

void handle_ack(int sig) {
    ack = 1;
}

int main() {
    printf("Sender PID = %d\n", getpid());
    printf("Введите PID receiver: ");
    scanf("%d", &receiver_pid);
    int num;
    printf("Введите число для отправки: ");
    scanf("%d", &num);
    struct sigaction sa = {0};
    sa.sa_handler = handle_ack;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    printf("Отправка...\n");
    for (int bit = 31; bit >= 0; bit--) {
        ack = 0;
        int b = (num >> bit) & 1;
        if (b == 0) {
            kill(receiver_pid, SIGUSR1);
        } else {
            kill(receiver_pid, SIGUSR2);
        }
        while (!ack) {
            pause();
        }
    }
    kill(receiver_pid, SIGINT);
    printf("Передача завершена\n");
    return 0;
}