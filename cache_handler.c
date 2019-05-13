#define _GNU_SOURCE


#include "cache_handler.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>


// /* return current time in format for 'If-Modify-Since', but char* is static */
// char *cache_get_time(const struct tm *timeptr) {

//     static char wday_name[7][3] = {
//         "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
//     };
//     static char mon_name[12][3] = {
//         "Jan", "Feb", "Mar", "Apr", "May", "Jun",
//         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
//     };
//     static char result[30];
//     result[29] = 0;

//     sprintf(result, "%.3s, %.2d %.3s %d %.2d:%.2d:%.2d GMT",
//         wday_name[timeptr->tm_wday],
//         timeptr->tm_mon, mon_name[timeptr->tm_mday],
//         1900 + timeptr->tm_year, timeptr->tm_hour,
//         timeptr->tm_min, timeptr->tm_sec);

//     return result;
// }


/* This function create file return fd */
int cache_create_file_by_hostname(char* hostname, int major) {

    int ret;
    int minor_start_position;
    int count_file;
    int num_of_minor;
    char file_content[1024];
    char path[512];

    if(getcwd(path, 512) == NULL) {
        printf("Fail to get current directory\n");
        return -1;
    }

    minor_start_position = sprintf(path, "%s/%s/%d_", path, hostname, major);

    // if mkdir fail
    if(mkdir(hostname, 0777)) {
        if(errno != EEXIST) {
            return -1;
        }
    }

    // clear file content
    bzero(file_content, 1024);

    // set path to major_0
    sprintf(path+minor_start_position, "%d", 0);

    // if major_0 not exist
    if((count_file = open(path, O_RDWR, 0644)) == -1) {
        // if fail to open
        if((count_file = open(path, O_CREAT|O_RDWR, 0644)) == -1) {
            printf("Fail to open or create the major file\n");
            return -1;
        }
        // open OK, write 1 to major_0
        else {
            num_of_minor = 1;
            sprintf(file_content, "%d", num_of_minor);
            write(count_file, file_content, 1024);
            close(count_file);
        }
    }
    // if major_0 exist and successfully opened
    else {
        // fail to read
        if(read(count_file, file_content, 1024) <= 0) {
            close(count_file);
            printf("Fail to read minor value\n");
            return -1;
        }
        // non first value
        else {
            if((lseek(count_file, 0, SEEK_SET)) == -1) {
                printf("Fail to lseek\n");
                return -1;
            }
            num_of_minor = atoi(file_content);
            ++num_of_minor;
            sprintf(file_content, "%d", num_of_minor);
            write(count_file, file_content, 1024);
            close(count_file);
        }
    }

    sprintf(path+minor_start_position, "%d", num_of_minor);

    // if fail to open
    if((ret = open(path, O_CREAT|O_RDWR, 0644)) <= 0) {
        return -1;
    }

    return ret;
}


/* This function delete files */
int cache_delete_file_by_hostname(char* hostname, int major, int minor) {

    int minor_start_position;
    int count_file;
    int num_of_minor;
    char file_content[1024];
    char path[512];

    if(minor <= 0 || major < 0) {
        return -1;
    }

    bzero(file_content, 1024);

    if(getcwd(path, 512) == NULL) {
        printf("Fail to get current directory\n");
        return -1;
    }

    minor_start_position = sprintf(path, "%s/%s/%d_", path, hostname, major);

    // remove the file
    sprintf(path+minor_start_position, "%d", minor);
    if(unlink(path)) {
        printf("Fail to remove the file\n");
        return -1;
    }


    sprintf(path+minor_start_position, "%d", 0);
    // get the minor number, if fail to open
    if((count_file = open(path, O_RDWR, 0644)) <= 0) {
        printf("Fail to open major_0\n");
        return -1;
    }
    // open OK
    else {
        // if can't read
        if(read(count_file, file_content, 1024) <= 0) {
            printf("Fail to read minor number\n");
            close(count_file);
            return -1;
        }
        // read + write
        else {
            num_of_minor = atoi(file_content);
            --num_of_minor;
            sprintf(file_content, "%d", num_of_minor);
            lseek(count_file, 0, SEEK_SET);
            write(count_file, file_content, 1024);
        }
        close(count_file);
    }

    // num_of_minor is original-1

    // if minor is not the last one
    if(num_of_minor + 1 != minor) {
        // path is new path
        sprintf(path+minor_start_position, "%d", minor);
        // file_content is old path
        sprintf(file_content, "%s", path);
        sprintf(file_content+minor_start_position, "%d", num_of_minor+1);
        if(rename(file_content, path)) {
            printf("Fail to move file\n");
            return -1;
        }
    }
    return 0;
}



/* This function get hash value (x%101) */
int cache_uri_hash(char* req_buffer) {

    int i = 1;
    int hash = 0;
    char* uri = req_buffer + 4;

    while(*uri != '\r' && *uri != '\n' && *uri != '\0' && *uri != ' ') {
        hash += *uri << i;
        ++i;
        ++uri;
    }


    if(!(uri = strcasestr(req_buffer, "host:"))) {
        return -1;
    }
    while(*uri != '\r' && *uri != '\n' && *uri != '\0' && *uri != ' ') {
        hash += *uri;
        ++uri;
    }

    hash = hash % 101;
    if(hash < 0) {
        hash *= -1;
        hash = hash % 101;
    }

    return hash;

}


/* This function create a file given req_buffer */
int cache_add_file(char* req_buffer) {

    int cache_fd;   // fd for cache file
    int major;      // record the hash of req_buffer
    char* temp;    // record movement of req_buffer
    char* host;     // record start of host
    char* end;      // record end of host

    // get hash by req_buffer
    major = cache_uri_hash(req_buffer);
    
    // get starting point of host
    host = strcasestr(req_buffer, "host:");
    if(host == NULL) {
        printf("Fail to add cache file\n");
        return -1;
    }
    host += 6;
    end = host;
    while(*end != '\r') {
        if(*end == '\0') {
            printf("Fail to add cache file\n");
            return -1;
        }
        ++end;
    }
    *end = '\0';

    // get cache file fd by host and major(hash)
    cache_fd = cache_create_file_by_hostname(host, major);
    *end = '\r';
    // if fail to get fd
    if(cache_fd == -1) {
        printf("Fail to add cache file\n");
        return -1;
    }

    // write GET and HOST to file
    temp = req_buffer;
    while(*temp != '\r') {
        if(*temp == '\0') {
            return -1;
        }
        write(cache_fd, temp, 1);
        ++temp;
    }
    write(cache_fd, "\r\n", 2);
    while(host != end) {
        write(cache_fd, host, 1);
        ++host;
    }    
    write(cache_fd, "\r\n", 2);

    return cache_fd;
}


/* This function delete file */
int cache_delete_file(char* req_buffer) {

    return -1;

}

