/**
 *  This function handle all actions for proxy networking 
 *  one thread used to read, one used to write
 * 
 *  Return -1 if fail or timeout, should close this fd
 * 
 */
int proxy_routine(int fd, char* req_buffer, char* res_buffer, int size, int request_id);


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
            // printf("Header captured, request [%d]:\n", request_id);
            serverfd = connect_server(req_buffer);
            reformat_request_header(req_buffer);
            printf("\n\n================== Request header =========================\n");
            printf("%s\n", req_buffer);
            if(serverfd == -1){
                printf("Cannot connect to host\n");
                clear_buffer(req_buffer, res_buffer, size);
                return -1;
            }
            else {
                int data_len;
                timeout = 0;
                // printf("Connected to server fd:[%d]\n", serverfd);
                forward_packet(serverfd, req_buffer, strlen(req_buffer));
                while((ret = get_reqres_header(serverfd, res_buffer, size, request_id)) != 0) {
                    // printf("Waitng response header");
                }
                // printf("Got response header, request [%d]\n", request_id);
                printf("\n\n================== Response header =========================\n");
                printf("%s\n", res_buffer);
                data_len = get_content_length(res_buffer);
                // printf("Parsed Content-Length: [%d]\n", ret);
                bzero(buf, 10240);
                ret = get_data_by_len(serverfd, buf, data_len);
                if(ret < 0) {
                    printf("Cannot get data\n");
                    clear_buffer(req_buffer, res_buffer, size);
                    return -1;
                }

                // printf("Got data\n");
                printf("\n\n================== Data(%d) =========================\n", data_len);
                
                // int tempfd = open("data", O_CREAT | O_RDWR, 0644);
                // write(tempfd, buf, data_len);
                // close(tempfd);
                printf("%s\n", buf);

                forward_packet(fd, res_buffer, strlen(res_buffer));
                forward_packet(fd, buf, data_len);
                continue;
                clear_buffer(req_buffer, res_buffer, size);
                close(serverfd);
                // exit(0);
            }
            return 0;
        }
        clear_buffer(req_buffer, res_buffer, size);
    }    

    return 0;
}
