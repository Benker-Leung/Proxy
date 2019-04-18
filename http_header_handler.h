#ifndef __HTTP_HEADER_HANDLER_H
#define __HTTP_HEADER_HANDLER_H

enum HTTP_METHOD{NOT_SUPPORTED=0, GET, POST, CONNECT};

/**
 *  This function helps to reformat the request header,
 *  e.g set [GET http://sgss.edu.hk/] to [GET /]
 * 
 *  Return (+ve) if no error, (-ve) if error occur (wrong format)
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

#endif