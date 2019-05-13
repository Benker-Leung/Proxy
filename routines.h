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
int no_cache_routine(int serverfd, struct thread_param* tp, struct header_status* hs);


/**
 *  This function helps to handle the http with cache hit in local
 *  success(1) means cache is valid and the request is handled
 *  fail (0) means cache is not valid, and cache is deleted, do the same as no cache
 *  
 *  Return 1 if success, 0 if fail
 */
int cache_routine(int serverfd, struct thread_param* tp, struct header_status* hs, int cache_fd);


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