#include "lookup.h"

int main(int argc,char* argv[]){

  MYSQL *conn;
  const int PORT = 0;
  char *UNIX_SOCKET = NULL;
  const int CLIENT_FLAG = 0;
  char qryString[1000000];
  MYSQL_RES *res;
  MYSQL_ROW row;
  int num_cols, num_rows, i;
  

  // Handle the command-line options
  if ( initialize(argc, argv) ) { return 1; }

  // Initialize MySQL database connection
  conn = mysql_init(NULL);

  // Must set this option for the C API to allow loading data from
  // 	local file 
  mysql_options(conn, MYSQL_OPT_LOCAL_INFILE, NULL);

  // Connect to the MySQL database
  if ( !mysql_real_connect(conn, server, user, password, "", PORT, UNIX_SOCKET, CLIENT_FLAG) ) {
       printf(">> Database Connection ERROR: %s\n\n", mysql_error(conn));
       return 1;
  }

  // Check to see if digiPlanes exists, as this is from whence we derive our information
  if (!table_exists(conn, geometry, "digiPlanes")){
	printf("ERROR: Geometry schema or digiPlanes table specified does not exist; Exiting...\n\n");
	return 1;
  }
  
  // Remove the Lookup table if it exists
  if (overwrite(conn, database, table)) return 1;
  
  // Create the schema, should it not already exist
  sprintf(qryString, "CREATE DATABASE IF NOT EXISTS %s", database);
  if ( mysql_query(conn, qryString))
  {
	  printf("Create Database Error: %s\n", mysql_error(conn));
	  return 1;
  }
  
  // Set the default schema
  sprintf(qryString, "USE %s", database);
  if ( mysql_query(conn, qryString) ){
	  printf("Set default schema Error: %s\n", mysql_error(conn));
	  return 1;
  }
  
  // Create a table containing the x,y,z coordinates of each paddle of each hodoscope plane
  if ( make_corners(conn, geometry, database) ){
	  printf("Error compiling Hodoscope Paddle Corners; Exiting...\n");
	  return 1;
  }
  
  // Match a wire number for each chamber in the same station to each corner of each paddle
  if ( match_wires(conn, geometry, database) ){
	  printf("Error matching wires to hodo paddle corners; Exiting...\n");
	  return 1;
  }

  // Find the max and min wires for each paddle, and from that create a lookup table matching
  // 	paddles to wires
  if ( make_lookup(conn, database, table) ){
	  printf("Error making final lookup table; Exiting...\n");
	  return 1;
  }

  // Success!
  printf("Schema: \"%s\"\nTable: \"%s\"\nGeometry \"%s\"\nLookup table successfully created.\n\n",
	database, table, geometry);

  // Close up shop
  mysql_close(conn);

  return 0;

}

int initialize(int argc,char* argv[]){

  int i;
  int required[2];

  for (i=0;i<2;i++) { required[i]=0; }
  
  // Set default values
  server = "e906-gat2.fnal.gov";
  user = "production";
  password = "***REMOVED***";
  database = "";
  table = "Lookup";

  for (i=0;i<argc;i++){
	// Source Geometry Schema
	if (!strcmp(argv[i],"-g")){
		geometry = argv[i+1];
		required[0]=1;
	}
	// Destination Schema
	else if (!strcmp(argv[i],"-d")){
		database = argv[i+1];
		required[1]=1;
	}
	// Destination Table
	else if (!strcmp(argv[i],"-t")){
		table = argv[i+1];
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
	else if (!strcmp(argv[i],"--help")){
		printf("\n\n-g: Source geometry schema\n"
			"-d: Destination schema name\n"
			"-t: Destination table name (optional: default 'Lookup')\n"
			"-h: server host (optional: default 'e906-gat2.fnal.gov')\n"
			"-u: user (optional: default 'production')\n"
			"-p: password (optional: default password)\n\n");
		return 1;	
	}
  }

  // If every int in the array is a 1, then all necessary information has been entered
  if (!(required[0] && required[1])) {
	printf("Some required options were not included. Exiting!\n");
    	printf("\n\n-g: Source geometry schema\n"
		"-d: Destination schema name\n"
		"-t: Destination table name (optional: default 'Lookup')\n"
		"-h: server host (optional: default 'e906-gat2.fnal.gov')\n"
		"-u: user (optional: default 'production')\n"
		"-p: password (optional: default password)\n\n");
   	return 1;
  }
  return 0;
}

int overwrite(MYSQL *conn, char * schema, char * table){

  char qryString[1000];
  char overwrite_flag[32];

  // If the table exists, ask if the user wants to overwrite it. If 'y', then drop the table.
  if ( table_exists(conn, schema, table) ) {
	printf("WARNING: Table already exists. Overwrite? (y/n)\n>> ");
	scanf("%s",&overwrite_flag);
	if ( !strcmp(overwrite_flag,"y") ){
		sprintf(qryString,"DROP TABLE %s.%s", schema, table);

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

int table_exists(MYSQL *conn, const char * schema, char * table){

  MYSQL_RES *res;
  MYSQL_ROW row;
  int exists;
  char qryString[10000];

  sprintf(qryString, "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES "
	"WHERE TABLE_NAME='%s' AND TABLE_SCHEMA='%s'", table, schema);
	
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

  mysql_free_result(res);

  if ( exists > 1 ) { exists = 1; }

  return exists;

}

int schema_exists(MYSQL *conn, const char * schema){

  MYSQL_RES *res;
  MYSQL_ROW row;
  int exists;
  char qryString[10000];

  sprintf(qryString, "SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA "
	"WHERE SCHEMA_NAME='%s'", schema);
	
  if( mysql_query(conn, qryString) )
  {
	printf("Table Check Error: %s\n", mysql_error(conn));
	return 1;
  }

  res = mysql_store_result(conn);	// Store the query result
  row = mysql_fetch_row(res);		// Grab the first (and only) row
  exists = atoi(row[0]);		// Store the count
  mysql_free_result(res);

  if ( exists > 1 ) exists = 1;

  return exists;

}

int make_corners(MYSQL *conn, char * geo, char * schema){

  int detectorID;
  int stationID;
  char *detectorName;
  double spacing;
  double overlap;
  int numElements;
  double angleFromVert;
  double costh, sinth;
  double x0;
  double y0;
  double z0;
  double x1, x2, x3, x4;
  double y1, y2, y3, y4;
  double width;
  double height;
  const float pi = acos(-1);
  
  char qryString[1000000];
  MYSQL_RES *res;
  MYSQL_ROW row;
  int num_cols, num_rows, i, j, rowCount;
  
  // Create table that will contain hodo corner locations
  sprintf(qryString, "CREATE TEMPORARY TABLE IF NOT EXISTS Corners ("
		  "`detectorID` tinyint(3) unsigned DEFAULT NULL,"
		  "`stationID` tinyint(3) unsigned DEFAULT NULL,"
		  "`detectorName` varchar(6) DEFAULT NULL,"
		  "`elementID` smallint(5) DEFAULT NULL,"
		  "`x` double DEFAULT NULL,"
		  "`y` double DEFAULT NULL,"
		  "`z` double DEFAULT NULL)");
  if ( mysql_query(conn, qryString))
  {
	  printf("digiPlanes Query Error: %s\n", mysql_error(conn));
	  return 1;
  }
  
  // Grab information regarding all hodo planes
  sprintf(qryString, "SELECT * FROM %s.digiPlanes WHERE detectorName LIKE 'H_T' OR detectorName LIKE 'H_B'", geo);
  if ( mysql_query(conn, qryString))
  {
	  printf("digiPlanes Query Error: %s\n", mysql_error(conn));
	  return 1;
  }
  

  j = 0;
  res = mysql_store_result(conn);
  num_cols = mysql_num_fields(res);
  num_rows = mysql_num_rows(res);
  while (row = mysql_fetch_row(res)){
	  // Each row corresponds to a single hodoscope plane
	  detectorID = atoi(row[0]);
	  stationID = atoi(row[1]);
	  detectorName = row[2];
	  spacing = atof(row[5]);
	  overlap = atof(row[6]);
	  numElements = atoi(row[7]);
	  angleFromVert = atof(row[8]);		// For hodo's this is either 0 or 90 degrees
	  sinth = sin(pi/180.0*angleFromVert);
	  costh = cos(pi/180.0*angleFromVert);
	  x0 = atof(row[10]);			// This is the location of the center of the plane
	  y0 = atof(row[11]);			//
	  z0 = atof(row[12]);			//
	  width = atof(row[13]);
	  height = atof(row[14]);
	  
	  for(i=1;i<(numElements+1);i++){
		  // Calculate the x,y coordinates of each corner of each paddle
		  x1 = x0+(-costh*0.5*width-sinth*0.5*width)+(((i-1)*spacing-(i-1)*overlap)*costh);
		  y1 = y0+(costh*0.5*height-sinth*0.5*height)+(((i-1)*spacing-(i-1)*overlap)*sinth);
	  
		  x2 = x0+(-costh*0.5*width-sinth*0.5*width)+((i*spacing-(i-1)*overlap)*costh);
		  y2 = y0+(costh*0.5*height-sinth*0.5*height)+((i*spacing-(i-1)*overlap)*sinth);
	  
		  x3 = x0+(-costh*0.5*width+sinth*0.5*width)+(((i-1)*spacing-(i-1)*overlap)*costh);
		  y3 = y0+(-costh*0.5*height-sinth*0.5*height)+(((i-1)*spacing-(i-1)*overlap)*sinth);
	  
		  x4 = x0+(-costh*0.5*width-sinth*0.5*width)+((i*spacing-(i-1)*overlap)*costh);
		  y4 = y0+(-costh*0.5*height-sinth*0.5*height)+((i*spacing-(i-1)*overlap)*sinth);
	  
		  // Add these entries into the table
		  if (j>0){
			sprintf(qryString,"%s, (%i, %i, \"%s\", %i, %f, %f, %f), "
				"(%i, %i, \"%s\", %i, %f, %f, %f), "
				"(%i, %i, \"%s\", %i, %f, %f, %f), "
				"(%i, %i, \"%s\", %i, %f, %f, %f)", qryString, 
				detectorID, stationID, detectorName, i, x1, y1, z0,
				detectorID, stationID, detectorName, i, x2, y2, z0,
				detectorID, stationID, detectorName, i, x3, y3, z0,
				detectorID, stationID, detectorName, i, x4, y4, z0);
		  } 
		  if (j==0) {
			sprintf(qryString,"INSERT INTO Corners VALUES (%i, %i, \"%s\", %i, %f, %f, %f), "
				"(%i, %i, \"%s\", %i, %f, %f, %f), "
				"(%i, %i, \"%s\", %i, %f, %f, %f), "
				"(%i, %i, \"%s\", %i, %f, %f, %f)",
				detectorID, stationID, detectorName, i, x1, y1, z0,
				detectorID, stationID, detectorName, i, x2, y2, z0,
				detectorID, stationID, detectorName, i, x3, y3, z0,
				detectorID, stationID, detectorName, i, x4, y4, z0);
			j++;
		  }
		  rowCount+=4;
		  // Submit if the INSERT gets too beefy
		  if(rowCount>=5000){
			
			printf("%s\n",qryString);
			if ( mysql_query(conn, qryString))
			{
				printf("Corner INSERT Query Error: %s\n", mysql_error(conn));
				return 1;
			}
			sprintf(qryString,"");
			j = 0;
			rowCount = 0;
		  }
	  }
	  
	  if(rowCount>0){
		
		if ( mysql_query(conn, qryString))
		{
			printf("Corner INSERT Query Error: %s\n", mysql_error(conn));
			return 1;
		}
		sprintf(qryString,"");
		j = 0;
	  }
  }
  
  mysql_free_result(res);

  return 0;
}

int match_wires(MYSQL *conn, char * geo, char * schema){

  int hodoDetectorID, wireDetectorID;
  int hodoStationID, wireStationID;
  char *hodoDetectorName, *wireDetectorName;
  double hx, hy, hz;
  int hodoElementID;
  double wireSpacing;
  int numElements;
  double chamberHeight, chamberWidth;
  double x0, y0, z0;
  double angleFromVert;
  double costh, sinth;
  double xOffset;
  double bx, by;
  long int wireElementID;
  const float pi = acos(-1);
  
  char qryString[1000000];
  MYSQL_RES *res;
  MYSQL_ROW row;
  int num_cols, num_rows, i, rowCount;

  // Create table that will match a wire for each chamber to each corner coordinate
  sprintf(qryString, "CREATE TEMPORARY TABLE IF NOT EXISTS HodoWire ("
		  "`stationID` tinyint(3) unsigned DEFAULT NULL,"
		  "`hodoDetectorID` tinyint(3) unsigned DEFAULT NULL,"
		  "`hodoDetectorName` varchar(6) DEFAULT NULL,"
		  "`hodoElementID` smallint(5) DEFAULT NULL,"
		  "`wireDetectorID` tinyint(3) unsigned DEFAULT NULL,"
		  "`wireDetectorName` varchar(6) DEFAULT NULL,"
		  "`wireElementID` smallint(5) DEFAULT NULL, "
		  "`numWire` smallint(5) DEFAULT NULL)");
  if ( mysql_query(conn, qryString))
  {
	  printf("HodoWire CREATE Query Error: %s\n", mysql_error(conn));
	  return 1;
  }
  
  // Join each corner with each chamber that it shares a station with
  sprintf(qryString,"SELECT c.*, d.detectorID, d.detectorName, d.spacing, d.numElements, "
	  "d.angleFromVert, d.xPrimeOffset, d.x0, d.y0, d.z0, d.height, d.width\n"
	  "FROM Corners c INNER JOIN "
	  "(SELECT * FROM %s.digiPlanes WHERE detectorName LIKE 'D%') d "
	  "ON c.stationID=d.stationID",geo);
  if ( mysql_query(conn, qryString))
  {
	  printf("Corner-wire matching Query Error: %s\n", mysql_error(conn));
	  return 1;
  }
  
  i = 0;
  rowCount = 0;
  res = mysql_store_result(conn);
  num_cols = mysql_num_fields(res);
  num_rows = mysql_num_rows(res);
  while (row = mysql_fetch_row(res)){
	  // Each row corresponds to a single hodoscope plane
	  hodoDetectorID = atoi(row[0]);
	  hodoStationID = atoi(row[1]);
	  hodoDetectorName = row[2];
	  hodoElementID = atoi(row[3]);
	  hx = atof(row[4]);
	  hy = atof(row[5]);
	  hz = atof(row[6]);
	  wireDetectorID = atoi(row[7]);
	  wireDetectorName = row[8];
	  wireSpacing = atof(row[9]);
	  numElements = atoi(row[10]);
	  angleFromVert = atof(row[11]);
	  sinth = sin(pi/180.0*angleFromVert);
	  costh = cos(pi/180.0*angleFromVert);
	  xOffset = atof(row[12]);
	  x0 = atof(row[13]);
	  y0 = atof(row[14]);
	  z0 = atof(row[15]);
	  chamberHeight = atof(row[16]);
	  chamberWidth = atof(row[17]);

	  // If a paddle corner is outside the boundary of a chamber, move the corner to the boundary
	  if(hx>(x0+0.5*chamberWidth))		hx = (x0+0.5*chamberWidth);
	  else if (hx<(x0-0.5*chamberWidth)) 	hx = (x0-0.5*chamberWidth);
	  if(hy>(y0+0.5*chamberHeight))		hy = (y0+0.5*chamberHeight);
	  else if (hy<(y0-0.5*chamberHeight)) 	hy = (y0-0.5*chamberHeight);

	  // Calculate the wire number for a given paddle corner
	  // 	NOTE NOTE NOTE! This is not adjusting for any z-displacement!
	  //bx = hx + wireSpacing*(numElements+1)*0.5*costh - xOffset*costh;
	  //by = hy + wireSpacing*(numElements+1)*0.5*sinth - xOffset*sinth;
	  //wireElementID = round((bx*costh-by*sinth)/wireSpacing);
	  wireElementID = round((hx/wireSpacing)*costh-(hy/wireSpacing)*sinth+(numElements+1)/2-xOffset/wireSpacing);
	  if (wireElementID<0) wireElementID = 1;
	  if (wireElementID>numElements) wireElementID = numElements;

	  // The chambers can (and usually are) smaller than the hodo planes. In these cases,
	  // 	the wire number may be negative, or in excess of the actual number of wires.
	  //	Exclude these cases and keep only corners that have real wire numbers.
	  if(wireElementID>0 && wireElementID<=numElements){
		if ( i == 0 ) {
			sprintf(qryString,"INSERT INTO HodoWire VALUES (%i, %i, \"%s\", %i, %i, \"%s\", %i, %i)",
				hodoStationID, hodoDetectorID, hodoDetectorName, hodoElementID, wireDetectorID, wireDetectorName, wireElementID, numElements);
			i++;
		} else {
			sprintf(qryString,"%s, (%i, %i, \"%s\", %i, %i, \"%s\", %i, %i)", qryString,
				hodoStationID, hodoDetectorID, hodoDetectorName, hodoElementID, wireDetectorID, wireDetectorName, wireElementID, numElements);
		}
	  }
	  rowCount++;
		  // Submit if the INSERT gets too beefy
	  if(rowCount>=5000){
		if ( mysql_query(conn, qryString))
		{
			printf("HodoWire INSERT Query Error: %s\n", mysql_error(conn));
			return 1;
		}
		i = 0;
		rowCount = 0;
		sprintf(qryString,"");
	  }
  }
	  
  if(rowCount>0){
	if ( mysql_query(conn, qryString))
	{
		printf("HodoWire INSERT Query Error: %s\n", mysql_error(conn));
		return 1;
	}
	sprintf(qryString,"");
	i = 0;
  }

  mysql_free_result(res);
  
  return 0;
}


int make_lookup(MYSQL *conn, char * schema, char * table){

  
  int hodoDetectorID, wireDetectorID;
  int stationID;
  char *hodoDetectorName, *wireDetectorName;
  int hodoElementID;
  int numElements;
  int maxWire, minWire;
  
  char qryString[1000000];
  MYSQL_RES *res;
  MYSQL_ROW row;
  int num_cols, num_rows, i, j, rowCount;

  // Create the final lookup table
  sprintf(qryString, "CREATE TABLE IF NOT EXISTS %s ("
		  "`stationID` tinyint(3) unsigned DEFAULT NULL,"
		  "`hodoDetectorID` tinyint(3) unsigned DEFAULT NULL,"
		  "`hodoDetectorName` varchar(6) DEFAULT NULL,"
		  "`hodoElementID` smallint(5) DEFAULT NULL,"
		  "`wireDetectorID` tinyint(3) unsigned DEFAULT NULL,"
		  "`wireDetectorName` varchar(6) DEFAULT NULL,"
		  "`wireElementID` smallint(5) DEFAULT NULL,"
		  "INDEX USING BTREE (stationID),"
		  "INDEX USING BTREE (hodoDetectorName, hodoElementID),"
		  "INDEX USING BTREE (wireDetectorName, wireElementID))", table);
  if ( mysql_query(conn, qryString))
  {
	  printf("%s CREATE Query Error: %s\n", table, mysql_error(conn));
	  return 1;
  }
  
  // Get teh max and min wires for each corner of each paddle
  sprintf(qryString,"SELECT stationID, hodoDetectorID, hodoDetectorName, hodoElementID, wireDetectorID, "
	  "wireDetectorName, MIN(wireElementID) AS `minWire`, MAX(wireElementID) AS `maxWire`, numWire "
	  "FROM HodoWire GROUP BY stationID, hodoDetectorID, hodoDetectorName, hodoElementID, wireDetectorID, wireDetectorName");
  if ( mysql_query(conn, qryString))
  {
	  printf("Max/MinWire Query Error: %s\n", mysql_error(conn));
	  return 1;
  }
  
  i = 0;
  rowCount = 0;
  res = mysql_store_result(conn);
  num_cols = mysql_num_fields(res);
  num_rows = mysql_num_rows(res);
  while (row = mysql_fetch_row(res)){
	  // Each row corresponds to a single hodoscope paddle
	  stationID = atoi(row[0]);
	  hodoDetectorID = atoi(row[1]);
	  hodoDetectorName = row[2];
	  hodoElementID = atoi(row[3]);
	  wireDetectorID = atoi(row[4]);
	  wireDetectorName = row[5];
	  minWire = atoi(row[6]);
	  maxWire = atoi(row[7]);
	  numElements = atoi(row[8]);

	  // If there's only one hodo corner covering a chamber, the min and max will be the same
	  // 	in this case, see which end the wires on and adjust either the minWire to 1 or the 
	  // 	maxWire to the max wire number.
	  if ( minWire == maxWire ){
		if (minWire < (numElements/2)) minWire = 1;
	  	else maxWire = numElements;
	  }

	  for (j=minWire;j<=maxWire;j++){
		if ( i == 0 ) {
			sprintf(qryString,"INSERT INTO %s VALUES (%i, %i, \"%s\", %i, %i, \"%s\", %i)", table, 
				stationID, hodoDetectorID, hodoDetectorName, hodoElementID, wireDetectorID, wireDetectorName, j);
			i++;
		} else {
			sprintf(qryString,"%s, (%i, %i, \"%s\", %i, %i, \"%s\", %i)", qryString,
				stationID, hodoDetectorID, hodoDetectorName, hodoElementID, wireDetectorID, wireDetectorName, j);
		}
		rowCount++;
		if (rowCount==5000){
			if ( mysql_query(conn, qryString) )
			{
				printf("%s INSERT Error: %s\n", table, mysql_error(conn));
				return 1;
			}
			rowCount=0;
			i = 0;
		}
	  }

  }

  if (rowCount>0){
	if ( mysql_query(conn, qryString) )
	{
		printf("%s INSERT Error: %s\n", table, mysql_error(conn));
		return 1;
	}
  }

  mysql_free_result(res);

  return 0;

}
