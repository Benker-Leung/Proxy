#define _GNU_SOURCE
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>


#include "logger.h"
#include "network_handler.h"
#include "routines.h"
#include "http_header_handler.h"
#include "cache_handler.h"


// struct header_status {
//     int http_method;    // indicate the method, only support(GET, POST)
//     int is_persistent;  // indicate the HTTP version
//     int hv_data;        // indicate whether HTTP hv data or nt
//     int is_chunked;     // indicate the data transfer method
//     int data_length;    // if is not chunked, the data len should be specify in content-length
// };

void print_header_status(struct header_status *hs) {
    printf("HTTP_Method: [%d]\n", hs->http_method);
    printf("Persistent: [%d]\n", hs->is_persistent);
    printf("Have_data: [%d]\n", hs->hv_data);
    printf("Is_chunked: [%d]\n", hs->is_chunked);
    printf("Data_length: [%d]\n", hs->data_length);
    return;
}


/* clear the buffer */
void clear_buffer(char* req_buffer, char* res_buffer, int size) {
    bzero(req_buffer, size);
    bzero(res_buffer, size);
    return;
}

/* get server file descriptor given ip, called by connect_server() */
int get_serverfd(char* ip_buf, int port) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa;
    bzero(&sa, sizeof(struct sockaddr_in));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_aton(ip_buf, &sa.sin_addr);

    if(connect(sockfd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        log("Connection fail to ip:[%s]\n", ip_buf);
        close(sockfd);
        return -1;
    }
    return sockfd;
}


/* ================================= Actual function can be used outside are below =================== */


/* get the listen fd given port and maxListen(no actual use) */
int get_listen_fd(int port, int maxListen) {

    int i;
    struct sockaddr_in servaddr; // use for config listen addr

    // get TCP socket, which only support IPv4
    int listenfd;
    
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log("Error in creating socket, errno: [%d]\n", errno);
        return 0;
    }

    bzero((char*)&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;    // only support IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;  // listen 0.0.0.0
    servaddr.sin_port = htons(port);   // listen to port

    // reuse the address, prevent errno 98
    i = 1;
    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int))) < 0) {
        log("Cannot reuse socket, errno: [%d]\n", errno);
        return 0;
    }

    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) < 0) {
        log("Error in binding, errno: [%d]\n", errno);
        close(listenfd);
        return 0;
    }

    if(listen(listenfd, maxListen) < 0) {
        log("Error in listening to socket, errno: [%d]\n", errno);
        close(listenfd);
        return 0;
    }

    return listenfd;
}

/* get ip addr and put into ip_buf */
int get_ip_by_host(char* host, char* ip_buf) {
    
    struct addrinfo hints;
    struct addrinfo* result;
    struct addrinfo* next;
    int status;

    hints.ai_family = AF_INET;      // IPv4 only
    hints.ai_socktype = SOCK_STREAM;// TCP
    hints.ai_protocol = 0;          // TCP

    // first try not remove 'www.'
    status = getaddrinfo(host, "http", &hints, &result);
    // if error, try remove 'www.' and again
    if(status != 0) {
        // remove "www."
        if(strncmp(host, "www.", 4) == 0) {
            host += 4;
        }
        status = getaddrinfo(host, "http", &hints, &result);
        if(status != 0) {
            log("Invalid host: [%s], ErrMsg(%d):[%s]\n", host, status, gai_strerror(status));
            return -1;
        }
    }

    for(next = result; next != NULL; next = next->ai_next) {
        struct sockaddr_in *p;
        p = (struct sockaddr_in*)next->ai_addr;
        if(inet_ntop(AF_INET, &(p->sin_addr), ip_buf, 15) != NULL) {
            log("Accepted request to %s(%s)\n", host, ip_buf);
            freeaddrinfo(result);
            return 1;
        }
    }
    freeaddrinfo(result);
    return -1;
}

/* get request/response header and put to buf */
int get_reqres_header(int fd, char* buf, int size, int request_id) {

    // get offset
    int i = strlen(buf);
    int j = 0;
    char r = 0;

    while(i < size-1) {
        // read 1 byte each time
        // j = recv(fd, &r, 1, MSG_DONTWAIT);
        j = recv(fd, &r, 1, MSG_NOSIGNAL);
        if(j <= 0) {
            // not ready to read
            if(errno == EAGAIN) {
                // printf("Not ready to read\n");
                // printf("***\n");
                return -EAGAIN;
            }
            // no data read
            else if(j == 0){
                return 1;
            }
            else {
                // log("Error in reading, errno: [%d], request [%d]\n", errno, request_id);
                return -1;
            }
        }
        buf[i++] = r;
        if(i >= 4) {
            // reach end of header
            if(buf[i-4] == 13 && buf[i-3] == 10 && buf[i-2] == 13 && buf[i-1] == 10) {
                buf[i] = '\0';
                // printf("{%s}", buf);
                return 0;
            }
        }
    }

    // header too long
    // printf("HTTP header too long, refuse the forward, request [%d]\n", request_id);
    log("HTTP header too long, denied, request [%d]\n", request_id);
    return -1;

}

/* get the server fd given req_buffer if success */
int connect_server(char* req_buffer, int port, char* hostname) {

    char* start;
    char* end;
    int ret;
    char ip_buf[16];
    bzero(ip_buf, 16);

    start = end = strcasestr(req_buffer, "Host:");
    if(start) {
        while(*end != '\r' && *end != '\0')
            ++end;
        *end = '\0';
        start += 6;
        strncpy(hostname, start, 255);
    }
    else {
        // fail to parse host
        return -1;
    }
    ret = get_ip_by_host(start, ip_buf);
    *end = '\r';
    if(ret < 0)
        // cannot solve ip
        return -1;

    ret = get_serverfd(ip_buf, port);
    if(ret <= 0)
        // cannot get server fd
        return -1;

    // got the server fd
    return ret;

}

/* get the https server fd given req_buffer if success */
int connect_https_server(char* req_buffer) {

    int ret;
    int port;
    char* start;
    char* start2;
    char* start3;
    char ip_buf[16];
    bzero(ip_buf, 16);

    start = strcasestr(req_buffer, "host:");
    if(!start) {
        return -1;
    }

    start += 6;
    // get the port number
    while(*start != ':') {
        if(*start == '\0' || *start == '\r' || *start == '\n')
            return -1;
        ++start;
    }
    start3 = start2 = start + 1;
    while(*start != '\r') {
        if(*start == '\0' || *start == '\n')
            return -1;
        ++start;
    }
    *start = '\0';
    port = atoi(start2);
    *start = '\r';

    // get the host name
    start = strcasestr(req_buffer, "host:");
    start += 6;
    --start3;
    *start3 = '\0';

    ret = get_ip_by_host(start, ip_buf);
    *start3 = ':';
    if(ret == -1)
        return -1;
    
    ret = get_serverfd(ip_buf, port);
    if(ret == -1)
        return -1;

    return ret;
}

/* forward the packet given server fd and buffer */
int forward_packet(int serverfd, char* buf, int len, int cache_fd) {

    int ret;  

    while(len != 0) {
        // ret = send(serverfd, buf, len, MSG_NOSIGNAL);
        ret = write(serverfd, buf, len);

        // cache
        if(cache_fd > 0) {
            write(cache_fd, buf, ret);
        }

        if(ret <= 0) {
            log("Cannot forward request to server, errno: [%d]\n", errno);
            return ret;
        }
        len -= ret;
        buf += ret;
    }
    return 1;
}


/* forward data len */
int forward_data_length(int dest_fd, int from_fd, char* buf, int buf_size, int length, int cache_fd) {

    int ret;
    bzero(buf, buf_size);

    while(length > 0) {
        // read from from fd
        // ret = recv(from_fd, buf, buf_size, MSG_NOSIGNAL);
        ret = read(from_fd, buf, buf_size);
        if(ret <= 0) {
            return -1;
        }
        length -= ret;
        // write to destination fd
        // ret = send(dest_fd, buf, ret, MSG_NOSIGNAL);
        ret = write(dest_fd, buf, ret);
        
        // cache
        if(cache_fd > 0) {
            write(cache_fd, buf, ret);
        }

        if(ret <= 0) {
            return -1;
        }
        bzero(buf, buf_size);
    }

    if(ret == -1)
        return ret;

    return 1;
}

/* forward data chunked */
int forward_data_chunked(int dest_fd, int from_fd, int cache_fd) {

    int ret;
    int status;
    char c;

    status = 0;
    while(1) {
        
        // ret = recv(from_fd, &c, 1, MSG_NOSIGNAL);
        ret = read(from_fd, &c, 1);
        if(ret <= 0)
            return -1;

        switch(c) {
            case 13:    // \r
                if(status%2 == 0)
                    ++status;
                break;
            case 10:    // \n
                if(status%2)
                    ++status;
                break;
            default:
                status = 0;
                break;
        }
        
        // ret = send(dest_fd, &c, 1, MSG_NOSIGNAL);
        ret = write(dest_fd, &c, 1);

        // cache
        if(cache_fd > 0) {
            write(cache_fd, &c, 1);
        }

        if(ret <= 0) {
            return -1;
        }

        if(status == 4)
            break;
        
    }
    return 1;
}


/* init header_status */
int init_header_status(struct header_status* hs, char* req_buf, enum HTTP_HEADER type) {

    int ret;


    // only request need to specify the request method
    if(type == REQUEST) {
        // get request method
        ret = get_request_method(req_buf);
        if(ret == NOT_SUPPORTED) {
            // printf("The http request method is not supported\n");
            return -1;
        }
        hs->http_method = ret;

        // get persistency of the request
        ret = is_persistent(req_buf);
        if(ret == -1) {
            // printf("The http version is not supported\n");
            return -1;
        }
        hs->is_persistent = ret;

        // determine can cache or not
        hs->cacheable = is_cacheable(req_buf);

        // method that do not have data
        if(hs->http_method == GET || hs->http_method == CONNECT) {
            return 0;
        }

    }

    // get the response code, 0 if fail
    hs->response_code = get_response_code(req_buf);

    // determine is chunked or not
    ret = is_chunked(req_buf);
    if(ret == -1) {
        return -1;
    }
    // data is chunked
    else if(ret == 1) {
        hs->is_chunked = 1;
        hs->hv_data = 1;
        hs->data_length = -1;
    }
    // check content-length if data is not chunked
    else if(ret == 0) {
        ret = get_content_length(req_buf);
        // if no content-length
        if(ret == -1) {
            hs->data_length = 0;
            hs->hv_data = 0;
        } 
        // if ot content-length
        else {
            hs->data_length = ret;
            hs->hv_data = 1;
        }
    }

    return 0;
}
