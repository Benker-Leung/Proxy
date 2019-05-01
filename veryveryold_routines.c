int proxy_routine(int clientfd, char* req_buffer, char* res_buffer, int buf_size, int request_id, char timeout_allow) {

    return proxy_routines(clientfd, req_buffer, res_buffer, buf_size, request_id, timeout_allow);

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
                ret = connect_server(req_buffer, 80);
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