#include <stdio.h>
#include <string.h>
#include "city_manager.h"

int main(int argc, char *argv[]) {
    char role[32] = "";
    char user[64] = "";
    char command[32] = "";
    char district[64] = "";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0) 
            strcpy(role, argv[++i]);
        else if (strcmp(argv[i], "--user") == 0) 
            strcpy(user, argv[++i]);
        else if (strcmp(argv[i], "--add") == 0) { 
            strcpy(command, "add"); strcpy(district, argv[++i]); }
        else if (strcmp(argv[i], "--list") == 0) { 
            strcpy(command, "list"); strcpy(district, argv[++i]); }
        else if (strcmp(argv[i], "--view") == 0) { 
            strcpy(command, "view"); strcpy(district, argv[++i]); }
        else if (strcmp(argv[i], "--remove_report") == 0) { 
            strcpy(command, "remove_report"); strcpy(district, argv[++i]); }
        else if (strcmp(argv[i], "--update_threshold") == 0) { 
            strcpy(command, "update_threshold"); strcpy(district, argv[++i]); }
        else if (strcmp(argv[i], "--filter") == 0) { 
            strcpy(command, "filter"); strcpy(district, argv[++i]); }
    }

    printf("role=%s user=%s command=%s district=%s\n", role, user, command, district);
return 0;
}