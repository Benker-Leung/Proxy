#include <errno.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "routines.h"
#include "network_handler.h"
#include "http_header_handler.h"
#include "logger.h"
#include "cache_handler.h"

/* set recv and response to timeout */
int set_socket_timeout(int fd, int t) {
    
    int ret;
    struct timeval timeout = {t, 0};

    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
    if(ret) {
        return -1;
    }
    ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval));
    if(ret) {
        return -1;
    }
    return 0;

}


/* previous routine */
// int proxy_routines(int clientfd, char* req_buffer, char* res_buffer, int buf_size, int request_id, int timeout_allow) {
int proxy_routines(struct thread_param* tp) {

    int ret;                    // store return value
    int serverfd = -1;
    struct header_status req_hs;
    char hostname[256];

    // set socket timeout
    if(set_socket_timeout(tp->clientfd, MY_TIMEOUT)) {
        printf("Fail to set socket option to clientfd\n");
        return -1;
    }

    clear_buffer(tp->req_buffer, tp->res_buffer, HEADER_BUFFER_SIZE);


    // infinity loop
    while(1) {

        bzero(&req_hs, sizeof(struct header_status));
        bzero(tp->req_buffer, HEADER_BUFFER_SIZE);

        // try to get the request header from client
        ret = get_reqres_header(tp->clientfd, tp->req_buffer, HEADER_BUFFER_SIZE, tp->id);
        if(ret == -EAGAIN || ret == 1) {
            printf("Time out for thread[%d]\n", tp->id);
            return -EAGAIN;
        }
        else if(ret == 0) {

            // init the request header
            ret = init_header_status(&req_hs, tp->req_buffer, REQUEST);
            if(ret == -1) {
                printf("================== HEADER NOT SUPPORTED ====================\n%s\n", tp->req_buffer);
                ret = -1;
                goto EXIT_PROXY_ROUTINES;
            }

            // use different routines here
            switch(req_hs.http_method) {
                case CONNECT:
                    ret = connect_https_server(tp->req_buffer);
                    if(ret == -1) {
                        printf("Error in handling https request, thread_id:[%d]\n", tp->id);
                        // printf("%s\n", req_buffer);
                        ret = -1;
                        goto EXIT_PROXY_ROUTINES;
                    }
                    serverfd = ret;
                    // call the routine
                    ret = https_routine(tp->clientfd, serverfd, tp->req_buffer, tp->res_buffer, HEADER_BUFFER_SIZE, 2 * MY_TIMEOUT);
                    goto EXIT_PROXY_ROUTINES;

                case GET:
                case POST:

                    // format the request header correctly, and ret store the port#
                    ret = reformat_request_header(tp->req_buffer);
                    if(ret < 0) {
                        printf("Error in reformat request[%s]\n", tp->req_buffer);
                        goto EXIT_PROXY_ROUTINES;
                    }

                    // only connect to server for the first time
                    if(serverfd < 0) {
                        ret = connect_server(tp->req_buffer, ret, hostname);
                        if(ret == -1) {
                            printf("Fail to connect to server[%s]\n", hostname);
                            goto EXIT_PROXY_ROUTINES;
                        }
                        serverfd = ret;
                        // set socket timeout
                        if(set_socket_timeout(serverfd, MY_TIMEOUT)) {
                            printf("Fail to set socket\n");
                            ret = -1;
                            goto EXIT_PROXY_ROUTINES;
                        }

                    } else {
                        int port = ret;
                        // check whether is same host or not
                        ret = is_same_hostname(tp->req_buffer, hostname);
                        // if not the same hostname, new connection should be done
                        if(ret == 0) {
                            printf("Not same hostname detected!\n%s\n", tp->req_buffer);
                            close(serverfd);    // close the previous server socket
                            serverfd = -1;
                            ret = connect_server(tp->req_buffer, port, hostname);
                            if(ret == -1) {
                                printf("Fail to connect host[%s]\n", hostname);
                                goto EXIT_PROXY_ROUTINES;
                            }
                            serverfd = ret;
                            // set socket opt
                            if(set_socket_timeout(serverfd, MY_TIMEOUT)) {
                                printf("Fail to set socket\n");
                                ret = -1;
                                goto EXIT_PROXY_ROUTINES;
                            }
                        }
                        // if error
                        else if(ret < 0) {
                            printf("No hostname!!\n");
                            goto EXIT_PROXY_ROUTINES;
                        } else {
                            printf("Same hostname detected\n");
                        }
                    }
                    
                    // call the routine
                    ret = no_cache_routine(serverfd, tp, &req_hs);
                    if(ret == serverfd) {
                        continue;
                    }
                    if(ret == 0)
                        printf("Not persistent\n");
                    goto EXIT_PROXY_ROUTINES;

                default:
                    printf("================== METHOD NOT SUPPORTED YET ====================\n");
                    printf("%s\n", tp->req_buffer);
                    ret = -1;
                    goto EXIT_PROXY_ROUTINES;
            }
        }
        else {
            printf("Fail to read client request for thread_id[%d]\n", tp->id);
            ret = -1;
            goto EXIT_PROXY_ROUTINES;
        }
    }
EXIT_PROXY_ROUTINES:
    if(serverfd > 0)
        close(serverfd);
    return ret;
}

/* routine for http request without cache */
// int no_cache_routine(int clientfd, int serverfd, char* req_buffer, char* res_buffer, int buf_size, char timeout_allow, int thread_id) {
int no_cache_routine(int serverfd, struct thread_param* tp, struct header_status* hs) {

    int ret;        // record the return value result
    // int is_persistent = 0;

    // bzero(&hs, sizeof(struct header_status));
    // ret = init_header_status(&hs, tp->req_buffer, REQUEST);
    // is_persistent = hs->is_persistent;

    // forward the request header to server
    ret = forward_packet(serverfd, tp->req_buffer, strlen(tp->req_buffer));
    if(ret < 0) {
        printf("Fail to forward packet to serverfd\n");
        return -1;
    }

    // if it is post, get the payload and forward
    if(hs->http_method == POST) {
        if(hs->is_chunked) {
            ret = forward_data_chunked(serverfd, tp->clientfd);
        }
        else if(hs->hv_data) {
            bzero(tp->req_buffer, HEADER_BUFFER_SIZE);
            ret = forward_data_length(serverfd, tp->clientfd, tp->req_buffer, HEADER_BUFFER_SIZE, hs->data_length);
        }
        else {
            return -1;
        }
    }

    // clear buffer
    bzero(tp->res_buffer, HEADER_BUFFER_SIZE);
    ret = get_reqres_header(serverfd, tp->res_buffer, HEADER_BUFFER_SIZE, tp->id);
    if(ret < 0) {
        printf("Fail to get response header from serverfd\n");
        return -1;
    }

    printf("======================= Request [%d] =======================\n%s======================= Response [%d] =======================\n%s", tp->id, tp->req_buffer, tp->id, tp->res_buffer);

    // forward the response header to client
    ret = forward_packet(tp->clientfd, tp->res_buffer, strlen(tp->res_buffer));
    if(ret < 0) {
        printf("Fail to forward response header to client\n");
        return -1;
    }

    bzero(hs, sizeof(struct header_status));

    // parse the response header
    ret = init_header_status(hs, tp->res_buffer, RESPONSE);

    if(hs->is_chunked) {
        ret = forward_data_chunked(tp->clientfd, serverfd);
        if(ret < 0) {
            printf("Fail to forward chunked data to clientfd\n");
            return -1;
        }
    }
    else if(hs->hv_data) {
        ret = forward_data_length(tp->clientfd, serverfd, tp->res_buffer, HEADER_BUFFER_SIZE, hs->data_length);
        if(ret < 0) {
            printf("Fail to forward data length to clientfd\n");
            return -1;
        }
    }
    if(hs->is_persistent) {
        return serverfd;
    }

    return 0;

}

/* routine for https request */
int https_routine(int clientfd, int serverfd, char* req_buffer, char* res_buffer, int buf_size, char timeout_allow) {

    int wrote;
    int ret;
    char timeout = 0;

    clear_buffer(req_buffer, res_buffer, buf_size);

    // response to client that connection established
    ret = send(clientfd, "HTTP/1.1 200 Connection Established\r\n\r\n", 40, MSG_NOSIGNAL);

    if(ret != 40) {
        printf("Error in writing response to client CONNECT\n");
        return -1;
    }

    while(1) {

        if(timeout >= timeout_allow) {
            printf("Timeout for HTTPS request\n");
            return 0;
        }
        else if(timeout > 0) {
            sleep(1);
        }

        // read from client
        ret = recv(clientfd, req_buffer, buf_size, MSG_DONTWAIT|MSG_NOSIGNAL);
        if(ret <= 0) {
            if(errno == EAGAIN) {
                ++timeout;
            }
            else {
                printf("Error for https request during reading from client\n");
                return -1;
            }
        }
        // forward to server
        else {
            timeout = 0;
            wrote = send(serverfd, req_buffer, ret, MSG_NOSIGNAL);
            if(wrote != ret) {
                printf("Error for https request during forwarding from client to server");
                return -1;
            }
            bzero(req_buffer, buf_size);
        }

        // read from server
        ret = recv(serverfd, res_buffer, buf_size, MSG_DONTWAIT|MSG_NOSIGNAL);
        if(ret <= 0) {
            if(errno == EAGAIN) {
                ++timeout;
            }
            else {
                printf("Error for https request during reading from server\n");
                return -1;
            }
        }
        // forward to client
        else {
            timeout = 0;
            wrote = send(clientfd, res_buffer, ret, MSG_NOSIGNAL);
            if(wrote != ret) {
                printf("Error for https request during forwarding from server to client");
                return -1;
            }
            bzero(res_buffer, buf_size);
        }
        
    }

    return 0;
}





