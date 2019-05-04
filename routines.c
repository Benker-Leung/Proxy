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

/* previous routine */
int proxy_routines(int clientfd, char* req_buffer, char* res_buffer, int buf_size, int request_id, char timeout_allow) {

    int ret;                    // store return value
    // int req_ready = 1;          // indicate that request buffer is ready for next usage
    int serverfd = -1;
    struct header_status req_hs;
    // struct header_status res_hs;

    // char client_timeout = 0;    // indicate the # of time client timeout

    // bzero(req_buffer, buf_size);    // to identify http request header attribute
    // bzero(res_buffer, buf_size);    // to identify http response header attribute

    struct timeval timeout = {30, 0};
    ret = setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
    if(ret)
        return -1;
    ret = setsockopt(clientfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval));
    if(ret)
        return -1;

    clear_buffer(req_buffer, res_buffer, buf_size);
    bzero(&req_hs, sizeof(struct header_status));
    // bzero(&res_hs, sizeof(struct header_status));

    // infinity loop
    while(1) {

        // // reached timeout
        // if(client_timeout >= timeout_allow) {
        //     log("Timeout for request[%d]\n", request_id);
        //     ret = -EAGAIN;
        //     goto EXIT_PROXY_ROUTINES;
        // }
        // // sleep to wait for a while
        // else if (client_timeout > 0){
        //     sleep(1);
        // }

        // // if the prev req is not yet retreive all
        // if(!req_ready) {
        //     return -1;
        // }

        bzero(req_buffer, buf_size);
        // try to get the request header from client
        ret = get_reqres_header(clientfd, req_buffer, buf_size, request_id);

        if(ret == -EAGAIN) {
            // // not ready to read request from client, timeout++
            // ++client_timeout;
            // // this should be change in future
            // continue;
            return -EAGAIN;
        }
        else if(ret == 0) {

            // reset client timeout, if got one request header
            // client_timeout = 0;

            // init the request header
            ret = init_header_status(&req_hs, req_buffer, REQUEST);
            if(ret == -1) {
                printf("================== HEADER NOT SUPPORTED ====================\n");
                printf("%s\n", req_buffer);
                ret = -1;
                goto EXIT_PROXY_ROUTINES;
            }

            // use different routines here
            switch(req_hs.http_method) {
                case CONNECT:
                    timeout_allow = 15;
                    ret = connect_https_server(req_buffer);
                    if(ret == -1) {
                        // printf("Error in handling https request id:[%d]\n", request_id);
                        // printf("%s\n", req_buffer);
                        ret = -1;
                        goto EXIT_PROXY_ROUTINES;
                    }
                    serverfd = ret;
                    // call the routine
                    ret = https_routine(clientfd, serverfd, req_buffer, res_buffer, buf_size, timeout_allow);
                    goto EXIT_PROXY_ROUTINES;

                case GET:
                    timeout_allow = 5;

                    // format the request header correctly
                    ret = reformat_request_header(req_buffer);
                    if(ret < 0)
                        goto EXIT_PROXY_ROUTINES;

                    // printf("======================= Request =======================\n");
                    // printf("%s", req_buffer);
                    // only connect to server for the first time
                    if(serverfd < 0) {
                        ret = connect_server(req_buffer, ret);
                        if(ret == -1) {
                            goto EXIT_PROXY_ROUTINES;
                        }
                        serverfd = ret;
                        ret = setsockopt(serverfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
                        if(ret) {
                            ret = -1;
                            goto EXIT_PROXY_ROUTINES;
                        }
                        ret = setsockopt(serverfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval));
                        if(ret) {
                            ret = -1;
                            goto EXIT_PROXY_ROUTINES;
                        }
                    }
                    
                    // call the routine
                    ret = no_cache_routine(clientfd, serverfd, req_buffer, res_buffer, buf_size, timeout_allow, request_id);
                    if(ret == serverfd) {
                        continue;
                    }
                    goto EXIT_PROXY_ROUTINES;

                default:
                    printf("================== METHOD NOT SUPPORTED YET ====================\n");
                    printf("%s\n", req_buffer);
                    ret = -1;
                    goto EXIT_PROXY_ROUTINES;
            }

            // correctly format the client request header
            ret = reformat_request_header(req_buffer);
            if(ret < 0) {
                // printf("Wrong request header for request[%d]\n", request_id);
                ret = -1;
                goto EXIT_PROXY_ROUTINES;
            }
            
            // clear the request buffer
            bzero(req_buffer, buf_size);
        }
        else {
            // printf("Fail to read client request for request[%d]\n", request_id);
            ret = -1;
            goto EXIT_PROXY_ROUTINES;
        }
    }
EXIT_PROXY_ROUTINES:
    if(serverfd > 0)
        close(serverfd);
    return ret;
}

/* routine for https request */
int https_routine(int clientfd, int serverfd, char* req_buffer, char* res_buffer, int buf_size, char timeout_allow) {

    int wrote;
    int ret;
    char timeout = 0;

    clear_buffer(req_buffer, res_buffer, buf_size);

    // response to client that connection established
    ret = write(clientfd, "HTTP/1.1 200 Connection Established\r\n\r\n", 40);

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
        ret = recv(clientfd, req_buffer, buf_size, MSG_DONTWAIT);
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
            wrote = write(serverfd, req_buffer, ret);
            if(wrote != ret) {
                printf("Error for https request during forwarding from client to server");
                return -1;
            }
            bzero(req_buffer, buf_size);
        }

        // read from server
        ret = recv(serverfd, res_buffer, buf_size, MSG_DONTWAIT);
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
            wrote = write(clientfd, res_buffer, ret);
            if(wrote != ret) {
                printf("Error for https request during forwarding from server to client");
                return -1;
            }
            bzero(res_buffer, buf_size);
        }
        
    }

    return 0;
}

/* routine for http request without cache */
int no_cache_routine(int clientfd, int serverfd, char* req_buffer, char* res_buffer, int buf_size, char timeout_allow, int thread_id) {

    int ret;        // record the return value result
    int is_persistent = 0;
    struct header_status hs;

    bzero(&hs, sizeof(struct header_status));
    ret = init_header_status(&hs, req_buffer, REQUEST);
    is_persistent = hs.is_persistent;

    // forward the request header to server
    ret = forward_packet(serverfd, req_buffer, strlen(req_buffer));
    if(ret < 0)
        return -1;

    // clear buffer
    bzero(res_buffer, buf_size);
    ret = get_reqres_header(serverfd, res_buffer, buf_size, 1);
    if(ret < 0)
        return -1;

    printf("\n======================= Request Start [%d] =======================\n", thread_id);
    printf("%s", req_buffer);
    printf("======================= Request End [%d] =======================\n", thread_id);
    printf("\n======================= Response Start [%d] =======================\n", thread_id);
    printf("%s", res_buffer);
    printf("\n======================= Response End [%d] =======================\n", thread_id);

    // forward the response header to client
    ret = forward_packet(clientfd, res_buffer, strlen(res_buffer));
    if(ret < 0)
        return -1;

    bzero(&hs, sizeof(struct header_status));

    // parse the response header
    ret = init_header_status(&hs, res_buffer, RESPONSE);

    if(hs.is_chunked) {
        ret = forward_data_chunked(clientfd, serverfd);
        if(ret < 0)
            return -1;
    }
    else if(hs.hv_data) {
        ret = forward_data_length(clientfd, serverfd, res_buffer, buf_size, hs.data_length);
        if(ret < 0)
            return -1;
    }

    if(is_persistent) {
        return serverfd;
    }

    return 0;

}







