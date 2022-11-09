#include "config.h"

// globals
struct lclock * clock_ptr;
struct psblock * psinfo;


// functions
float getProb(){
    return (1.0*(rand()%100))/100.0;
}

double getRandomDuration(int max){
    max= max * 100;
    return  (1.0*(rand()% max))/100;
}

void clear_child_shared_mem(){
    shmdt(clock_ptr);
    shmdt(psinfo);
}

// SIGINT signal handle master
void signalHandle(){
    printf("CHILD:Handled the SIGINT signal \n");
    clear_child_shared_mem();
    exit(0);
}

// SIGUSR1 signal handle master
void signalHandleterminate(){
    printf("CHILD: Handled the SIGUSR1 signal\n");
    clear_child_shared_mem();
    exit(0);
}

// main code
int main(int argc, char ** argv){

    setpgid(0,5);

    signal(SIGINT,signalHandle);
    signal(SIGUSR1,signalHandleterminate);

    int semid1 = atoi(argv[1]);
    int semid2 = atoi(argv[2]);
    int pblockId = atoi(argv[3]);
    int clockId = atoi(argv[4]);

    // SEM  OPERATIONS
    semLock.sem_num=0;
    semLock.sem_op=-1;
    semLock.sem_flg=0;

    semRelease.sem_num=0;
    semRelease.sem_op=1;
    semRelease.sem_flg=0;


    // read the  shared memory and get the pid and time quantum
     clock_ptr  = (struct lclock*)shmat(clockId,NULL,0);
    if(clock_ptr==(struct lclock*)-1){
        perror("CHILD : cannot attach clock\n");
        exit(0);
    }
    psinfo = (struct psblock *)shmat(pblockId,NULL,0);
    if(psinfo == (struct psblock* ) -1){
        perror("CHILD : cannot attach process block\n");
        exit(0);
    }
    srand(time(0)+psinfo->pid);

    // printf("CHILD : waiting for the lock\n");
    // wait on some semaphore to start
    while(1){
        semop(semid1,&semLock,1);
        // printf("pid = %d\ttime=%df\n",psinfo->pid,psinfo->max_burst_t);
        // generate random numbe to decide the teermination of the process
        float prob = getProb();
        // printf("%f\n",prob);
        // if termination 
        if(prob <= TERMINATION_PROB){
            // break the loop
            break;
        }

        // else if not
        prob = getProb();

        // if blocking reply with the info and random runtime less than the quantum
        if(prob<=BLOCKING_PROB){
            psinfo->state = BLOCKED;
            psinfo->burst_t = getRandomDuration(psinfo->max_burst_t);
            psinfo->cpu_t +=psinfo->burst_t;
        }
        // else reply the oss with the info of usage of full time usage
        else{
            psinfo->state = RUNNING;
            psinfo->burst_t = psinfo->max_burst_t;
            psinfo->cpu_t +=psinfo->burst_t;
        }
       semop(semid2,&semRelease,1);
    }

    //reply the oss with the info and random runtime less than the quantum
    psinfo->state = TERMINATE;
    psinfo->burst_t = getRandomDuration(psinfo->max_burst_t);

    psinfo->exit_t.sec = clock_ptr->sec;
    psinfo->exit_t.nsec = clock_ptr->nsec;

    psinfo->cpu_t +=psinfo->burst_t;

    // printf("CHILD: child process ends\n");
    semop(semid2,&semRelease,1);
    clear_child_shared_mem();
    return 0;
}
