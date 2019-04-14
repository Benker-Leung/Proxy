#ifndef __HTTP_HEADER_HANDLER_H
#define __HTTP_HEADER_HANDLER_H

enum HTTP_METHOD{NOT_SUPPORTED=0, GET, POST};

/**
 *  This function helps to reformat the request header,
 *  e.g set [GET http://sgss.edu.hk/] to [GET /]
 * 
 *  Return (+ve) if no error, (-ve) if error occur (wrong format)
 * 
 */
int reformat_request_header(char* req_buf);

/**
 *  This function parse the content length from header
 * 
 *  Return +ve if success, (-ve) if fail
 * 
 */
int get_content_length(char* buf);

/**
 *  This function check whether the request is persistent or not
 * 
 *  Return 1 if consistent, 0 if not consistent
 *          -1 if wrong syntax
 * 
 */
int is_persistent(char* buf);

/**
 *  This function check whether the data part is chunked or not
 * 
 *  Return 1 if chunked, 0 if not chunked, and -1 if error
 * 
 */
int is_chunked(char *buf);

/**
 *  This function get the request method, check enum above
 * 
 *  Return (+ve) if success parsed, 0 if wrong or not supported
 */
int get_request_method(char* buf);

#endif