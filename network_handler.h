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
 *  This function get request header, assume non-blocking takes place
 * 
 *  Return -EAGAIN if not ready to read
 *         -1 if error, check errno
 *          0 if success
 */
int get_request_header(int fd, char* buf, int size, int request_id);


int get_request_data();

int connect_server();
int preprocess_request();

int forward_request_header();
int forward_request_data();

int get_response_header();
int get_response_data();


/**
 *  This function handle all actions for proxy networking 
 *  one thread used to read, one used to write
 * 
 *  Return -1 if fail or timeout, should close this fd
 * 
 */
int proxy_routine(int fd, char* req_buffer, char* res_buffer, int size, int request_id);
