/*
GET / HTTP/1.1
Host: lyleungad.student.ust.hk
Connection: keep-alive
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3729.131 Safari/537.36
Accept-Encoding: gzip, deflate
Accept-Language: en-US,en;q=0.9,zh-TW;q=0.8,zh;q=0.7
Cookie: _ga=GA1.2.1568566955.1551766752
If-None-Match: "b4-5854ef33cdc12-gzip"
If-Modified-Since: Sat, 30 Mar 2019 12:30:18 GMT
*/


/*

My flow:
- all cache files and folder are under the cache_files

- all manipulation should acquire a lock which base on the hash value of url

-- first file creation situation
1. use the url to create the hash value, say 10
2. if it is the first '10' file, create 10_0, which record the count of file under '10'
3. the real http header and content is in 10_1, 10_0 will be integer 1
4. the first line in 10_1 should be the url say /a.png

-- non-first file creation situation
1. same
2. same, but not create 10_0
3. same, but create file 10_2, 10_0 will be increase by 1
4. the first line is url

-- last file deletion situation
1. create hash
2. 10_0 should have value 1, delete 10_0
3. delete itself

-- non-last file deletion situation
1. create hash
2. check 10_0, if it is the last indexed file, value x in 10_0 same as 10_x, just delete
3. if not, then remove itself, set 10_x as itself then decrement the value in 10_0


*/

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>

/**
 *  This function gets the current time in format of 'If-Modify-Since'
 * 
 *  Return the static char address, need to use sprintf to copy to dest
 */
/* return current time in format for 'If-Modify-Since', but char* is static */
char *asctime(const struct tm *timeptr) {

    static char wday_name[7][3] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static char mon_name[12][3] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static char result[30];
    result[29] = 0;

    sprintf(result, "%.3s, %.2d %.3s %d %.2d:%.2d:%.2d GMT",
        wday_name[timeptr->tm_wday],
        timeptr->tm_mon, mon_name[timeptr->tm_mday],
        1900 + timeptr->tm_year, timeptr->tm_hour,
        timeptr->tm_min, timeptr->tm_sec);

    return result;
}

/**
 *  This function create the files given Major number and hostname
 *  the minor number is auto assigned
 * 
 *  Return +ve(fd) if success, -1 if fail
 */
/* This function create the files given Major number and hostname */
int cache_create_file(char* hostname, int major) {

    int minor = -1;
    int ret;
    int minor_start_position;
    int count_file;
    int major_file;
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

/**
 *  This function delete the files given Major number and Minor number and hostname
 * 
 *  Return 0 if success, -1 if fail
 */
/* This function delete files */
int cache_delete_file(char* hostname, int major, int minor) {

    int ret;
    int minor_start_position;
    int count_file;
    int major_file;
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


int main() {
    
    int ret;
    char* time_string;
    struct tm* time_struct;
    time_t result;
    result = time(NULL);
    
    time_string = asctime(localtime(&result));
    printf("Time :%s\n", time_string);
    printf("Length [%ld]\n", strlen(time_string));

    // ret = cache_create_file("sgss.edu.hk", 10);
    // printf("num_of_minor [%d]\n", ret);
    // close(ret);

    ret = cache_delete_file("sgss.edu.hk", 10, 1);
    printf("Remove result code [%d]\n", ret);

    return 0;
}
