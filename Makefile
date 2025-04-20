CC = gcc

CFLAGS = -Wall -g -std=c99

LDFLAGS = -lm

SOURCES = main.c process.c scheduler.c

OBJECTS = $(SOURCES:.c=.o)

TARGET = probsched

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Executável $(TARGET) criado com sucesso."

%.o: %.c process.h scheduler.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
	@echo "Ficheiros gerados removidos."

.PHONY: all clean