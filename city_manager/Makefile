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

# Regula implicită: construiește executabilul
# "all" este prima regulă => este default când scriem doar "make"
all: $(TARGET)

# Regula de link: combină fișierele obiect într-un executabil
# $@ = TARGET (city_manager)
# $^ = toate dependențele (OBJS)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Regula de compilare: compilează fiecare .c în .o
# %.o: %.c city_manager.h  → orice .o depinde de .c și de header
# -c = compilează fără link (produce .o)
# $< = primul prerequisite (fișierul .c)
%.o: %.c city_manager.h
	$(CC) $(CFLAGS) -c $<

# Curăță fișierele generate (obiect + executabil)
clean:
	rm -f $(OBJS) $(TARGET)

# "phony" = aceste ținte nu sunt fișiere reale, sunt comenzi
.PHONY: all clean
