#include <errno.h>

#include "routines.h"
#include "network_handler.h"
#include "http_header_handler.h"
#include "logger.h"

/* previous routine */
int proxy_routines(int clientfd, char* req_buffer, char* res_buffer, int buf_size, int request_id, char timeout_allow) {

    int ret;                    // store return value
    int req_ready = 1;          // indicate that request buffer is ready for next usage

    struct header_status req_hs;
    struct header_status res_hs;

    char client_timeout = 0;    // indicate the # of time client timeout

    bzero(req_buffer, buf_size);    // to identify http request header attribute
    bzero(res_buffer, buf_size);    // to identify http response header attribute
    bzero(&req_hs, sizeof(struct header_status));
    bzero(&res_hs, sizeof(struct header_status));

    // infinity loop
    while(1) {

        // reached timeout
        if(client_timeout >= timeout_allow) {
            log("Timeout for request[%d]\n", request_id);
            ret = -EAGAIN;
            goto EXIT_PROXY_ROUTINES;
        }
        // sleep to wait for a while
        else if (client_timeout > 0){
            sleep(1);
        }

        // try to get the request header from client
        ret = get_reqres_header(clientfd, req_buffer, buf_size, request_id);
        if(ret == -EAGAIN) {
            // not ready to read request from client, timeout++
            ++client_timeout;
        }
        else if(ret == 0) {
            
            // reset client timeout, if got one request header
            client_timeout = 0;

            // init the request header
            ret = init_header_status(&req_hs, req_buffer, REQUEST);
            if(ret == -1) {
                printf("================== NOT SUPPORTED HEADER ====================\n");
                printf("%s\n", req_buffer);
                goto EXIT_PROXY_ROUTINES;
            }

            switch(req_hs.http_method) {
                case CONNECT:
                    printf("%s", req_buffer);
                    return -EFAULT;
                default:
                    printf("================== NOT SUPPORTED HEADER ====================\n");
                    return -1;
            }

            // correctly format the client request header
            ret = reformat_request_header(req_buffer);
            if(ret < 0) {
                printf("Wrong request header for request[%d]\n", request_id);
                ret = -1;
            }
            
            // clear the request buffer
            bzero(req_buffer, buf_size);
        }
        else {
            printf("Fail to read client request for request[%d]\n", request_id);
            ret = -1;
        }
    }
EXIT_PROXY_ROUTINES:
    return ret;
}

/* routine for https request */
int https_routine(char* req_header) {

}

