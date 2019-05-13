#define _GNU_SOURCE
#include <string.h>

#include <strings.h>
#include <stdlib.h>

#include "http_header_handler.h"
#include "logger.h"


/* This function get uri start and end */
int get_uri_end(char* req_buffer, char** uri, char** uri_end) {

    *uri = req_buffer;
    while(**uri != ' ') {
        if(**uri == '\0') {
            return -1;
        }
        ++*uri;
    }
    ++*uri;
    *uri_end = *uri;
    while(**uri_end != ' ') {
        if(**uri_end == '\0') {
            return -1;
        }
        ++*uri_end;
    }
    **uri_end = '\0';
    return 0;
}



/* This function get host start and end */
int get_host_end(char* req_buffer, char** host, char** end) {

    // get starting point of host
    *host = strcasestr(req_buffer, "host:");
    if(*host == NULL) {
        printf("Fail to add cache file\n");
        return -1;
    }
    *host += 6;
    *end = *host;
    while(**end != '\r') {
        if(**end == '\0') {
            printf("Fail to add cache file\n");
            return -1;
        }
        ++*end;
    }
    **end = '\0';
    return 0;

}

/* reformat request header */
int reformat_request_header(char* req_buf) {
    
    int len = strlen(req_buf);
    int host_len;
    int i, j;
    char* start;
    char* end;
    char* first_line_end;

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

    // only remove the http in the first line
    first_line_end = req_buf;
    while(*first_line_end != '\r') {
        if(*first_line_end == '\n' || *first_line_end == '\0')
            return -1;
        ++first_line_end;
    }
    *first_line_end = '\0';
    // remove http://.... to \0, in order words, it remains the uri
    i = 7+host_len;
    j = 0;
    start = strcasestr(req_buf, "http://");
    if(start) {
        while(j != i) {
            start[j++] = '\0';
        }
    }
    *first_line_end = '\r';

    // remove the additional port info in HOST:
    if(port_len > 0) {
        i = 0;
        while(port_len--) {
            req_buf[port_offset + i++] = 0;
        }
    }

    // // DEBUG
    // // remove Accept-Encoding ??
    // start = strcasestr(req_buf+j+i, "Accept-Encoding:");
    // i=0;
    // if(start) {
    //     while(start[i] != '\n'){
    //         start[i++] = '\0';
    //     }
    //     start[i] = '\0';
    // }

    // remove the \0 in the req_buf
    i = 0;
    // find the first \0
    while(req_buf[i] != '\0')
        ++i;
    j = i;
    while(j != len) {
        // if \0, skip
        if(req_buf[j] == '\0') {
            ++j;
            continue;
        }
        req_buf[i++] = req_buf[j++];
    }
    req_buf[i] = '\0';


    return port;
}

/* get Content-Length if any */
int get_content_length(char* buf) {

    char* start;
    char* end;
    int len;

    start = end = strcasestr(buf, "Content-Length:");
    if(!start) {
        // log("No content_length\n");
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

/* determine whether the host is the same */
int is_same_hostname(char* req_buffer, char* hostname) {

    char* start;
    char* end;
    int ret;

    start = end = strcasestr(req_buffer, "Host:");
    if(start) {
        while(*end != '\r' && *end != '\0')
            ++end;
        *end = '\0';
        start += 6;
        // printf("Original host[%s], New host[%s]\n", hostname, start);
        ret = strcmp(start, hostname);
        if(ret == 0) {
            // same
            *end = '\r';
            return 1;
        }
        else {
            strncpy(hostname, start, 255);
            *end = '\r';
            // different
            return 0;
        }
    }
    else {
        // fail to parse host
        return -1;
    }
}

