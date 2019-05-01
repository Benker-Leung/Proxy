#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>


#include "logger.h"
#include "network_handler.h"
#include "routines.h"



#define DEFAULT_MAX_THREAD 3
#define HEADER_BUFFER_SIZE 4096

/* global variables used */
struct thread_param* tps;   // thread_param array
char* thread_status;    // status array, 'a' as available, 'o' as occupied
pthread_t* thread;       // use to pthread_join
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;  // used for synchronization
int port;               // port number
int max_thread;         // max thread
int proxyfd;               // proxyfd

/* used to param to a thread */
struct thread_param {
    int id;     // to distinguish thread_id
    int fd;     // to store the connection file descriptor
    char request_header_buffer[HEADER_BUFFER_SIZE];   // only support 4KB header
    char response_header_buffer[HEADER_BUFFER_SIZE];
};

/* init the global variables about thread and port */
void init_proxy(int argc, char** argv) {

    max_thread = DEFAULT_MAX_THREAD;

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

    // setup the listen fd
    if((proxyfd = get_listen_fd(port, max_thread)) <= 0) {
        log("Fail to open listen fd, errno: [%d]\n", errno);
        exit(0);
    }

    // allocate and init the status to available 'a'
    thread_status = malloc(sizeof(char) * max_thread);
    memset(thread_status, 'a', max_thread);
    // allocate and init thread array
    thread = calloc(max_thread, sizeof(pthread_t));
    // allocate thread_param arrays
    tps = calloc(max_thread, sizeof(struct thread_param));
    

    log("Start proxy, listen at port [%d]\n", port);
    log("Max_thread supported:[%d]\n", max_thread);

}

/* action to be taken before exit */
void cleanup_before_exit() {

    int i;

    // lock
    pthread_mutex_lock(&lock);
    for(i=0; i<max_thread; ++i) {
        if(thread_status[i] == 'o') {
            close(tps[i].fd);
            pthread_cancel(thread[i]);
        }
    }
    // unlock
    pthread_mutex_unlock(&lock);
    // destroy the lock
    pthread_mutex_destroy(&lock);
    // close the listen fd
    close(proxyfd);
    log("Closing proxy...\n\n\n");
    printf("\n");
    exit(0);
}


/* action to be taken by network thread */
void* thread_network_action(void *args) {

    int status;
    struct thread_param* tp = args;

    bzero(tp->request_header_buffer, HEADER_BUFFER_SIZE);
    if((status = proxy_routines(tp->fd, tp->request_header_buffer, tp->response_header_buffer, HEADER_BUFFER_SIZE, tp->id, 10)) != 0) {
        if(status == -EAGAIN)
            printf("Time out for thread[%d]\n", tp->id);
        else
            printf("Error in proxy routine, thread[%d]\n", tp->id);
    }

    // lock
    pthread_mutex_lock(&lock);

    thread_status[tp->id] = 'z';
    printf("clearing thread[%d], release fd[%d]\n", tp->id, tp->fd);
    close(tp->fd);

    // unlock
    pthread_mutex_unlock(&lock);
    pthread_exit(NULL);
}

int main(int argc, char** argv) {

    int i, j, k;
    int ret;
    // capture ctrl+c to cleanup before exit
    signal(SIGINT, cleanup_before_exit);


    init_proxy(argc, argv);
    
    i=0;    // for counting
    while(1) {  // loop to accept and handle new connection
        // printf("=========================== 1 ==================================\n");
        k=0;    // k for check connection well handled or not
        if((j = accept(proxyfd, NULL, NULL)) <= 0) {            
            log("Cannot accept connection, errno: [%d]\n", errno);
            printf("Fail to accept, sleeping for 3 seconds...\n");
            sleep(3);
            continue;
        }
        // printf("*****************************************************************\n");
        // lock
        pthread_mutex_lock(&lock);
        // printf("=========================== 2 ==================================\n");
        for(i=0; i<max_thread; ++i) {
            if(thread_status[i] == 'z') {
                pthread_join(thread[i], NULL);
                thread_status[i] = 'a';
            }
            // if any thread is available
            if(thread_status[i] == 'a' || thread_status[i] == 0) {
                // assign i-th resouces to this new connection
                tps[i].id = i;
                tps[i].fd = j;
                ret = pthread_create(&thread[i], NULL, thread_network_action, &tps[i]);
                if(ret) {
                    printf("Fail to create thread\n");
                    thread[i] = 'a';
                    continue;
                }
                // set the status to occupied
                thread_status[i] = 'o';
                k = 1;
                printf("Allocated thread[%d], fd[%d] to new connection\n", i, j);
                break;
            }
        }
        // unlock
        pthread_mutex_unlock(&lock);
        // printf("=========================== 3 ==================================\n");
        if(!k) {
            close(j);
            printf("No thread available now\n");
            log("Too many connections, max_thread num:[%d] may not handle\n", max_thread);
            sleep(1);
        }
    }

    return 0;
}