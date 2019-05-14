#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include <pthread.h>

#define NUM_OF_CACHE_LOCK 101
#define DEFAULT_MAX_THREAD 3
#define MY_TIMEOUT 5
#define HEADER_BUFFER_SIZE 4096

// use for determine http request method
enum HTTP_METHOD{NOT_SUPPORTED=0, GET, POST, CONNECT};

// use for determine http header type
enum HTTP_HEADER{REQUEST=0, RESPONSE};

// use for store restricted_websites
struct restricted_websites {

    int num_of_entries; // record number of entries
    int num_of_sites;   // number of restricted web sites
    char** list;        // array of string to store websites chars

};

struct header_status {
    int http_method;    // indicate the method, only support(GET, POST)
    int is_persistent;  // indicate the HTTP version
    int hv_data;        // indicate whether HTTP hv data or nt
    int is_chunked;     // indicate the data transfer method
    int data_length;    // if is not chunked, the data len should be specify in content-length
    int cacheable;      // if can cache(1), cannot cache(0)
    int response_code;  // e.g 200 OK
};

/* used to param to a thread */
struct thread_param {
    int id;     // to distinguish thread_id
    int clientfd;     // to store the connection file descriptor
    char req_buffer[HEADER_BUFFER_SIZE];   // only support 4KB header
    char res_buffer[HEADER_BUFFER_SIZE];
    // char* request_header_buffer;   // only support 4KB header
    // char* response_header_buffer;
    pthread_mutex_t *thread_lock;               // for synchronize open or close socket
    struct restricted_websites* rw;
};




#endif
