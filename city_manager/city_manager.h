#ifndef CITY_MANAGER_H
#define CITY_MANAGER_H

#include <time.h>
#include <sys/stat.h>

/* ---------------------------------------------------------------
 * Structura Report - o înregistrare binară fixă în reports.dat
 * Dimensiunea fixă permite lseek() precis: offset = i * sizeof(Report)
 * --------------------------------------------------------------- */
typedef struct {
    int    id;              /* ID unic bazat pe time(NULL) */
    char   inspector[64];  /* numele inspectorului */
    double latitude;
    double longitude;
    char   category[32];   /* road / lighting / flooding */
    int    severity;       /* 1=minor, 2=moderate, 3=critical */
    time_t timestamp;
    char   description[256];
} Report;

/* ---------------------------------------------------------------
 * Declarații funcții din permissions.c
 * --------------------------------------------------------------- */

/*
 * format_permissions - converteste mode_t în string simbolic de 9 caractere
 * Exemplu: 0664 → "rw-rw-r--"
 * buf trebuie să aibă cel puțin 10 bytes (9 caractere + '\0')
 */
void format_permissions(mode_t mode, char *buf);

/*
 * log_operation - scrie o linie de log în <district>/logged_district
 * Format: [timestamp] role=... user=... op=...
 */
void log_operation(const char *district, const char *role,
                   const char *user, const char *operation);

/*
 * check_read_permission - verifică bitul de citire cu stat()
 * manager → verifică S_IRUSR (bitul owner)
 * inspector → verifică S_IRGRP (bitul group)
 * Returnează 1 dacă are permisiune, 0 dacă nu
 */
int check_read_permission(const char *path, const char *role);

/*
 * check_write_permission - verifică bitul de scriere cu stat()
 * manager → verifică S_IWUSR
 * inspector → verifică S_IWGRP
 * Returnează 1 dacă are permisiune, 0 dacă nu
 */
int check_write_permission(const char *path, const char *role);

/* ---------------------------------------------------------------
 * Declarații funcții din reports.c
 * --------------------------------------------------------------- */

/*
 * create_symlink - creează symlink-ul "active_reports-<district>" → "<district>/reports.dat"
 * Folosește lstat() (nu stat()) pentru a inspecta link-ul în sine
 * Detectează și elimină dangling links
 */
void create_symlink(const char *district);

/*
 * list_reports - listează toate rapoartele + info fișier (permisiuni, size, data)
 * Folosește stat() pentru metadata, read() în buclă pentru înregistrări
 */
void list_reports(const char *district, const char *role, const char *user);

/*
 * view_report - afișează un singur raport după ID (scanare liniară cu read())
 */
void view_report(const char *district, int target_id,
                 const char *role, const char *user);

/*
 * remove_report_cmd - șterge o înregistrare cu lseek() + ftruncate()
 * Permis doar pentru manager
 */
void remove_report_cmd(const char *district, int target_id,
                       const char *role, const char *user);

/*
 * update_threshold - rescrie district.cfg cu noul threshold
 * Permis doar pentru manager
 */
void update_threshold(const char *district, int new_value,
                      const char *role, const char *user);

/* ---------------------------------------------------------------
 * Declarații funcții din filter.c
 * --------------------------------------------------------------- */

/*
 * filter_reports - filtrează rapoartele după o condiție (ex: "severity>=2")
 */
void filter_reports(const char *district, const char *condition,
                    const char *role, const char *user);

/* ---------------------------------------------------------------
 * Declarații funcții din city_manager.c
 * --------------------------------------------------------------- */
void create_district(const char *district);
void add_report(const char *district, const char *user, const char *role);

#endif /* CITY_MANAGER_H */
