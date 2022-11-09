#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <limits.h>

const int DEFAULT_SS = 3;

const int MAX_PROCESS_GEN_SEC = 4;
const int MAX_PROCESS_GEN_NSEC = INT_MAX;

const int CLOCK_INC_SEC = 1;
const int MAX_CLOCK_INC_NSEC = 1000;

const int MAX_PROCESS_BLOCK_SEC = 5;
const int MAX_PROCESS_BLOCK_NSEC = 1000;

int sem_key_init = 4213;
int sem_key_end = 5213;
int sem_clock = 4111;

#define MAX_PROCESSES 20
#define HELP_STATEMENT "USAGE\n---------\n./oss 				Runs with default time of 3 seconds\n./oss -s t			Allows user to set maximum seconds before termination\n./oss -l f 			Allows user to specify name for the log file\n"
#define MAX_PROCESS_BLOCK 18
#define DEFAULT_LOG_FILE "oss_log_0.txt"
#define MAX_LINE_IN_LOG_FILE 10000
#define TERMINATION_PROB 0.30
#define BLOCKING_PROB 0.30
#define REALTIME_PROCESS_PROB 0.10
#define Q1_BURST_TIME 10
#define Q2_BURST_TIME 20
#define Q3_BURST_TIME 40
#define MAX_NANO_SEC 1000000000

enum processState{READY,RUNNING,BLOCKED,TERMINATE};
enum processType{USER,REALTIME};
struct sembuf semLock, semRelease;

struct lclock{
    unsigned int sec;
    unsigned long int nsec;
};

struct psblock{
    enum processType type;
    struct lclock init_t;
    struct lclock block_t;
    struct lclock exit_t;
    double burst_t;
    int max_burst_t;
    double cpu_t;
    double total_block_t;
    int pid;
    unsigned int priority;
    enum processState state;
};

struct stat{
    double avgwait_t;
    double avgcpu_t;
    double avgtotal_t;
    double avgBlockQueue_t;
};

#endif
