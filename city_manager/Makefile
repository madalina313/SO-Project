# Makefile pentru city_manager
#
# Variabile:
#   CC      = compilatorul C
#   CFLAGS  = opțiuni de compilare:
#               -Wall   : activează toate warning-urile uzuale
#               -Wextra : warning-uri suplimentare
#               -g      : include informații de debugging (pentru gdb)
#   SRCS    = fișierele sursă
#   OBJS    = fișierele obiect generate din surse (înlocuim .c cu .o)
#   TARGET  = executabilul final

CC     = gcc
CFLAGS = -Wall -Wextra -g

SRCS   = city_manager.c reports.c permissions.c filter.c
OBJS   = $(SRCS:.c=.o)
TARGET = city_manager

# monitor_reports este un executabil separat - are propriul main()
# nu face parte din city_manager, deci nu se linkează împreună
MONITOR_TARGET = monitor_reports

# Regula implicită: construiește ambele executabile
# "all" este prima regulă => este default când scriem doar "make"
all: $(TARGET) $(MONITOR_TARGET)

# Regula de link: combină fișierele obiect într-un executabil
# $@ = TARGET (city_manager)
# $^ = toate dependențele (OBJS)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# monitor_reports se compilează și linkează dintr-un singur fișier .c
# Nu depinde de city_manager.h deoarece nu folosește structurile din el
# $@ = monitor_reports, $< = monitor_reports.c
$(MONITOR_TARGET): monitor_reports.c
	$(CC) $(CFLAGS) -o $@ $<

# Regula de compilare: compilează fiecare .c în .o
# %.o: %.c city_manager.h  → orice .o depinde de .c și de header
# -c = compilează fără link (produce .o)
# $< = primul prerequisite (fișierul .c)
%.o: %.c city_manager.h
	$(CC) $(CFLAGS) -c $<

# Curăță fișierele generate (obiect + executabile)
clean:
	rm -f $(OBJS) $(TARGET) $(MONITOR_TARGET)

# "phony" = aceste ținte nu sunt fișiere reale, sunt comenzi
.PHONY: all clean
