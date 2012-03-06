#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <mysql.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int MAPPING = 0;
int CALIBRATION = 1;
int HIGHVOLTAGE = 2;
int COPY = 0;
int LOAD = 1;

char *source;
char *destination;

char *server;
char *user;
char *password;
char *database;

int mode, method;
char *mode_string;

int initialize(int argc,char* argv[]);
int overwrite(MYSQL *conn);
void show_warnings(MYSQL *conn);
int file_exists(const char * fileName);
int table_exists(MYSQL *conn, const char * schema);
int create_table(MYSQL *conn);
int load_data(MYSQL *conn);
int copy_data(MYSQL *conn);

int main(int argc,char* argv[]){

  MYSQL *conn;
  const int PORT = 0;
  char *UNIX_SOCKET = NULL;
  const int CLIENT_FLAG = 0;

  char check_string[100];

  // Handle the command-line options
  if ( initialize(argc, argv) ) { return 1; }

  // Initialize MySQL database connection
  conn = mysql_init(NULL);

  // Must set this option for the C API to allow loading data from
  // 	local file 
  mysql_options(conn, MYSQL_OPT_LOCAL_INFILE, NULL);

  // Connect to the MySQL database
  if ( !mysql_real_connect(conn, server, user, password, database, PORT, UNIX_SOCKET, CLIENT_FLAG) ) {
       printf(">> Database Connection ERROR: %s\n\n", mysql_error(conn));
       return 1;
  } else {
  	printf(">> Database connection: Success\n\n");
  }

  // Now check if the schema and table exist already, prompt to replace if it does
  if ( overwrite(conn) ) { return 1; }

  // Make all the necessary schemas and tables, should they not already exist
  create_table(conn);

  if (method==LOAD ) {
	// Check if the file exists in the current working directory
	if (file_exists(source)){
	    if ( load_data(conn) ) { return 1; }
	} else {
	    printf(">> ERROR: File ""%s"" not found. Please try again.\n\n", source);
	}
  } else if ( method==COPY ) {
	// Check if the source table exists
	if ( table_exists(conn, source) ) {
	    if ( copy_data(conn) ) { return 1; }
  	} else {
	    printf(">> ERROR: Source schema or table not found. Please try again.\n\n");
	    return 1;
	}
  }

  printf(">> Upload of %s, destination %s successful.\n>> Exiting...\n\n", 
	mode_string, destination);

  mysql_close(conn);

  return 0;

}

int initialize(int argc,char* argv[]){

  int i;
  int required[5];

  for (i=0;i<5;i++) { required[i]=0; }
  
  // Set default values
  server = "e906-gat2.fnal.gov";
  user = "production";
  password = "***REMOVED***";
  database = "";

  for (i=0;i<argc;i++){
	// Filename
	if (!strcmp(argv[i],"-f")){
		source = argv[i+1];
		method = LOAD;
		required[0]=1;
	}
	// Source Schema
	if (!strcmp(argv[i],"-s")){
		source = argv[i+1];
		method = COPY;
		required[1]=1;
	}
 	// Mapping
	else if (!strcmp(argv[i],"-m")){
		mode = MAPPING;
		required[2]=1;
	}
	// Calibration
	else if (!strcmp(argv[i],"-c")){
		mode = CALIBRATION;
		required[3]=1;
	}
	// High Voltage Map
	else if (!strcmp(argv[i],"-hv")){
		mode = HIGHVOLTAGE;
		required[4]=1;
	}
	// Version
	else if (!strcmp(argv[i],"-d")){
		destination = argv[i+1];
		required[5]=1;
	}
	// MySQL Host
	else if (!strcmp(argv[i],"-h")){
		server = argv[i+1];
	}
	// MySQL User Name
	else if (!strcmp(argv[i],"-u")){
		user = argv[i+1];
	}
	// MySQL Password
	else if (!strcmp(argv[i],"-p")){
		password = argv[i+1];
	}
	// Print Help
	else if (!strcmp(argv[i],"help")){
		printf("\n\n-f: File to upload\n"
			"-s: Source schema\n"
			"-m: Load a mapping file\n"
			"-c: Load a calibration file\n"
			"-hv: Load high voltage mapping file\n"
			"-d: Destination schema name (10P01-A, 10P02-B, 11P03-B, etc)\n"
			"-h: server host (optional: default 'e906-gat2.fnal.gov')\n"
			"-u: user (optional: default 'production')\n"
			"-p: password (optional: default password)\n\n");	
	}
  }

  // If every int in the array is a 1, then all necessary information has been entered
  if ((required[0] || required[1]) && (required[2] || required[3] || required[4]) && (required[5])){
	if( mode==CALIBRATION ) mode_string="Calibration";
	else if ( mode==MAPPING ) mode_string="Mapping";
	else if ( mode==HIGHVOLTAGE ) mode_string="HVMap";
  } else {
	printf("Some required options were not included. Exiting!\n");
    	printf("\n\n-f: File to upload\n"
			"-s: Source schema\n"
			"-m: Load a mapping file\n"
			"-c: Load a calibration file\n"
			"-hv: Load high voltage mapping file\n"
			"-d: Destination schema name (10P01-A, 10P02-B, 11P03-B, etc)\n"
			"-h: server host (optional: default 'e906-gat2.fnal.gov')\n"
			"-u: user (optional: default 'production')\n"
			"-p: password (optional: default password)\n\n");
   	return 1;
  }
  return 0;
}

int overwrite(MYSQL *conn){

  char qryString[10000];
  char overwrite_flag[32];

  if ( table_exists(conn, destination) ) {
	printf("WARNING: Table already exists. Overwrite? (y/n)\n>> ");
	scanf("%s",&overwrite_flag);
	if ( !strcmp(overwrite_flag,"y") ){
		sprintf(qryString,"DROP TABLE %s.%s", destination, mode_string);

		if(mysql_query(conn, qryString)){
			printf("%s\n", qryString);
			printf(">> Error clearing out previous table: %s\n", mysql_error(conn));
			return 1;
		}
	} else {
		printf("Exiting...\n\n");
		return 1;
	}
  }


  return 0;
}

void show_warnings(MYSQL *conn){
// If MySQL Warnings should occur, this will print them to screen
	
	MYSQL_RES *res;
	MYSQL_ROW row;
	int num_cols, i;
	
	mysql_query(conn, "SHOW WARNINGS");
	res = mysql_store_result(conn);
	num_cols = mysql_num_fields(res);
    	printf("\n\nLevel\tCode\tMessage\n");
    	while (row = mysql_fetch_row(res))
    	{
		for (i=0; i < num_cols; i++)	
		{
			printf("%s\t", row[i]);
		}
        	printf("\n");   
    	}

	printf("\n\n");
	mysql_free_result(res);
	return;
}

int file_exists(const char * fileName){
    // 1 returned if file exists, 0 if not
    FILE * file;

    if (file = fopen(fileName, "r"))
    {
        fclose(file);
        return 1;
    }
    return 0;
}

int table_exists(MYSQL *conn, const char * schema){

  MYSQL_RES *res;
  MYSQL_ROW row;
  int exists;
  char qryString[10000];

  sprintf(qryString, "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
	"WHERE TABLE_NAME='%s' AND TABLE_SCHEMA='%s'", mode_string, schema);
	
  if( mysql_query(conn, qryString) )
  {
	printf("Table Check Error: %s\n", mysql_error(conn));
	return 1;
  }

  // Store the query result
  res = mysql_store_result(conn);

  // Grab the first (and only) row
  row = mysql_fetch_row(res);

  // Store the value, which is the total number of elements having their cal/map values updated
  exists = atoi(row[0]);

  if ( exists > 1 ) { exists = 1; }

  return exists;

}

int create_table(MYSQL *conn){

  char qryString[10000];

  // If the current calmap destination schema doesn't exist yet, make it
  sprintf(qryString, "CREATE DATABASE IF NOT EXISTS %s", destination);
	
  if( mysql_query(conn, qryString) )
  {
	printf("Create Database Error: %s\n", mysql_error(conn));
	return 1;
  }

  // Set the appropriate schema as the default schema for our work
  sprintf(qryString,"USE %s", destination);

  if( mysql_query(conn,qryString) )
  {
	printf("Select Default Database Error: %s\n", mysql_error(conn));
	return 1;
  }

  if ( mode==CALIBRATION ){
  	// Make the calibration table if it doesn't yet exist
  	sprintf(qryString, "CREATE TABLE IF NOT EXISTS `Calibration` (\n"
  		"`rocID INT NOT NULL, \n"
		"`boardID` int NOT NULL, \n"
  		"`channelID` int NOT NULL, \n"
  		"`t0` double NOT NULL, \n"
  		"`sdtr` double NOT NULL, \n"
  		"`comment` text NOT NULL, \n"
		") ENGINE=MyISAM DEFAULT CHARSET=latin1");
 
  	if( mysql_query(conn, qryString) )
  	{
		printf("Create Calibration Table Error: %s\n", mysql_error(conn));
		return 1;
  	}

  } else if ( mode==MAPPING ){
  	// Make the mapping table
  	sprintf(qryString, "CREATE TABLE IF NOT EXISTS `Mapping` (\n"
  		"`rocID` INT NOT NULL, \n"
		"`boardID` INT, \n"
		"`channelID` INT NOT NULL, \n"
		"`lsbID` INT, \n"
		"`asdqCableID` VARCHAR(32), \n"
		"`asdqChannelID` INT, \n"
		"`detectorName` CHAR(6), \n"
		"`elementID` TINYINT UNSIGNED, \n"
		"INDEX (rocID, boardID, channelID)\n"
		") ENGINE=MyISAM DEFAULT CHARSET=latin1");
	
	if( mysql_query(conn, qryString) )
	{
		printf("Create Mapping Table Error: %s\n", mysql_error(conn));
		return 1;
	}

  } else if ( mode==HIGHVOLTAGE ){
  	// Make the HV mapping table
  	sprintf(qryString, "CREATE TABLE IF NOT EXISTS `HVMap` (\n"
  		"`crateID` INT(11) NOT NULL, \n"
		"`channelID` int(11), \n"
		"`detectorName` CHAR(6), \n"
		"`elementID` TINYINT UNSIGNED\n"
		"`nominalVoltage` INT\n"
		"`setVoltage` INT\n"
		") ENGINE=MyISAM DEFAULT CHARSET=latin1");
	
	if( mysql_query(conn, qryString) )
	{
		printf("Create HVMap Table Error: %s\n", mysql_error(conn));
		return 1;
	}
  }

  return 0;

}

int load_data(MYSQL *conn){

  char qryString[10000];

  // Load the file directly into the table
  sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' \n"
	"INTO TABLE %s.%s \n"
	"FIELDS TERMINATED BY '\\t' \n"
	"LINES TERMINATED BY '\\n' \n"
	"IGNORE 1 LINES",
	source, destination, mode_string);

  if(mysql_query(conn, qryString))
  {
	printf("%s\n", qryString);
	printf(">> Error During File Load: %s\n", mysql_error(conn));
	return 1;
  }

  if( mysql_warning_count(conn) > 0 )
  {
	printf("Warnings have occured from the upload:\n");
	show_warnings(conn);
  }

  return 0;
}

int copy_data(MYSQL *conn){

	char qryString[10000];

	sprintf(qryString, "INSERT INTO %s.%s SELECT * FROM %s.%s",
		destination, mode_string, source, mode_string);

	if( mysql_query(conn, qryString) )
	{
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}

	return 0;
}

