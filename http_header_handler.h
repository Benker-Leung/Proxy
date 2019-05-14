#ifndef __HTTP_HEADER_HANDLER_H
#define __HTTP_HEADER_HANDLER_H

#include "data_structure.h"

/**
 *  This function get uri and uri_end, and set end to \0
 * 
 *  Return 0 if success, -1 if fail
 */
int get_uri_end(char* req_buffer, char** uri, char** uri_end);


/**
 *  This function get host and end, and set end to \0
 * 
 *  Return 0 if success, -1 if fail
 */
int get_host_end(char* req_buffer, char** host, char** end);

/**
 *  This function helps to reformat the request header,
 *  e.g set [GET http://sgss.edu.hk/] to [GET /]
 * 
 *  Return port number if no error, (-ve) if error occur (wrong format)
 * 
 */
int reformat_request_header(char* req_buf);


/**
 *  This function parse the content length given header buffer
 * 
 *  Return +ve if success, (-ve) if fail
 * 
 */
int get_content_length(char* buf);

/**
 *  This function check persistent given header buffer
 * 
 *  Return 1 if consistent, 0 if not consistent
 *          -1 if wrong syntax
 * 
 */
int is_persistent(char* req_buf);

/**
 *  This function check chunked or not given header buffer
 * 
 *  Return 1 if chunked, 0 if not chunked, and -1 if error
 * 
 */
int is_chunked(char *req_buf);


/**
 *  This function get the request method(enum) given req_buffer
 * 
 *  Return (+ve) if success parsed, 0 if wrong or not supported
 */
int get_request_method(char* req_buf);

/**
 *  This function determine whether the host is the same
 * 
 *  Return 1 if same, 0 if differ, -1 if fail
 */
int is_same_hostname(char* req_buffer, char* hostname);


/**
 *  This function determine base on request whether can be cache or not
 * 
 *  Return 1 if can cache, 0 if cannot cache, -1 if fail
 */
int is_cacheable(char* req_buffer);

/**
 *  This function get the response code e.g 200
 * 
 *  Return response code(+ve) if success, 0 if fail
 * 
 */
int get_response_code(char* res_buffer);

/**
 *  This function determine the host is in restricted or not
 * 
 *  Return 1 is able to access, 0 if cannot
 */
int can_access_web(char* host);


#endif