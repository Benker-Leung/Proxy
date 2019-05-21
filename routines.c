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

/* This function helps to handle the restricted results */
void restricted_routine(struct thread_param* tp) {

    int ret;

    ret = write(tp->clientfd, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 55\r\n\r\n", 71);
    if(ret <= 0)
        return;
    ret = write(tp->clientfd, "<html><head><h1>NOT FOUND 404(proxy)</h1></head></html>", 55);
    return;

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
                    ret = connect_https_server(tp->req_buffer, tp->rw);
                    if(ret == -1) {
                        printf("Error in handling https request, thread_id:[%d]\n", tp->id);
                        // printf("%s\n", req_buffer);
                        ret = -1;
                        goto EXIT_PROXY_ROUTINES;
                    }
                    else if(ret == 0) {
                        restricted_routine(tp);
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
                        ret = connect_server(tp->req_buffer, ret, hostname, tp->rw);
                        if(ret == -1) {
                            printf("Fail to connect to server[%s]\n", hostname);
                            goto EXIT_PROXY_ROUTINES;
                        }
                        else if(ret == 0) {
                            restricted_routine(tp);
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
                            ret = connect_server(tp->req_buffer, port, hostname, tp->rw);
                            if(ret == -1) {
                                printf("Fail to connect host[%s]\n", hostname);
                                goto EXIT_PROXY_ROUTINES;
                            }
                            else if(ret == 0) {
                                restricted_routine(tp);
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
    int lock_num;   // lock number
    int cache_fd = -1;   // cached file represent this request

    lock_num = cache_uri_hash(tp->req_buffer);
    pthread_mutex_lock(&(tp->thread_lock[lock_num]));

    if(hs->cacheable)
        cache_fd = cache_get_file_fd(tp->req_buffer);
    // when cache is found, the other routine should be responsible
    if(cache_fd > 0) {
        // validate cache
        ret = cache_routine(serverfd, tp, hs, &cache_fd);
        if(ret) {
            printf("Error in handling the cache routine\n");
        }
        if(is_persistent(tp->req_buffer)) {
            ret = serverfd;
        }
        goto EXIT_NO_CACHE_ROUTINE;
    }

    if(hs->cacheable && hs->http_method == GET)
    // new cache_fd, with miss local cache
        cache_fd = cache_add_file(tp->req_buffer);

    // forward the request header to server, if possible, cache the header
    ret = forward_packet(serverfd, tp->req_buffer, strlen(tp->req_buffer), cache_fd);
    if(ret < 0) {
        printf("Fail to forward packet to serverfd\n");
        ret = -1;
        goto EXIT_NO_CACHE_ROUTINE;
    }

    if(cache_fd > 0){
        ret = cache_add_date(cache_fd, NULL);
        if(ret)
            printf("Fail to add date to cache file\n");
    }


    // if it is post, get the payload and forward
    if(hs->http_method == POST) {
        if(hs->is_chunked) {
            ret = forward_data_chunked(serverfd, tp->clientfd, -1);
        }
        else if(hs->hv_data) {
            bzero(tp->req_buffer, HEADER_BUFFER_SIZE);
            ret = forward_data_length(serverfd, tp->clientfd, tp->req_buffer, HEADER_BUFFER_SIZE, hs->data_length, -1);
        }
        else {
            ret = -1;
            goto EXIT_NO_CACHE_ROUTINE;
        }
    }

    // clear buffer
    bzero(tp->res_buffer, HEADER_BUFFER_SIZE);
    ret = get_reqres_header(serverfd, tp->res_buffer, HEADER_BUFFER_SIZE, tp->id);
    if(ret < 0) {
        printf("Fail to get response header from serverfd\n");
        ret = -1;
        goto EXIT_NO_CACHE_ROUTINE;
    }

    printf("======================= Request [%d] =======================\n%s======================= Response [%d] =======================\n%s", tp->id, tp->req_buffer, tp->id, tp->res_buffer);

    lseek(cache_fd, 4096, SEEK_SET);
    // forward the response header to client, if possible, cache response header to lseek 4096
    ret = forward_packet(tp->clientfd, tp->res_buffer, strlen(tp->res_buffer), cache_fd);
    if(ret < 0) {
        printf("Fail to forward response header to client\n");
        ret = -1;
        goto EXIT_NO_CACHE_ROUTINE;
    }

    bzero(hs, sizeof(struct header_status));

    // parse the response header
    ret = init_header_status(hs, tp->res_buffer, RESPONSE);

    // delete cache file if response is not 200
    if(hs->response_code != 200) {
        close(cache_fd);
        cache_fd = -1;
        cache_delete_file(tp->req_buffer);
    }

    if(hs->is_chunked) {
        ret = forward_data_chunked(tp->clientfd, serverfd, cache_fd);
        if(ret < 0) {
            printf("Fail to forward chunked data to clientfd\n");
            ret = -1;
            goto EXIT_NO_CACHE_ROUTINE;
        }
    }
    else if(hs->hv_data) {
        ret = forward_data_length(tp->clientfd, serverfd, tp->res_buffer, HEADER_BUFFER_SIZE, hs->data_length, cache_fd);
        if(ret < 0) {
            printf("Fail to forward data length to clientfd\n");
            ret = -1;
            goto EXIT_NO_CACHE_ROUTINE;
        }
    }
    if(hs->is_persistent) {
        ret = serverfd;
        goto EXIT_NO_CACHE_ROUTINE;
    }
    ret = 0;
EXIT_NO_CACHE_ROUTINE:
    if(ret == -1 && cache_fd > 0) {
        close(cache_fd);
        cache_fd = -1;
        cache_delete_file(tp->req_buffer);
    }
    close(cache_fd);
    pthread_mutex_unlock(&(tp->thread_lock[lock_num]));
    return ret;
}

/* routine for local cache hit */
int cache_routine(int serverfd, struct thread_param* tp, struct header_status* hs, int* cache_fd_ptr) {

    int ret;        // for return code
    int cache_fd = *cache_fd_ptr;
    // validate the cache, read the prepared request(with modify date)
    bzero(tp->res_buffer, HEADER_BUFFER_SIZE);
    lseek(cache_fd, 0, SEEK_SET);

    // forward the prepared request in file to server
    forward_data_chunked(serverfd, cache_fd, -1);


    // get the response header
    ret = get_reqres_header(serverfd, tp->res_buffer, HEADER_BUFFER_SIZE, tp->id);
    if(ret < 0) {
        printf("Fail to get response header from serverfd\n");
        return -1;
    }

    // get the header status
    ret = init_header_status(hs, tp->res_buffer, RESPONSE);

    // if no response code, something wrong
    if(!hs->response_code) {
        printf("Fail to get response code\n");
        return -1;
    }

    // if 304 (not modified)
    // forward the cache response header and cache response data
    if(hs->response_code == 304) {
        lseek(cache_fd, 4096, SEEK_SET);
        // forward from cache to client (response header)
        ret = forward_data_chunked(tp->clientfd, cache_fd, -1);
        if(ret == -1) {
            printf("Fail to forward response header from cache to client\n");
            return -1;
        }
        // forward from cache to client (payload)
        ret = forward_data_chunked(tp->clientfd, cache_fd, -1);
        if(ret == -1) {
            printf("Fail to forward data from cache to client\n");
            return -1;
        }
        log("Cache used\n");
        return 0;
    }
    // if 200
    else if(hs->response_code == 200) {
        // delete the cache file
        close(cache_fd);
        *cache_fd_ptr = -1;
        if(cache_delete_file(tp->req_buffer)) {
            printf("Fail to delete outdated cache\n");
            return -1;
        }
        // add a new cache file
        if((cache_fd = cache_add_file(tp->req_buffer)) == -1) {
            printf("Fail to add new cache file\n");
            close(cache_fd);
            return -1;
        }
        // write new request header and date
        if(cache_add_date(cache_fd, tp->req_buffer)) {
            printf("Fail to write new content to cache\n");
            close(cache_fd);
            return -1;
        }
        lseek(cache_fd, 4096, SEEK_SET);
        // write response header to client
        if(forward_packet(tp->clientfd, tp->res_buffer, strlen(tp->res_buffer), cache_fd) < 0) {
            printf("Fail to forward response header\n");
            close(cache_fd);
            return -1;
        }
        // write response payload to client
        if(hs->is_chunked) {
            ret = forward_data_chunked(tp->clientfd, serverfd, cache_fd);
            if(ret < 0) {
                printf("Fail to forward chunked data to clientfd\n");
                close(cache_fd);
                return -1;
            }
        }
        else if(hs->hv_data) {
            ret = forward_data_length(tp->clientfd, serverfd, tp->res_buffer, HEADER_BUFFER_SIZE, hs->data_length, cache_fd);
            if(ret < 0) {
                printf("Fail to forward data length to clientfd\n");
                close(cache_fd);
                return -1;
            }
        }
        log("Cache update\n");
        close(cache_fd);
        return 0;
    }
    // other than 200 and 304
    else {
        printf("Received not expected response code [%d]\n", hs->response_code);
        return -1;
    }
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

