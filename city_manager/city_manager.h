#ifndef CITY_MANAGER_H
#define CITY_MANAGER_H

#include <time.h>

typedef struct {
    int id;
    char inspector[64];
    double latitude;
    double longitude;
    char category[32];
    int severity;
    time_t timestamp;
   char description[256];
} Report;

#endif