

// globals
int lc_sid;
struct lclock *lc_ptr;
int sem_clock;
int res_des_id [MAX_RESOURCE_COUNT];
struct res_des *res_des_oss [MAX_RESOURCE_COUNT];
int req_pkt_set_id[MAX_PROCESS_IN_SYSTEM];
struct request_packet * req_pkt_set [MAX_PROCESS_IN_SYSTEM];
int process_req_map[MAX_PROCESS_IN_SYSTEM];
int need_table_id;
int bit_vector[MAX_PROCESS_IN_SYSTEM];
int sendQ_id;
int recvQ_id_set[MAX_PROCESS_IN_SYSTEM];
int sem_process_id [MAX_PROCESS_IN_SYSTEM];
int childPids[MAX_PROCESSES];

unsigned int allocation_table[MAX_PROCESS_IN_SYSTEM][MAX_RESOURCE_COUNT];
unsigned int ** need_table;
unsigned int available_count[MAX_RESOURCE_COUNT];
unsigned int total_count[MAX_RESOURCE_COUNT];
int is_shared_vec[MAX_RESOURCE_COUNT];

int t_s;
char logEntry[1000];
int createNewProcess;
int keepRunning;
int process_itr;
int process_in_systm;


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

void updateProcessCreationTime(){
    int msec = getRandom(MAX_PROCESS_GEN_MSEC,1);
    updateTime(msec);
}

struct lclock updateTerminationTime(){
    int msec = getRandom(MAX_PROCESS_TERMINATION_MSEC,0);
    return updateTime(msec);
}

// returns in millisec
double getTimeDiff(struct lclock a,struct lclock b){
    double diff=0.0;
    diff = (a.sec - b.sec)*1000; //convert to millisec
    diff += (1.0*(a.nsec>b.nsec?(a.nsec-b.nsec):(b.nsec-a.nsec))/MAX_NANO_SEC)*1000; // convert to mllisec
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

// clear the shared mem before exit
void clear_oss_shared_mem(){

    // detach the process blocks

    // detach the clock
    shmdt(lc_ptr);
    shmctl(lc_sid,IPC_RMID,NULL);

    // remove clock semaphore
    semctl(sem_clock,0,IPC_RMID);
    // remove the semaphores
}

// SIGINT signal handle master
void signalHandle(){
    printf("OSS:Handled the SIGINT signal \n");
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

int process_request(struct request_packet * p){
    // banker algorithm
    return 0;
}

struct request_packet * read_packet(enum recvType type){
    int status;
    struct send_pkt p;

    if(type ==WAIT){
        status = msgrcv(sendQ_id,&p,sizeof(p.req_pkt_id),1,0);
    }
    else{
        status = msgrcv(sendQ_id,&p,sizeof(p.req_pkt_id),1,IPC_NOWAIT);
    }
    return req_pkt_set[p.req_pkt_id];
}


void write_pkt(int status,int id){
    struct recv_pkt p;
    p.mtype=1;
    p.status = status;

    msgsnd(recvQ_id_set[id],&p,sizeof(p.status),0);

    if(status == 1){
        // processed request
    }
    else if(status  == -1){
        // deadklock
    }
    else{
        // null
    }
 }

// child process
void childProcess(int req_pkt_id){

    unsigned int *t = (unsigned int *) shmat(need_table_id,(void *)0,0);
    unsigned int ** need_table = & t;

    int allocated[MAX_RESOURCE_COUNT];
    int need[MAX_RESOURCE_COUNT];

    for(int i =0;i<MAX_RESOURCE_COUNT;i++){
        if(is_shared_vec[i]==FALSE){
            need_table[req_pkt_id][i] = getRandom(total_count[i],0);
            need[i]=need_table[req_pkt_id][i];
            allocated[i]=0;
        }
    }

    struct lclock init_time = updateTerminationTime();

    printf("CHILD ");

    struct lclock curr_time ;

    curr_time.sec = init_time.sec;
    curr_time.nsec = init_time.nsec;
    printClock(curr_time);

    struct request_packet * p = (struct request_packet*) shmat(req_pkt_set_id[req_pkt_id],(void*)0,0);
    int recvQ_id = recvQ_id_set[req_pkt_id];
    int sem_id = sem_process_id[req_pkt_id];

    int B = 3;
    enum request_state state ;
    struct send_pkt send_msg;
    struct recv_pkt recv_msg;
    recv_msg.mtype=1;

    send_msg.mtype=1;
    send_msg.req_pkt_id = req_pkt_id;

    int receiving =0;
    int notFull = TRUE;

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
            break;
        }

        if(receiving ==0){

            int task = getRandom(B,0);
            if(task==0){
                state = release;
            }
            else if ((task==1) && (notFull==TRUE)){
                state = request;
            }
            else{
                state =none;
            }

            if(state != none){
                for(int i=0;i<MAX_RESOURCE_COUNT;i++){
                    if(is_shared_vec[i]==FALSE){
                        float select_res = getProb();
                        if(select_res>=0.50){

                            if(state == request)
                                p->resource_instance_count[i] = getRandom((need[i]-allocated[i]),0);
                            else
                                p->resource_instance_count[i] = getRandom(allocated[i],0);

                            p->type[i] = state;
                        }
                        else{
                            p->type[i] = none;
                        }
                    }
                }
                int status = msgsnd(sendQ_id,&send_msg,sizeof(send_msg.req_pkt_id),0);
                if(status == -1){
                    char errorMsg [100];
                    snprintf(errorMsg,100,"cannot pass send_msg CHILD:%d",req_pkt_id);
                    perror(errorMsg);
                    exit(0);
                }
                receiving =1;
            }
        }

        int status = msgrcv(recvQ_id,&recv_msg,sizeof(recv_msg.status),1,IPC_NOWAIT);
        if(status == -1){
            char errorMsg [100];
            snprintf(errorMsg,100,"cannot receive msg CHILD:%d",req_pkt_id);
            perror(errorMsg);
        }
        else{
            // allotated or released resources
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
                }
                else if(state == release){
                    for (size_t i = 0; i < MAX_RESOURCE_COUNT; i++){
                        if(is_shared_vec[i]==FALSE){
                            allocated[i] -=p->resource_instance_count[i];
                        }
                    }
                    notFull=TRUE;
                }
            }
            // send to wait queue
            else if(recv_msg.status == -1){
                semop(sem_id,&semLock,1);
            }
        }
        curr_time = updateTerminationTime();
        semop(sem_id,&semRelease,1);
    }

    // release all resources
    for(int i=0;i<MAX_RESOURCE_COUNT;i++){
        p->resource_instance_count[i] = allocated[i];
        p->type[i] = release;
    }

    msgsnd(sendQ_id,&send_msg,sizeof(send_msg.req_pkt_id),0);

    exit(EXIT_SUCCESS);
}

int setNewProcess(){
    if(createNewProcess==TRUE && process_itr<=MAX_PROCESSES){

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

                setpblock(free_request_pkt);
                process_req_map[free_request_pkt] = process_itr;
                childPids[process_itr]=id;
                process_itr++;
                return 1;
            }
        }
        return -2;
    }
    return -3;
}

void logMaster(){
    FILE * fptr;
    char logfile [] = "logfile.yxt";
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);

    fptr = fopen(logfile,"a");
    fprintf(fptr,"%d::%lld\t:\t%s",lc_ptr->sec,lc_ptr->nsec,logEntry);
    fclose(fptr);
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

    // initialise clock sempaphore
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
    snprintf(logEntry,1000,"OSS: started with t_s as %d\n",t_s);
    logMaster();

    createNewProcess = TRUE;
    alarm(0);
    printf("alarm : %d\n",alarm(t_s));

    // initialise the resources
    int res_des_size = sizeof(struct res_des);

    for(int i=0;i<MAX_RESOURCE_COUNT;i++){
        res_des_id[i] = shmget((res_des_key_st+i),res_des_size,0666|IPC_CREAT);
        if(res_des_id[i] == -1){
            perror("Unable to get memory for resource\n");
            exit(0);
        }
        res_des_oss[i] = (struct res_des *)shmat(res_des_id[i],(void*)0,0);
        if(res_des_oss[i] == (struct res_des *)-1){
            perror("Unable to attach to memory for resource\n");
            exit(0);
        }
        float isSharedRes =  getProb();

        // initialise the resource descriptors
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

    //initialise request Set
    int request_pkt_size = sizeof(struct request_packet);

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){

        req_pkt_set_id[i] = shmget((request_packet_key_st+i),request_pkt_size,0666|IPC_CREAT);
        if(req_pkt_set_id[i] == -1){
            perror("Unable to get memory for resource\n");
            exit(0);
        }
        req_pkt_set[i] = (struct request_packet *)shmat(req_pkt_set_id[i],(void*)0,0);
        if(req_pkt_set[i] == (struct request_packet*)-1){
            perror("Unable to attach to memory for resource\n");
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

    // initalize need table
    need_table_id  = shmget(need_table_key,(sizeof(unsigned int)*MAX_PROCESS_IN_SYSTEM*MAX_RESOURCE_COUNT),IPC_CREAT|0666);
    unsigned int *t = (unsigned int *) shmat(need_table_id,(void *)0,0);
    need_table = & t;
    // initialize the sem for processes

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        sem_process_id[i] = semget((sem_process_key_st+i),1,0666|IPC_CREAT);
        if(sem_process_id[i]==-1){
            perror("semget clock error");
            exit(1);
        }

        int status = semctl(sem_process_id[i],0,SETVAL,1);
        if(status == -1){
            perror("semctl clock error");
            exit(1);
        }

    }

    // while termination not achieved

    keepRunning=TRUE;
    process_itr=0;
    process_in_systm=0;

    while(keepRunning){

        // create the processes
        status = -2;

        if(process_in_systm <=MAX_PROCESS_IN_SYSTEM){
            status = setNewProcess();
            if(status == 1){
                process_in_systm++;
            }
            else if(status==-2 ){
                // print the error
                perror("system full with process!!\n");
            }
            else if(status==-3 ){
                // print the error
                perror("Totoal processes created\n");
            }
        }

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
            write_pkt(status, p->packet_id);
        }

    }
    return 0;
}
