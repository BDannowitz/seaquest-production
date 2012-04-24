#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <mysql.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

char *geometry;
char *server;
char *user;
char *password;
char *database;
char *table;

int initialize(int argc,char* argv[]);
int overwrite(MYSQL *conn, char * schema, char * table);
void show_warnings(MYSQL *conn);
int file_exists(const char * fileName);
int table_exists(MYSQL *conn, const char * schema, char * table);
int schema_exists(MYSQL *conn, const char * schema);
int make_corners(MYSQL *conn, char * geo, char * schema);
int match_wires(MYSQL *conn, char * geo, char * schema);
int make_lookup(MYSQL *conn, char * schema, char * table);
