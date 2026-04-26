/*
 * filter.c - Parsarea și evaluarea condițiilor de filtrare
 *
 * Permite filtrarea rapoartelor după câmpuri și operatori:
 *
 *   severity=3         → afișează rapoartele cu severity exact 3
 *   severity>=2        → afișează rapoartele cu severity >= 2
 *   category=road      → afișează rapoartele din categoria "road"
 *   inspector=alice    → afișează rapoartele scrise de "alice"
 *
 * Câmpuri suportate:
 *   id, severity            → câmpuri numerice (suportă toți operatorii)
 *   category, inspector     → câmpuri string (suportă doar '=')
 *
 * Operatori suportați: =  >=  <=  >  <
 */

#include <fcntl.h>       /* open(), O_RDONLY */
#include <unistd.h>      /* read(), close() */
#include <sys/stat.h>    /* stat(), struct stat */
#include <string.h>      /* strcmp(), strstr(), strchr(), strncpy(), memset() */
#include <stdio.h>       /* printf() */
#include <stdlib.h>      /* atoi() */
#include "city_manager.h"

/* ---------------------------------------------------------------
 * Structura Condition
 *
 * Reține o condiție de filtrare după parsare.
 * Exemplu: "severity>=2" → field="severity", op=">=", value="2"
 *
 * Definită local în filter.c deoarece e un detaliu de implementare
 * intern - nu trebuie expusă în header.
 * --------------------------------------------------------------- */
typedef struct {
    char field[32];  /* câmpul din Report: severity, category, inspector, id */
    char op[4];      /* operatorul: =, >=, <=, >, < */
    char value[64];  /* valoarea de comparat (ca string, convertim la nevoie) */
} Condition;

/* ---------------------------------------------------------------
 * parse_condition
 *
 * Parsează un string de condiție într-o structură Condition.
 *
 * Algoritm:
 *   Parcurgem șirul și căutăm operatorul. Trebuie să căutăm în ordinea:
 *   ">=" înainte de ">" (altfel ">" ar fi găsit primul din ">=")
 *   "<=" înainte de "<"
 *
 *   strstr(s, sub) → returnează pointer la prima apariție a sub-șirului `sub`
 *                    în șirul `s`, sau NULL dacă nu există
 *   strchr(s, c)   → returnează pointer la prima apariție a caracterului `c`
 *                    în șirul `s`, sau NULL dacă nu există
 *
 * Odată găsit operatorul (la adresa `pos`):
 *   field = caracterele de la cond_str până la pos (lungime = pos - cond_str)
 *   value = caracterele de la pos + strlen(op) până la final
 *
 * Returnează: 0 la succes, -1 la eroare
 * --------------------------------------------------------------- */
static int parse_condition(const char *cond_str, Condition *cond) {
    const char *pos;  /* pointer în cond_str la începutul operatorului */
    int field_len;

    /* Inițializăm toată structura cu zero - previne caractere random */
    memset(cond, 0, sizeof(Condition));

    /*
     * Căutăm operatorii în ORDINE IMPORTANTĂ:
     * >= și <= înainte de > și <
     * (altfel ">=" ar fi recunoscut ca ">" cu valoarea "=...")
     */
    if ((pos = strstr(cond_str, ">=")) != NULL) {
        strcpy(cond->op, ">=");
    } else if ((pos = strstr(cond_str, "<=")) != NULL) {
        strcpy(cond->op, "<=");
    } else if ((pos = strchr(cond_str, '>')) != NULL) {
        strcpy(cond->op, ">");
    } else if ((pos = strchr(cond_str, '<')) != NULL) {
        strcpy(cond->op, "<");
    } else if ((pos = strchr(cond_str, '=')) != NULL) {
        strcpy(cond->op, "=");
    } else {
        printf("Error: invalid condition '%s'\n", cond_str);
        printf("Expected: field<op>value  (e.g. severity>=2, category=road)\n");
        printf("Operators: =  >=  <=  >  <\n");
        printf("Fields:    id, severity, category, inspector\n");
        return -1;
    }

    /* Extragem câmpul: tot ce e înainte de operator */
    field_len = (int)(pos - cond_str);
    if (field_len <= 0 || field_len >= (int)sizeof(cond->field)) {
        printf("Error: missing or too long field name in condition '%s'\n", cond_str);
        return -1;
    }
    strncpy(cond->field, cond_str, field_len);
    cond->field[field_len] = '\0';  /* terminăm string-ul manual */

    /* Trim: eliminăm spațiile de la sfârșitul field-ului.
     * Dacă userul scrie "severity >=2", field devine "severity " fără trim.
     * Mergem de la sfârșit spre stânga și înlocuim spațiile cu '\0'. */
    int trim = field_len - 1;
    while (trim >= 0 && cond->field[trim] == ' ') {
        cond->field[trim] = '\0';
        trim--;
    }

    /* Extragem valoarea: tot ce e după operator */
    const char *val_start = pos + strlen(cond->op);
    if (*val_start == '\0') {
        printf("Error: missing value after operator in '%s'\n", cond_str);
        return -1;
    }
    strncpy(cond->value, val_start, sizeof(cond->value) - 1);
    cond->value[sizeof(cond->value) - 1] = '\0';

    return 0;  /* succes */
}

/* ---------------------------------------------------------------
 * match_condition
 *
 * Verifică dacă un raport satisface condiția dată.
 *
 * Pentru câmpuri numerice (id, severity):
 *   Convertim cond->value din string la int cu atoi()
 *   Comparăm cu operatorul potrivit
 *
 * Pentru câmpuri string (category, inspector):
 *   Folosim strcmp() pentru egalitate
 *   Operatorii <, >, >=, <= nu au sens semantic pentru aceste câmpuri
 *   → afișăm warning și returnăm 0
 *
 * Returnează: 1 dacă raportul satisface condiția, 0 dacă nu
 * --------------------------------------------------------------- */
static int match_condition(const Report *r, const Condition *cond) {
    /* Câmpul "severity" - valoare numerică */
    if (strcmp(cond->field, "severity") == 0) {
        int rval = r->severity;
        int cval = atoi(cond->value);  /* convertim string la int */

        if (strcmp(cond->op, "=")  == 0) return rval == cval;
        if (strcmp(cond->op, ">=") == 0) return rval >= cval;
        if (strcmp(cond->op, "<=") == 0) return rval <= cval;
        if (strcmp(cond->op, ">")  == 0) return rval >  cval;
        if (strcmp(cond->op, "<")  == 0) return rval <  cval;
    }

    /* Câmpul "id" - valoare numerică */
    else if (strcmp(cond->field, "id") == 0) {
        int rval = r->id;
        int cval = atoi(cond->value);

        if (strcmp(cond->op, "=")  == 0) return rval == cval;
        if (strcmp(cond->op, ">=") == 0) return rval >= cval;
        if (strcmp(cond->op, "<=") == 0) return rval <= cval;
        if (strcmp(cond->op, ">")  == 0) return rval >  cval;
        if (strcmp(cond->op, "<")  == 0) return rval <  cval;
    }

    /* Câmpul "category" - string, doar '=' */
    else if (strcmp(cond->field, "category") == 0) {
        if (strcmp(cond->op, "=") != 0) {
            printf("Warning: field 'category' supports only '=' operator.\n");
            return 0;
        }
        /*
         * strcmp(a, b) == 0 înseamnă că șirurile sunt IDENTICE.
         * Returnăm 1 (match) dacă categoria raportului = valoarea din condiție.
         */
        return strcmp(r->category, cond->value) == 0;
    }

    /* Câmpul "inspector" - string, doar '=' */
    else if (strcmp(cond->field, "inspector") == 0) {
        if (strcmp(cond->op, "=") != 0) {
            printf("Warning: field 'inspector' supports only '=' operator.\n");
            return 0;
        }
        return strcmp(r->inspector, cond->value) == 0;
    }

    /* Câmp necunoscut */
    else {
        printf("Warning: unknown field '%s'.\n", cond->field);
        printf("Supported fields: id, severity, category, inspector\n");
    }

    return 0;  /* implicit: nu satisface condiția */
}

/* ---------------------------------------------------------------
 * filter_reports
 *
 * Filtrează rapoartele după mai multe condiții cu logică AND.
 * Un raport e afișat doar dacă satisface TOATE condițiile din array.
 *
 * Pași:
 *   1. Validare de bază: cel puțin o condiție, districtul există
 *   2. Parsăm TOATE condițiile cu parse_condition() → array de Condition
 *   3. Validăm severity dacă e printre câmpuri
 *   4. check_read_permission() cu stat()
 *   5. open() + buclă read()
 *   6. Pentru fiecare raport: match_condition() pe TOATE condițiile (AND)
 *   7. log_operation()
 *
 * De ce array de Condition și nu verificare pe string-uri direct?
 *   Parsăm o singură dată la început, nu la fiecare raport.
 *   Evităm parsarea repetată (O(N*M) parsări → O(M) parsări + O(N*M) comparații)
 * --------------------------------------------------------------- */
void filter_reports(const char *district,
                    const char *conditions[], int num_conditions,
                    const char *role, const char *user) {
    /* Pas 1: trebuie cel puțin o condiție */
    if (num_conditions == 0) {
        printf("Error: no filter condition specified. Use --condition \"field=value\"\n");
        return;
    }

    /* Validare district cu stat() */
    struct stat dst;
    if (stat(district, &dst) == -1 || !S_ISDIR(dst.st_mode)) {
        printf("Error: district '%s' does not exist.\n", district);
        return;
    }

    /* Pas 2: parsăm toate condițiile într-un array de structuri Condition */
    Condition conds[MAX_CONDITIONS];
    for (int i = 0; i < num_conditions; i++) {
        if (parse_condition(conditions[i], &conds[i]) == -1) {
            return;  /* o condiție invalidă → oprim totul */
        }
        /* Pas 3: validare severity pentru fiecare condiție */
        if (strcmp(conds[i].field, "severity") == 0) {
            int sv = atoi(conds[i].value);
            if (sv < 1 || sv > 3) {
                printf("Warning: severity must be between 1 and 3 (condition: %s)\n",
                       conditions[i]);
                return;
            }
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    /* Pas 4: verificare permisiuni cu stat() */
    if (!check_read_permission(path, role)) {
        printf("Error: role '%s' does not have read permission on %s\n",
               role, path);
        log_operation(district, role, user, "filter_DENIED");
        return;
    }

    /* Pas 5: deschidem fișierul */
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open reports.dat failed");
        return;
    }

    /* Afișăm toate condițiile active */
    printf("=== Filter: ");
    for (int i = 0; i < num_conditions; i++) {
        printf("[%s]", conditions[i]);
        if (i < num_conditions - 1) printf(" AND ");
    }
    printf(" | district: %s ===\n\n", district);

    Report r;
    int count = 0;
    int total = 0;

    /* Pas 6: citim și filtrăm cu AND pe toate condițiile */
    while (read(fd, &r, sizeof(Report)) == (ssize_t)sizeof(Report)) {
        total++;

        /*
         * AND logic: raportul trebuie să satisfacă TOATE condițiile.
         * Dacă o condiție nu e satisfăcută, trecem la raportul următor.
         */
        int all_match = 1;
        for (int i = 0; i < num_conditions; i++) {
            if (!match_condition(&r, &conds[i])) {
                all_match = 0;
                break;
            }
        }

        if (all_match) {
            char ts[64];
            struct tm *tm_info = localtime(&r.timestamp);
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

            printf("--- Report #%d ---\n", r.id);
            printf("  Inspector:   %s\n", r.inspector);
            printf("  Location:    %.6f, %.6f\n", r.latitude, r.longitude);
            printf("  Category:    %s\n", r.category);
            printf("  Severity:    %d\n", r.severity);
            printf("  Timestamp:   %s\n", ts);
            printf("  Description: %s\n", r.description);
            printf("\n");
            count++;
        }
    }
    close(fd);

    printf("Matched %d of %d report(s).\n", count, total);

    /* Pas 7: logăm toate condițiile folosite */
    char log_msg[200];
    int offset = snprintf(log_msg, sizeof(log_msg), "filter(");
    for (int i = 0; i < num_conditions && offset < (int)sizeof(log_msg) - 2; i++) {
        offset += snprintf(log_msg + offset, sizeof(log_msg) - offset,
                           "%s%s", conditions[i], i < num_conditions - 1 ? " AND " : "");
    }
    snprintf(log_msg + offset, sizeof(log_msg) - offset, ")");
    log_operation(district, role, user, log_msg);
}
