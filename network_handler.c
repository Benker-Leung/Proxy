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
#include "http_header_handler.h"


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
    int ret;
    char ip_buf[16];
    bzero(ip_buf, 16);

    start = end = strcasestr(req_buffer, "Host:");
    if(start) {
        while(*end != '\r' && *end != '\0')
            ++end;
        *end = '\0';
        start += 6;
    }
    else {
        // fail to parse host
        return -1;
    }
    ret = get_ip_by_host(start, ip_buf);
    *end = '\r';
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
int forward_packet(int serverfd, char* buf, int len) {

    int ret;

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


/* read specific bytes */
int get_data_by_len(int fd, char* buf, int bytes_to_read) {
    
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

/* init header_status */
int init_header_status(struct header_status* hs, char* req_buf, enum HTTP_HEADER type) {

    int ret;


    // only request need to specify the request method
    if(type == REQUEST) {
        // get request method
        ret = get_request_method(req_buf);
        if(ret == NOT_SUPPORTED) {
            printf("The http request method is not supported\n");
            return -1;
        }
        hs->http_method = ret;

        // get persistency of the request
        ret = is_persistent(req_buf);
        if(ret == -1) {
            printf("The http version is not supported\n");
            return -1;
        }
        hs->is_persistent = ret;
    }
    

    // determine is chunked or not
    ret = is_chunked(req_buf);
    if(ret == -1) {
        printf("Error when determine is chunked\n");
        return -1;
    }
    // data is chunked
    else if(ret == 1) {
        hs->hv_data = 1;
        hs->data_length = -1;
    }
    // check content-length if data is not chunked
    else if(ret == 0) {
        ret = get_content_length(req_buf);
        if(ret == -1) {
            hs->data_length = 0;
            hs->hv_data = 0;
        }
    }

    return 0;
}


int proxy_routine(int clientfd, char* req_buffer, char* res_buffer, int buf_size, int request_id, char timeout_allow) {

    // // this should be move the argument list later
    // char timeout_allow = 10;

    struct header_status req_hs;
    struct header_status res_hs;

    int ret;                    // store return value
    int serverfd = -1;
    int expected_req = 0;       // indicate the number of request header from server side
    int data_len = 0;           // indicate the length of data need to be forward or receive

    char client_timeout = 0;    // indicate the # of time client timeout
    char server_timeout = 0;    // indicate the # of time server timeout
    char data_buf[10240];       // 10KB data buffer

    bzero(req_buffer, buf_size);
    bzero(res_buffer, buf_size);
    bzero(&req_hs, sizeof(struct header_status));
    bzero(&res_hs, sizeof(struct header_status));

    // infinity loop
    while(1) {

        // reached timeout
        if(client_timeout >= timeout_allow || server_timeout >= timeout_allow) {
            log("Timeout for request[%d]\n", request_id);
            ret = -EAGAIN;
            goto EXIT_PROXY_ROUTINE;
        }
        // sleep to wait for a while
        else if (client_timeout > 0 || server_timeout > 0){
            sleep(1);
        }

GET_CLIENT_REQUEST:
        // try to get the request header from client
        ret = get_reqres_header(clientfd, req_buffer, buf_size, request_id);
        if(ret == -EAGAIN) {
            // not ready to read request from client, timeout++
            ++client_timeout;
            // go to read data from server side if possible
            if(expected_req > 0)
                goto GET_SERVER_RESPONSE;
            else
                continue;
        }
        else if(ret == 0) {
            
            // reset client timeout, if got one request header
            client_timeout = 0;

            // init the request header
            ret = init_header_status(&req_hs, req_buffer, REQUEST);
            if(ret == -1) {
                printf("================== NOT SUPPORTED HEADER ====================\n");
                printf("%s\n", req_buffer);
                goto EXIT_PROXY_ROUTINE;
            }
            // print header_status
            print_header_status(&req_hs);

            // get the serverfd for the first time
            if(serverfd == -1) {
                // get the connected serverfd
                ret = connect_server(req_buffer);
                if(ret <= 0) {
                    // fail to connect to this host
                    printf("Cannot connect to host for request[%d]\n", request_id);
                    ret = -1;
                    goto EXIT_PROXY_ROUTINE;
                }
                serverfd = ret;
            }

            // correctly format the client request header
            ret = reformat_request_header(req_buffer);
            if(ret < 0) {
                printf("Wrong request header for request[%d]\n", request_id);
                ret = -1;
                goto EXIT_PROXY_ROUTINE;
            }

            // forward the client request header
            ret = forward_packet(serverfd, req_buffer, strlen(req_buffer));
            if(ret < 0) {
                printf("Fail to forward client request header for request[%d]\n", request_id);
                ret = -1;
                goto EXIT_PROXY_ROUTINE;
            }

            printf("================================ Request Header ================================\n");
            printf("%s\n", req_buffer);
            
            // clear the request buffer
            bzero(req_buffer, buf_size);
            
            // increase the expected number of response header from the server side
            ++expected_req;

            // try to get more request header from client side
            goto GET_CLIENT_REQUEST;            
        }
        else {
            printf("Fail to read client request for request[%d]\n", request_id);
            ret = -1;
            goto EXIT_PROXY_ROUTINE;
        }

GET_SERVER_RESPONSE:
        // if expect is zero, then continue to get client request
        if(expected_req == 0)
            continue;

        // try to get the request header from server
        ret = get_reqres_header(serverfd, res_buffer, buf_size, request_id);
        if(ret == -EAGAIN) {
            // not ready to read request from server, timeout++
            ++server_timeout;
            goto GET_CLIENT_REQUEST;
        }
        else if(ret == 0) {

            // reset server timeout
            server_timeout = 0;

            // init the response header
            ret = init_header_status(&res_hs, req_buffer, RESPONSE);
            if(ret == -1) {
                goto EXIT_PROXY_ROUTINE;
            }
            // print header_status
            print_header_status(&res_hs);

            // get the content-length value if any
            ret = get_content_length(res_buffer);

            // if have content-length, just forward the response header back to client
            if(ret > 0) {
                data_len = ret;
                ret = get_data_by_len(serverfd, data_buf, data_len);
                if(ret == -1) {
                    printf("Error when getting response payload for request[%d]\n", request_id);
                    ret = -1;
                    goto EXIT_PROXY_ROUTINE;
                }
                printf("================================ Response Data ================================\n\n\n");
            }
            else {
                data_len = 0;
            }

            // forward the server response header
            ret = forward_packet(clientfd, res_buffer, strlen(res_buffer));
            if(ret < 0) {
                printf("Fail to forward server response header for request[%d]\n", request_id);
                ret = -1;
                goto EXIT_PROXY_ROUTINE;
            }

            // forward the data if any
            if(data_len > 0) {
                ret = forward_packet(clientfd, data_buf, data_len);
                if(ret < 0) {
                    printf("Fail to forward server payload for request[%d]\n", request_id);
                    ret = -1;
                    goto EXIT_PROXY_ROUTINE;
                }
                // clear data buffer
                bzero(data_buf, data_len);
                data_len = 0;
            }
            
            printf("================================ Response Header ================================\n");
            printf("%s\n", res_buffer);

            // clear the request buffer
            bzero(res_buffer, buf_size);

            // decrease the expected number of response header from the server side
            --expected_req;

            // try to get more response header if any
            if(expected_req > 0) {
                goto GET_SERVER_RESPONSE;
            }
            else
                goto GET_CLIENT_REQUEST;            
        }
        else {
            printf("Fail to read response header for request[%d]\n", request_id);
            ret = -1;
            goto EXIT_PROXY_ROUTINE;
        }
    }
EXIT_PROXY_ROUTINE:
    if(serverfd != -1)
        close(serverfd);
    return ret;
}
