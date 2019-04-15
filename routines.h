#ifndef __ROUTINES_H
#define __ROUTINES_H

/**
 *  This function helps to decide actual which routine 
 *  a particular request takes.
 * 
 *  Return -1 if fail or timeout, should close this fd
 * 
 */
int proxy_routines(int fd, char* req_buffer, char* res_buffer, int size, int request_id, char timeout_allow);


/**
 *  This function helps to handle the https request
 * 
 *  Return?
 * 
 */
int https_routine(char* req_header);

/**
 *  This function helps to handle the http with cache miss in local
 * 
 *  Return?
 * 
 */
int no_cache_routine(char* req_header);

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