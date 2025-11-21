CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS = -lrt -lpthread
else
    LDFLAGS = -lpthread
endif

TARGETS = librarian reader observer

COMMON_OBJ = common.o
LIBRARIAN_OBJ = librarian.o $(COMMON_OBJ)
READER_OBJ = reader.o $(COMMON_OBJ)
OBSERVER_OBJ = observer.o $(COMMON_OBJ)

.PHONY: all clean

all: $(TARGETS)

librarian: $(LIBRARIAN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

reader: $(READER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

observer: $(OBSERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) *.o
	rm -rf /tmp/library_observers
	rm -f /dev/shm/library_shm*

clean-all: clean
	@echo "Очистка системных ресурсов..."
	-rm -f /dev/shm/*book_sem*
	-rm -rf /tmp/library_observers
	@echo "Очистка завершена"

