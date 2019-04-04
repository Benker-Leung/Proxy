#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "logger.h"

int logger_start = 0;

void init_logger() {

    // if logger started
    if(logger_start)
        return;
    // open log file
    int log_fd = open("log", O_CREAT|O_WRONLY|O_APPEND, 0644);
    
    close(1);
    
    // assign log_fd to 1, if fail, print err msg
    if(dup2(log_fd, 1) == -1) {
        fprintf(stderr, "Cannot dup2 to stdout, errno:[%d]\n", errno);
        return;
    }
    // close non-use
    close(log_fd);
    logger_start = 1;
    return;
}

void print_time() {

    time_t my_time = time(0);

    struct tm* my_tm;
    my_tm = localtime(&my_time);

    if(my_tm != NULL)
        fprintf(stdout, "%.4d/%.2d/%.2d %.2d:%.2d:%.2d : ", my_tm->tm_year+1900, my_tm->tm_mon+1, my_tm->tm_mday, my_tm->tm_hour, my_tm->tm_min, my_tm->tm_sec);
    // if no time available
    else {
        fprintf(stderr, "No time available: ");
    }
    return;
}


