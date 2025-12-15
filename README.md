# AoL Assessment - Time-Sharing Scheduler Simulation (Round Robin)

### Authors:
* <a href="https://github.com/kensunjaya" target="_blank">2702273315 - Kenneth Sunjaya</a>
* <a href="https://github.com/sherlyoktavia" target="_blank">2702262993 - Sherly Oktavia Willisa</a>

<hr />
This project demonstrates a user-space simulation of a preemptive time-sharing scheduler using POSIX signals and timers in C.
It mimics how an operating system performs Round Robin scheduling with timer interrupts and context switching.

## Key Concepts
- `SIGALRM` acts as a timer interrupt (Q)
- `SIGSTOP` preempts the currently running process
- `SIGCONT` resumes the next scheduled process
- `fork()` creates child processes
- `setitimer()` generates periodic preemption
- `pause()` keeps processes idle and signal-driven

## Process State Models
- `RUNNING` process currently scheduled
- `STOPPED` preempted by the scheduler
- `TERMINATED` process exited cleanly

## How to Compile and Run
```bash
gcc time_share.c -o time_share
./time_share 5
```
You can change the `5` to any values between **1 to 4,294,967,295**. This indicates the number of child processes
