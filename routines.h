#ifndef __ROUTINES_H
#define __ROUTINES_H

#include "data_structure.h"

/**
 *  This function helps to set the given socket the timeout
 * 
 *  Return 0 if success, -1 if fail
 */
int set_socket_timeout(int fd, int t);

/**
 *  This function helps to decide actual which routine 
 *  a particular request takes.
 * 
 *  Return -1 if fail or timeout, should close this fd
 * 
 */
// int proxy_routines(int fd, char* req_buffer, char* res_buffer, int size, int request_id, int timeout_allow);
int proxy_routines(struct thread_param* tp);


/**
 *  This function helps to handle the https request
 * 
 *  Return?
 * 
 */
int https_routine(int clientfd, int serverfd, char* req_buffer, char* res_buffer, int buf_size, char timeout_allow);

/**
 *  This function helps to handle the http with cache miss in local
 * 
 *  Jobs including forward the request, get back and update the response object
 * 
 *  Return?
 * 
 */
int no_cache_routine(int clientfd, int serverfd, char* req_buffer, char* res_buff, int buf_size, char timeout_allow, int thread_id);

/**
 *  This function helps to handle the http with cache hit in local
 * 
 *  Return?
 * 
 */
int cache_routine(char* req_header);
/**
 *  This function helps to handle the cache hit but outdate
 * 
 *  Return?
 */
int outdate_cache_routine(char* req_header);
/**
 *  This function helps to handle the cache hit and not outdate
 * 
 *  Return?
 */
int indate_cache_routine(char* req_header);




#endif