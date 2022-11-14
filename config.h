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
#include <sys/msg.h>

#ifndef _CONFIG_H
#define _CONFIG_H

#include <limits.h>

#define MAX_PROCESSES 20
#define HELP_STATEMENT "USAGE\n---------\n./oss 				Forks user process randomly between 1 and 5 seconds\n./oss -h			Prints usage\n./oss -v [0 | 1]		0 for non-verbose mode | 1 for verbose mode\n"
#define MAX_PROCESS_IN_SYSTEM 18
#define MAX_LINE_IN_LOG_FILE 10000
#define TERMINATION_PROB 0.30
#define SHARED_RESOURCE_PROB 0.50
#define MAX_NANO_SEC 1000000000
#define NANO_NANO_SEC_PER_MILLISEC MAX_NANO_SEC/1000
#define MAX_RESOURCE_COUNT 20
#define MAX_RESOURCE_INSTANCE 10

const int DEFAULT_SS = 5;
const int MAX_PROCESS_GEN_SEC = 0;
const int MAX_PROCESS_GEN_MSEC =500;
const int MAX_PROCESS_TERMINATION_MSEC = 250;
const int TRUE =1;
const int FALSE =0;
const int CLOCK_INC_SEC = 1;
const int MAX_CLOCK_INC_NSEC = 1000;

enum resource_type {EXCLUSIVE,SHARED};
enum request_state {request,release,none};
enum recvType {WAIT,NOWAIT};

struct sembuf semLock, semRelease;

struct lclock{
    unsigned int sec;
    unsigned long long int nsec;
};

struct res_des{
    unsigned int resurceId ;
    enum resource_type type;
    unsigned int instance_count;
    unsigned int instance_process_map [MAX_RESOURCE_INSTANCE];
};

struct request_packet{
    int packet_id;
    unsigned int resource_instance_count[MAX_RESOURCE_COUNT];
    enum request_state type[MAX_RESOURCE_COUNT];
};

struct send_pkt{
    long int mtype;
    int req_pkt_id;
};

struct recv_pkt{
    long int mtype;
    int status;
};


key_t lc_key = 1234;
key_t sem_clock_key = 1545;
key_t res_des_key_st = 1645; // 1645 + 20 = 1665
key_t msgQ_key = 1745;
key_t request_packet_key_st = 4300; // 4300 + 18 = 4318
key_t request_packet_msgQ_key = 4321; // 4323
key_t recv_msgQ_key_st = 4323; // 4323 + 18 = 4341
key_t sem_process_key_st = 4423; // 4423 + 28 = 4461
key_t max_required_table_key = 5000;

#endif
