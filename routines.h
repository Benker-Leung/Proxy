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
 *  Return 0 if timeout, -1 if fail
 * 
 */
int https_routine(int clientfd, int serverfd, char* req_buffer, char* res_buffer, int buf_size, char timeout_allow);

/**
 *  This function helps to handle the http with cache miss in local
 * 
 *  Jobs including forward the request, get back and update the response object
 * 
 *  Return +ve(server_fd) is persistent, 0 if not persistent, -1 if fail
 * 
 */
int no_cache_routine(int serverfd, struct thread_param* tp, struct header_status* hs);


/**
 *  This function helps to handle the http with cache hit in local
 *  no need to close any fd!
 *  
 *  Return 0 if handled (403, 200), -1 if fail for any reasons
 */
int cache_routine(int serverfd, struct thread_param* tp, struct header_status* hs, int* cache_fd);


#endif