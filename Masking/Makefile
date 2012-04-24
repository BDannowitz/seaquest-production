CC = gcc

MYSQL_LIB 	= /usr/lib64/mysql
CCFLAGS 	= -I/usr/include/mysql
all: lookup

lookup: clean
	$(CC) -std=c99 -lm -o lookup lookup.c $(CCFLAGS) -L$(MYSQL_LIB) -lmysqlclient

clean: 
	rm -f lookup.o lookup
