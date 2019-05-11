#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#define MY_TIMEOUT 5
#define HEADER_BUFFER_SIZE 4096

// use for determine http request method
enum HTTP_METHOD{NOT_SUPPORTED=0, GET, POST, CONNECT};

// use for determine http header type
enum HTTP_HEADER{REQUEST=0, RESPONSE};

/* used to param to a thread */
struct thread_param {
    int id;     // to distinguish thread_id
    int clientfd;     // to store the connection file descriptor
    char req_buffer[HEADER_BUFFER_SIZE];   // only support 4KB header
    char res_buffer[HEADER_BUFFER_SIZE];
    // char* request_header_buffer;   // only support 4KB header
    // char* response_header_buffer;
    // pthread_mutex_t *thread_lock;               // for synchronize open or close socket
};


#endif
