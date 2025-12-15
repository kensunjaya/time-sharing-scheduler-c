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
pid_t *children = NULL; // kita set NULL dulu, nanti di malloc setelah tau N dari argumen di command line
volatile int current = 0;

// scheduler adalah fungsi yang akan dijalankan setiap kali timer preemption (SIGALRM) terjadi
void scheduler(int sig) {
    (void)sig; // hanya untuk menghindari warning unused parameter / var pada compiler karena kita ga pernah pakai nilai sig di fungsi ini
    if (children[current] > 0) {
        kill(children[current], SIGSTOP); // stop proses yang sedang berjalan
    }

    // cari proses berikutnya yang masih hidup
    current = (current + 1) % N;
    if (children[current] > 0) {
        kill(children[current], SIGCONT); // lanjutkan proses berikutnya
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
    write(STDOUT_FILENO, msgbuf, strlen(msgbuf)); // gunakan write karena async-signal-safe, ini yang diprint di terminal saat proses di-continue
}

void shutdown_handler(int sig) {
    (void)sig;

    // Matikan timer (menghentikan preemption) sehingga kernel tidak lagi mengirim SIGALRM
    // ini harus dilakukan dulu agar tidak terjadi race condition saat kita mau mengterminate child processes
    struct itimerval it = {0};
    setitimer(ITIMER_REAL, &it, NULL); // it_value = 0, it_interval = 0. fungsi ini mematikan timer round robin

    // Hentikan semua child dengan aman
    for (int i = 0; i < N; ++i) {
        if (children[i] > 0) {
            kill(children[i], SIGCONT); // harus di continue dulu, karena saat kondisi STOP tidak bisa di terminate. Tanpa ini, code bisa stuck forever di bagian waitpid yg setelah ini
            kill(children[i], SIGTERM); // minta terminate, karena child punya handler untuk SIGTERM (child_term_handler), maka _exit(0) akan membersihkan proses dengan rapi
        }
    }

    // Tunggu semua child exit (hindari terjadi zombie process)
    for (int i = 0; i < N; ++i) {
        waitpid(children[i], NULL, 0); // yg dilakukan di sini: mengambil exit status dan menghapus entry di process table kernel.
    }

    // Kok bukan printf? Karena printf tidak async-signal-safe, bisa menyebabkan deadlock kalau dipanggil di signal handler (shutdown  handler ini adalah signal handler)
    write(STDOUT_FILENO, "\nParent: simulation finished.\n", 30);

    _exit(0); // _exit merupakan versi lebih aman daripada exit pada signal handler
}

// child_main mendefinisikan apa aja yg dilakukan sebuah process ketika ia diberi CPU dan bagaimana ia bereaksi terhadap sinyal dari scheduler (parent)
void child_main(int id) {
    snprintf(msgbuf, sizeof(msgbuf), "Child %d (pid=%d) resumed\n", id, getpid()); // siapkan pesan yang akan ditampilkan saat proses di-continue
    struct sigaction sa;
    // handler untuk SIGCONT (resume process)
    sa.sa_handler = child_cont_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCONT, &sa, NULL);

    // hadnler untuk SIGTERM (terminate process)
    sa.sa_handler = child_term_handler;
    sigaction(SIGTERM, &sa, NULL);

    while (1) {
        pause(); // Wait for signals
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
        pid_t retval = fork(); // retval = child's PID
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

    // [SIGSTOP] harus di sigstop dulu setelah difork, kalau enggak nanti langsung jalan semua secara bersamaan, dan scheduler lose control
    for (int i = 0; i < N; ++i) {
        kill(children[i], SIGSTOP);
    }

    // [SIGCONT] setelah distop semua, beri giliran process pertama untuk run lagi. Kali ini h anya process pertama dulu yang jalan, gak bersamaan dengan proses lainnya
    current = 0;
    kill(children[current], SIGCONT);

    // signal handler milik parent (scheduler). 
    struct sigaction sa_parent;
    // Ketika SIGALRM datang, jalankan fungsi scheduler()
    sa_parent.sa_handler = scheduler;

    // sigemptyset memperbolehkan signal lain masuk selama scheduler sedang berjalan, kalau sigaddset maka artinya tambahkan signal yang tidak boleh masuk selama scheduler berjalan
    sigemptyset(&sa_parent.sa_mask);

    // [sa_parent.sa_flags = SA_RESTART] kalau system call (misal sleep()) terganggu oleh signal, maka system call tersebut akan di-restart kembali otomatis
    // tanpa adanya SA_RESTART, maka sleep() bisa berhenti lebih cepat dan program jadi tidak stabil
    sa_parent.sa_flags = SA_RESTART;

    // setiap ada SIGALRM yg datang ke process parent ini, maka jalankan scheduler()
    if (sigaction(SIGALRM, &sa_parent, NULL) < 0) {
        perror("sigaction SIGALRM");
    }

    // signal handler untuk shutdown (menghandle SIGINT dari Ctrl+C dan SIGTERM dari kill)
    struct sigaction sa_shutdown;
    sa_shutdown.sa_handler = shutdown_handler;
    sigemptyset(&sa_shutdown.sa_mask);
    sa_shutdown.sa_flags = SA_RESTART;
    sigaction(SIGINT,  &sa_shutdown, NULL);  // Ctrl+C
    sigaction(SIGTERM, &sa_shutdown, NULL);  // kill

    // * sampai tahap ini, parent sudah siap untuk menghandle timer interrupt (SIGALRM) dan menjalankan scheduler secara berkala

    struct itimerval it;
    it.it_interval.tv_sec = QUANTUM_MS / 1000; // bagian detik
    it.it_interval.tv_usec = (QUANTUM_MS % 1000) * 1000; // bagian mikrodetik

    it.it_value = it.it_interval; // menyatakan kapan timer pertama kali akan berjalan. Kalau QUANTUM_MS = 500ms, maka setelah 500ms pertama SIGALRM akan dikirimkan

    // setitimer mengatur timer untuk mengirim SIGALRM ke proses ini (parent) setiap QUANTUM_MS milidetik
    if (setitimer(ITIMER_REAL, &it, NULL) < 0) {
        perror("setitimer");
    }
    // * sampai tahap ini, quantum timer udah aktif dan preemption sudah berjalan periodik
    
    while (1) {
        // ini agar parent process tetap hidup, menunggu sinyal SIGALRM untuk menjalankan scheduler
        // Kalau gak ada ini, parent process akan exit dan child-childnya jadi zombie
        pause();
    }

    return 0;
}
