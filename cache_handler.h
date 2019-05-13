#ifndef CACHE_HANDLER_H
#define CACHE_HANDLER_H

// /**
//  *  This function gets the current time in format of 'If-Modify-Since'
//  * 
//  *  Return the static char address, need to use sprintf to copy to dest
//  */
// char *cache_get_time(const struct tm *timeptr);


/**
 *  This function create the files given Major number and hostname
 *  the minor number is auto assigned
 * 
 *  Return +ve(fd) if success, -1 if fail
 */
int cache_create_file_by_hostname(char* hostname, int major);


/**
 *  This function delete the files given Major number and Minor number and hostname
 * 
 *  Return 0 if success, -1 if fail
 */
int cache_delete_file_by_hostname(char* hostname, int major, int minor);



/**
 *  This function get the hash value base on uri
 * 
 *  Return 0-100, use (x%101)
 */
int cache_uri_hash(char* req_buffer);


/**
 *  This function add a file base on req_buffer
 * 
 *  Return +ve(fd) if success, -1 if fail
 */
int cache_add_file(char* req_buffer);


/**
 *  This function search a file base on req_buffer
 * 
 *  Return +ve(fd) if success, -1 if fail
 */
int cache_search_file(char* req_buffer);

/**
 *  This function delete cache file by req_buffer
 * 
 *  Return 0 if success, -1 if fail
 */
int cache_delete_file(char* req_buffer);




#endif