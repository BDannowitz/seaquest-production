CC = gcc

MYSQL_LIB 	= /usr/lib64/mysql
CCFLAGS 	= -I/usr/include/mysql
all: loadcalmap 

loadcalmap: clean
	$(CC) -g -lm -o loadcalmap loadcalmap.c $(CCFLAGS) -L$(MYSQL_LIB) -lmysqlclient

clean: 
	rm -f loadcalmap.o loadcalmap
