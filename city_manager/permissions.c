/*
 * permissions.c - Funcții ajutătoare pentru verificarea permisiunilor
 *                 și logarea operațiilor
 *
 * Toate verificările de permisiuni se fac cu stat() - citim st_mode
 * și facem AND cu măștile de biți standard POSIX.
 *
 * Logica rolurilor (simulată la nivel de aplicație):
 *   "manager"   = tratat ca proprietar (owner) al fișierului → biții user
 *   "inspector" = tratat ca membru al grupului              → biții group
 *
 * Permisiunile relevante din proiect:
 *   reports.dat    → 0664 (rw-rw-r--)  inspector POATE citi și scrie
 *   district.cfg   → 0640 (rw-r-----)  inspector poate doar citi, NU scrie
 *   logged_district→ 0644 (rw-r--r--)  inspector NU poate scrie
 */

#include <fcntl.h>       /* open(), O_WRONLY, O_APPEND, O_CREAT */
#include <unistd.h>      /* write(), close() */
#include <sys/stat.h>    /* stat(), struct stat, S_IRUSR etc. */
#include <string.h>      /* strcmp(), strlen() */
#include <time.h>        /* time(), ctime() */
#include <stdio.h>       /* snprintf() */
#include "city_manager.h"

/* ---------------------------------------------------------------
 * format_permissions
 *
 * Converteste biții de permisiuni dintr-un mode_t în 9 caractere simbolice.
 *
 * mode_t este un întreg pe mai mulți biți. Fiecare bit are o semnificație:
 *
 *   Biți 8-6 (owner/user):
 *     S_IRUSR = 0400 → bitul 8: owner poate citi
 *     S_IWUSR = 0200 → bitul 7: owner poate scrie
 *     S_IXUSR = 0100 → bitul 6: owner poate executa
 *
 *   Biți 5-3 (group):
 *     S_IRGRP = 0040 → bitul 5: grup poate citi
 *     S_IWGRP = 0020 → bitul 4: grup poate scrie
 *     S_IXGRP = 0010 → bitul 3: grup poate executa
 *
 *   Biți 2-0 (others):
 *     S_IROTH = 0004 → bitul 2: alții pot citi
 *     S_IWOTH = 0002 → bitul 1: alții pot scrie
 *     S_IXOTH = 0001 → bitul 0: alții pot executa
 *
 * Operatorul & (AND pe biți):
 *   (mode & S_IRUSR) este non-zero DOAR dacă bitul S_IRUSR este setat
 *   Exemplu: mode=0664=110110100b
 *            S_IRUSR   =100000000b
 *            AND        =100000000b → non-zero → 'r'
 *
 * buf: trebuie să aibă cel puțin 10 bytes
 * --------------------------------------------------------------- */
void format_permissions(mode_t mode, char *buf) {
    /* Poziția 0-2: permisiunile owner-ului */
    buf[0] = (mode & S_IRUSR) ? 'r' : '-';
    buf[1] = (mode & S_IWUSR) ? 'w' : '-';
    buf[2] = (mode & S_IXUSR) ? 'x' : '-';

    /* Poziția 3-5: permisiunile grupului */
    buf[3] = (mode & S_IRGRP) ? 'r' : '-';
    buf[4] = (mode & S_IWGRP) ? 'w' : '-';
    buf[5] = (mode & S_IXGRP) ? 'x' : '-';

    /* Poziția 6-8: permisiunile celorlalți (others) */
    buf[6] = (mode & S_IROTH) ? 'r' : '-';
    buf[7] = (mode & S_IWOTH) ? 'w' : '-';
    buf[8] = (mode & S_IXOTH) ? 'x' : '-';

    /* Terminatorul de string */
    buf[9] = '\0';
}

/* ---------------------------------------------------------------
 * log_operation
 *
 * Scrie o linie de log în fișierul <district>/logged_district.
 *
 * De ce O_APPEND?
 *   Cu O_APPEND, fiecare write() se face ATOMIC la sfârșitul fișierului.
 *   Sistemul de operare garantează că două scrieri simultane nu se suprapun.
 *   Fără O_APPEND, ar trebui să facem lseek() manual + write(), care
 *   nu e atomic (risc de date corupte în scenarii concurrent).
 *
 * time(NULL) → returnează secunde Unix de la 1 Ian 1970 (epoch)
 * ctime()    → converteste time_t în string "Www Mmm DD HH:MM:SS YYYY\n"
 *              (include \n la sfârșit → îl eliminăm)
 * --------------------------------------------------------------- */
void log_operation(const char *district, const char *role,
                   const char *user, const char *operation) {
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);

    /*
     * O_WRONLY  = deschidem doar pentru scriere
     * O_APPEND  = toate write() se duc la sfârșitul fișierului
     * O_CREAT   = creăm fișierul dacă nu există
     * 0644      = permisiuni la creare: rw-r--r--
     */
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        /* Dacă log-ul eșuează, nu oprim programul - continuăm */
        return;
    }

    /* Obținem timestamp-ul curent */
    time_t now = time(NULL);
    char *ts = ctime(&now);  /* ex: "Tue Apr 22 10:30:00 2026\n" */
    if (ts) {
        /* Eliminăm '\n' de la sfârșit (ctime îl adaugă automat) */
        ts[strlen(ts) - 1] = '\0';
    }

    /* Construim linia de log */
    char entry[512];
    int len = snprintf(entry, sizeof(entry),
                       "[%s] role=%-10s user=%-20s op=%s\n",
                       ts ? ts : "unknown-time",
                       role, user, operation);

    /* write() - system call direct, fără buffering stdio */
    write(fd, entry, len);
    close(fd);
}

/* ---------------------------------------------------------------
 * check_read_permission
 *
 * Verifică dacă rolul dat are drept de CITIRE pe fișierul specificat.
 *
 * Pasul 1: stat(path, &st) - umple structura stat cu metadatele fișierului
 *   st.st_mode conține tipul fișierului + permisiunile în biți
 *
 * Pasul 2: verificăm bitul relevant din st.st_mode cu AND pe biți
 *   manager   → S_IRUSR (0400) = bitul de read al proprietarului
 *   inspector → S_IRGRP (0040) = bitul de read al grupului
 *
 * Returnează: 1 (are permisiune) sau 0 (nu are)
 * --------------------------------------------------------------- */
int check_read_permission(const char *path, const char *role) {
    struct stat st;

    /* stat() urmărește symlink-urile până la fișierul real */
    if (stat(path, &st) == -1) {
        /* Fișierul nu există sau nu e accesibil */
        return 0;
    }

    if (strcmp(role, "manager") == 0) {
        /* Managerul = proprietarul fișierului → verificăm S_IRUSR */
        return (st.st_mode & S_IRUSR) ? 1 : 0;
    } else {
        /* Inspector și orice alt rol → drepturi de grup → S_IRGRP */
        return (st.st_mode & S_IRGRP) ? 1 : 0;
    }
}

/* ---------------------------------------------------------------
 * check_write_permission
 *
 * Identic cu check_read_permission, dar verifică biții de SCRIERE:
 *   manager   → S_IWUSR (0200)
 *   inspector → S_IWGRP (0020)
 *
 * Exemplu practic cu permisiunile din proiect:
 *   district.cfg are 0640 = rw-r-----
 *   Bitul S_IWGRP (grup write) este 0 → inspector NU poate scrie ✓
 *   Bitul S_IWUSR (owner write) este 1 → manager POATE scrie ✓
 * --------------------------------------------------------------- */
int check_write_permission(const char *path, const char *role) {
    struct stat st;

    if (stat(path, &st) == -1) {
        return 0;
    }

    if (strcmp(role, "manager") == 0) {
        return (st.st_mode & S_IWUSR) ? 1 : 0;
    } else {
        return (st.st_mode & S_IWGRP) ? 1 : 0;
    }
}
