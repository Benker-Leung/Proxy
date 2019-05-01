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

    int port_offset = 0;
    int port_len = 0;
    int port = 80;  // default port


    start = end = strcasestr(req_buf, "Host:");
    if(start) {
        while(*end != '\r' && *end != '\0')
            ++end;
        *end = '\0';
        // get host start address
        start += 6;
        host_len = strlen(start);

        // check if port is appened or not
        start = strcasestr(start, ":");
        if(start) {
            int temp;
            port_offset = start-req_buf;
            start += 1;
            sscanf(start, "%d", &port);
            temp = port;
            port_len = 2;
            while(temp/=10)
                ++port_len;
        }
        *end = '\r';
    }
    else {
        // fail to parse host
        return -1;
    }

    
    // remove http://.... to \0, in order words, it remains the uri
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

    // remove the additional port info in HOST:
    if(port_len > 0) {
        i = 0;
        while(port_len--) {
            req_buf[port_offset + i++] = 0;
        }
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

    return port;
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
        log("Wrong HTTP version, or not supported HTTP version\n");
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
int get_request_method(char* req_buf) {
    
    int ret;

    ret = strncasecmp(req_buf, "GET", 3);
    if(ret == 0) {
        return GET;
    }
    ret = strncasecmp(req_buf, "POST", 4);
    if(ret == 0) {
        return POST;
    }
    ret = strncasecmp(req_buf, "CONNECT", 7);
    if(ret == 0) {
        return CONNECT;
    }
    
    log("HTTP Method not supported by this proxy\n");
    return NOT_SUPPORTED;

}

