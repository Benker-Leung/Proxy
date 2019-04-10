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
 *  This function helps to reformat the request header,
 *  e.g set [GET http://sgss.edu.hk/] to [GET /]
 * 
 *  Return (+ve) if no error, (-ve) if error occur (wrong format)
 * 
 */
int reformat_request_header(char* req_buf);


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
 *  This function get request/response header, assume non-blocking takes place
 * 
 *  Return -EAGAIN if not ready to read
 *         -1 if error, check errno
 *          0 if success
 */
int get_reqres_header(int fd, char* buf, int size, int request_id);


/**
 *  This function handle all details stuff e.g parse hostname, get ip etc.
 *
 *  Return (+ve) ==> fd if success, -1 if fail 
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
 *  This function parse the content length from header
 * 
 *  Return +ve if success, (-ve) if fail
 * 
 */
int get_content_length(char* buf);


/**
 *  This function handle all actions for proxy networking 
 *  one thread used to read, one used to write
 * 
 *  Return -1 if fail or timeout, should close this fd
 * 
 */
int proxy_routine(int fd, char* req_buffer, char* res_buffer, int size, int request_id);












int get_request_data();
int preprocess_request();
int forward_request_data();
int get_response_data();


