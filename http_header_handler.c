#define _GNU_SOURCE
#include <string.h>

#include <strings.h>
#include <stdlib.h>

#include "http_header_handler.h"
#include "logger.h"

/* reformat request header */
int reformat_request_header(char* req_buf) {
    
    int len = strlen(req_buf);
    int host_len;
    int i, j;
    char* start;
    char* end;


    start = end = strcasestr(req_buf, "Host:");
    if(start) {
        while(*end != '\r' && *end != '\0')
            ++end;
        *end = '\0';
        // get host start address
        start += 6;
        host_len = strlen(start);
        *end = '\r';
    }
    else {
        // fail to parse host
        return -1;
    }

    
    // remove http:// or https:// to \0
    i = 7+host_len;
    j = 0;
    start = strcasestr(req_buf, "http://");
    if(!start) {
        log("Wrong HTTP header syntax\n");
        return -1;
    }
    while(j != i) {
        start[j++] = '\0';
    }

    // remove Accept-Encoding ??
    start = strcasestr(req_buf+j+i, "Accept-Encoding:");
    i=0;
    if(start) {
        while(start[i] != '\n'){
            start[i++] = '\0';
        }
        start[i] = '\0';
    }

    char* req = calloc(len+1, sizeof(char));
    i = j = 0;
    while(i != len) {
        if(req_buf[i] != '\0') {
            // copy those not \0 char only
            req[j++] = req_buf[i];
        }
        ++i;
    }
    req[j++] = '\0';
    bzero(req_buf, len);
    strncpy(req_buf, req, j);
    free(req);
    return 1;
}

/* get Content-Length if any */
int get_content_length(char* buf) {

    char* start;
    char* end;
    int len;

    start = end = strcasestr(buf, "Content-Length:");
    if(!start) {
        log("No content_length\n");
        return -1;
    }
    start += 16;
    // get number in Content-Length:
    while(*end != '\r'){
        if(*end == '\0' || *end == '\n'){
            log("Wrong format of header Content-Length field\n");
            return -1;
        }
        ++end;
    }
    *end = '\0';
    len = atoi(start);
    *end = '\r';
    return len;
}

/* get HTTP version, consistent or not */
int is_persistent(char* buf) {

    char* version = buf;
    // get the end of first line
    while(*version != '\r') {
        if(*version == '\0' || *version == '\n')
            return -1;
        ++version;
    }
    // check previous is 1 or 0
    --version;

    if(*version == '0')
        return 0;
    else if(*version == '1')
        return 1;
    else {
        log("Wrong HTTP version, or not supported\n");
        return -1;
    }
}

/* get content-delivery style, chunked or not */
int is_chunked(char *buf) {
    
    char *start;
    char *end;

    // get the start of it
    start = strcasestr(buf, "Transfer-Encoding:");
    // if not specify, then not chunked
    if(!start) {
        return 0;
    }
    end = strcasestr(start, "chunked");
    // if chunked not appear, then not chunked
    if(!end) {
        return 0;
    }
    // if specificed, then it is chunked
    else {
        return 1;
    }

}

/* get method of http request header */
int get_request_method(char* buf) {
    
    int ret;

    ret = strncasecmp(buf, "GET", 3);
    if(ret == 0) {
        return GET;
    }
    ret = strncasecmp(buf, "POST", 4);
    if(ret == 0) {
        return POST;
    }
    ret = strncasecmp(buf, "CONNECT", 8);
    if(ret == 0) {
        return CONNECT;
    }
    
    log("HTTP Method not supported by this proxy\n");
    return NOT_SUPPORTED;

}

