/*
 * city_manager.c - Programul principal + create_district + add_report
 *
 * Utilizare:
 *   ./city_manager --role <rol> --user <user> --<comandă> <district> [opțiuni]
 *
 * Exemple:
 *   ./city_manager --role manager  --user admin  --list downtown
 *   ./city_manager --role inspector --user alice  --add  downtown
 *   ./city_manager --role manager  --user admin  --view downtown --id 1745300000
 *   ./city_manager --role manager  --user admin  --remove_report downtown --id 1745300000
 *   ./city_manager --role manager  --user admin  --update_threshold downtown --value 3
 *   ./city_manager --role inspector --user alice  --filter downtown --condition "severity>=2"
 *
 * Argumente:
 *   --role <rol>       : "manager" sau "inspector"
 *   --user <user>      : numele utilizatorului
 *   --add <district>   : adaugă un raport nou
 *   --list <district>  : listează toate rapoartele
 *   --view <district>  : afișează un raport după ID (necesită --id)
 *   --remove_report <district> : șterge un raport (necesită --id, doar manager)
 *   --update_threshold <district> : actualizează threshold (necesită --value, doar manager)
 *   --filter <district> : filtrează rapoarte (necesită --condition)
 *   --id <id>          : ID-ul raportului (pentru view și remove_report)
 *   --value <n>        : noua valoare threshold (pentru update_threshold)
 *   --condition <expr> : condiția de filtrare (pentru filter), ex: "severity>=2"
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>    /* mkdir(), stat(), chmod() */
#include <fcntl.h>       /* open() */
#include <time.h>        /* time() */
#include <unistd.h>      /* close(), write() */
#include "city_manager.h"
#include <sys/wait.h>
#include <signal.h>      /* kill(), SIGUSR1 */

/* ---------------------------------------------------------------
 * create_district
 *
 * Creează structura de directoare și fișiere pentru un district nou:
 *   <district>/reports.dat    - fișier binar cu rapoarte (0664)
 *   <district>/district.cfg   - configurație cu threshold (0640)
 *   <district>/logged_district - log-ul operațiilor (0644)
 *
 * Permisiunile sunt deliberat diferite:
 *   0664 (rw-rw-r--) → inspector (grup) poate citi și SCRIE rapoarte
 *   0640 (rw-r-----) → inspector (grup) poate doar CITI cfg, nu modifica
 *   0644 (rw-r--r--) → inspector (grup) poate doar CITI log-ul
 *
 * mkdir(path, mode) creează directorul cu permisiunile date.
 *   0750 = rwxr-x--- → manager execută/listează, inspector execută/listează
 *   (x pe director înseamnă drept de traversare/intrare)
 *
 * Apelăm explicit chmod() după open() pentru a suprascrie efectul umask.
 * umask este un filtru al procesului care poate scoate biți din permisiuni.
 * Apelând chmod() după creare, setăm permisiunile exact cum vrem.
 * --------------------------------------------------------------- */
void create_district(const char *district) {
    struct stat st;

    /* Verificăm dacă districtul există deja */
    if (stat(district, &st) == 0) {
        printf("District '%s' already exists.\n", district);
        return;
    }

    /* Creăm directorul principal al districtului */
    if (mkdir(district, 0750) == -1) {
        perror("mkdir failed");
        return;
    }

    char path[256];

    /* ---- reports.dat: 0664 = rw-rw-r-- ---- */
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    int fd = open(path, O_CREAT | O_WRONLY, 0664);
    if (fd == -1) { perror("open reports.dat failed"); return; }
    close(fd);
    chmod(path, 0664);  /* setăm explicit, ignorând umask */

    /* ---- district.cfg: 0640 = rw-r----- ---- */
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    fd = open(path, O_CREAT | O_WRONLY, 0640);
    if (fd == -1) { perror("open district.cfg failed"); return; }
    write(fd, "threshold=1\n", 12);  /* valoare implicită */
    close(fd);
    chmod(path, 0640);

    /* ---- logged_district: 0644 = rw-r--r-- ---- */
    snprintf(path, sizeof(path), "%s/logged_district", district);
    fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd == -1) { perror("open logged_district failed"); return; }
    close(fd);
    chmod(path, 0644);

    printf("District '%s' created successfully.\n", district);
    printf("  reports.dat    : 0664 (rw-rw-r--)\n");
    printf("  district.cfg   : 0640 (rw-r-----)\n");
    printf("  logged_district: 0644 (rw-r--r--)\n");

    /* Creăm și symlink-ul pentru noul district */
    create_symlink(district);
}

/* ---------------------------------------------------------------
 * add_report
 *
 * Citește datele unui raport de la tastatură și îl adaugă în
 * <district>/reports.dat.
 *
 * Structura Report este scrisă BINAR (write(fd, &r, sizeof(Report))):
 *   - Nu e text, nu are separatori
 *   - Dimensiunea exactă: sizeof(Report) bytes
 *   - Fiecare raport începe la offset i * sizeof(Report)
 *
 * O_APPEND: fiecare write() se duce la sfârșitul fișierului, ATOMIC.
 * --------------------------------------------------------------- */
void add_report(const char *district, const char *user, const char *role) {
    struct stat st;

    /* Creăm districtul dacă nu există */
    if (stat(district, &st) == -1) {
        create_district(district);
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    /* Verificare permisiuni de scriere - inspectorul POATE scrie (0664) */
    if (!check_write_permission(path, "inspector")) {
        /* Dacă nici inspectorul nu poate scrie, e o problemă de configurare */
        printf("Error: no write permission on reports.dat\n");
        return;
    }

    /* Citim datele raportului de la tastatură */
    Report r;
    memset(&r, 0, sizeof(Report));  /* inițializăm cu zero - evităm date random */

    r.id = (int)time(NULL);  /* ID unic bazat pe timestamp Unix */
    strncpy(r.inspector, user, sizeof(r.inspector) - 1);

    /* Validare latitude: repetăm până userul introduce o valoare între -90 și 90 */
    do {
        printf("Latitude (-90 to 90): ");
        if (scanf("%lf", &r.latitude) != 1) {
            printf("Invalid input.\n");
            return;
        }
        if (r.latitude < -90.0 || r.latitude > 90.0) {
            printf("Error: latitude must be between -90 and 90. Try again.\n");
        }
    } while (r.latitude < -90.0 || r.latitude > 90.0);

    /* Validare longitude: repetăm până userul introduce o valoare între -180 și 180 */
    do {
        printf("Longitude (-180 to 180): ");
        if (scanf("%lf", &r.longitude) != 1) {
            printf("Invalid input.\n");
            return;
        }
        if (r.longitude < -180.0 || r.longitude > 180.0) {
            printf("Error: longitude must be between -180 and 180. Try again.\n");
        }
    } while (r.longitude < -180.0 || r.longitude > 180.0);

    /* Validare category: repetăm până userul introduce road, lighting sau flooding */
    do {
        printf("Category (road/lighting/flooding): ");
        if (scanf("%31s", r.category) != 1) {
            printf("Invalid input.\n");
            return;
        }
        if (strcmp(r.category, "road")     != 0 &&
            strcmp(r.category, "lighting") != 0 &&
            strcmp(r.category, "flooding") != 0) {
            printf("Error: category must be 'road', 'lighting', or 'flooding'. Try again.\n");
        }
    } while (strcmp(r.category, "road")     != 0 &&
             strcmp(r.category, "lighting") != 0 &&
             strcmp(r.category, "flooding") != 0);

    /* Validare severity: repetăm până userul introduce 1, 2 sau 3 */
    do {
        printf("Severity (1=minor, 2=moderate, 3=critical): ");
        if (scanf("%d", &r.severity) != 1) {
            printf("Invalid input.\n");
            return;
        }
        if (r.severity < 1 || r.severity > 3) {
            printf("Error: severity must be 1, 2, or 3. Try again.\n");
        }
    } while (r.severity < 1 || r.severity > 3);

    r.timestamp = time(NULL);

    /* Validare description: repetăm până userul introduce ceva non-gol */
    getchar();  /* consumăm '\n' rămas în buffer după scanf */
    do {
        printf("Description (cannot be empty): ");
        if (fgets(r.description, sizeof(r.description), stdin) == NULL) {
            printf("Invalid input.\n");
            return;
        }
        /* fgets include '\n' la sfârșit - descrierea e goală dacă primul char e '\n' */
        if (r.description[0] == '\n') {
            printf("Error: description cannot be empty. Try again.\n");
        }
    } while (r.description[0] == '\n');

    /* Deschidem pentru append - adăugăm la sfârșitul fișierului */
    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd == -1) { perror("open reports.dat failed"); return; }

    /* Scriem structura ca date binare brute */
    write(fd, &r, sizeof(Report));
    close(fd);

    printf("Report added successfully! ID: %d\n", r.id);

    /* Logăm operația */
    log_operation(district, role, user, "add_report");

    /*
     * Notificăm monitor_reports dacă rulează.
     *
     * Cum funcționează:
     *   1. Încercăm să deschidem .monitor_pid cu open() (O_RDONLY)
     *   2. Dacă fișierul nu există → open() returnează -1 → notify_monitor_FAIL
     *   3. Dacă există, citim PID-ul ca șir de caractere cu read()
     *   4. Dacă read() eșuează sau PID invalid → notify_monitor_FAIL
     *   5. kill(pid, SIGUSR1) trimite semnalul - dacă returnează 0, a reușit
     *   6. Logăm rezultatul (OK sau FAIL) în logged_district în toate cazurile
     */
    {
        int notify_ok = 0;  /* presupunem eșec până dovedim succesul */

        int pid_fd = open(".monitor_pid", O_RDONLY);
        if (pid_fd != -1) {
            char pid_buf[32];
            memset(pid_buf, 0, sizeof(pid_buf));
            ssize_t n = read(pid_fd, pid_buf, sizeof(pid_buf) - 1);
            close(pid_fd);

            if (n > 0) {
                pid_t monitor_pid = (pid_t)atoi(pid_buf);
                if (monitor_pid > 0 && kill(monitor_pid, SIGUSR1) == 0) {
                    notify_ok = 1;
                }
            }
        }

        if (notify_ok) {
            log_operation(district, role, user, "notify_monitor_OK");
        } else {
            /* Acoperă toate cazurile de eșec:
             *   - .monitor_pid nu există (monitorul nu rulează)
             *   - read() a eșuat sau fișier gol
             *   - PID invalid (≤ 0)
             *   - kill() a eșuat (monitorul s-a oprit între timp)
             */
            log_operation(district, role, user, "notify_monitor_FAIL");
        }
    }
}

/* ---------------------------------------------------------------
 * remove_district
 *
 * Sterge un district intreg si symlink-ul corespunzator.
 * Doar pentru manager.
 *
 * Cum functioneaza:
 * 1. fork() creaza un proces copil identic cu parintele
 * 2. In procesul copil apelam execvp() care inlocuieste procesul
 *    cu comanda "rm -rf <district>"
 * 3. Parintele asteapta cu waitpid() pana copilul termina
 * --------------------------------------------------------------- */
void remove_district(const char *district, const char *role) {
    /* Doar managerul poate sterge districte */
    if (strcmp(role, "manager") != 0) {
        printf("Error: only 'manager' role can remove districts.\n");
        return;
    }

    /* Verificam ca districtul exista */
    struct stat st;
    if (stat(district, &st) == -1) {
        printf("Error: district '%s' does not exist.\n", district);
        return;
    }

    /* Cream procesul copil */
    pid_t pid = fork();

    if (pid == -1) {
        /* fork() a esuat */
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        /* ---- PROCESUL COPIL ---- */
        /* execvp inlocuieste procesul curent cu "rm -rf <district>" */
        char *args[] = { "rm", "-rf", (char *)district, NULL };
        execvp("rm", args);

        /* Daca ajungem aici, execvp a esuat */
        perror("execvp failed");
        _exit(1);
    }

    /* ---- PROCESUL PARINTE ---- */
    /* Asteptam sa termine copilul */
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("District '%s' deleted successfully.\n", district);

        /* Stergem si symlink-ul */
        char symlink_name[256];
        snprintf(symlink_name, sizeof(symlink_name), "active_reports-%s", district);
        unlink(symlink_name);
        printf("Symlink '%s' removed.\n", symlink_name);
    } else {
        printf("Error: failed to delete district '%s'.\n", district);
    }
}

/* ---------------------------------------------------------------
 * main - parser de argumente și dispatcher de comenzi
 *
 * Parcurgem argv[] secvențial. Când găsim o opțiune care ia un argument
 * (ex: --role), facem ++i ca să avansăm la argumentul următor.
 *
 * Variabile colectate:
 *   role      → rolul utilizatorului ("manager" sau "inspector")
 *   user      → numele utilizatorului
 *   command   → comanda de executat (determinată de flag-ul --xxx)
 *   district  → directorul districtului
 *   target_id → ID-ul raportului (pentru --view, --remove_report)
 *   threshold → noua valoare threshold (pentru --update_threshold)
 *   condition → condiția de filtrare (pentru --filter)
 * --------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    /*
     * umask(0) dezactivează filtrul de permisiuni al procesului.
     * Fără asta, umask-ul shell-ului (de obicei 022) ar putea elimina biți
     * din permisiunile cerute la open()/mkdir().
     * Apelând umask(0) devreme în main(), toate operațiile ulterioare
     * folosesc exact permisiunile pe care le specificăm noi cu chmod().
     */
    umask(0);
    char role[32]     = "";
    char user[64]     = "";
    char command[32]  = "";
    char district[64] = "";
    int  target_id    = 0;
    int  threshold_val = 0;
    /*
     * Array de condiții pentru --filter.
     * Stocăm pointeri direct în argv[] - sunt valizi pe toată durata main().
     * MAX_CONDITIONS e definit în header.
     */
    const char *conditions[MAX_CONDITIONS];
    int num_conditions = 0;

    /* Parcurgem argumentele din linia de comandă */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strncpy(role, argv[++i], sizeof(role) - 1);
        }
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strncpy(user, argv[++i], sizeof(user) - 1);
        }
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            strcpy(command, "add");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) {
            strcpy(command, "list");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        else if (strcmp(argv[i], "--view") == 0 && i + 1 < argc) {
            strcpy(command, "view");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        else if (strcmp(argv[i], "--remove_report") == 0 && i + 1 < argc) {
            strcpy(command, "remove_report");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        else if (strcmp(argv[i], "--update_threshold") == 0 && i + 1 < argc) {
            strcpy(command, "update_threshold");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            strcpy(command, "filter");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        else if (strcmp(argv[i], "--create_district") == 0 && i + 1 < argc) {
            strcpy(command, "create_district");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        /* Argumente auxiliare */
        else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            target_id = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--value") == 0 && i + 1 < argc) {
            threshold_val = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--remove_district") == 0 && i + 1 < argc) {
            strcpy(command, "remove_district");
            strncpy(district, argv[++i], sizeof(district) - 1);
        }
        else if (strcmp(argv[i], "--condition") == 0 && i + 1 < argc) {
            if (num_conditions < MAX_CONDITIONS) {
                conditions[num_conditions++] = argv[++i];
            } else {
                printf("Warning: maximum %d conditions allowed, ignoring extra.\n",
                       MAX_CONDITIONS);
                i++;  /* consumăm argumentul ignorat */
            }
        }
    }

    /* Validare de bază */
    if (role[0] == '\0') {
        printf("Error: --role is required (manager or inspector)\n");
        printf("Usage: %s --role <role> --user <user> --<command> <district> [options]\n",
               argv[0]);
        return 1;
    }
    if (user[0] == '\0') {
        printf("Error: --user is required\n");
        return 1;
    }
    if (command[0] == '\0') {
        printf("Error: no command specified.\n");
        printf("Commands: --add, --list, --view, --remove_report,\n");
        printf("          --update_threshold, --filter, --create_district\n");
        return 1;
    }

    /* Dispatcher: apelăm funcția corespunzătoare comenzii */
    if (strcmp(command, "create_district") == 0) {
        create_district(district);
    }
    else if (strcmp(command, "add") == 0) {
        add_report(district, user, role);
    }
    else if (strcmp(command, "list") == 0) {
        list_reports(district, role, user);
    }
    else if (strcmp(command, "view") == 0) {
        view_report(district, target_id, role, user);
    }
    else if (strcmp(command, "remove_report") == 0) {
        remove_report_cmd(district, target_id, role, user);
    }
    else if (strcmp(command, "update_threshold") == 0) {
        update_threshold(district, threshold_val, role, user);
    }
    else if (strcmp(command, "filter") == 0) {
        filter_reports(district, conditions, num_conditions, role, user);
    }
    else if (strcmp(command, "remove_district") == 0) {
        remove_district(district, role);
    }
    else {
        printf("Unknown command: '%s'\n", command);
        return 1;
    }

    return 0;
}
