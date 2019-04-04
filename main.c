#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


#include "logger.h"
#define DEFAULT_MAX_THREAD 3

char* thread_status;    // 'a' as available, 'o' as occupied
pthread_t* thread;       // use to pthread_join

struct thread_param {
    int id;
};

void thread_action(void *args) {
    struct thread_param* tp = args;
    sleep(4);
    printf("Thread %d done\n", tp->id);
    thread_status[tp->id] = 'a';
    free(tp);
}

int main(int argc, char** argv) {

    int port, i, j, k;
    int max_thread = DEFAULT_MAX_THREAD;

    // check port number specify or not
    if(argc < 2) {
        printf("Missing the port number\n");
        printf("Usage: %s port_number [max_thread]\n", argv[0]);
        log("Fail to start proxy as missing port number\n");
        exit(0);
    }
    // assign port number
    port = atoi(argv[1]);

    // if max_thread specified, change to new one
    if(argc > 2) {
        max_thread = atoi(argv[2]);
        if(max_thread <= 0) {
            max_thread = DEFAULT_MAX_THREAD;
        }
    }

    // allocate and initial the status to available 'a'
    thread_status = malloc(sizeof(char) * max_thread);
    thread = calloc(max_thread, sizeof(pthread_t));
    memset(thread_status, 'a', max_thread);

    log("Start proxy at port [%d]\n", port);
    log("Assigned max_thread :[%d]\n", max_thread);


    // assume 10 request
    k = 10;
    while(k!=0) {
        // j record success allocate or not
        j = 0;
        // find a available thread location
        for(i=0; i<max_thread; ++i) {
            if(thread_status[i] == 'a') {
                struct thread_param* tp = calloc(1, sizeof(struct thread_param));
                tp->id = i;
                pthread_create(&thread[i], NULL, thread_action, tp);
                thread_status[i] = 'o';
                j = 1;
                break;
            }
        }
        // sleep(1);
        if(!j)
            printf("Fail to handle request [%d]\n", k);
        else
            printf("Handled request [%d]\n", k);
        --k;
    }

    void* ret;
    for(i=0; i<max_thread; ++i) {
        if(pthread_join(thread[i], &ret))
            log("Fail to join thread:[%d]\n", i);
    }


    return 0;
}