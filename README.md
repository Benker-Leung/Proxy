# Ben's simple Proxy

This proxy is for comp4621 project.


### Logger usage:

```
#incldue "logger.h"
log("^%d^ This is a log message with current time", 0);
```

### http_header_handler usage:

```
#include "http_header_handler.h"

/**
 *  This function helps to reformat the request header,
 *  e.g set [GET http://sgss.edu.hk/] to [GET /]
 * 
 *  Return (+ve) if no error, (-ve) if error occur (wrong format)
 * 
 */
int reformat_request_header(char* req_buf);

/**
 *  This function parse the content length from header
 * 
 *  Return +ve if success, (-ve) if fail
 * 
 */
int get_content_length(char* buf);

/**
 *  This function check whether the request is persistent or not
 * 
 *  Return 1 if consistent, 0 if not consistent
 *          -1 if wrong syntax
 * 
 */
int is_persistent(char* buf);

/**
 *  This function check whether the data part is chunked or not
 * 
 *  Return 1 if chunked, 0 if not chunked, and -1 if error
 * 
 */
int is_chunked(char *buf);

/**
 *  This function get the request method, check enum above
 * 
 *  Return (+ve) if success parsed, 0 if wrong or not supported
 */
int get_request_method(char* buf);
```


### network_handler usage:
```
/**
 *  This function clear the buffer with given size 
 * 
 */
void clear_buffer(char* req_buffer, char* res_buffer, int size);


/**
 *  This function get file descriptor given ip, default port is 80
 * 
 *  Return (+ve) if success, -1 if fail
 * 
 */
int get_serverfd(char* ip_buf);


/* ================================= Actual function can be used outside are below =================== */


/**
 *  This function return fd that can accept connection
 * 
 *  Return nonzero if success, zero if fail
 * 
 */
int get_listen_fd(int port, int maxListen);


/**
 *  This function get ip by host name
 * 
 *  Return -1 if resolve fail, 1 if success
 * 
 */
int get_ip_by_host(char* host, char* ip_buf);

/**
 *  This function get request/response header, non-blocking takes place
 * 
 *  Return -EAGAIN if not ready to read
 *         -1 if error, check errno
 *          0 if success
 */
int get_reqres_header(int fd, char* buf, int size, int request_id);


/**
 *  This function handle all details stuff e.g parse hostname, get ip etc.
 *
 *  Return (+ve) ==> connected serverfd if success, -1 if fail 
 *
 */
int connect_server(char* req_buffer);


/**
 *  This function forward the req header only
 *
 *  Return (+ve) if success, (-ve) if fail
 *
 */
int forward_packet(int serverfd, char* req_buffer, int len);


/**
 *  This function read specific bytes from fd
 * 
 *  Return zero(0) if success, -1 if fail
 * 
 */
 int get_data_by_len(int fd, char* buf, int bytes_to_read);


/**
 *  This function init the header status struct
 * 
 *  Return 0 if success, return -1 if fail
 * 
 */
int init_header_status(struct header_status* hs, char* req_buf, enum HTTP_HEADER type);


/**
 *  This function handle all actions for proxy networking 
 *  one thread used to read, one used to write
 * 
 *  Return -1 if fail or timeout, should close this fd
 * 
 */
int proxy_routine(int fd, char* req_buffer, char* res_buffer, int size, int request_id);


/**
 *  This function handle all actions for proxy networking 
 *  one thread used to read, one used to write
 * 
 *  Return -1 if fail or timeout, should close this fd
 * 
 */
int proxy_routine_2(int fd, char* req_buffer, char* res_buffer, int size, int request_id);
```

