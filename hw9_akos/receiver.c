#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

volatile sig_atomic_t sender_pid = 0;
volatile sig_atomic_t bit_pos = 31;
volatile sig_atomic_t result = 0;
volatile sig_atomic_t bit_received = 0;
volatile sig_atomic_t ready = 0;

void handle_bit0(int sig, siginfo_t *info, void *context) {
    if (!ready) return;
    if (info->si_pid == sender_pid) {
        result &= ~(1U << bit_pos);
        bit_received = 1;
    }
}

void handle_bit1(int sig, siginfo_t *info, void *context) {
    if (!ready) return;
    if (info->si_pid == sender_pid) {
        result |= (1U << bit_pos);
        bit_received = 1;
    }
}

void handle_finish(int sig) {
    if (!ready) return;
    printf("\nПриём завершён. Получено число: %d\n", result);
    exit(0);
}

int main() {
    printf("Receiver PID = %d\n", getpid());
    struct sigaction sa0 = {0}, sa1 = {0}, safin = {0};
    sa0.sa_sigaction = handle_bit0;
    sa1.sa_sigaction = handle_bit1;
    safin.sa_handler = handle_finish;
    sa0.sa_flags = SA_SIGINFO;
    sa1.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &sa0, NULL);
    sigaction(SIGUSR2, &sa1, NULL);
    sigaction(SIGINT, &safin, NULL);
    printf("Введите PID sender: ");
    scanf("%d", &sender_pid);
    ready = 1;
    printf("Ожидание битов...\n");
    while (1) {
        pause();
        if (bit_received) {
            kill(sender_pid, SIGUSR1);
            bit_received = 0;
            bit_pos--;
            if (bit_pos < 0) {
                printf("\nПриём завершён. Получено число: %d\n", result);
                exit(0);
            }
        }
    }
}