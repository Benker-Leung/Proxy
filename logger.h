#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdio.h>
extern int logger_start;

/* open file and change stdout to fd */
void init_logger();

/* print the current time */
void print_time();

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
