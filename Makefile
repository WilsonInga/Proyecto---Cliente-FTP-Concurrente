# Compilador de C
CC = gcc

# Flags de compilacion: -g  y -Wall 
CFLAGS = -g -Wall

# Nombre del ejecutable final
TARGET = clienteFTP

# Lista de todos los archivos fuente .c
SRCS = clienteFTP.c \
       connectsock.c \
       connectTCP.c \
       errexit.c \
       passivesock.c \
       passiveTCP.c

# Convierte la lista de .c a .o (archivos objeto)
OBJS = $(SRCS:.c=.o)

# Regla principal
all: $(TARGET)

# Regla para enlazar el ejecutable final
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Regla generica para compilar .c a .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regla "clean" para limpiar los archivos generados
clean:
	rm -f $(OBJS) $(TARGET)

# Declara "all" y "clean" como reglas "phony" (no son archivos)
.PHONY: all clean