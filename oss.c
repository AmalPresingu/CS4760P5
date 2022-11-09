#include "config.h"

// globals
char logfilename[100];
char logfile[200];
int t_s = DEFAULT_SS;
int pid_counter=0;
int pb_map[MAX_PROCESSES];
int system_pids[MAX_PROCESSES];
key_t pb_kset[MAX_PROCESS_BLOCK] ;
int pb_id_set[MAX_PROCESS_BLOCK];
struct psblock * pb_ptr [MAX_PROCESS_BLOCK];
unsigned int actual_pids[MAX_PROCESSES];

struct stat rstat,ustat;

key_t lc_key = 1234;
int lc_sid;
struct lclock *lc_ptr;

int bitVector[MAX_PROCESS_BLOCK];
int userProcessCount;
int lineCountLogfile=0;
int logFileNumber=1;
char logEntry[1005];

//sem
key_t psemkeyinit[MAX_PROCESSES];
int psemidinit [MAX_PROCESSES];

key_t psemkeyend[MAX_PROCESSES];
int psemidend [MAX_PROCESSES];


// queue
struct node {
    int pid;
    void * next;
};

struct node * q1,*q2,*q3,*blockedQ;

void push(struct node ** head , int pid){

    struct node * new_node = (struct node *) malloc(sizeof(struct node));
    (new_node)->pid = pid;
    (new_node)->next = NULL;
    if(*head==NULL)
        *head  = new_node;
    else{
        struct node * init = *head;
        while(init->next!=NULL){
            init=(init)->next;
        }
        init->next = new_node;
    }
}

int pop(struct node ** head){
    struct node * top = (*head);
    *head=(*head)->next;
    int pid = top->pid;
    free(top);
    return pid;
}

bool isEmpty(struct node ** head){
    if(*head==NULL)
        return true;
    return false;
}



void printQ(struct node ** head){
    struct node * qptr = *head;

    while(qptr!=NULL){
        printf("%d\t",qptr->pid);
        qptr=qptr->next;
    }
    printf("\n");
}

float getProb(){
    return (1.0*(rand()%100))/100.0;
}

// random time
unsigned int getRandom(int max,int min){
    return (unsigned int) rand()% (max-min+1) + min;
}

struct lclock getRandomTime(unsigned int maxs , unsigned int maxns){
    struct lclock t;
    t.sec = lc_ptr->sec + getRandom(maxs,0);
    t.nsec = lc_ptr->nsec + getRandom(maxns,0);
    if(t.nsec >= MAX_NANO_SEC)
    {
        t.sec += t.nsec/MAX_NANO_SEC;
        t.nsec = t.nsec%MAX_NANO_SEC;
    }
    return t;
}

struct lclock processInitTime(){
    // srand(time(0));
    return  getRandomTime(MAX_PROCESS_GEN_SEC,MAX_PROCESS_GEN_NSEC);
}

struct lclock blockTime(int pid){
    srand(time(0)+pid);
    return  getRandomTime(MAX_PROCESS_BLOCK_SEC,MAX_PROCESS_BLOCK_NSEC);
}

void incresaseClock(){
    lc_ptr->sec = lc_ptr->sec +CLOCK_INC_SEC;
    lc_ptr->nsec = lc_ptr->nsec + getRandom(MAX_CLOCK_INC_NSEC,0);
    if(lc_ptr->nsec >= MAX_NANO_SEC)
    {
        lc_ptr->sec += lc_ptr->nsec/MAX_NANO_SEC;
        lc_ptr->nsec = lc_ptr->nsec%MAX_NANO_SEC;
    }
}

void addContextSwitchTime(){
    lc_ptr->sec = lc_ptr->sec;
    lc_ptr->nsec = lc_ptr->nsec + getRandom(10000,100);
    if(lc_ptr->nsec >= MAX_NANO_SEC)
    {
        lc_ptr->sec += lc_ptr->nsec/MAX_NANO_SEC;
        lc_ptr->nsec = lc_ptr->nsec%MAX_NANO_SEC;
    }
}

void addBlockProcessSwitchTime(){
    lc_ptr->sec = lc_ptr->sec;
    lc_ptr->nsec = lc_ptr->nsec + getRandom(100000,10000);
    if(lc_ptr->nsec >= MAX_NANO_SEC)
    {
        lc_ptr->sec += lc_ptr->nsec/MAX_NANO_SEC;
        lc_ptr->nsec = lc_ptr->nsec%MAX_NANO_SEC;
    }
}

void printClock(struct lclock s){
    printf("%d:%ld\n",s.sec,s.nsec);
}

// clear the shared mem before exit
void clear_oss_shared_mem(){

    // detach the process blocks
    for(int i=0;i<MAX_PROCESS_BLOCK;i++){
        shmdt(pb_ptr[i]);
        shmctl(pb_id_set[i],IPC_RMID,NULL);
    }

    // detach the clock
    shmdt(lc_ptr);
    shmctl(lc_sid,IPC_RMID,NULL);

    // remove the semaphores
    for(int i=0;i<MAX_PROCESSES;i++){
            semctl(psemidinit[i],0,IPC_RMID);
            semctl(psemidend[i],0,IPC_RMID);
    }

    // delete the processes created
    for (int i=0;i<pid_counter;i++)
    {
        kill(actual_pids[i],SIGUSR1);
    }
}

// SIGINT signal handle master
void signalHandle(){
    printf("OSS:Handled the SIGINT signal \n");
    clear_oss_shared_mem();
    exit(0);
}

// SIGALARM signal handle master
void signalHandleAlarm(){
    printf("OSS: Handled the SIGALARM signal.Exiting as timeout\n");
    clear_oss_shared_mem();
    exit(0);
}

// SIGTERM signal handle master
void signalHandleterminate(){
    printf("OSS: Handled the SIGTERM signal\n");
    clear_oss_shared_mem();
    exit(0);
}

void childProcess(int pid){
    char semId1[10];
    char pblockId[10];
    char clockId[10];
    char semId2[10];
    snprintf(semId1,10,"%d",psemidinit[pid]);
    snprintf(semId2,10,"%d",psemidend[pid]);
    snprintf(pblockId,10,"%d",pb_id_set[pb_map[pid]]);
    snprintf(clockId,10,"%d",lc_sid);
    // setpgid(0,2137);
    int child  = fork();
    if(child == 0){
        execl("./child","./child",semId1,semId2,pblockId,clockId,NULL);
        exit(0) ;
    }
}

int checkFreePBlock(){
    for(int i=0;i<MAX_PROCESS_BLOCK;i++){
        if(bitVector[i]==0)
            return i;
    }
    return -1;
}

void setpblock(int blockId){
    bitVector[blockId]=1;
}

void freepblock(int blockId){
    bitVector[blockId]=0;
}

int compareLclock(struct lclock t){
    if(t.sec < lc_ptr->sec)
        return 1;
    else if(t.sec == lc_ptr->sec && t.nsec < lc_ptr->nsec)
        return 1;
    else if(t.sec == lc_ptr->sec && t.nsec == lc_ptr->nsec)
        return 1;
    else
        return 0;
}

/***
    enum processType type;
    lclock init_t;
    double cpu_t;
    double total_t;
    double burst_t;
    int max_burst_t;
    int pid;
    unsigned int priority;
    enum processState state;
*/

int setNewProcess(struct lclock * t){
    if(compareLclock(*t)==1){
        int freePblock =   checkFreePBlock();
        if(freePblock == -1)
            return -1;
        else{
            int pid  = pid_counter;
            if(pid == MAX_PROCESSES)
                return -3;

            pb_map[pid] = freePblock;
            setpblock(freePblock);

            pb_ptr[freePblock]->init_t.sec= t->sec;
            pb_ptr[freePblock]->init_t.nsec= t->nsec;
            pb_ptr[freePblock]->pid = pid_counter;
            float p = getProb();

            // mark the type of process created and increment the variable
            if(p<=REALTIME_PROCESS_PROB){
                pb_ptr[freePblock]->type = REALTIME;
                pb_ptr[freePblock]->priority = REALTIME;
            }
            else{
                pb_ptr[freePblock]->type = USER;
                pb_ptr[freePblock]->priority = USER;
                userProcessCount++;
            }

            pb_ptr[freePblock]->state = READY;

            push(&q1,pid);
            pb_ptr[freePblock]->max_burst_t = Q1_BURST_TIME;
            pb_ptr[freePblock]->cpu_t=0.0;
            pb_ptr[freePblock]->total_block_t=0.0;

            int childPid = -1;
            childPid = fork();
            if(childPid == 0){
                childProcess(pid);
                exit(0) ;
            }
            else{
                actual_pids[pid_counter] = childPid;
            }
            return freePblock;
        }
    }
    return -2;
}

int getSchedulablePid(){
    // printf("block\t - ");
    // printQ(&blockedQ);
    // printf("Q1\t - ");
    // printQ(&q1);
    // printf("Q2\t - ");
    // printQ(&q2);
    // printf("Q3\t - ");
    // printQ(&q3);
    if(isEmpty(&q1)==false)
        return pop(&q1);
    else if(isEmpty(&q2)==false)
        return pop(&q2);
    else if(isEmpty(&q3)==false)
        return pop(&q3);
    return -1;
}

void logMaster(){
    FILE * fptr;

    snprintf(logfile,200,"0%d_%s",logFileNumber,logfilename);
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);

    fptr = fopen(logfile,"a");
    // fprintf(fptr,"%s\t:\t%d::%ld\t:\t%s",asctime(tm),lc_ptr->sec,lc_ptr->nsec,logEntry);
    fprintf(fptr,"%d::%ld\t:\t%s",lc_ptr->sec,lc_ptr->nsec,logEntry);
    fclose(fptr);
    lineCountLogfile+=2;

    // printf("%s\b\b\b%d::%ld : %s",asctime(tm),lc_ptr->sec,lc_ptr->nsec,logEntry);
    //printf("%d::%ld : %s",lc_ptr->sec,lc_ptr->nsec,logEntry);

    if(lineCountLogfile == MAX_LINE_IN_LOG_FILE){
        logFileNumber ++;
        lineCountLogfile=0;
    }
}

double getTimeDiff(struct lclock a,struct lclock b){
    double diff=0.0;
    diff = (a.sec - b.sec)*1000; //convert to millisec
    diff += (1.0*(a.nsec>b.nsec?(a.nsec-b.nsec):(b.nsec-a.nsec))/MAX_NANO_SEC)*1000; // convert to mllisec
    return diff;
}
\
// aging algorithm
bool checkAging(struct lclock init_t, double cpu_t){
    double total_t =  getTimeDiff(*lc_ptr,init_t);

    double avgcpu  =  ustat.avgcpu_t / userProcessCount;
    double avgTotal = ustat.avgtotal_t / userProcessCount;
    double avgRatio = avgcpu/avgTotal;

    double ratio = cpu_t/total_t;

    if(avgRatio > ratio )
        return true;
    else
        return false;

}

void printStat(struct stat s){
    printf("Avg Wait Time : %lf\n",s.avgwait_t);
    printf("Avg CPU Time : %lf\n",s.avgcpu_t);
    printf("Avg Time in Block Queue: %lf\n",s.avgBlockQueue_t);
    printf("Avg TOTAL Time in the System: %lf\n",s.avgtotal_t);

}

void signalHandleUsr1(){
}

int main(int argc, char ** argv){

    signal(SIGINT,signalHandle);
    signal(SIGALRM,signalHandleAlarm);
    signal(SIGTERM,signalHandleterminate);
    signal(SIGUSR1,signalHandleUsr1);

    // RESET SEM
    semLock.sem_num=0;
    semLock.sem_op=-1;
    semLock.sem_flg=0;

    semRelease.sem_num=0;
    semRelease.sem_op=1;
    semRelease.sem_flg=0;

    // default values
    t_s = DEFAULT_SS;
    snprintf(logfilename,100,"%s",DEFAULT_LOG_FILE);

    // decode the inputs
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-h")==0){
            printf(HELP_STATEMENT);
            return 0;
        }
        else if(strcmp(argv[i],"-l")==0){
            strcpy(logfilename,argv[i+1]);
            i++;
        }
        else if(strcmp(argv[i],"-s")==0){
            t_s = atoi(argv[i+1]);
            i++;
        }
    }

    // initialize the clock
    lc_sid = shmget(lc_key,sizeof(struct lclock),IPC_CREAT|0666);
    if(lc_sid==-1 ){
        perror("Unable to create clock memory");
        exit(0);
    }
    lc_ptr = (struct lclock*)shmat(lc_sid,NULL,0);
    if(lc_ptr==(struct lclock *)-1 ){
        perror("Unable to attach clock struct");
        exit(0);
    }

    lc_ptr->sec = 0;
    lc_ptr->nsec = 0;

    rstat.avgcpu_t = 0.0;
    rstat.avgtotal_t=0.0;
    rstat.avgwait_t = 0.0;
    ustat.avgcpu_t = 0.0;
    ustat.avgtotal_t=0.0;
    ustat.avgwait_t = 0.0;

    snprintf(logEntry,1000,"OSS: started logfilename as %s and t_s as %d\n",logfilename,t_s);
    logMaster();

    alarm(t_s);

    // init bit vector
    for(int i=0;i<MAX_PROCESSES;i++){
        bitVector[i]=0;
    }

    // initialize the process blocks
    int init_key = 12456;
    for(int i=0;i<MAX_PROCESS_BLOCK;i++){
        pb_kset[i] =  init_key+i;
        pb_id_set[i] = shmget(pb_kset[i],sizeof(struct psblock),IPC_CREAT| 0666);
        if(pb_id_set[i]==-1 ){
            perror("Unable to create process block ");
            exit(0);
        }
        pb_ptr [i] = (struct psblock *) shmat(pb_id_set[i],NULL,0);
        if(pb_ptr[i]==(struct psblock *)-1 ){
            perror("Unable to attach process block ");
            exit(0);
        }

    }

    // initialize semaphores
    for(int i=0;i<MAX_PROCESSES;i++){
        psemkeyinit[i] = sem_key_init+i;
        psemidinit[i] = semget(psemkeyinit[i],1,0666|IPC_CREAT);
        if(psemidinit[i]==-1){
            perror("semGet error");
            exit(0);
        }
        int status = semctl(psemidinit[i],0,SETVAL,0);
        if(status == -1){
            perror("semCtl init error");
            exit(0);
        }
        psemkeyend[i] = sem_key_end+i;
        psemidend[i] = semget(psemkeyend[i],1,0666|IPC_CREAT);
        if(psemidend[i]==-1){
            perror("semGet error");
            exit(0);
        }
        status = semctl(psemidend[i],0,SETVAL,0);
        if(status == -1){
            perror("semCtl error");
            exit(0);
        }
    }

    struct lclock new_p_t;
    int status;
    double cpuIdleTime=0.0;
    // genrate new process time
    new_p_t = processInitTime();
    // printClock(new_p_t);

    // cpuIdleTime+=getTimeDiff(new_p_t,*lc_ptr);

    // lc_ptr->sec = new_p_t.sec;
    // lc_ptr->nsec = new_p_t.nsec;
//    while processes less than 50
    while(1){

        // check the blocked processes if the time is up for any of them put them in the queue.
        if(isEmpty(&blockedQ) == false){
            int temp_blk_id = pb_map[blockedQ->pid];
            if(compareLclock(pb_ptr[temp_blk_id]->block_t)==1){
                int bpid = pop(&blockedQ);
                pb_ptr[temp_blk_id]->state = READY;
                pb_ptr[temp_blk_id]->max_burst_t = Q1_BURST_TIME;
                addBlockProcessSwitchTime();
                push(&q1,bpid);

                snprintf(logEntry,1000,"OSS: BLOCKED PROCESS %d PUT IN READY QUEUE\n",bpid);
                logMaster();
            }
            else{
                if(isEmpty(&q1) && isEmpty(&q2) && isEmpty(&q3)){
                    int bpid = pop(&blockedQ);
                    pb_ptr[temp_blk_id]->state = READY;
                    pb_ptr[temp_blk_id]->max_burst_t = Q1_BURST_TIME;
                    addBlockProcessSwitchTime();
                    push(&q1,bpid);

                    cpuIdleTime+=getTimeDiff(pb_ptr[temp_blk_id]->block_t,*lc_ptr);
                    lc_ptr->sec = pb_ptr[temp_blk_id]->block_t.sec;
                    lc_ptr->nsec = pb_ptr[temp_blk_id]->block_t.nsec;

                    snprintf(logEntry,1000,"OSS: BLOCKED PROCESS %d PUT IN READY QUEUE\n",bpid);
                    logMaster();
                }
            }
        }

        if(pid_counter>=MAX_PROCESSES && isEmpty(&q1) && isEmpty(&q2) && isEmpty(&q3) && isEmpty(&blockedQ))
            break;
        // if queue is empty set time to process start time

        if(pid_counter<MAX_PROCESSES && isEmpty(&q1) && isEmpty(&q2) && isEmpty(&q3) && isEmpty(&blockedQ)){
            if(compareLclock(new_p_t)!=1){
                cpuIdleTime+=getTimeDiff(new_p_t,*lc_ptr);
                lc_ptr->sec = new_p_t.sec;
                lc_ptr->nsec = new_p_t.nsec;
            }
        }

        if(pid_counter<MAX_PROCESSES){
            snprintf(logEntry,1000,"OSS: process %d launch at %d::%ld \n",pid_counter,new_p_t.sec,new_p_t.nsec);
            logMaster();
            // generate new process time if current time more than the process time , generate the process. else leave the time
            status = setNewProcess(&new_p_t);
        }

        if(status>=0){
            pid_counter++;
            new_p_t = processInitTime();
        }
        else if(status == -1){
            snprintf(logEntry,1000,"OSS: NO FREE BLOCK - Skipping process launch\n");
            logMaster();
            new_p_t = processInitTime();
        }
        else if (status == -2){
            snprintf(logEntry,1000,"OSS: FUTURE PROCESS - Skipping process launch\n");
            logMaster();
        }
        else if(status == -3){
            snprintf(logEntry,1000,"OSS: PROCESS  COUNT LIMIT REACHED- Skipping process launch \n");
            logMaster();
        }


        // schedule the process
        int pid = getSchedulablePid();
        if(pid !=-1){
            int blockId = pb_map[pid];

            pb_ptr[blockId]->state = RUNNING;

        // increase the time by some random value for context switch
            addContextSwitchTime();
            snprintf(logEntry,1000,"OSS: CONTEXT SWITCH OUT FOR PROCESS %d\n",pid);
            logMaster();

            semop(psemidinit[pid],&semRelease,1);
            // wait for the process to reply
            // printf("OSS: Wait for client to complete\n");
            semop(psemidend[pid],&semLock,1);

            // increment the time as per the process took
            lc_ptr->nsec += (unsigned long int)(pb_ptr[blockId]->burst_t*1000000); // millisec to nanosec conversion

        // increase the time by some random value for context switch
            addContextSwitchTime();
            snprintf(logEntry,1000,"OSS: CONTEXT SWITCH IN FOR PROCESS %d\n",pid);
            logMaster();

            // set the process in applicable queue  -- blocked or ready
            if(pb_ptr[blockId]->state == TERMINATE){
                // increment the stats variables for the avg wait execution and run times.
                double total_t =  getTimeDiff(pb_ptr[blockId]->exit_t,pb_ptr[blockId]->init_t);
                // printf("pid:%d\ttotal_t:%lf\tcpu t : %lf\t block_t : %lf\n",pid,total_t,pb_ptr[blockId]->cpu_t,pb_ptr[blockId]->total_block_t);

                if(pb_ptr[blockId]->priority == REALTIME){
                    rstat.avgcpu_t += pb_ptr[blockId]->cpu_t;
                    rstat.avgwait_t+= (total_t - pb_ptr[blockId]->cpu_t - pb_ptr[blockId]->total_block_t);
                    rstat.avgBlockQueue_t+= pb_ptr[blockId]->total_block_t;
                    rstat.avgtotal_t += total_t;
                }
                else{
                    ustat.avgcpu_t += pb_ptr[blockId]->cpu_t;
                    ustat.avgwait_t+= (total_t - pb_ptr[blockId]->cpu_t - pb_ptr[blockId]->total_block_t);
                    ustat.avgtotal_t += total_t;
                    ustat.avgBlockQueue_t+= pb_ptr[blockId]->total_block_t;
                }

                freepblock(blockId);
                snprintf(logEntry,1000,"OSS: PROCESS %d TERMINATED\n",pid);
                logMaster();
            }
            else if(pb_ptr[blockId]->state == BLOCKED){
                struct lclock temp = blockTime(pid);
                pb_ptr[blockId]->block_t.sec = temp.sec ;
                pb_ptr[blockId]->block_t.nsec = temp.nsec ;
                pb_ptr[blockId]->total_block_t +=  getTimeDiff(pb_ptr[blockId]->block_t,*lc_ptr);
                push(&blockedQ,pid);
                snprintf(logEntry,1000,"OSS: PROCESS %d BLOCKED FOR I/O\n",pid);
                logMaster();
            }
            else {
                // if priority put again in q1
                if(pb_ptr[blockId]->priority == REALTIME){
                    pb_ptr[blockId]->max_burst_t = Q1_BURST_TIME;
                    push(&q1,pid);
                    snprintf(logEntry,1000,"OSS: PROCESS %d PUSHED IN Q1 as High Priority\n",pid);
                    logMaster();
                }
                // else put in q2 or q3
                else if(pb_ptr[blockId]->max_burst_t == Q1_BURST_TIME){
                    pb_ptr[blockId]->max_burst_t = Q2_BURST_TIME;
                    push(&q2,pid);
                    snprintf(logEntry,1000,"OSS: PROCESS %d PUSHED IN Q2\n",pid);
                    logMaster();
                }
                else if(pb_ptr[blockId]->max_burst_t == Q2_BURST_TIME){
                    pb_ptr[blockId]->max_burst_t = Q3_BURST_TIME;
                    push(&q3,pid);
                    snprintf(logEntry,1000,"OSS: PROCESS %d PUSHED IN Q3\n",pid);
                    logMaster();
                }
                else if(pb_ptr[blockId]->max_burst_t == Q3_BURST_TIME){

                    if(checkAging(pb_ptr[blockId]->init_t,pb_ptr[blockId]->cpu_t)==true){
                        pb_ptr[blockId]->max_burst_t = Q2_BURST_TIME;
                        push(&q2,pid);
                        snprintf(logEntry,1000,"OSS: PROCESS %d PUSHED IN Q2 AGING APPLIED\n",pid);
                        logMaster();
                    }
                    else{
                        pb_ptr[blockId]->max_burst_t = Q3_BURST_TIME;
                        push(&q3,pid);
                        snprintf(logEntry,1000,"OSS: PROCESS %d PUSHED IN Q3\n",pid);
                        logMaster();
                    }

                }
                pb_ptr[blockId]->state = READY;
           }
        }
        // increase the time by 1.XX sec
        incresaseClock();
        snprintf(logEntry,1000,"OSS: TIME CLOCK INCREASED\n");
        logMaster();
    }

    int realProcessCount = MAX_PROCESSES - userProcessCount;

    rstat.avgcpu_t /= realProcessCount;
    rstat.avgwait_t /= realProcessCount;
    rstat.avgtotal_t /= realProcessCount;
    rstat.avgBlockQueue_t /= realProcessCount;

    ustat.avgcpu_t /= userProcessCount;
    ustat.avgwait_t /= userProcessCount;
    ustat.avgtotal_t /= userProcessCount;
    ustat.avgBlockQueue_t /= userProcessCount;

    printf("Total User Processes %d\n",userProcessCount);
    printStat(ustat);
    printf("Total RealTime Processes %d\n",realProcessCount);
    printStat(rstat);
    printf("CPU Idle Time : %lf\n",cpuIdleTime);
    clear_oss_shared_mem();
    return 0;
}
