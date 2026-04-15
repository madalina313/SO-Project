#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include "city_manager.h"

void create_district(const char *district) {
    // creaza folderul districtului
        if (mkdir(district, 0750) == -1) {
            perror("mkdir failed");
        return;
    }

    // creaza reports.dat
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    int fd = open(path, O_CREAT | O_WRONLY, 0664);
        if (fd == -1) { perror("open reports.dat failed"); return; }
    close(fd);
    chmod(path, 0664);

    // creaza district.cfg
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    fd = open(path, O_CREAT | O_WRONLY, 0640);
        if (fd == -1) { perror("open district.cfg failed"); return; }
    // scrie threshold default
    write(fd, "threshold=1\n", 12);
    close(fd);
    chmod(path, 0640);

    // creaza logged_district
    snprintf(path, sizeof(path), "%s/logged_district", district);
    fd = open(path, O_CREAT | O_WRONLY, 0644);
        if (fd == -1) { perror("open logged_district failed"); return; }
    close(fd);
    chmod(path, 0644);

    printf("District '%s' created successfully.\n", district);
}

int main(int argc, char *argv[]) {
    char role[32] = "";
    char user[64] = "";
    char command[32] = "";
    char district[64] = "";

    for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--role") == 0) strcpy(role, argv[++i]);
    else if (strcmp(argv[i], "--user") == 0) strcpy(user, argv[++i]);
    else if (strcmp(argv[i], "--add") == 0) { strcpy(command, "add"); strcpy(district, argv[++i]); }
    else if (strcmp(argv[i], "--list") == 0) { strcpy(command, "list"); strcpy(district, argv[++i]); }
    else if (strcmp(argv[i], "--view") == 0) { strcpy(command, "view"); strcpy(district, argv[++i]); }
    else if (strcmp(argv[i], "--remove_report") == 0) { strcpy(command, "remove_report"); strcpy(district, argv[++i]); }
    else if (strcmp(argv[i], "--update_threshold") == 0) { strcpy(command, "update_threshold"); strcpy(district, argv[++i]); }
    else if (strcmp(argv[i], "--filter") == 0) { strcpy(command, "filter"); strcpy(district, argv[++i]); }
    }

    if (strcmp(command, "add") == 0) {
        create_district(district);
    } else {
         printf("role=%s user=%s command=%s district=%s\n", role, user, command, district);
    }

    return 0;
}