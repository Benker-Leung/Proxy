#ifndef __NETWORK_HANDLER_H
#define __NETWORK_HANDLER_H

enum HTTP_HEADER{REQUEST=0, RESPONSE};

struct header_status {
    int http_method;    // indicate the method, only support(GET, POST)
    int is_persistent;  // indicate the HTTP version
    int hv_data;        // indicate whether HTTP hv data or nt
    int is_chunked;     // indicate the data transfer method
    int data_length;    // if is not chunked, the data len should be specify in content-length
};

/**
 *  This function clear the buffer with given size 
 * 
 */
void clear_buffer(char* req_buffer, char* res_buffer, int size);


/**
 *  This function get file descriptor given ip and port
 * 
 *  Return (+ve) if success, -1 if fail
 * 
 */
int get_serverfd(char* ip_buf, int port);

/**
 *  This function just print header type info
 */
void print_header_status(struct header_status *hs);


/* ================================= Actual function can be used outside are below =================== */


/**
 *  This function return fd that can accept connection given port and maxListen
 * 
 *  Return nonzero if success, zero if fail
 * 
 */
int get_listen_fd(int port, int maxListen);


/**
 *  This function fill in ip_buf given host, ip_buf with 16 bytes
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
 *  This function return serverfd given req_buffer
 *
 *  Return (+ve) ==> connected serverfd if success, -1 if fail 
 *
 */
int connect_server(char* req_buffer, int port, char* hostname);


/**
 *  This function return serverfd given req_buffer
 * 
 *  Return (+ve) ===> connected serverfd if success, -1 if fail
 * 
 */
int connect_https_server(char* req_buffer);


/**
 *  This function forward the packet in buf with length len, blocking write used
 *
 *  Return (+ve) if success, (-ve) if fail
 *
 */
int forward_packet(int serverfd, char* buf, int len);


/**
 *  This function read specific bytes from fd
 * 
 *  Return zero(0) if success, -1 if fail
 * 
 */
 int get_data_by_len(int fd, char* buf, int bytes_to_read);


 /**
  *  This function read and forward data specified in length
  * 
  *  Return 1 if success, -1 if fail
  */
int forward_data_length(int clientfd, int serverfd, char* buf, int buf_size, int length);

 /**
  *  This function read and forward chunked data
  * 
  *  Return 1 if success, -1 if fail
  */
 int forward_data_chunked(int clientfd, int serverfd);


/**
 *  This function init the header status struct
 * 
 *  Return 0 if success, return -1 if fail
 * 
 */
int init_header_status(struct header_status* hs, char* req_buf, enum HTTP_HEADER type);



#endif
