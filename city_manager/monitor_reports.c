#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#define PID_FILE ".monitor_pid"

/*
 * Handler pentru SIGUSR1
 * Apelat automat cand city_manager trimite SIGUSR1
 * Inseamna ca un raport nou a fost adaugat
 */
void handle_sigusr1(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "[monitor] New report added.\n", 28);
}

/* 
 * Handler pentru SIGINT (Ctrl+C)
 * Stergem .monitor_pid si oprim programul
 */
void handle_sigint(int sig) {
    (void)sig;
    unlink(PID_FILE);
    write(STDOUT_FILENO, "[monitor] Shutting down.\n", 25);
    _exit(0);
}

int main() {
    /* Scriem PID-ul nostru in .monitor_pid */
    char pid_str[32];
    int len = snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());

    int fd = open(PID_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) { perror("open .monitor_pid"); return 1; }
    write(fd, pid_str, len);
    close(fd);

    printf("[monitor] Started with PID %d\n", getpid());

    /* Configuram SIGUSR1 cu sigaction */
    struct sigaction sa_usr1;
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    /* Configuram SIGINT cu sigaction */
    struct sigaction sa_int;
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    /* Asteptam semnale la infinit */
    while (1) {
        pause();
    }

    return 0;
}