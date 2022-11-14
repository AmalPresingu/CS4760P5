//Name    :    Amal Presingu
//Date    :    9/13/2022

#include "config.h"

// globals
int lc_sid;
struct lclock *lc_ptr;
int sem_clock;
int res_des_id [MAX_RESOURCE_COUNT];
struct res_des *res_des_oss [MAX_RESOURCE_COUNT];
int req_pkt_set_id[MAX_PROCESS_IN_SYSTEM];
struct request_packet * req_pkt_set [MAX_PROCESS_IN_SYSTEM];
int process_req_map[MAX_PROCESS_IN_SYSTEM];
int max_required_table_id;
int bit_vector[MAX_PROCESS_IN_SYSTEM];
int sendQ_id;
int recvQ_id_set[MAX_PROCESS_IN_SYSTEM];
int sem_process_id [MAX_PROCESS_IN_SYSTEM];
int childPids[MAX_PROCESSES];

unsigned int allocation_table[MAX_PROCESS_IN_SYSTEM][MAX_RESOURCE_COUNT];
unsigned int (*max_required_table)[MAX_RESOURCE_COUNT];
unsigned int available_count[MAX_RESOURCE_COUNT];
unsigned int total_count[MAX_RESOURCE_COUNT];
int is_shared_vec[MAX_RESOURCE_COUNT];
int verbose = 1;
int logCounter=0;
int t_s;
#define logSize 100000
char logEntry[logSize];
int createNewProcess;
int keepRunning;
int process_itr;
int process_in_systm;

int processes_terminated;
// queue
struct node {
    int pid;
    void * next;
};

struct node * wait_q;

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

//------------------------------------------------------------------

float getProb(){
    return (1.0*(rand()%100))/100.0;
}

unsigned int getRandom(int max,int min){
    if(max==min) return min;
    return (unsigned int) rand()% (max-min+1) + min;
}

//------------------------------------------------------------------
// random time
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

struct lclock updateTime(int millisec){

    long long int nsec = millisec*NANO_NANO_SEC_PER_MILLISEC;
    struct lclock c;
    semop(sem_clock,&semLock,1);

    lc_ptr->nsec += nsec;

    if(lc_ptr->nsec >= MAX_NANO_SEC)
    {
        lc_ptr->sec += lc_ptr->nsec/MAX_NANO_SEC;
        lc_ptr->nsec = lc_ptr->nsec%MAX_NANO_SEC;
    }
    c.sec = lc_ptr->sec;
    c.nsec = lc_ptr->nsec;
    semop(sem_clock,&semRelease,1);
    return c;
}

struct lclock getTime(){

    struct lclock c;
    semop(sem_clock,&semLock,1);
    c.sec = lc_ptr->sec;
    c.nsec = lc_ptr->nsec;
    semop(sem_clock,&semRelease,1);
    return c;
}

void updateProcessCreationTime(){
    int msec = getRandom(MAX_PROCESS_GEN_MSEC,1);
    updateTime(msec);
}

struct lclock updateTerminationTime(){
    int msec = getRandom(MAX_PROCESS_TERMINATION_MSEC,0);
    return updateTime(msec);
}

// returns in milliseconds
double getTimeDiff(struct lclock a,struct lclock b){
    double diff=0.0;
    diff = (a.sec - b.sec)*1000; //convert to milliseconds
    diff += (1.0*(a.nsec>b.nsec?(a.nsec-b.nsec):(b.nsec-a.nsec))/MAX_NANO_SEC)*1000; // convert to mlliseconds
    return diff;
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


void printClock(struct lclock s){

    printf("%d:%lld\n",s.sec,s.nsec);
}

// clear the shared memory before exit
void clear_oss_shared_mem(){

    // detach the process blocks
    for(int i=0;i< MAX_RESOURCE_COUNT;i++){
        shmdt(res_des_oss[i]);
        shmctl(res_des_id[i],IPC_RMID,NULL);
    }

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        shmdt(req_pkt_set[i]);
        shmctl(req_pkt_set_id[i],IPC_RMID,NULL);
    }

    msgctl(sendQ_id,IPC_RMID,NULL);

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        msgctl(recvQ_id_set[i],IPC_RMID,NULL);
    }

    shmdt(max_required_table);
    shmctl(max_required_table_id,IPC_RMID,NULL);

    // detach the clock
    shmdt(lc_ptr);
    shmctl(lc_sid,IPC_RMID,NULL);

    // remove clock semaphore
    semctl(sem_clock,0,IPC_RMID);
    // remove the semaphores
        for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
            semctl(sem_process_id[i],0,IPC_RMID);
        }

        // remove child processes
        for(int i=0;i<process_in_systm;i++){
            kill (childPids[i], SIGINT);
        }
}

// SIGINT signal handle master
void signalHandle(){
    printf("OSS:Handled the SIGINT signal\n");
    clear_oss_shared_mem();
    exit(0);
}

// SIGTERM signal handle master
void signalHandleterminate(){
    printf("OSS: Handled the SIGTERM signal\n");
    clear_oss_shared_mem();
    exit(0);
}

// SIGALARM signal handle for alarm
void signalHandleAlarm(){
    printf("OSS: Handled the SIGALRM signal\n");
    createNewProcess = FALSE;
}

// SIGUSR1 signal handle for alarm
void signalHandleUsr1(){
    // do nothing
}


void logMaster(){
    FILE * fptr;
    char logfile [] = "logfile.txt";

    fptr = fopen(logfile,"a");
    struct lclock l = getTime();

    fprintf(fptr,"%d::%lld\t:\t%s\n",l.sec,l.nsec,logEntry);
    bzero(logEntry,logSize);
    fclose(fptr);
}


// bit vector
int checkFreeRequestStruct(){
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        if(bit_vector[i]==0)
            return i;
    }
    return -1;
}

void setpblock(int id){
    bit_vector[id]=1;
}

void freepblock(int id){
    bit_vector[id]=0;
}

//printing table
void printMatrix(unsigned int a[MAX_PROCESS_IN_SYSTEM][MAX_RESOURCE_COUNT],char *s){
    printf("%s\n",s);
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        for(int j=0;j<MAX_RESOURCE_COUNT;j++){
            printf("%d\t",a[i][j]);
        }
        printf("\n");
    }
}

void printMatrixp(unsigned int **a,char *s){
    printf("%s\n",s);
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        for(int j=0;j<MAX_RESOURCE_COUNT;j++){
            printf("%d\t",a[i][j]);
        }
        printf("\n");
    }
}

void printfVector(unsigned int *t,char *s,int size){
    printf("%s\n",s);
    for(int i=0;i<size;i++){
            printf("%d\t",t[i]);
    }
        printf("\n");
}

// process requests
int process_request(struct request_packet * p){

    // printf("Request Type%d : child %d\n",p->type[0],p->packet_id);
    // printfVector(&(available_count[0]),"available count",MAX_RESOURCE_COUNT);
    // printfVector(&(p->resource_instance_count[0]),"instance count",MAX_RESOURCE_COUNT);

    if(p->type[0]==release){
        for(int i=0;i<MAX_RESOURCE_COUNT;i++){
            if(is_shared_vec[i]==FALSE){
                // increase available count
                available_count[i]+=p->resource_instance_count[i];
                // decrease the allocation value
                allocation_table[p->packet_id][i]-=p->resource_instance_count[i];
            }
        }
        int is_terminated=TRUE;
        for(int i=0;i<MAX_RESOURCE_COUNT;i++){
            if(max_required_table[p->packet_id][i]!=0){
                is_terminated=FALSE;
                break;
            }
        }
        if(is_terminated==TRUE){

            // reply with status 1
            // printfVector(&(available_count[0]),"available count after",MAX_RESOURCE_COUNT);
            // printfVector(&(p->resource_instance_count[0]),"instance count after",MAX_RESOURCE_COUNT);
            if(verbose==1){
                snprintf(logEntry,logSize,"MASTER :  P%d Terminated \n Resources released ",process_req_map[p->packet_id]);
                int len = strlen(logEntry);
                for(int log=0;log<MAX_RESOURCE_COUNT;log++){
                    if(is_shared_vec[log]==FALSE && p->resource_instance_count[log]>0){
                        char t[100];
                        snprintf(t,100,"R%d:%d\t",log,p->resource_instance_count[log]);
                        strncpy(&logEntry[len],t,strlen(t));
                        len = strlen(logEntry);
                    }
                }
                logMaster();
            }
            // indicate process terminated
            return 2;
        }

        // reply with status 1
        // printfVector(&(available_count[0]),"available count after",MAX_RESOURCE_COUNT);
        // printfVector(&(p->resource_instance_count[0]),"instance count after",MAX_RESOURCE_COUNT);

        if(verbose==1){

            snprintf(logEntry,logSize,"MASTER : Acknowledged  P%d released ",process_req_map[p->packet_id]);
            int len = strlen(logEntry);
            for(int log=0;log<MAX_RESOURCE_COUNT;log++){
                if(is_shared_vec[log]==FALSE && p->resource_instance_count[log]>0){
                    char t[100];
                    snprintf(t,100,"R%d : %d\t",log,p->resource_instance_count[log]);
                    strncpy(&logEntry[len],t,strlen(t));
                    len = strlen(logEntry);
                }
            }
            logMaster();
        }

        return 1;
    }

    // request
    else if(p->type[0]==request){
        int is_available=TRUE;


        snprintf(logEntry,logSize,"MASTER: Detected Process P%d requesting ",process_req_map[p->packet_id]);
        int len = strlen(logEntry);
        for(int log=0;log<MAX_RESOURCE_COUNT;log++){
            if(is_shared_vec[log]==FALSE && p->resource_instance_count[log]>0){
                char t[100];
                snprintf(t,100,"R%d : %d\t",log,p->resource_instance_count[log]);
                strncpy(&logEntry[len],t,strlen(t));
                len = strlen(logEntry);
            }
        }
       logMaster();
        // banker algorithm

        // check if resource available
        for(int i=0;i<MAX_RESOURCE_COUNT;i++){
            if(is_shared_vec[i]==FALSE){
                if(p->resource_instance_count[i]<=available_count[i])
                    continue;
                else{
                    is_available=FALSE;
                    break;
                }
            }
        }
        if(verbose==1){
            snprintf(logEntry,logSize,"MASTER running Deadlock Detection");
            logMaster();
        }

        if(is_available == FALSE){

            if(verbose==1){
                snprintf(logEntry,logSize,"MASTER : unsafe state detected, Request not allocated");
                logMaster();
            }
            return -1;
        }

        // if available check for the safety algorithm
        {
            // create tmp matrices of allocation , need , available and max.
            int tmp_aloc[MAX_PROCESS_IN_SYSTEM][MAX_RESOURCE_COUNT];
            int tmp_need[MAX_PROCESS_IN_SYSTEM][MAX_RESOURCE_COUNT];
            int tmp_max[MAX_PROCESS_IN_SYSTEM][MAX_RESOURCE_COUNT];
            int tmp_avail[MAX_RESOURCE_COUNT];


            for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){

                for(int j=0;j<MAX_RESOURCE_COUNT;j++){

                    if(is_shared_vec[j]==FALSE){

                        tmp_aloc[i][j] = allocation_table[i][j];
                        tmp_max[i][j] = max_required_table[i][j];
                        tmp_need[i][j] = tmp_max[i][j] - tmp_aloc[i][j];
                        tmp_avail[j] = available_count[j];
                    }
                }
            }

            // printMatrix(allocation_table,"allocation_table");
            // printMatrix(max_required_table,"max_required_table");
            //if allocated
            for(int j=0;j<MAX_RESOURCE_COUNT;j++){
                if(is_shared_vec[j]==FALSE){
                    tmp_avail[j]-=p->resource_instance_count[j];
                    tmp_aloc[p->packet_id][j] += p->resource_instance_count[j];
                    tmp_need[p->packet_id][j] = tmp_max[p->packet_id][j] - tmp_aloc[p->packet_id][j];
                }
            }


            // check
            int is_safe = TRUE;
            int process_bit_vec[MAX_PROCESS_IN_SYSTEM];

            for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
                process_bit_vec[i]=FALSE;
            }

            for(int k=0;k<MAX_PROCESS_IN_SYSTEM;k++){
                for(int itr=0;itr<MAX_PROCESS_IN_SYSTEM;itr++){
                    int status = TRUE;
                    if(process_bit_vec[itr]==FALSE){

                        // check if need fulfilled
                        for(int i=0;i<MAX_RESOURCE_COUNT;i++){
                            if(is_shared_vec[i]==FALSE){
                                if(tmp_need[itr][i]<=tmp_avail[i])
                                    continue;
                                else{
                                    status = FALSE;
                                    break;
                                }
                            }
                        }

                        // if fulfilled
                        if(status == TRUE){
                            for(int i=0;i<MAX_RESOURCE_COUNT;i++){
                                if(is_shared_vec[i]==FALSE){
                                    tmp_avail[i]+=tmp_max[itr][i];
                                }
                            }
                            process_bit_vec[itr]=TRUE;
                        }
                    }
                }
            }

            for(int itr=0;itr<MAX_PROCESS_IN_SYSTEM;itr++){
                if(process_bit_vec[itr]==FALSE){
                    is_safe = FALSE;
                    break;
                }
            }

            // if safe then allocate
            if(is_safe == TRUE){
                for(int j=0;j<MAX_RESOURCE_COUNT;j++){
                    if(is_shared_vec[j]==FALSE){
                        available_count[j]-=p->resource_instance_count[j];
                        allocation_table[p->packet_id][j] += p->resource_instance_count[j];
                    }
                }
                // printfVector(&(available_count[0]),"available count after",MAX_RESOURCE_COUNT);
                // printfVector(&(p->resource_instance_count[0]),"instance count after",MAX_RESOURCE_COUNT);

                snprintf(logEntry,logSize,"MASTER : safe state detected, request allocated");
                logMaster();

                snprintf(logEntry,logSize,"MASTER: Granting P%d  ",process_req_map[p->packet_id]);
                int len = strlen(logEntry);
                for(int log=0;log<MAX_RESOURCE_COUNT;log++){
                    if(is_shared_vec[log]==FALSE && p->resource_instance_count[log]>0){
                        char t[100];
                        snprintf(t,100,"R%d : %d\t",log,p->resource_instance_count[log]);
                        strncpy(&logEntry[len],t,strlen(t));
                        len = strlen(logEntry);
                    }
                }
                logMaster();

                logCounter++;
                if(logCounter==20 && verbose==1){
                    logCounter=0;
                    snprintf(logEntry,logSize,"MASTER: AllocationTable\n");
                    int len = strlen(logEntry);
                    for(int log=0;log<MAX_PROCESS_IN_SYSTEM;log++){
                        for(int j=0;j<MAX_RESOURCE_COUNT;j++){
                            char t[100];
                            snprintf(t,100,"%d\t",allocation_table[log][j]);
                            strncpy(&logEntry[len],t,strlen(t));
                            len = strlen(logEntry);
                        }
                        char t[100];
                        snprintf(t,100,"\n");
                        strncpy(&logEntry[len],t,strlen(t));
                        len = strlen(logEntry);
                    }
                    logMaster();
                }
                return 1;
            }
            //else
            else{

                if(verbose){
                    snprintf(logEntry,logSize,"MASTER : unsafe state detected, Request not allocated");

                    logMaster();
                }

                return -1;
            }
        }
    }
    return 0;
}

struct request_packet * read_packet(enum recvType type){
    int status;
    struct send_pkt p;

//    if(type ==WAIT)
    {
        status = msgrcv(sendQ_id,&p,sizeof(p.req_pkt_id),1,IPC_NOWAIT);
    }
 //   else
    {
  //      status = msgrcv(sendQ_id,&p,sizeof(p.req_pkt_id),1,IPC_NOWAIT);
    }
    if(status==-1)
        return NULL;
    return req_pkt_set[p.req_pkt_id];
}


void write_pkt(int status,int id){

    snprintf(logEntry,logSize,"MASTER: Current Resource\n");
    int len = strlen(logEntry);
    for(int log=0;log<MAX_RESOURCE_COUNT;log++){
        if(is_shared_vec[log]==FALSE){
            char t[100];
            snprintf(t,100,"R%d : %d\t",log,available_count[log]);
            strncpy(&logEntry[len],t,strlen(t));
            len = strlen(logEntry);
        }
    }
    logMaster();

    // success
    if(status == 1){
        struct recv_pkt p;
        p.mtype=1;
        p.status = status;
        msgsnd(recvQ_id_set[id],&p,sizeof(p.status),0);
    }
    // push in wait_q in make the process sleep
    else if(status  == -1){
        struct recv_pkt p;
        p.mtype=1;
        p.status = status;
        msgsnd(recvQ_id_set[id],&p,sizeof(p.status),0);
        push(&wait_q,id);
        snprintf(logEntry,logSize,"MASTER : pushing P%d in wait queue",process_req_map[id]);
        logMaster();
    }
    else if (status == 2){

        //clear allocation table
        for(int i=0;i<MAX_RESOURCE_COUNT;i++){
            allocation_table[id][i]=0;
        }

        // clear request_packet
        for(int i=0;i<MAX_RESOURCE_COUNT;i++){
            req_pkt_set[id]->resource_instance_count[i]=0;
            req_pkt_set[id]->type[i] = none;
        }

        // free the block
        freepblock(id);
        process_req_map[id] = 0;
        // printfVector(process_req_map,"----------------------process map",MAX_PROCESS_IN_SYSTEM);
        process_in_systm--;

        // send no msg process terminated
    }
    else{
        // do nothing
    }
 }


void handleQueue(){
    while(isEmpty(&wait_q)!=TRUE){
        int pid = wait_q->pid;
        int status = process_request(req_pkt_set[pid]);
        if(status==1){
            semop(sem_process_id[pid],&semRelease,1);
            // printf("Request Type:%d , CHILD:%d , status:%d\n",req_pkt_set[pid]->type[0],req_pkt_set[pid]->packet_id,status);
            write_pkt(status,pid);
            pop(&wait_q);
        }
        else{
            break;
        }
    }
}


// child process
void childProcess(int req_pkt_id){

    unsigned int (* max_table)[MAX_RESOURCE_COUNT] =shmat(max_required_table_id,(void *)0,0);

    unsigned int allocated[MAX_RESOURCE_COUNT];
    unsigned int need[MAX_RESOURCE_COUNT];

    for(int i =0;i<MAX_RESOURCE_COUNT;i++){
        if(is_shared_vec[i]==FALSE){
            max_required_table[req_pkt_id][i] = getRandom(total_count[i],0);
            need[i]=max_required_table[req_pkt_id][i];
            allocated[i]=0;
        }
        else{
            max_required_table[req_pkt_id][i] = 1;
            need[i]=max_required_table[req_pkt_id][i];
            allocated[i]=need[i];
        }

    }

    struct lclock init_time = updateTerminationTime();

    // printf("CHILD %d\t",req_pkt_id);

    // for(int i=0;i<MAX_RESOURCE_COUNT;i++)
    //     printf("R%d:%d\t",i,max_required_table[req_pkt_id][i]);
    // printf("\n");

    struct lclock curr_time ;

    curr_time.sec = init_time.sec;
    curr_time.nsec = init_time.nsec;

    // printClock(curr_time);

    struct request_packet * p = (struct request_packet*) shmat(req_pkt_set_id[req_pkt_id],(void*)0,0);
    int recvQ_id = recvQ_id_set[req_pkt_id];
    int sem_id = sem_process_id[req_pkt_id];

    int B = 10;
    enum request_state state ;
    struct send_pkt send_msg;
    struct recv_pkt recv_msg;
    recv_msg.mtype=1;

    send_msg.mtype=1;
    send_msg.req_pkt_id = req_pkt_id;

    int receiving =0;
    int notFull = TRUE;
    int notEmpty = TRUE;

    while(keepRunning){

        semop(sem_id,&semLock,1);

        double life_time = getTimeDiff(curr_time,init_time);

        // float is_terminate = getProb();
        // if(is_terminate<=TERMINATION_PROB && life_time>1000.00){
        //     keepRunning = false;
        //     break;
        // }

        if(notFull==FALSE && life_time>1000.00){
            keepRunning = false;
            semop(sem_id,&semRelease,1);
            break;
        }

        if(receiving ==0){

            int task = getRandom(B,0);
            if((task>=0) && (task<=2) && (notEmpty == TRUE)){
                state = release;
            }
            else if ((task>=3) && (task<=8) && (notFull==TRUE)){
                state = request;
            }
            else{
                state = none;
            }

            // printf("CHILD:%d Request_type %d\n",req_pkt_id,state);

            if(state != none){
                for(int i=0;i<MAX_RESOURCE_COUNT;i++){
                    if(is_shared_vec[i]==FALSE){
                        if(state == request)
                            p->resource_instance_count[i] = getRandom((need[i]-allocated[i]),0);
                        else
                            p->resource_instance_count[i] = getRandom(allocated[i],0);
                    }
                    p->type[i] = state;
                }


                // for(int l=0;l<MAX_RESOURCE_COUNT;l++)
                //     printf("R%d:%d\t",l,p->resource_instance_count[l]);
                // printf("\n");

                int status = msgsnd(sendQ_id,&send_msg,sizeof(send_msg.req_pkt_id),0);
                if(status == -1){
                    char errorMsg [100];
                    snprintf(errorMsg,100,"cannot pass send_msg CHILD:%d",req_pkt_id);
                    perror(errorMsg);
                    semop(sem_id,&semRelease,1);
                    exit(0);
                }
                receiving =1;
            }
        }

        int status = msgrcv(recvQ_id,&recv_msg,sizeof(recv_msg.status),1,IPC_NOWAIT);
        if(status == -1){
        //    char errorMsg [100];
        //    snprintf(errorMsg,100,"cannot receive msg CHILD:%d",req_pkt_id);
        //    perror(errorMsg);
        }
        else{
            // allocated or released resources
            if(recv_msg.status == 1){
                receiving = 0;
                int count = 0;
                if(state==request){
                    for (size_t i = 0; i < MAX_RESOURCE_COUNT; i++){
                        if(is_shared_vec[i]==FALSE){
                            allocated[i] +=p->resource_instance_count[i];
                            if(allocated[i]==need[i])
                                count++;
                        }
                        else{
                            count++;
                        }
                    }
                    if(count == MAX_RESOURCE_COUNT)
                        notFull=FALSE;
                    notEmpty = TRUE;
                }
                else if(state == release){
                    for (size_t i = 0; i < MAX_RESOURCE_COUNT; i++){
                        if(is_shared_vec[i]==FALSE){
                            allocated[i] -=p->resource_instance_count[i];
                            if(allocated[i]==0)
                                count++;
                        }
                        else{
                            count++;
                        }
                    }
                    if(count == MAX_RESOURCE_COUNT)
                        notEmpty=FALSE;
                    notFull=TRUE;
                }
            }
            else if(recv_msg.status == -1){
                semop(sem_id,&semLock,1);
                // status becomes 1
            }
            // printf("CHILD:%d received status:%d\n",req_pkt_id,recv_msg.status);

            // for(int i=0;i<MAX_RESOURCE_COUNT;i++)
            //     printf("R%d:%d\t",i,allocated[i]);
            // printf("\n");
        }
        curr_time = updateTerminationTime();
        semop(sem_id,&semRelease,1);
    }

    // release all resources
    for(int i=0;i<MAX_RESOURCE_COUNT;i++){
        p->resource_instance_count[i] = allocated[i];
        p->type[i] = release;
        max_table[req_pkt_id][i] = 0;
    }
    msgsnd(sendQ_id,&send_msg,sizeof(send_msg.req_pkt_id),0);

    exit(EXIT_SUCCESS);
}

int setNewProcess(){
    if(createNewProcess==TRUE && process_itr<MAX_PROCESSES){

        int free_request_pkt = checkFreeRequestStruct();
        if(free_request_pkt!=-1){

            int id = fork();
            if(id==-1){
                perror("Cannot create new process");
                return -1;
            }
            else if(id==0){
                int id = free_request_pkt;
                childProcess(id);
                exit(0);
            }
            else{
                // printf("OSS:%d process created \n",process_itr+1);
                setpblock(free_request_pkt);
                process_req_map[free_request_pkt] = process_itr;
        // printfVector(process_req_map,"----------------------process map",MAX_PROCESS_IN_SYSTEM);
                childPids[process_itr]=id;
                process_itr++;
                return 1;
            }
        }
        return -2;
    }
    return -3;
}

int main(int argc, char ** argv){

    signal(SIGINT,signalHandle);
    signal(SIGTERM,signalHandleterminate);
    signal(SIGUSR1,signalHandleUsr1);
    signal(SIGALRM, signalHandleAlarm);

    // RESET SEM
    semLock.sem_num=0;
    semLock.sem_op=-1;
    semLock.sem_flg=0;

    semRelease.sem_num=0;
    semRelease.sem_op=1;
    semRelease.sem_flg=0;

    // default values
    t_s = DEFAULT_SS;

    // decode the inputs
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-h")==0){
            printf(HELP_STATEMENT);
            return 0;
        }
        else if(strcmp(argv[i],"-v")==0){
            verbose = atoi(argv[i+1]);
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

    // initialize clock semaphore
    sem_clock = semget(sem_clock_key,1,0666|IPC_CREAT);
    if(sem_clock==-1){
        perror("semget clock error");
        exit(1);
    }

    int status = semctl(sem_clock,0,SETVAL,1);
    if(status == -1){
        perror("semctl clock error");
        exit(1);
    }

    // set the alarm
    snprintf(logEntry,1000,"MASTER: started with t_s as %d\n",t_s);
    logMaster();

    createNewProcess = TRUE;
//    alarm(t_s);

    // initialize the resources
    int res_des_size = sizeof(struct res_des);

    for(int i=0;i<MAX_RESOURCE_COUNT;i++){
        res_des_id[i] = shmget((res_des_key_st+i),res_des_size,0666|IPC_CREAT);
        if(res_des_id[i] == -1){
            perror("Unable to get memory for resource\n");
            clear_oss_shared_mem();
            exit(0);
        }
        res_des_oss[i] = (struct res_des *)shmat(res_des_id[i],(void*)0,0);
        if(res_des_oss[i] == (struct res_des *)-1){
            perror("Unable to attach to memory for resource\n");
            clear_oss_shared_mem();
            exit(0);
        }
        float isSharedRes =  getProb();

        // initialize the resource descriptors
        res_des_oss[i]->resurceId=i+1;
        is_shared_vec[i] = FALSE;

        if(isSharedRes<=SHARED_RESOURCE_PROB){
            res_des_oss[i]->type=SHARED;
            res_des_oss[i]->instance_count = 1;
            is_shared_vec[i]=TRUE;
        }
        else{
            res_des_oss[i]->type=EXCLUSIVE;
            res_des_oss[i]->instance_count  =  getRandom(MAX_RESOURCE_INSTANCE,1);

            for(int j=0;j<res_des_oss[i]->instance_count;j++){
                res_des_oss[i]->instance_process_map[j]=0;
            }
        }
        available_count[i] = res_des_oss[i]->instance_count;
        total_count[i] = res_des_oss[i]->instance_count;
    }

    // printf("Resources Available\n");
    // for(int i=0;i<MAX_RESOURCE_COUNT;i++){
    //     printf("R%d:%d\t",i,total_count[i]);
    // }
    // printf("\n");

    //initialize request Set
    int request_pkt_size = sizeof(struct request_packet);

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){

        req_pkt_set_id[i] = shmget((request_packet_key_st+i),request_pkt_size,0666|IPC_CREAT);
        if(req_pkt_set_id[i] == -1){
            perror("Unable to get memory for request packet\n");
            clear_oss_shared_mem();
            exit(0);
        }
        req_pkt_set[i] = (struct request_packet *)shmat(req_pkt_set_id[i],(void*)0,0);
        if(req_pkt_set[i] == (struct request_packet*)-1){
            perror("Unable to attach to memory for request packet\n");
            clear_oss_shared_mem();
            exit(0);
        }

        req_pkt_set[i]->packet_id = i;
        for(int j=0;j<MAX_RESOURCE_COUNT;j++){
            req_pkt_set[i]->resource_instance_count[j]=0;
            req_pkt_set[i]->type[j] = none;
        }
    }

    // msg queues initialization
    sendQ_id = msgget(request_packet_msgQ_key,IPC_CREAT|0666);
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++)
        recvQ_id_set[i] = msgget((recv_msgQ_key_st+i),IPC_CREAT|0666);

    // initialize need table
    max_required_table_id  = shmget(max_required_table_key,sizeof (unsigned int[MAX_PROCESS_IN_SYSTEM][MAX_RESOURCE_COUNT]),IPC_CREAT|0666);
    max_required_table = shmat(max_required_table_id,(void *)0,0);
    // initialize the semaphore for processes

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        sem_process_id[i] = semget((sem_process_key_st+i),1,0666|IPC_CREAT);
        if(sem_process_id[i]==-1){
            perror("semget clock error");
            clear_oss_shared_mem();
            exit(1);
        }

        int status = semctl(sem_process_id[i],0,SETVAL,1);
        if(status == -1){
            perror("semctl clock error");
            clear_oss_shared_mem();
            exit(1);
        }

    }

    // while termination not achieved

    keepRunning=TRUE;
    process_itr=0;
    process_in_systm=0;
    processes_terminated =0;

    while(keepRunning){

        // create the processes
        status = -2;
        if(process_in_systm==0 && process_itr==MAX_PROCESSES){
            keepRunning=FALSE;
            break;
        }

        if(process_in_systm <=MAX_PROCESS_IN_SYSTEM){
            status = setNewProcess();
            if(status == 1){
                process_in_systm++;
            }
            else if(status==-2 ){
                // print the error
  //              printf("system full with process!!\n");
            }
            else if(status==-3 ){
                // print the error
//                printf("Total processes created\n");
            }
        }
        handleQueue();
        // if max process reached
        struct request_packet *p=NULL;

        if(status < 1){
            // check for any new request with wait
           p =  read_packet(WAIT);
        }
        else{
            // check for any new request without wait
           p =  read_packet(NOWAIT);
        }

        if(p!=NULL){
            status = process_request(p);
            // printf("Request Type:%d , CHILD:%d , status:%d\n",p->type[0],p->packet_id,status);
            write_pkt(status, p->packet_id);
        }
    }
    clear_oss_shared_mem();
    return 0;
}
