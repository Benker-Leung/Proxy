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
 * 
 *  Return 0 if success, -1 if fail
 */
/* This function create the files given Major number and hostname */
int cache_create_file(char* hostname, int major) {

    // char path[512];     // for store absolute path

    // if(getcwd(path, 512) == NULL) {
    //     return -1;
    // }
    // sprintf(path, "%s/cache_files/%s", path, hostname); // get the final abs path

    // if mkdir fail
    if(mkdir(hostname, 0777)) {
        if(errno != EEXIST) {
            return -1;
        }
    }

    

    return 0;
}

/**
 *  This function delete the files given Major number and Minor number and hostname
 * 
 *  Return 0 if success, -1 if fail
 */
/* This function delete files */
int cache_delete_file(char* hostname, int major, int minor) {

}


int main() {
    
    char* time_string;
    struct tm* time_struct;
    time_t result;
    result = time(NULL);
    
    time_string = asctime(localtime(&result));

    printf("Time :%s\n", time_string);
    printf("Length [%d]\n", strlen(time_string));

    printf("result code[%d]\n", cache_create_file("sgss.edu.hk", 10));

    return 0;
}




