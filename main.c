#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "logger.h"
#include "network_handler.h"
#include "routines.h"
#include "data_structure.h"
#include "cache_handler.h"

// #define MY_TIMEOUT 5
// #define DEFAULT_MAX_THREAD 3
// #define HEADER_BUFFER_SIZE 4096

/* global variables used */
struct thread_param* tps;   // thread_param array
char* thread_status;    // status array, 'a' as available, 'o' as occupied
pthread_t* thread;       // use to pthread_join
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;  // used for synchronization
pthread_mutex_t cache_lock[NUM_OF_CACHE_LOCK];
pthread_attr_t pt_attr;
int port;               // port number
int max_thread;         // max thread
int proxyfd;               // proxyfd
int available_threads;      // the total available_thread


/* Init the restricted websites */
void init_restricted_websites(struct restricted_websites* rw) {

    char c;
    int i;
    int j = 0;


    // try to open
    int fd = open("restricted_websites", O_RDWR, 0644);
    if(fd <= 0) {
        // the file not exist
        rw->list = NULL;
        rw->num_of_sites = 0;
        rw->num_of_entries = 0;
        return;
    }

    // default make 10 entries, each 64 char
    rw->num_of_entries = 10;
    rw->num_of_sites = 0;
    rw->list = calloc(rw->num_of_entries, sizeof(char*));
    for(i=0; i<rw->num_of_entries; ++i) {
        rw->list[i] = calloc(64, sizeof(char));
    }

    // append content
    while(read(fd, &c, 1) > 0) {
        // need to extend
        if(rw->num_of_sites >= rw->num_of_entries) {
            rw->list = realloc(rw->list, (rw->num_of_entries+10) * sizeof(char*));
            if(rw->list == NULL) {
                printf("No mem\n");
                exit(0);
            }
            for(i=rw->num_of_entries; i<rw->num_of_entries+10; ++i) {
                rw->list[i] = calloc(64, sizeof(char));
            }
            rw->num_of_entries += 10;
        }
        // next one
        if(c == '\n' || j >= 63) {
            if(j == 0) {
                break;
            }
            rw->list[rw->num_of_sites][j] = '\0';
            rw->num_of_sites += 1;
            j = 0;
        }
        else {
            rw->list[rw->num_of_sites][j++] = c;
        }
    }
    return;
}



/* init the global variables about thread and port */
void init_proxy(int argc, char** argv) {

    int i;

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
    available_threads = max_thread;

    // setup the listen fd
    if((proxyfd = get_listen_fd(port, max_thread)) <= 0) {
        log("Fail to open listen fd, errno: [%d]\n", errno);
        exit(0);
    }

    // allocate and init the status to available 'a'
    thread_status = calloc(max_thread, sizeof(char));
    if(thread_status == NULL) {
        printf("Cannot allocate memory for thread_status\n");
        exit(0);
    }

    memset(thread_status, 'a', max_thread);

    
    // allocate and init thread array
    thread = calloc(max_thread, sizeof(pthread_t));
    if(thread == NULL) {
        printf("Cannot allocate memory for thread\n");
        exit(0);
    }
    // allocate thread_param arrays
    tps = calloc(max_thread, sizeof(struct thread_param));
    if(tps == NULL) {
        printf("Cannot allocate memory for thread_params\n");
        exit(0);
    }

    // init mutex
    for(i=0; i<NUM_OF_CACHE_LOCK; ++i) {
        if(pthread_mutex_init(&cache_lock[i], NULL)) {
            printf("Fail to init cache mutex\n");
            exit(0);
        }
    }

    // assign mutex
    for(i=0; i<max_thread; ++i) {
        tps[i].thread_lock = cache_lock;
    }

    // // assign the same lock to all thread_param object
    // for(i=0; i<max_thread; ++i) {
    //     // tps[i].thread_lock = &lock;
    //     tps[i].request_header_buffer = calloc(HEADER_BUFFER_SIZE, sizeof(char));
    //     tps[i].response_header_buffer = calloc(HEADER_BUFFER_SIZE, sizeof(char));
    //     if(tps[i].request_header_buffer == NULL || tps[i].response_header_buffer == NULL) {
    //         printf("Cannot allocate memory for tps header buffer\n");
    //         exit(0);
    //     }
    //     // clear_buffer(tps[i].request_header_buffer, tps[i].response_header_buffer, HEADER_BUFFER_SIZE);
    // }

    i = pthread_attr_init(&pt_attr);
    if(i) {
        printf("Cannot set pthread attr\n");
        exit(0);
    }
    i = pthread_attr_setstacksize(&pt_attr, 4194304);   // set 4MB
    if(i) {
        printf("Cannot set pthread stack size\n");
        exit(0);
    }

    // get list of restricted websites
    init_restricted_websites(&restricted_website);


    log("Start proxy, listen at port [%d]\n", port);
    log("Max_thread supported:[%d]\n", max_thread);

    // create cache_files directory
    if(mkdir("cache_files", 0744)) {
        if(errno != EEXIST) {
            printf("Cannot create cache_files dir\n");
            exit(0);
        }
    }
    // change directory to cache folder
    if(chdir("./cache_files")) {
        printf("Fail to change directory to cache_files\n");
        exit(0);
    }

}

/* do nothing function */
void do_nothing() {
    return;
}

/* action to be taken before exit */
void cleanup_before_exit() {

    int i;

    // lock
    pthread_mutex_lock(&lock);
    for(i=0; i<max_thread; ++i) {
        if(thread_status[i] == 'o') {
            close(tps[i].clientfd);
            pthread_cancel(thread[i]);
        }
    }
    // unlock
    pthread_mutex_unlock(&lock);
    // destroy the lock
    pthread_mutex_destroy(&lock);
    // destroy attr
    pthread_attr_destroy(&pt_attr);
    // close the listen fd
    close(proxyfd);
    printf("\nClosing proxy...\n\n\n");
    printf("\n");
    exit(0);
}


/* action to be taken by network thread */
void* thread_network_action(void *args) {

    int status;
    struct thread_param* tp = args;

    bzero(tp->req_buffer, HEADER_BUFFER_SIZE);
    if((status = proxy_routines(tp)) != 0) {
        if(status == -EAGAIN)
            printf_with_time("Detected Time out for thread[%d]\n", tp->id);
        else
            printf_with_time("Error in proxy routine, thread[%d], return code from routine[%d]\n", tp->id, status);
    }
    close(tp->clientfd);
    // lock
    pthread_mutex_lock(&lock);

    thread_status[tp->id] = 'z';
    ++available_threads;

    // unlock
    pthread_mutex_unlock(&lock);

    printf_with_time("Exit thread[%d], released fd[%d], (not atomic)available thread[%d]\n", tp->id, tp->clientfd, available_threads);
    // pthread_exit(NULL);
    return NULL;
}


int main(int argc, char** argv) {

    int i, j, k;
    int ret;
    void* res;
    // capture ctrl+c to cleanup before exit
    signal(SIGINT, cleanup_before_exit);
    signal(SIGPIPE, do_nothing);


    init_proxy(argc, argv);
    
    while(1) {  // loop to accept and handle new connection
        k=0;    // k for check connection well handled or not
        if((j = accept(proxyfd, NULL, NULL)) <= 0) {            
            log("Cannot accept connection, errno: [%d]\n", errno);
            printf_with_time("Fail to accept, sleeping for 3 seconds...\n");
            sleep(3);
            continue;
        }
        // sleep(3);
        // lock
        pthread_mutex_lock(&lock);
        for(i=0; i<max_thread; ++i) {
            if(thread_status[i] == 'z') {
                pthread_join(thread[i], &res);
                thread_status[i] = 'a';
                // printf_with_time("Cleared thread[%d]\n", i);
            }
            // if any thread is available
            if(thread_status[i] == 'a') {
                // assign i-th resouces to this new connection
                tps[i].id = i;
                tps[i].clientfd = j;
                // ret = pthread_create(&thread[i], &pt_attr, thread_network_action, &tps[i]);
                ret = pthread_create(&thread[i], &pt_attr, thread_network_action, &tps[i]);
                if(ret) {
                    // printf("Fail to create thread\n");
                    thread[i] = 'a';
                    
                    // unlock
                    pthread_mutex_unlock(&lock);
                    continue;
                }
                // set the status to occupied
                thread_status[i] = 'o';
                --available_threads;
                k = 1;
                break;
            }
        }
        // unlock
        pthread_mutex_unlock(&lock);
        if(!k) {
            close(j);
            printf_with_time("No thread available now\n");
            // close(proxyfd);
            sleep(3);
            continue;
            // if((proxyfd = get_listen_fd(port, max_thread)) <= 0) {
            //     log("Fail to open listen fd, errno: [%d]\n", errno);
            //     exit(0);
            // }
            // log("Too many connections, max_thread num:[%d] may not handle\n", max_thread);
            // sleep(3);
        }
        printf_with_time("Allocated thread[%d], fd[%d] to new connection, (not atomic)available thread remains[%d]\n", i, j, available_threads);
    }

    return 0;
}