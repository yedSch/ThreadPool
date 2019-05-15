
//Yedaya Schwalm 302320056

#include "threadPool.h"


typedef struct {
    void *args;
    void (*func)(void *);

} Function;

//
//
// Error report
 void funcError() {
    write(2, "Error \n", strlen("Error \n"));
    exit(-1);
}


//Running the threadpool, getting the task as void*
static void *exec(void *x) {
    // try to convert to thread pool
    ThreadPool *tp = (ThreadPool *) x;
    if (tp == NULL) {
        funcError();
    }

    // checking the status of threads.
    int isOffline = (tp->progSts == STOPPED);
    int isRunning = (tp->progSts == RUNNING);
    int isQueueNotEmpty = !(osIsQueueEmpty(tp->queue));

// running until a change in the state/
    while (isRunning || (isOffline && isQueueNotEmpty)) {
        //mutex lock.
        if (pthread_mutex_lock(&(tp->mutex)) != 0) {
            funcError();
        }

        // block on condition
        while (tp->progSts == RUNNING && osIsQueueEmpty(tp->queue)) {

            if (pthread_cond_wait(&(tp->condition), &(tp->mutex)) != 0) {
                funcError();
            }
        }

        // start raising tasks from queue
        Function *processrun = (Function *) osDequeue(tp->queue);

        // mutex lock.
        if (pthread_mutex_unlock(&(tp->mutex)) != 0) {
            funcError();
        }

        // run the processrun
        if (processrun != NULL) {
            ((processrun->func))(processrun->args);
            free(processrun);
        }

        // update loop condition vars
        isRunning = (tp->progSts == RUNNING);
        isOffline = (tp->progSts == STOPPED);
        isQueueNotEmpty = !(osIsQueueEmpty(tp->queue));
    }

    pthread_exit(NULL);
}


// the function gets num of threads
// it creates and returns a thread pull with this num of threads
ThreadPool *tpCreate(int threadNum) {
    // case no positive num threads
    if (threadNum < 1) {
        return NULL;
    }

    // else try to create thread pool
    ThreadPool *tp = (ThreadPool *) malloc(sizeof(ThreadPool));
    if (tp == NULL) {
        funcError();
    }

    // set thread pool's fields
    tp->progSts = RUNNING;
    tp->numOfThread = threadNum;
    tp->queue = osCreateQueue();

    // try to init mutex
    if (pthread_mutex_init(&(tp->mutex), NULL) != 0) {
        free(tp);
        funcError();
    }

    // try to init condition
    if (pthread_cond_init(&(tp->condition), NULL) != 0) {
        free(tp);
        funcError();
    }

    // try to alloc threads
    int threadsSize = sizeof(pthread_t) * (size_t) threadNum;
    tp->threads = (pthread_t *) malloc(threadsSize);
    if (tp->threads == NULL) {
        free(tp);
        funcError();
    }

    // try to create threads
    int i;
    for (i = 0; i < threadNum; ++i) {
        if (pthread_create(&(tp->threads[i]), NULL, exec, (void *) tp) != 0) {
            tpDestroy(tp, 0);
            funcError();
        }
    }

    return tp;
}


//this function gets the threadpool, the function and args and runs them.
int tpInsertTask(ThreadPool *tp, void (*computeFunc)(void *), void *args) {
    // case thread pool isn't running
    if (tp->progSts != RUNNING) {
        return -1;
    }

    //
    // trying to allocate the task.
    Function *task = (Function *) malloc(sizeof(Function));
    if (task == NULL) {
        funcError();
    }

    //
    //
    task->args = args;
    task->func = computeFunc;

    // lock thread pool's mutex
    if (pthread_mutex_lock(&(tp->mutex)) != 0) {
        funcError();
    }

    // insert task to queue
    osEnqueue(tp->queue, task);

    // signal that queue isn't empty
    if (pthread_cond_broadcast(&(tp->condition)) != 0) {
        funcError();
    }

    // unlock thread pool's mutex
    if (pthread_mutex_unlock(&(tp->mutex)) != 0) {
        funcError();
    }

    return 0;
}


//the function receives a pointer to the threadpool and
// checks if it shoudl wait or not.
// it updates thread pool's Status according to shouldWaitForTasks and frees all
void tpDestroy(ThreadPool *tp, int shouldWaitForTasks) {
    // destroy tasks.
    if (tp->progSts == STOPPED) {
        return;
    }

    // mutex lock
    if (pthread_mutex_lock(&(tp->mutex)) != 0) {
        funcError();
    }

    // waiting is 0 - tasks need to be free.
    if (shouldWaitForTasks == 0) {
        while (!osIsQueueEmpty(tp->queue)) {
            free(osDequeue(tp->queue));
        }
    }

    // change status to stopped.

    tp->progSts = STOPPED;

    // unlock mutex
    if (pthread_mutex_unlock(&(tp->mutex)) != 0) {
        funcError();
    }

    // use condition to unblock threads.
    if (pthread_cond_broadcast(&(tp->condition)) != 0) {
        funcError();
    }

    // threads join
    int i;
    for (i = 0; i < tp->numOfThread; ++i) {

        if (pthread_join(tp->threads[i], NULL) != 0) {
            funcError();
        }
    }



// free data.
//


    free(tp->threads);
    osDestroyQueue(tp->queue);

    if (pthread_mutex_destroy(&(tp->mutex)) != 0) {
        funcError();
    }

    if (pthread_cond_destroy(&(tp->condition)) != 0) {
        funcError();
    }

    free(tp);
}
