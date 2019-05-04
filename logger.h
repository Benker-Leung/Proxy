#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdio.h>
extern int logger_start;

/* open file and change stdout to fd */
void init_logger();

/* print the current time */
void print_time();

/* print time on stderr */
void print_time_stderr();

// logger marcos
#define log(...)\
        do {if(!logger_start) init_logger();\
            print_time();\
            fprintf(stdout, __VA_ARGS__);\
            fflush(stdout); } while(0)

#define printf(...)\
        do{\
            if(logger_start) fprintf(stderr, __VA_ARGS__);\
            else fprintf(stdout, __VA_ARGS__);\
            } while(0)

#define printf_with_time(...)\
        do {if(!logger_start) init_logger();\
            print_time_stderr();\
            fprintf(stderr, __VA_ARGS__); } while(0)


/* !!!!! seems cannot fflush stdout, the result may be unexpected !!! */
/*
#define printf(...)\
        do{\
            if(logger_start) fprintf(stderr, __VA_ARGS__);\
            else fprintf(stdout, __VA_ARGS__);\
            fflush(stdout);\
        } while(0)
*/



#endif
