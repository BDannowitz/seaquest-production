CC = gcc

MYSQL_LIB 	= /usr/lib64/mysql
CCFLAGS 	= -I/usr/include/mysql
all: decoda

decoda: clean 
	$(CC) -g -O3 -lm -o decoda coda_read.c coda_read.h $(CCFLAGS) -L$(MYSQL_LIB) -lmysqlclient -lmyevio

clean: 
	rm -f coda_read.o decoda

