#include "os2021_thread_api.h"
#include "function_libary.h"
#include <string.h>
#include <json-c/json.h>
#include "parsed_json.c"
struct itimerval Signaltimer;
int threadID = 1;
ucontext_t dispatch_context;
ucontext_t timer_context;

typedef struct
{
    ucontext_t mycontext;
    int pid;
    char* name;
    char* entry_func;
    int cancel_mode;
    int other_cancel;
    char* state;
    char* b_priority; // base priority
    char* c_priority; // current priority
    int tq;
    int needtowait; // wait time
    int q_time; // stay in ready (system time)
    int waiting_time; // stay in waiting (system time)
} MyThread;

MyThread runningThread;

typedef struct QueueNode
{
    MyThread thread;
    struct QueueNode* next;
} QueueNode;

typedef struct Queue
{
    QueueNode* head;
    QueueNode* tail;
} Queue;

void QueueInit(Queue* q)
{
    if (q == NULL)
    {
        printf("Queue malloc error!\n");
        exit(-1);
    }
    q->head = NULL;
    q->tail = NULL;
}

void QueueDestroy(Queue* q)
{
    QueueNode* current = q->head;
    while(current)
    {
        QueueNode* _next = current->next;
        free(current);
        current = _next;
    }
    q->head = q->tail = NULL;
}

void QueuePush(Queue* q, MyThread x)
{
    QueueNode* newnode = (QueueNode*)malloc(sizeof(QueueNode));
    if(q==NULL)
    {
        printf("queue doesn't exist\n");
    }
    if (newnode == NULL)
    {
        printf("malloc error!\n");
        exit(-1);
    }
    newnode->thread = x;
    newnode->next = NULL;
    //printf("f name: %s\n", (newnode->thread).name);
    if (q->head == NULL)
    {
        q->head = q->tail = newnode;
        return;
    }
    else
    {
        q->tail->next = newnode;
        q->tail = newnode;
    }
    //printf("Push success\n");
}

MyThread QueuePop(Queue* q)
{
    MyThread catchThread = q->head->thread;
    QueueNode* next = q->head->next;
    free(q->head);
    q->head = next;
    if (q->head == NULL)
    {
        q->tail = NULL;
    }
    return catchThread;
}

MyThread QueueFront(Queue* q)
{
    return q->head->thread;
}

MyThread QueueBack(Queue* q)
{
    return q->tail->thread;
}

int QueueEmpty(Queue* q)
{
    if(q==NULL)
        printf("Queue NULLLLLL\n");
    return q->head == NULL ? 1 : 0;
}

int QueueSize(Queue* q)
{
    QueueNode* current = q->head;
    int size = 0;
    while (current)
    {
        ++size;
        current = current->next;
    }
    return size;
}

MyThread delNode(Queue* q, int id) // for waiting queue
{

    // Store head node
    QueueNode* temp = q->head;
    QueueNode* prev = NULL;

    // If head node itself holds
    // the key to be deleted
    if (temp != NULL && (temp->thread).pid == id)
    {
        q->head = temp->next; // Changed head
        //delete temp;            // free old head
        return temp->thread;
    }

    // Else Search for the key to be deleted,
    // keep track of the previous node as we
    // need to change 'prev->next' */
    else
    {
        while (temp != NULL && (temp->thread).pid != id)
        {
            prev = temp;
            temp = temp->next;
        }

        // If key was not present in linked list
        //if (temp == NULL)
        //return NULL;
        if(temp->next==NULL)
            q->tail = temp;
        // Unlink the node from linked list
        prev->next = temp->next;

        // Free memory
        return temp->thread;
    }
}
int inQueue(Queue* q, char* job)
{
    QueueNode* current = q->head;
    while(current)
    {
        if(strcmp((current->thread).name, job)==0 )
        {
            return 1;
        }
        current = current->next;
    }
    return 0;

}

MyThread getThread(Queue* q, char* job)
{
    // must check inQueue() first
    QueueNode* current = q->head;
    while(current)
    {
        if(strcmp((current->thread).name, job)==0 )
        {
            return current->thread;
        }
        current = current->next;
    }
    printf("GET THREAD FAILED\n");
    return (q->head)->thread; // just in case
}

void updateThread(Queue* q, MyThread x)
{
    QueueNode* current = q->head;
    while(current)
    {
        if(strcmp((current->thread).name, x.name)==0 )
        {
            current->thread = x;
            printf("uodated success\n");
        }
        current = current->next;
    }
}

void QueueCalReady(Queue* q)
{
    QueueNode* current = q->head;
    while(current)
    {
        (current->thread).q_time += 10;
        current = current->next;
    }
}

void QueueCalWaiting(Queue* q)
{
    QueueNode* current = q->head;
    while(current)
    {
        (current->thread).waiting_time += 10;
        (current->thread).needtowait -= 10;
        current = current->next;
    }
}

void printThread(MyThread temp)
{

    printf("*\t%-9d", temp.pid);
    printf("%-10s", temp.name);
    printf("%-10s", temp.state);
    printf("%-12s", temp.b_priority);
    printf("%-12s", temp.c_priority);
    printf("%-10d", temp.q_time);
    printf("%-7d", temp.waiting_time);
    printf("*\n");
}

void printQueue(Queue* q)
{
    QueueNode* current = q->head;
    while(current)
    {
        printThread(current->thread);
        current = current->next;
    }
}
/* declare queue */
static Queue* readyH;
static Queue* readyM;
static Queue* readyL;
static Queue* waiting[8][3];
static Queue* waitTimeQ;
static Queue* toReclaim;

int isLegalFunction(char* function)
{
    char* f1 = "Function1";
    char* f2 = "Function2";
    char* f3 = "Function3";
    char* f4 = "Function4";
    char* f5 = "Function5";
    if(!(strcmp(function,f1)) && !(strcmp(function,f2)) && !(strcmp(function, f3)) && !(strcmp(function, f4)) && !(strcmp(function, f5)))
        return 0;
    else
        return 1;
}

void* parsefunction(char* function)
{
    if(strcmp(function,"Function1")==0)
        return &Function1;
    else if(strcmp(function,"Function2")==0)
        return &Function2;
    else if(strcmp(function,"Function3")==0)
        return &Function3;
    else if(strcmp(function,"Function4")==0)
        return &Function4;
    else if(strcmp(function,"Function5")==0)
        return &Function5;
    else if(strcmp(function,"ResourceReclaim")==0)
        return &ResourceReclaim;
    else
    {
        printf("parse function error\n");
    }
}
int OS2021_ThreadCreate(char *job_name, char *p_function, char* priority, int cancel_mode)
{
    threadID++;
    if(!(isLegalFunction(p_function)))
        return -1;
    MyThread threadCreate;
    threadCreate.name = job_name;
    threadCreate.state = "READY";
    threadCreate.entry_func = p_function;
    threadCreate.b_priority = priority;
    threadCreate.c_priority = priority;
    threadCreate.cancel_mode = cancel_mode;
    threadCreate.other_cancel = 0;
    threadCreate.q_time = 0;
    threadCreate.waiting_time = 0;
    threadCreate.pid = threadID;
    CreateContext(&threadCreate.mycontext, NULL, parsefunction(p_function));
    //QueuePush(readyH, threadCreate);
    if(strcmp(priority, "H") == 0)
        QueuePush(readyH, threadCreate);
    else if(strcmp(priority, "M") == 0)
        QueuePush(readyM, threadCreate);
    else if(strcmp(priority, "L") == 0)
    {
        QueuePush(readyL, threadCreate);
    }
    else
        printf("Illegal priority");
    CreateContext(&threadCreate.mycontext, NULL, parsefunction(p_function));
    return threadID;
}

//void OS2021_ThreadCancel(char *job_name){}

void OS2021_ThreadWaitEvent(int event_id)
{
    printf("%s wants to wait for event %d\n", runningThread.name, event_id);
    runningThread.state = "WAITING";
    if(strcmp(runningThread.c_priority, "H") == 0)
        QueuePush(waiting[event_id][0], runningThread);
    else if(strcmp(runningThread.c_priority, "M") == 0)
        QueuePush(waiting[event_id][1], runningThread);
    else if(strcmp(runningThread.c_priority, "L") == 0)
    {
        QueuePush(waiting[event_id][2], runningThread);
    }
    //printf("get 1\n");
    //setcontext(&dispatch_context);
    swapcontext(&runningThread.mycontext, &dispatch_context);
}

void OS2021_ThreadSetEvent(int event_id)
{
    printf("some set event %d\n", event_id);
    MyThread temp;
    if(!(QueueEmpty(waiting[event_id][0])))  // H priority
    {
        //printf("wait high\n");
        temp = QueuePop(waiting[event_id][0]);
        temp.state = "READY";
        printf("%s changes the status of %s to READY\n", runningThread.name,
               temp.name);
        QueuePush(readyH, temp);
    }
    else if(!(QueueEmpty(waiting[event_id][1])))  // M priority
    {
        //printf("wait medium\n");
        temp = QueuePop(waiting[event_id][1]);
        temp.state = "READY";
        printf("%s changes the status of %s to READY\n", runningThread.name,
               temp.name);
        temp.c_priority = "H";
        printf("The priority of thread %s is changed from M to H\n", temp.name);
        QueuePush(readyH, temp); // Feedback upgrade
    }
    else if(!(QueueEmpty(waiting[event_id][2])))  // L priority
    {
        //printf("wait low\n");
        temp = QueuePop(waiting[event_id][2]);
        temp.state = "READY";
        printf("%s changes the status of %s to READY\n", runningThread.name,
               temp.name);
        temp.c_priority = "M";
        printf("The priority of thread %s is changed from L to M\n", temp.name);
        QueuePush(readyM, temp); // Feedback upgrade
    }
    else
        return;

}

void OS2021_ThreadWaitTime(int msec)
{
    runningThread.state = "WAITING";
    runningThread.needtowait = 10*msec;
    QueuePush(waitTimeQ, runningThread);
    //printf("get 2\n");
    swapcontext(&runningThread.mycontext, &dispatch_context);
    //setcontext(&dispatch_context);
}

void OS2021_DeallocateThreadResource()
{
    MyThread r;

    while(QueueEmpty(toReclaim)==0)
    {
        r = QueuePop(toReclaim);
        printf("The memory space by %s has been released\n",r.name);
    }
    runningThread.state = "READY";
    QueuePush(readyL, runningThread);
    swapcontext(&runningThread.mycontext, &dispatch_context);
    //setcontext(&dispatch_context);
}
void cancelCheck(Queue* q, char* job)
{
    if(strcmp(job,"reclaimer")==0)
        return;
    MyThread temp;
    int temp_id;
    if(inQueue(q, job))
    {
        temp = getThread(q, job);
        temp_id = temp.pid;
        if(temp.cancel_mode == 1)  // deferred: cancel_mode = 1
        {
            temp.other_cancel = 1;
            updateThread(q, temp);
            return;
        }
        else // cancel directly
        {
            temp = delNode(q, temp_id);
            temp.state = "TERMINATED";
            QueuePush(toReclaim, temp);
        }

    }
    else
        return;
}

void OS2021_ThreadCancel(char *job_name)
{
    if(strcmp(job_name,"reclaimer")==0)
        return;
    cancelCheck(readyH, job_name);
    cancelCheck(readyM, job_name);
    cancelCheck(readyL, job_name);
    cancelCheck(waitTimeQ, job_name);
    for(int i = 0; i<8; i++)
    {
        for (int j = 0; j<3; j++)
        {
            cancelCheck(waiting[i][j], job_name);
        }
    }

}

void OS2021_TestCancel()
{
    if(runningThread.other_cancel == 1)
    {
        runningThread.state = "TERMINATED";
        QueuePush(toReclaim, runningThread);
        printf("some canceled %s\n", runningThread.name);
        setcontext(&dispatch_context);
    }
    else return;

}

void CreateContext(ucontext_t *context, ucontext_t *next_context, void *func)
{
    getcontext(context);
    context->uc_stack.ss_sp = malloc(STACK_SIZE);
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = next_context;
    makecontext(context,(void (*)(void))func,0);
}

void ResetTimer()
{
    Signaltimer.it_value.tv_sec = 0;
    Signaltimer.it_value.tv_usec = 10000;
    if(setitimer(ITIMER_REAL, &Signaltimer, NULL) < 0)
    {
        printf("ERROR SETTING TIME SIGALRM!\n");
    }
}

static int count = 0;

void Upgrade(MyThread up)
{
    if(strcmp(up.c_priority, "H") == 0)
    {
        up.state = "READY";
        QueuePush(readyH, up);
    }
    else if(strcmp(up.c_priority, "M") == 0)
    {
        up.c_priority = "H";
        printf("The priority of thread %s is changed from M to H\n", up.name);
        up.state = "READY";
        QueuePush(readyH, up);
    }
    else if(strcmp(up.c_priority, "L") == 0)
    {
        up.c_priority = "M";
        printf("The priority of thread %s is changed from L to M\n", up.name);
        up.state = "READY";
        QueuePush(readyM, up);
    }

}
void Downgrade()
{
    //printf("indown\n");
    if(strcmp(runningThread.c_priority, "H") == 0)
    {
        runningThread.state = "READY";
        runningThread.c_priority = "M";
        printf("The priority of thread %s is changed from H to M\n",
               runningThread.name);
        QueuePush(readyM, runningThread);
    }
    else if(strcmp(runningThread.c_priority, "M") == 0)
    {
        runningThread.state = "READY";
        runningThread.c_priority = "L";
        printf("The priority of thread %s is changed from M to L\n",
               runningThread.name);
        QueuePush(readyL, runningThread);

    }
    else if(strcmp(runningThread.c_priority, "L") == 0)
    {
        runningThread.state = "READY";
        QueuePush(readyL, runningThread);
    }

}

void Checker(int signo)
{
    count++;
    //printf("get signal, %d counts!\n", count);
    if(runningThread.pid == 0)
    {
        //setcontext(&dispatch_context);
        Dispatcher();
    }

    //calculate thread related time

    QueueCalReady(readyH);
    QueueCalReady(readyM);
    QueueCalReady(readyL);
    for(int i = 0; i<8; i++)
    {
        for (int j = 0; j<3; j++)
        {
            QueueCalWaiting(waiting[i][j]);
        }
    }
    QueueCalWaiting(waitTimeQ);

    // need to switch state?
    int qlen = QueueSize(waitTimeQ);
    int id_list[qlen];
    QueueNode *t = waitTimeQ->head;
    MyThread catch_del;
    int k = 0;
    while(t)
    {
        if((t->thread).needtowait<=0)
        {
            id_list[k] = (t->thread).pid;
            k++;
        }
        t = t->next;
    }

    for(k = 0; k<qlen; k++)
    {
        if(id_list[k] == 0)
            break;
        catch_del = delNode(waitTimeQ, id_list[k]);
        Upgrade(catch_del);
    }
    // running out of TQ?
    //printf("hereee\n");
    runningThread.tq -= 10;
    if(runningThread.tq <= 0)
    {
        Downgrade();
        //printf("get 3\n");
        swapcontext(&runningThread.mycontext, &dispatch_context);

        //setcontext(&dispatch_context);
        //Dispatcher();
    }

    //printf("get signal, %d counts!\n", ++count);
}
void Dispatcher()
{
    //getcontext(&dispatch_context);
    //printf("back to dispatch\n");
    if(!(QueueEmpty(readyH)))
    {
        runningThread = QueuePop(readyH);
        runningThread.state = "RUNNING";
        runningThread.tq = 100;
        //printf("get 4: running name: %s\n", runningThread.name);
        //swapcontext(&dispatch_context, &runningThread.mycontext);
        setcontext(&runningThread.mycontext);
    }
    else if(!(QueueEmpty(readyM)))
    {
        runningThread = QueuePop(readyM);
        runningThread.state = "RUNNING";
        runningThread.tq = 200;
        //printf("get 5: running name: %s\n", runningThread.name);
        setcontext(&runningThread.mycontext);
        //swapcontext(&dispatch_context, &runningThread.mycontext);
    }
    else if(!(QueueEmpty(readyL)))
    {
        runningThread = QueuePop(readyL);
        runningThread.state = "RUNNING";
        runningThread.tq = 300;
        //printf("get 6: running name: %s\n", runningThread.name);
        setcontext(&runningThread.mycontext);
        //swapcontext(&dispatch_context, &runningThread.mycontext);
    }
    else
    {
        printf("All ready queues are clear.\n");
        return;
    }
    //swapcontext(&dispatch_context, &runningThread.mycontext);

}

void handleCtrlZ()
{
    printf("\n**************************************************************");
    printf("****************\n");
    printf("*\t%-9s", "TID");
    printf("%-10s", "NAME");
    printf("%-10s", "STATE");
    printf("%-12s", "B_PRIORITY");
    printf("%-12s", "C_PRIORITY");
    printf("%-10s", "Q_TIME");
    printf("%-8s", "W_TIME *");
    //printf("\n");

    printThread(runningThread);
    printQueue(readyH);
    printQueue(readyM);
    printQueue(readyL);
    for(int i=0; i<8; i++)
    {
        for(int j = 0; j<3; j++)
        {
            printQueue(waiting[i][j]);
        }
    }
    printQueue(waitTimeQ);

    printf("**************************************************************");
    printf("****************\n");

}
void parseJson()
{
    OS2021_ThreadCreate("reclaimer","ResourceReclaim","L",1);
    ParsedJson();
}
void StartSchedulingSimulation()
{
    /* Reclaimer !!!! */

    /* Set Timer */
    Signaltimer.it_interval.tv_usec = 10000;
    Signaltimer.it_interval.tv_sec = 0;
    ResetTimer();
    //signal(SIGALRM, printMes);
    //signal(SIGALRM, Checker);

    readyH = (Queue*)malloc(sizeof(Queue));
    readyM = (Queue*)malloc(sizeof(Queue));
    readyL = (Queue*)malloc(sizeof(Queue));
    waitTimeQ = (Queue*)malloc(sizeof(Queue));
    toReclaim = (Queue*)malloc(sizeof(Queue));

    QueueInit(readyH);
    QueueInit(readyM);
    QueueInit(readyL);
    QueueInit(waitTimeQ);
    QueueInit(toReclaim);
    for(int i = 0; i<8; i++) //init waiting queue
    {
        for(int j = 0; j<3; j++)
        {
            waiting[i][j] = (Queue*)malloc(sizeof(Queue));
            QueueInit(waiting[i][j]);
        }
    }
    printf("queue init\n");

    runningThread.pid = 0;
    signal(SIGTSTP, handleCtrlZ);

    setitimer(ITIMER_REAL, &Signaltimer, NULL);
    if(setitimer(ITIMER_REAL, &Signaltimer, NULL) <0)
        printf("set timer failed\n");
    parseJson();
    CreateContext(&dispatch_context, NULL, &Dispatcher);
    signal(SIGALRM, Checker);

    while(1);

    /*Create Context*/
    //CreateContext(&dispatch_context, &timer_context, &Dispatcher);
    //setcontext(&dispatch_context);
}
