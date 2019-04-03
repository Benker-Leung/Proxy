#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdarg.h>

// logger marcos
#define log(...) do {printf(__VA_ARGS__); fflush(stdout); } while(0)


#endif
