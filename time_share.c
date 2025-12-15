/* 
    AoL Assessment - Time-Sharing Scheduler Simulation (Round Robin)
    Authors:
    2702273315 - Kenneth Sunjaya
    2702262993 - Sherly Oktavia Willisa
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

// SIGALRM  : menentukan kapan preemption terjadi
// SIGCONT  : melanjutkan eksekusi proses yang dihentikan
// SIGSTOP  : menghentikan proses secara paksa

// process state di kode ini ada 3 yaitu: RUNNING, STOPPED (hanya diberhentikan sementara), TERMINATED (process selesai dan tidak dapat dijalankan lagi), 

#define QUANTUM_MS 500 // setiap QUANTUM_MS milidetik akan terjadi preemption

int N = 0; // number of child processes
pid_t *children = NULL; 
volatile int current = 0;

void scheduler(int sig) {
    (void)sig;
    if (children[current] > 0) {
        kill(children[current], SIGSTOP);
    }

    current = (current + 1) % N;
    if (children[current] > 0) {
        kill(children[current], SIGCONT);
    }
}

void child_cont_handler(int sig);
char msgbuf[128];

void child_term_handler(int sig) {
    (void)sig; 
    _exit(0);
}

void child_cont_handler(int sig) {
    (void)sig;
    write(STDOUT_FILENO, msgbuf, strlen(msgbuf));
}

void shutdown_handler(int sig) {
    (void)sig;

    struct itimerval it = {0};
    setitimer(ITIMER_REAL, &it, NULL);

    for (int i = 0; i < N; ++i) {
        if (children[i] > 0) {
            kill(children[i], SIGCONT);
            kill(children[i], SIGTERM);
        }
    }

    for (int i = 0; i < N; ++i) {
        waitpid(children[i], NULL, 0);
    }

    write(STDOUT_FILENO, "\nParent: simulation finished.\n", 30);

    _exit(0);
}

void child_main(int id) {
    snprintf(msgbuf, sizeof(msgbuf), "Child %d (pid=%d) resumed\n", id, getpid());
    struct sigaction sa;

    sa.sa_handler = child_cont_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCONT, &sa, NULL);

    sa.sa_handler = child_term_handler;
    sigaction(SIGTERM, &sa, NULL);

    while (1) {
        pause();
    }
    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <number of child processes>\n", argv[0]);
        exit(1);
    }
    N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "Number of children must be positive.\n");
        exit(1);
    }
    children = malloc(sizeof(pid_t) * N);
    if (children == NULL) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < N; ++i) {
        pid_t retval = fork();
        if (retval < 0) {
            perror("fork");
            exit(1);
        }
        if (retval == 0) {
            // Child process
            child_main(i);
        }
        children[i] = retval;
    }

    for (int i = 0; i < N; ++i) {
        kill(children[i], SIGSTOP);
    }

    current = 0;
    kill(children[current], SIGCONT);

    struct sigaction sa_parent;
    sa_parent.sa_handler = scheduler;

    sigemptyset(&sa_parent.sa_mask);
    sa_parent.sa_flags = SA_RESTART;

    if (sigaction(SIGALRM, &sa_parent, NULL) < 0) {
        perror("sigaction SIGALRM");
    }

    struct sigaction sa_shutdown;
    sa_shutdown.sa_handler = shutdown_handler;
    sigemptyset(&sa_shutdown.sa_mask);
    sa_shutdown.sa_flags = SA_RESTART;
    sigaction(SIGINT,  &sa_shutdown, NULL);  // Ctrl+C
    sigaction(SIGTERM, &sa_shutdown, NULL);


    struct itimerval it;
    it.it_interval.tv_sec = QUANTUM_MS / 1000;
    it.it_interval.tv_usec = (QUANTUM_MS % 1000) * 1000;

    it.it_value = it.it_interval;

    if (setitimer(ITIMER_REAL, &it, NULL) < 0) {
        perror("setitimer");
    }
    
    while (1) {
        pause();
    }

    return 0;
}

