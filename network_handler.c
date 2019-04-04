#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "logger.h"
#include "network_handler.h"

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

int get_request_header(int fd, char* buf, int size) {

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
                printf("Not ready to read\n");
                return -EAGAIN;
            }
            // other error
            else {
                log("Error in reading, errno: [%d]\n", errno);
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
    printf("HTTP header too long, refuse the forward\n");
    log("HTTP header too long, denied\n");
    return -1;

}

int proxy_routine(int fd, char* req_buffer, char* res_buffer, int size) {


    int ret;
    char timeout = 0;
    
    while(1) {

        printf("routine...\n");
        if(timeout)
            sleep(3);

        if((ret = get_request_header(fd, req_buffer, size)) < 0) {
            // if not ready, give it a chance
            if(ret == -EAGAIN) {
                if(timeout < 3){
                    ++timeout;
                    continue;
                }
                else {
                    log("Timeout for the request fd:[%d]\n", fd);
                    printf("Timeout for the request fd:[%d]\n", fd);
                    return -EAGAIN;
                }
            }
            // error
            log("Error in get request header\n");
            printf("Cannot get request header\n");
            return -1;
        }
        else if(ret == 0) {
            printf("Header captured:\n");
            printf("%s\n", req_buffer);
            return 0;
        }
    }
    return 0;
}