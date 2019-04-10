#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include "logger.h"
#include "network_handler.h"

/* clear the buffer */
void clear_buffer(char* req_buffer, char* res_buffer, int size) {
    bzero(req_buffer, size);
    bzero(res_buffer, size);
    return;
}

/* get server file descriptor given ip, called by connect_server() */
int get_serverfd(char* ip_buf) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa;
    bzero(&sa, sizeof(struct sockaddr_in));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(80);
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

    status = getaddrinfo(host, "http", &hints, &result);
    if(status != 0) {
        log("Invalid host: [%s], return: [%d]\n", host, status);
        return -1;
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
        j = recv(fd, &r, 1, MSG_DONTWAIT);
        if(j <= 0) {
            // not ready to read
            if(errno == EAGAIN) {
                // printf("Not ready to read\n");
                return -EAGAIN;
            }
            // other error
            else {
                log("Error in reading, errno: [%d], request [%d]\n", errno, request_id);
                return -1;
            }
        }
        buf[i++] = r;
        if(i >= 4) {
            // reach end of header
            if(buf[i-4] == 13 && buf[i-3] == 10 && buf[i-2] == 13 && buf[i-1] == 10) {
                buf[i] = '\0';
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
int connect_server(char* req_buffer) {

    char* start;
    char* end;
    // char* temp;
    int ret;
    char ip_buf[16];
    bzero(ip_buf, 16);

    start = end = strstr(req_buffer, "Host:");
    if(start) {
        while(*end != '\r' && *end != '\0')
            ++end;
        *end = '\0';
        // temp = strstr(start, "www.");
        // if(temp) {
        //     start = temp+4;
        // }
        // else
        // {
        //     start += 6;
        // }
        start += 6;
    }
    else {
        // fail to parse host
        return -1;
    }
    ret = get_ip_by_host(start, ip_buf);
    *end = ' ';
    if(ret == -1)
        // cannot solve ip
        return -1;

    ret = get_serverfd(ip_buf);
    if(ret <= 0)
        // cannot get server fd
        return -1;

    // got the server fd
    return ret;

}

/* forward the request header given server fd and req_buffer */
int forward_packet(int serverfd, char* buf) {

    int ret;
    int len = strlen(buf);

    while(len != 0) {
        ret = write(serverfd, buf, len);
        if(ret < 0) {
            log("Cannot forward request to server, errno: [%d]\n", errno);
            return ret;
        }
        len -= ret;
    }
    return 1;
}

/* get Content-Length if any */
int get_content_length(char* buf) {

    char* start;
    char* end;
    int len;

    start = end = strstr(buf, "Content-Length:");
    if(!start) {
        log("No content_length\n");
        return -1;
    }
    start += 16;
    // get number in Content-Length:
    while(*end != ' ' && *end != '\r'){
        if(*end == '\0' || *end == '\n'){
            log("Wrong format of header Content-Length field\n");
            return -1;
        }
        ++end;
    }
    *end = '\0';
    len = atoi(start);
    *end = ' ';
    return len;
}

/* read specific bytes */
int get_data(int fd, char* buf, int bytes_to_read) {
    
    int ret;

    while(bytes_to_read > 0) {
        ret = read(fd, buf, bytes_to_read);
        if(ret <= 0) {
            log("Error in getting data\n");
            return -1;
        }
        bytes_to_read -= ret;
        buf += ret;
    }
    return 0;
}


/* handle 1 proxy http request routine */
int proxy_routine(int fd, char* req_buffer, char* res_buffer, int size, int request_id) {

    int ret;
    int serverfd;
    char timeout = 0;
    char buf[10240];


    while(1) {

        if(timeout)
            sleep(1);

        if((ret = get_reqres_header(fd, req_buffer, size, request_id)) < 0) {
            // if not ready, give it a chance
            if(ret == -EAGAIN) {
                if(timeout < 3){
                    ++timeout;
                    continue;
                }
                else {
                    log("Timeout for the request [%d]\n", request_id);
                    clear_buffer(req_buffer, res_buffer, size);
                    return -EAGAIN;
                }
            }
            // error
            log("Error in get request [%d] header\n", request_id);
            clear_buffer(req_buffer, res_buffer, size);
            return -1;
        }
        else if(ret == 0) {
            printf("Header captured, request [%d]:\n", request_id);
            serverfd = connect_server(req_buffer);
            printf("%s\n", req_buffer);
            if(serverfd == -1){
                printf("Cannot connect to host\n");
                clear_buffer(req_buffer, res_buffer, size);
                return -1;
            }
            else {
                timeout = 0;
                printf("Connected to server fd:[%d]\n", serverfd);
                forward_packet(serverfd, req_buffer);
                while((ret = get_reqres_header(serverfd, res_buffer, size, request_id)) != 0) {
                    // printf("Waitng response header");
                }
                printf("Got response header\n");
                ret = get_content_length(res_buffer);
                printf("Parsed Content-Length: [%d]\n", ret);
                bzero(buf, 10240);
                ret = get_data(serverfd, buf, ret);
                if(ret < 0) {
                    printf("Cannot get data\n");
                    clear_buffer(req_buffer, res_buffer, size);
                    return -1;
                }

                printf("Got data\n");
                printf("%s\n", buf);

                forward_packet(fd, res_buffer);
                forward_packet(fd, buf);

                clear_buffer(req_buffer, res_buffer, size);
                close(serverfd);
            }
            return 0;
        }
        clear_buffer(req_buffer, res_buffer, size);
    }    

    return 0;
}