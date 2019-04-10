/*
GET http://lyleungad.student.ust.hk/ HTTP/1.1
Host: lyleungad.student.ust.hk 
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:66.0) Gecko/20100101 Firefox/66.0
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Connection: keep-alive
Upgrade-Insecure-Requests: 1
Cache-Control: max-age=0
*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


#include "network_handler.h"

char* req_buf = "GET http://lyleungad.student.ust.hk/ HTTP/1.1\r\nHost: lyleungad.student.ust.hk\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:66.0) Gecko/20100101 Firefox/66.0\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nConnection: keep-alive\r\nUpgrade-Insecure-Requests: 1\r\nCache-Control: max-age=0\r\nContent-Length: 1234\r\n";



void test_get_serverfd() {

	char* ip_buf1 = "143.89.190.134";	// should work

	char* ip_buf2 = "175.159.124.60";	// should fail

	int ret;

	ret = get_serverfd(ip_buf1);
	if(ret < 0) {
		printf("Fail get_serverfd([%s]) with correct input\n", ip_buf1);
	}
	close(ret);
	ret = get_serverfd(ip_buf2);
	if(ret != -1) {
		printf("Fail get_serverfd([%s]) with incorrect input\n", ip_buf2);
	}
    printf("Finished get_serverfd() test\n\n");
	return;
}

void test_get_listen_fd() {

	int port1 = 33344;
	int port2 = 1111111;
	int maxListen1 = 5;
	int maxListen2 = 0;

	int ret;
	ret = get_listen_fd(port1, maxListen1);
	if(ret <= 0) {
		printf("Fail get_listen_fd([%d], [%d]) with correct input\n", port1, maxListen1);
	}
    close(ret);
	ret = get_listen_fd(port2, maxListen1);
	if(ret > 0) {
		printf("Fail get_listen_fd([%d], [%d]) with incorrect input\n", port2, maxListen1);
	}
	ret = get_listen_fd(port1, maxListen2);
	if(ret > 0) {
		printf("Fail get_listen_fd([%d], [%d]) with incorrect input\n", port1, maxListen2);
	}
    printf("Finished get_listen_fd() test\n\n");
	return;
}

void test_get_ip_by_host() {
	char* host1 = "www.sgss.edu.hk";		    // should work
	char* host2 = "lyleungad.student.ust.hk";	// should work
	char* host3 = "benkerleung.com";		    // should fail

	int ret;
	char ip_buf[16];

	ret = get_ip_by_host(host1, ip_buf);
	if(ret != 1) {
		printf("Fail get_ip_by_host([%s], char[16]) with correct input\n", host1);
	}
	ret = get_ip_by_host(host2, ip_buf);
	if(ret != 1) {
		printf("Fail get_ip_by_host([%s], char[16]) with correct input\n", host2);
	}
	ret = get_ip_by_host(host1, ip_buf);
	if(ret != -1) {
		printf("Fail get_ip_by_host([%s], char[16]) with incorrect input\n", host3);
	}
    printf("Finished get_ip_by_host() test\n\n");
	return;
}

void test_get_reqres_header() {
    
    char buf[16384];
    int size = 16384;
    int ret;
    
    int listenfd;

    pid_t pid = fork();

    if(pid == 0) {
        execlp("test.sh", "test.sh", NULL);
    }

    listenfd = get_listen_fd(12345, 5);
    if( listenfd <= 0) {
        printf("Dependency of get_reqres_header => get_listen_fd fail\n");
        return;
    }

    // loop to get file
    while((ret = get_reqres_header(listenfd, buf, size, 1)) != 0) {
        if(ret != -11) {
            printf("Fail get_reqres_header([%d], buf[16384], [%d], 1)\n", listenfd, size);
            close(listenfd);
            return;
        }
    }
    close(listenfd);
    wait(0);
    printf("Finished get_reqres_header() test\n\n");
    return;
}




int main() {

    test_get_serverfd();
    test_get_listen_fd();
    test_get_ip_by_host();
    test_get_reqres_header();
    test_get_reqres_header();

    return 0;
}
