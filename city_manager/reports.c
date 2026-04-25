/*
 * reports.c - Implementarea comenzilor: list, view, remove_report,
 *             update_threshold, și gestionarea symbolic link-urilor
 *
 * Toate operațiile pe fișiere folosesc EXCLUSIV system calls Unix:
 *   open(), close(), read(), write(), lseek(), ftruncate()
 *   stat(), fstat(), lstat(), symlink(), unlink()
 *
 * Fișierul reports.dat este un array binar de structuri Report
 * de dimensiune FIXĂ. Asta înseamnă că:
 *   - Înregistrarea i se află la offset i * sizeof(Report) bytes
 *   - Putem sări direct la orice înregistrare cu lseek()
 *   - Nu există separatori, headers sau text - date brute binare
 */

#include <fcntl.h>       /* open(), O_RDONLY, O_RDWR, O_WRONLY etc. */
#include <unistd.h>      /* read(), write(), close(), lseek(), ftruncate(), symlink(), unlink() */
#include <sys/stat.h>    /* stat(), fstat(), lstat(), struct stat */
#include <string.h>      /* strcmp(), strncpy(), snprintf() */
#include <stdio.h>       /* printf(), perror() */
#include <stdlib.h>      /* atoi() */
#include <time.h>        /* localtime(), strftime() */
#include "city_manager.h"

/* ---------------------------------------------------------------
 * district_exists - verifică dacă directorul districtului există
 *
 * Folosim stat() pe calea districtului.
 * S_ISDIR(st.st_mode) verifică dacă intrarea e un director.
 * Returnează 1 dacă există și e director, 0 altfel.
 * --------------------------------------------------------------- */
static int district_exists(const char *district) {
    struct stat st;
    if (stat(district, &st) == -1) return 0;
    return S_ISDIR(st.st_mode);
}

/* ---------------------------------------------------------------
 * print_report (funcție statică - vizibilă doar în acest fișier)
 *
 * Afișează toate câmpurile unui Report la stdout.
 * "static" înseamnă că funcția nu e vizibilă în afara acestui fișier
 * - e o convenție bună pentru funcții ajutătoare interne.
 * --------------------------------------------------------------- */
static void print_report(const Report *r) {
    /* Convertim timestamp Unix (time_t) în string uman-lizibil */
    char ts[64];
    struct tm *tm_info = localtime(&r->timestamp);
    /* strftime formatează conform unui pattern - similar cu printf dar pentru timp */
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("  ID:          %d\n",         r->id);
    printf("  Inspector:   %s\n",         r->inspector);
    printf("  Location:    %.6f, %.6f\n", r->latitude, r->longitude);
    printf("  Category:    %s\n",         r->category);
    printf("  Severity:    %d\n",         r->severity);
    printf("  Timestamp:   %s\n",         ts);
    printf("  Description: %s\n",         r->description);
}

/* ---------------------------------------------------------------
 * create_symlink
 *
 * Creează un symbolic link "active_reports-<district>" care pointează
 * la "<district>/reports.dat".
 *
 * Un symbolic link este un fișier special care conține o cale către
 * alt fișier. Sistemul de operare urmărește automat link-ul când
 * deschizi fișierul cu open() sau stat().
 *
 * Diferența crucială între stat() și lstat():
 *   stat(path, &st)  → URMĂREȘTE symlink-ul → returnează info despre TARGET
 *   lstat(path, &st) → NU urmărește → returnează info despre LINK ÎNSUȘI
 *
 * Dangling link = symlink care pointează la un fișier ce nu mai există.
 *   Detectare: lstat() reușește (link-ul există) dar stat() eșuează (target absent)
 *
 * syscall-uri folosite:
 *   lstat()  - inspectăm link-ul în sine
 *   stat()   - verificăm dacă targetul există (urmărind link-ul)
 *   S_ISLNK  - macro care verifică dacă st_mode indică un symlink
 *   unlink() - șterge fișierul/link-ul (nu fișierul targetat!)
 *   symlink(target, linkname) - creează symlink-ul
 * --------------------------------------------------------------- */
void create_symlink(const char *district) {
    char link_name[256];
    char target[256];

    /* Construim numele link-ului și calea spre target */
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);
    snprintf(target,    sizeof(target),    "%s/reports.dat",    district);

    struct stat lst;
    /*
     * lstat() returnează info despre LINK-UL ÎNSUȘI, nu despre ce pointează.
     * Dacă returnează 0 (succes), înseamnă că există ceva cu acel nume.
     */
    if (lstat(link_name, &lst) == 0) {
        /* Există ceva cu numele link_name. Verificăm dacă e symlink. */
        if (S_ISLNK(lst.st_mode)) {
            /*
             * E un symlink. Acum verificăm dacă targetul există.
             * stat() URMĂREȘTE symlink-ul → dacă target lipsește, eșuează.
             */
            struct stat follow;
            if (stat(link_name, &follow) == -1) {
                /* stat() a eșuat → target nu există → DANGLING LINK! */
                printf("Warning: dangling symlink detected: '%s' -> target missing!\n",
                       link_name);
                /* Ștergem link-ul mort; unlink() șterge intrarea din director */
                unlink(link_name);
                /* Continuăm mai jos să recreăm link-ul */
            } else {
                /* Link valid, targetul există - nu facem nimic */
                return;
            }
        } else {
            /* Există un fișier obișnuit cu același nume - nu îl suprascriem */
            printf("Warning: '%s' exists but is not a symlink.\n", link_name);
            return;
        }
    }
    /* Dacă am ajuns aici: link-ul nu există sau tocmai l-am șters (dangling) */

    /*
     * symlink(target, linkname)
     *   target   = calea la care va pointa link-ul
     *   linkname = numele link-ului nou creat
     * Returnează 0 la succes, -1 la eroare (ex: linkname deja există)
     */
    if (symlink(target, link_name) == -1) {
        perror("symlink failed");
    } else {
        printf("Symlink created: %s -> %s\n", link_name, target);
    }
}

/* ---------------------------------------------------------------
 * list_reports
 *
 * Listează toate rapoartele din <district>/reports.dat.
 *
 * Pași detaliați:
 *
 * 1. Creăm/verificăm symlink-ul cu create_symlink()
 *
 * 2. Verificăm permisiunile cu check_read_permission()
 *    → folosește stat() intern pentru a citi st_mode
 *
 * 3. stat() pentru metadata fișierului:
 *    st.st_mode  → biții de permisiuni
 *    st.st_size  → dimensiunea în bytes
 *    st.st_mtime → data ultimei MODIFICĂRI (modification time)
 *    (există și st_atime = access time, st_ctime = change time)
 *
 * 4. format_permissions() → convertim mode_t în "rw-rw-r--"
 *
 * 5. strftime() → formatăm st_mtime într-un string lizibil
 *
 * 6. lstat() pe symlink → detectăm dacă e dangling
 *    S_ISLNK(mode) → macro POSIX care verifică dacă e symlink
 *
 * 7. open() + read() în buclă → citim câte sizeof(Report) bytes
 *    read() returnează câți bytes a citit efectiv:
 *    - sizeof(Report) → am citit o înregistrare completă → continuăm
 *    - 0              → am ajuns la sfârșitul fișierului → ieșim din buclă
 *    - altceva        → eroare sau fișier corupt
 *
 * 8. log_operation() → scriem în logged_district
 * --------------------------------------------------------------- */
void list_reports(const char *district, const char *role, const char *user) {
    if (!district_exists(district)) {
        printf("Error: district '%s' does not exist.\n", district);
        return;
    }

    /* Pas 1: asigurăm că symlink-ul există și e valid */
    create_symlink(district);

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    /* Pas 2: verificare permisiuni ÎNAINTE de a deschide fișierul */
    if (!check_read_permission(path, role)) {
        printf("Error: role '%s' does not have read permission on %s\n",
               role, path);
        log_operation(district, role, user, "list_DENIED");
        return;
    }

    /* Pas 3: obținem metadata fișierului */
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("stat failed");
        return;
    }

    /* Pas 4 & 5: afișăm informații despre fișier */
    char perms[10];
    format_permissions(st.st_mode, perms);  /* biți → simboluri */

    char timebuf[64];
    struct tm *tm_info = localtime(&st.st_mtime); /* st_mtime = ultima modificare */
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("=== File Info: %s ===\n", path);
    printf("Permissions : %s\n",    perms);
    printf("Size        : %lld bytes\n", (long long)st.st_size);
    printf("Last modified: %s\n",   timebuf);

    /* Pas 6: verificăm statusul symlink-ului cu lstat() */
    char link_name[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);

    struct stat lst;
    if (lstat(link_name, &lst) == 0) {
        /* lstat() a reușit → există ceva cu acest nume */
        if (S_ISLNK(lst.st_mode)) {
            /* E un symlink → verificăm dacă targetul există cu stat() */
            struct stat follow;
            if (stat(link_name, &follow) == -1) {
                printf("Symlink '%s': DANGLING (target missing!)\n", link_name);
            } else {
                printf("Symlink '%s': OK (valid)\n", link_name);
            }
        }
    } else {
        printf("Symlink '%s': not found\n", link_name);
    }
    printf("\n");

    /* Pas 7: deschidem fișierul și citim înregistrările */
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open reports.dat failed");
        return;
    }

    Report r;
    int count = 0;
    ssize_t bytes_read;

    /*
     * Citim în buclă câte sizeof(Report) bytes.
     * read(fd, buf, n) returnează:
     *   n  → am citit n bytes cu succes
     *   0  → EOF (End Of File) - am terminat fișierul
     *   -1 → eroare
     * Comparăm cu (ssize_t)sizeof(Report) pentru a detecta citirile parțiale.
     */
    while ((bytes_read = read(fd, &r, sizeof(Report))) == (ssize_t)sizeof(Report)) {
        printf("--- Report #%d ---\n", r.id);
        print_report(&r);
        printf("\n");
        count++;
    }

    close(fd);

    if (count == 0) {
        printf("No reports found in district '%s'.\n", district);
    } else {
        printf("Total: %d report(s).\n", count);
    }

    /* Pas 8: logăm operația */
    log_operation(district, role, user, "list");
}

/* ---------------------------------------------------------------
 * view_report
 *
 * Afișează un singur raport identificat prin ID.
 *
 * De ce scanare liniară și nu lseek() direct?
 *   ID-ul raportului este time(NULL) la momentul creării - valori de tipul
 *   1745300000. Nu putem calcula poziția din ID.
 *   Dacă ID-urile ar fi 0,1,2,3... (indecși), am putea face:
 *     lseek(fd, target_id * sizeof(Report), SEEK_SET)
 *   Dar cu ID-uri timestamp, trebuie să scanăm secvențial.
 * --------------------------------------------------------------- */
void view_report(const char *district, int target_id,
                 const char *role, const char *user) {
    if (!district_exists(district)) {
        printf("Error: district '%s' does not exist.\n", district);
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    /* Verificare permisiuni cu stat() */
    if (!check_read_permission(path, role)) {
        printf("Error: role '%s' does not have read permission on %s\n",
               role, path);
        log_operation(district, role, user, "view_DENIED");
        return;
    }

    if (target_id == 0) {
        printf("Error: please specify a report ID with --id <id>\n");
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open reports.dat failed");
        return;
    }

    Report r;
    int found = 0;

    /* Citim înregistrare cu înregistrare până găsim ID-ul dorit */
    while (read(fd, &r, sizeof(Report)) == (ssize_t)sizeof(Report)) {
        if (r.id == target_id) {
            printf("=== Report #%d ===\n", r.id);
            print_report(&r);
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found) {
        printf("Error: report with ID %d not found.\n", target_id);
    }

    log_operation(district, role, user, "view");
}

/* ---------------------------------------------------------------
 * remove_report_cmd
 *
 * Șterge o înregistrare din fișierul binar reports.dat.
 * PERMIS DOAR PENTRU MANAGER.
 *
 * Algoritmul de ștergere cu lseek() + ftruncate():
 *
 * Să zicem că avem N rapoarte și vrem să ștergem raportul de la poziția i:
 *
 *   Înainte:  [R0][R1][R2][R3][R4]   (N=5, ștergem R2 de la i=2)
 *
 *   Pas 1: copiem R3 peste R2
 *           [R0][R1][R3][R3][R4]
 *
 *   Pas 2: copiem R4 peste R3
 *           [R0][R1][R3][R4][R4]
 *
 *   Pas 3: ftruncate la (N-1) * sizeof(Report)
 *           [R0][R1][R3][R4]          (N=4)
 *
 * lseek(fd, offset, SEEK_SET):
 *   Mută "cursorul" la byte-ul numărul `offset` față de începutul fișierului.
 *   Returnează noua poziție sau -1 la eroare.
 *   SEEK_SET = calculăm offset față de start (există și SEEK_CUR, SEEK_END)
 *
 * ftruncate(fd, length):
 *   Setează dimensiunea fișierului la exact `length` bytes.
 *   Dacă length < size curent → bytes de la sfârșit sunt eliberați (șterse).
 *   Dacă length > size curent → se adaugă bytes cu valoarea 0 (zero padding).
 * --------------------------------------------------------------- */
void remove_report_cmd(const char *district, int target_id,
                       const char *role, const char *user) {
    if (!district_exists(district)) {
        printf("Error: district '%s' does not exist.\n", district);
        return;
    }

    /* Restricție de rol: doar managerul poate șterge rapoarte */
    if (strcmp(role, "manager") != 0) {
        printf("Error: only 'manager' role can remove reports.\n");
        log_operation(district, role, user, "remove_report_DENIED");
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    /* Verificare permisiuni de scriere cu stat() */
    if (!check_write_permission(path, role)) {
        printf("Error: no write permission on %s\n", path);
        log_operation(district, role, user, "remove_report_DENIED");
        return;
    }

    if (target_id == 0) {
        printf("Error: please specify a report ID with --id <id>\n");
        return;
    }

    /*
     * O_RDWR = deschidem pentru CITIRE și SCRIERE simultan.
     * Avem nevoie de amândouă: citim înregistrările de după
     * și le scriem la poziția anterioară.
     */
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("open reports.dat failed");
        return;
    }

    /*
     * fstat() = stat() dar pe un file descriptor deschis în loc de cale.
     * Avantaj: atomicitate - nu se poate schimba fișierul între stat și open.
     */
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat failed");
        close(fd);
        return;
    }

    /* Calculăm numărul de înregistrări din dimensiunea fișierului */
    int num_reports = (int)(st.st_size / (off_t)sizeof(Report));

    if (num_reports == 0) {
        printf("No reports to remove.\n");
        close(fd);
        return;
    }

    /* ---- Pasul 1: găsim indexul raportului cu target_id ---- */
    int found_idx = -1;
    Report r;

    for (int i = 0; i < num_reports; i++) {
        /*
         * lseek(fd, offset, SEEK_SET)
         * Mutăm cursorul la poziția exactă a înregistrării i în fișier.
         * Offset-ul = i * sizeof(Report) bytes de la începutul fișierului.
         */
        if (lseek(fd, (off_t)(i * sizeof(Report)), SEEK_SET) == -1) {
            perror("lseek failed");
            close(fd);
            return;
        }
        if (read(fd, &r, sizeof(Report)) != (ssize_t)sizeof(Report)) break;

        if (r.id == target_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        printf("Error: report with ID %d not found.\n", target_id);
        close(fd);
        return;
    }

    /* ---- Pasul 2: compactăm fișierul - "tragem" înregistrările în față ---- */
    for (int i = found_idx + 1; i < num_reports; i++) {
        /* Citim înregistrarea de la poziția i */
        if (lseek(fd, (off_t)(i * sizeof(Report)), SEEK_SET) == -1) {
            perror("lseek failed"); close(fd); return;
        }
        if (read(fd, &r, sizeof(Report)) != (ssize_t)sizeof(Report)) break;

        /* O scriem la poziția i-1 (suprascriem "golul" creat de ștergere) */
        if (lseek(fd, (off_t)((i - 1) * sizeof(Report)), SEEK_SET) == -1) {
            perror("lseek failed"); close(fd); return;
        }
        write(fd, &r, sizeof(Report));
    }

    /* ---- Pasul 3: micșorăm fișierul cu o înregistrare ---- */
    off_t new_size = (off_t)((num_reports - 1) * sizeof(Report));
    if (ftruncate(fd, new_size) == -1) {
        perror("ftruncate failed");
        close(fd);
        return;
    }

    close(fd);

    printf("Report #%d removed successfully.\n", target_id);
    printf("Remaining reports: %d\n", num_reports - 1);

    log_operation(district, role, user, "remove_report");
}

/* ---------------------------------------------------------------
 * update_threshold
 *
 * Actualizează valoarea threshold din <district>/district.cfg.
 * PERMIS DOAR PENTRU MANAGER.
 *
 * district.cfg are formatul simplu:
 *   threshold=N\n
 *
 * Strategie: deschidem cu O_TRUNC (golim la 0 bytes), rescriem tot.
 * Alternativa ar fi să citim conținutul, să parsăm linia, să modificăm
 * și să scriem înapoi - mult mai complex pentru un fișier atât de simplu.
 *
 * Verificarea permisiunilor cu stat():
 *   district.cfg are 0640 = rw-r-----
 *   Inspector are drepturi de grup (S_IWGRP = bit 4 din grupul 3-5)
 *   0640: biții grupului sunt 100 (citire=1, scriere=0, execuție=0)
 *   → S_IWGRP & 0640 = 0 → inspector NU poate scrie → CORECT!
 * --------------------------------------------------------------- */
void update_threshold(const char *district, int new_value,
                      const char *role, const char *user) {
    if (!district_exists(district)) {
        printf("Error: district '%s' does not exist.\n", district);
        return;
    }

    /* Restricție de rol */
    if (strcmp(role, "manager") != 0) {
        printf("Error: only 'manager' role can update threshold.\n");
        log_operation(district, role, user, "update_threshold_DENIED");
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/district.cfg", district);

    /* Verificăm că fișierul există și obținem permisiunile cu stat() */
    struct stat st;
    if (stat(path, &st) == -1) {
        printf("Error: district.cfg not found in '%s'. Run create_district first.\n",
               district);
        return;
    }

    /* Verificare permisiuni de scriere cu stat() */
    if (!check_write_permission(path, role)) {
        printf("Error: no write permission on %s\n", path);
        /* Afișăm permisiunile actuale pentru debugging */
        char perms[10];
        format_permissions(st.st_mode, perms);
        printf("  Current permissions: %s\n", perms);
        log_operation(district, role, user, "update_threshold_DENIED");
        return;
    }

    if (new_value <= 0) {
        printf("Error: threshold must be a positive integer.\n");
        return;
    }

    /*
     * O_WRONLY = numai scriere
     * O_TRUNC  = trunchiază fișierul la 0 bytes la deschidere
     * → fișierul existent este golit, scriem conținut complet nou
     */
    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd == -1) {
        perror("open district.cfg failed");
        return;
    }

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "threshold=%d\n", new_value);
    write(fd, buf, len);
    close(fd);

    printf("Threshold updated to %d in '%s'.\n", new_value, district);
    log_operation(district, role, user, "update_threshold");
}
