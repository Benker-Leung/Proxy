#define _GNU_SOURCE


#include "cache_handler.h"
#include "http_header_handler.h"
#include "logger.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>

static char wday_name[7][3] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static char mon_name[12][3] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

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

/* This function write the if-modify-since to header */
int cache_add_date(int cache_fd, char* req_buffer) {

    // buffer for if modify since
    char date_tag[100];
    int ret;
    struct tm* timeptr;
    time_t result;
    result = time(NULL);
    timeptr = localtime(&result);

    // if request already written inside file
    if(req_buffer == NULL) {
        lseek(cache_fd, -2, SEEK_CUR);
    }
    // if request is in req_buffer not in file
    else {
        // write the request
        ret = write(cache_fd, req_buffer, sizeof(req_buffer));
        if(ret <= 0) {
            return -1;
        }
        lseek(cache_fd, -2, SEEK_CUR);
    }

    bzero(date_tag, 100);
    sprintf(date_tag, "%s %.3s, %.2d %.3s %d %.2d:%.2d:%.2d HKT\r\n\r\n", "If-Modified-Since:", wday_name[timeptr->tm_wday],
            timeptr->tm_mday, mon_name[timeptr->tm_mon], timeptr->tm_year+1900, timeptr->tm_hour, 
            timeptr->tm_min, timeptr->tm_sec);
        
    ret = write(cache_fd, date_tag, 100);
    if(ret <= 0) {
        return -1;
    }
    return 0;
}


/* This function get max minor */
int cache_get_max_minor(char* hostname, int major) {

    int count_file;
    int num_of_minor;
    char path[512];

    bzero(path, 512);

    if(getcwd(path, 512) == NULL) {
        printf("Fail to get current directory\n");
        return -1;
    }
    
    sprintf(path, "%s/%s/%d_0", path, hostname, major);

    // get the minor number, if fail to open
    if((count_file = open(path, O_RDWR, 0644)) <= 0) {
        printf("Fail to open major_0\n");
        return -1;
    }
    else {
        // if can't read
        if(read(count_file, path, 512) <= 0) {
            printf("Fail to read minor number\n");
            close(count_file);
            return -1;
        }
        // read
        else {
            num_of_minor = atoi(path);
        }
        close(count_file);
    }
    return num_of_minor;
}


/* This function get minor by req_buffer */
int cache_get_minor(char* req_buffer) {

    char c;
    int i;
    int j;
    int cache_fd;   // fd for cache file
    int major;      // record the hash of req_buffer
    int num_of_minor;   // record the max num of minor
    int minor_start_position;
    char* host;     // record start of host
    char* host_end;      // record end of host
    char* uri;
    char* uri_end;
    char path[512];
    char file_content[1024];

    bzero(path, 512);

    // get the hash value
    major = cache_uri_hash(req_buffer);

    // for prepare path
    if(getcwd(path, 512) == NULL) {
        printf("Fail to get current directory\n");
        return -1;
    }

    // get host and end
    if(get_host_end(req_buffer, &host, &host_end)) {
        printf("Fail to get host\n");
        return -1;
    }
    // get uri and end
    if(get_uri_end(req_buffer, &uri, &uri_end)) {
        printf("Fail to get uri\n");
        return -1;
    }
    // get num of minor
    if((num_of_minor = cache_get_max_minor(host, major)) == -1) {
        *host_end = '\r';
        *uri_end = ' ';
        printf("Fail to get max minor\n");
        return -1;
    }
    minor_start_position = sprintf(path, "%s/%s/%d_", path, host, major);
    *host_end = '\r';
    // *uri_end = ' ';

    // loop all files
    for(i=1; i<=num_of_minor; ++i) {
        // prepare full path
        sprintf(path+minor_start_position, "%d", i);

        // open and check
        if((cache_fd = open(path, O_RDWR, 0644)) <= 0) {
            continue;
        }
        bzero(file_content, 1024);
        // open OK,
        for(j=0; ;++j) {
            // fail to read
            if(read(cache_fd, &c, 1) <= 0) {
                file_content[0] = '\0';
                close(cache_fd);
                break;
            }
            // got first line "GET ..."
            if(c == '\r') {
                close(cache_fd);
                break;
            }
            else {
                file_content[j] = c;
            }
        }
        if(strcasestr(file_content, uri) != NULL) {
            *uri_end = ' ';
            return i;
        }
    }
    *uri_end = ' ';
    // means not found
    return 0;
}


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
            // last
            if(num_of_minor == 0) {
                close(count_file);
                if(unlink(path)) {
                    printf("Fail to remove major_0\n");
                    return -1;
                }
                return 0;
            }
            // non-last
            else {
                sprintf(file_content, "%d", num_of_minor);
                lseek(count_file, 0, SEEK_SET);
                write(count_file, file_content, 1024);
                close(count_file);
            }
        }
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
    unsigned int hash = 0;
    char* host;
    char* host_end;
    char* uri;
    char* uri_end;
    if(get_host_end(req_buffer, &host, &host_end)) {
        printf("Fail to get host\n");
        return -1;
    }
    if(get_uri_end(req_buffer, &uri, &uri_end)) {
        printf("Fail to get uri\n");
        *host_end = '\r';
        return -1;
    }

    while(uri != uri_end) {
        hash += *uri << i;
        ++i;
        ++uri;
    }
    *uri_end = ' ';
    while(host != host_end) {
        hash += *host;
        ++i;
        ++host;
    }
    *host_end = '\r';
    
    hash = hash % 101;

    return hash;

}


/* This function create a file given req_buffer */
int cache_add_file(char* req_buffer) {

    int cache_fd;   // fd for cache file
    int major;      // record the hash of req_buffer
    // char* temp;    // record movement of req_buffer
    char* host;     // record start of host
    char* end;      // record end of host

    // get hash by req_buffer
    major = cache_uri_hash(req_buffer);

    // get host and end
    if(get_host_end(req_buffer, &host, &end)) {
        printf("Fail to get host\n");
        return -1;
    }
    // get cache file fd by host and major(hash)
    cache_fd = cache_create_file_by_hostname(host, major);
    *end = '\r';
    // if fail to get fd
    if(cache_fd == -1) {
        printf("Fail to add cache file\n");
        return -1;
    }

    // // write GET and HOST to file
    // temp = req_buffer;
    // while(*temp != '\r') {
    //     if(*temp == '\0') {
    //         return -1;
    //     }
    //     write(cache_fd, temp, 1);
    //     ++temp;
    // }
    // write(cache_fd, "\r\n", 2);
    // while(host != end) {
    //     write(cache_fd, host, 1);
    //     ++host;
    // }    
    // write(cache_fd, "\r\n", 2);

    return cache_fd;
}


/* This function delete file */
int cache_delete_file(char* req_buffer) {

    int minor;
    int major;
    char* host;
    char* host_end;

    minor = cache_get_minor(req_buffer);
    if(minor <= 0) {
        printf("Fail to delete file\n");
        return -1;
    }

    major = cache_uri_hash(req_buffer);

    if(get_host_end(req_buffer, &host, &host_end)) {
        printf("Fail to delete file\n");
        return -1;
    }

    minor = cache_delete_file_by_hostname(host, major, minor);
    *host_end = '\r';
    return 0;

}


/* This function get file_fd by req_buffer */
int cache_get_file_fd(char* req_buffer) {

    int file_fd;
    int major;
    int minor;
    char* host;
    char* host_end;
    char path[512];

    major = cache_uri_hash(req_buffer);
    minor = cache_get_minor(req_buffer);
    if(minor <= 0) {
        printf("No such cache file\n");
        return -1;
    }

    if(get_host_end(req_buffer, &host, &host_end)) {
        return -1;
    }

    bzero(path, 512);

    if(getcwd(path, 512) == NULL) {
        printf("Fail to get current directory\n");
        return -1;
    }

    sprintf(path, "%s/%s/%d_%d", path, host, major, minor);
    *host_end = '\r';

    file_fd = open(path, O_RDWR, 0644);
    if(file_fd <= 0) {
        printf("No such cache file\n");
        return -1;        
    }
    return file_fd;
}
