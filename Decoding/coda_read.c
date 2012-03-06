/* ///////////////////////////////////////////////////////////////////////////
//=======================================================================//
//
//	CODA to MySQL Decoder - E906/SeaQuest
//
//	Creator: Bryan Dannowitz
//
//	Current Decoding Capabilities:
//	=============================
//	- Prestart, Go Event, and End Event CODA Events
//	- Start-of-Run Descriptor Event
//	- Old TDC format 
//	- Non-zero suppressed TDC format
//	- v1495 TDC format
//	- Latch format
//	- Slow Control Physics Events
//	- Begin Spill Event
//	- End Spill Event
//
//	Example Decoding Command Use:
//	=============================
//	Syntax:
//	decode -f fileName -m {1: online, 0: offline} -h SQLhostName
//	-u userName -p password -d schemaOutput
//
//	For decoding a file while its still being written:
//
//	./decode -f /full/path/and/filename.dat -m 1 -d test_output -s 10000
//
//	For decoding a finished file, sending to a host other than default:
//
//	./decode -f /full/path/and/filename.dat -m 0 -h host -u user 
//		 -p password -d test_output
//

//
//==========================================================================//
/////////////////////////////////////////////////////////////////////////// */

#include "coda_read.h"

//====================================================
// BEGIN MAIN DECODER CODE
//====================================================

int main(int argc,char* argv[]){

  // This is the array that holds CODA event information
  unsigned int physicsEvent[100000];
  
  // MySQL variables
  MYSQL *conn;
  database = "";
  const int PORT = 0;
  char *UNIX_SOCKET = NULL;
  const int CLIENT_FLAG = 0;
  unsigned int* enableLoadDataInfile;

  // File handling variables
  FILE *fp;
  long int fileSize;

  // Benchmarking variables
  clock_t cpuStart, cpuEnd;
  time_t timeStart, timeEnd;
  double cpuTotal; 

  case1 = 0; case2 = 0; case3 = 0; case4 = 0; case5 = 0; case6 = 0;

  // Some other variables used
  int i, j, physEvCount, status;
  unsigned int eventIdentifier;

  // Initialize some important values
  physEvCount = 0;
  codaEventNum = 0;
  tdcCount = 0;
  v1495Count = 0;
  latchCount = 0;
  force = 0;
  spillID = 0;
  slowControlCount = 0;
  fileName = "";
  sprintf(end_file_name, "");

  // Handle the command-line options
  if (initialize(argc, argv)) return 1;

  // Initialize MySQL database connection
  conn = mysql_init(NULL);

  // Must set this option for the C API to allow loading data from
  // 	local file (like we do for slow control events)
  mysql_options(conn, MYSQL_OPT_LOCAL_INFILE, NULL);

  // Connect to the MySQL database
  if (!mysql_real_connect(conn, server, user, password, database, 
	PORT, UNIX_SOCKET, CLIENT_FLAG)) {
       	printf("Database Connection ERROR: %s\n\n", mysql_error(conn));
       	return 1;
  } else {
  	printf("Database connection: Success\n\n");
  }

  // Create database and tables if they are not currently there.
  if (createSQL(conn, schema)){
	printf("SQL Table Creation ERROR\n");
	exit(1);
  }


  // Initialize event array
  for(i=0;i<100000;i++){
    physicsEvent[i] = 0;
  }

  // Check if specified file exists
  if (file_exists(fileName)){
	// Get file size
	fp = fopen(fileName, "r");
	fseek(fp, 0L, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	// evOpen will return an error if the file is less than
	//	a certain size, so wait until the file is big enough.
	//	...if the file *is* currently being written
	while (fileSize < 32768 && online == ON){
		sleep(5);
		// Get file size again
		//fp = fopen(fileName, "r");
		fseek(fp, 0L, SEEK_END);
		fileSize = ftell(fp);
 		fseek(fp, 0L, SEEK_SET);
	}

        printf("Loading... \"%s\"\n", fileName);
  	status = open_coda_file(fileName);

        if (status == SUCCESS){
		printf("Opening CODA File Status: OK!\n\n");
	}
        else { 
		printf("Problem Opening CODA File\nStatus Code: %x\n\nExiting!\n\n", status); 
		return 1; 
	}

  // If the file does NOT exist:
  } else {
	printf("File \"%s\" does not exist.\nExiting...\n", fileName);
	return 1;
  }

  cpuStart = clock();
  timeStart = time(NULL);
  // Read the first event
  spillID = 0;
  status = read_coda_file(physicsEvent,100000);
  eventIdentifier = physicsEvent[1];

  for(i=0; eventIdentifier != EXIT_CASE; i++){ 
     // If an error is hit while decoding an offline file, something's wrong
     if (status != SUCCESS){
	//printf("End file name: %s\nExitst? %i\n", end_file_name, file_exists(end_file_name));
	if (file_exists(end_file_name)){

	    // If there are still TDC data values waiting to be sent to the server, send them
	    if (tdcCount > 0) {
	    	make_tdc_query(conn);
	    }
	    if (latchCount > 0) {
		send_final_latch(conn);
	    }
	    if (scalerCount > 0) {
		send_final_scaler(conn);
	    }
	    if (v1495Count > 0) {
		make_v1495_query(conn);
	    }
   

	    printf("End File Found: %s\nExiting...\n\n", end_file_name);
	    
	    //mysql_stmt_close(runStmt);
	    //mysql_stmt_close(spillStmt);	
	    //mysql_stmt_close(codaEvStmt);
// 	    mysql_stmt_close(hitStmt);	
// 	    mysql_stmt_close(v1495Stmt);
	    mysql_close(conn);
	    close_coda_file();

	    cpuEnd = clock();
	    timeEnd = time(NULL);

	    cpuTotal = (double)( cpuEnd - cpuStart ) / (double)CLOCKS_PER_SEC;

	    printf("CPU Time: %fs\n", cpuTotal);
	    printf("Real Time: %f\n", (timeEnd - timeStart));
	    printf("%i Total CODA Events Decoded\n%i Physics Events Decoded\n\n", i, physEvCount);
	    printf("Average Rate: %f Events/s\n\n", ((double)i)/((double)( timeEnd - timeStart )));

	    return 0;

	} else {
	    // If an error is hit while decoding an online file, it likely just means
     	    // 	you need to wait for more events to be written.  Wait and try again.
	    if ( online == 0 ) { printf("ERROR: CODA Read Error in Offline Mode. No END file. Exiting...\n"); return 1; }
	    while ( status != SUCCESS ){
 		status = retry(fp, i, physicsEvent);
	    }
	}
    }

     // The last 4 hex digits will be 0x01cc for Prestart, Go Event, 
     //		and End Event, and 0x10cc for Physics Events
     switch (get_hex_bits(eventIdentifier,3,4)) {
	case CODA_EVENT:
		switch (eventIdentifier) {
		   case PRESTART_EVENT:
			// printf("Prestart Event\n");
			if ( prestartSQL(conn, physicsEvent) ) return 1;
			break;
		   case GO_EVENT:
			// printf("Go Event\n");
			goEventSQL(conn, physicsEvent); 
			break;
		   case END_EVENT:
			if ( endEventSQL(conn, physicsEvent) ) return 1;
			eventIdentifier=EXIT_CASE;
			printf("End Event Processed\n");
			break;
		   default:
			printf("Uncovered Case: %lx\n", eventIdentifier);
			if (tdcCount > 0) {
   				send_final_tdc(conn);
   			}
			if (latchCount > 0) {
   				send_final_latch(conn);
   			}
			return 1;
			break;
		}
		break;

	case PHYSICS_EVENT:
	       	if((codaEventNum % 10000 == 0) && (codaEventNum != 0)) printf("%i Events\n", codaEventNum);

 		if (get_hex_bits(physicsEvent[1],7,4)==140){
			// Start of run descriptor
			if ( eventRunDescriptorSQL(conn, physicsEvent) ) {
				printf("Beginning of Run Descriptor Event Processing Error\n");
				return 1;
			}
			break;
		} else {
			if ( physicsEvent[4]!=1 ) {
				if (eventSQL(conn, physicsEvent) ) return 1;
				physEvCount++;
			}
			break;
		}
	case 0:
		// Special case which requires waiting and retrying
		status = retry(fp, (i-1), physicsEvent);
		i--;
		break;
	default:
		// If no match to any given case, print it and exit.
		printf("Uncovered Case: %lx\n", eventIdentifier);
		return 1;
		break;
     }

     // Clear the buffer, read the next event, and go back through the loop.
     if (eventIdentifier != EXIT_CASE) {
	for(j=0;j<100000;j++) physicsEvent[j] = 0;
        status = read_coda_file(physicsEvent,100000);
        eventIdentifier = physicsEvent[1];
     }
   }

   // If there are still TDC data values waiting to be sent to the server, send them
   if (tdcCount > 0) {
   	make_tdc_query(conn);
   }
   // If there is still latch data waiting to be sent to the server, send it
   if (latchCount > 0) {
   	send_final_latch(conn);
   }
   if (scalerCount > 0) {
   	send_final_scaler(conn);
   }
   
   if (v1495Count > 0) {
	make_v1495_query(conn);
   }
   
   //mysql_stmt_close(runStmt);
   //mysql_stmt_close(spillStmt);
   //mysql_stmt_close(codaEvStmt);
//    mysql_stmt_close(hitStmt);
//    mysql_stmt_close(v1495Stmt);
   mysql_close(conn);
   close_coda_file();

   cpuEnd = clock();
   timeEnd = time(NULL);
   cpuTotal = (double)( cpuEnd - cpuStart ) / (double)CLOCKS_PER_SEC;

   printf("SlowControl Count: %i\n", slowControlCount);
   printf("CPU Time: %fs\n", cpuTotal);
   printf("Real Time: %f\n", (timeEnd - timeStart));
   printf("%i Total CODA Events Decoded\n%i Physics Events Decoded\n\n", i, physEvCount);
   printf("Average Rate: %f Events/s\n\n", ((double)i)/((double)( timeEnd - timeStart )));

   return 0;
};

// ==============================================================
// END MAIN DECODER CODE
// ==============================================================

// ===============================================================
// BEGIN FUNCTION DEFINITIONS
// ===============================================================

int initialize(int argc,char* argv[]){
// ================================================================
//
// This function will handle any and all command line options
//
// Returns:	0 on success
//		1 on bad command line entry
//

  int required[2], check[2], i;

  for (i=0;i<2;i++) { required[i]=0; check[i]=1; }
  
  // Set default values
  server = "e906-gat2.fnal.gov";
  user = "production";
  password = "***REMOVED***";

  for (i=1;i<argc;i++) 
  {
	// Filename
	if (!strcmp(argv[i],"-f")){
	   fileName = argv[i+1];
	   required[0]=1;
	}
	// Forced-overwrite option
	else if (!strcmp(argv[i],"--force")){
	   force = 1;
	}
 	// Online/Offline Flag
	else if (!strcmp(argv[i],"-m")){
	   online = atoi(argv[i+1]);
	   required[1]=1;
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
	// MySQL Schema
	else if (!strcmp(argv[i],"-d")){
	   schema = argv[i+1];
	}
	else if (!strcmp(argv[i],"-s")){
	   sampling = atoi(argv[i+1]);
	}
	// Print Help
	else if (!strcmp(argv[i],"help")){
	   printf("\n\n-f: file to decode -- Required\n"
		"-m: mode {0: offline, 1: online} -- Required\n"
		"-h: server host (optional: default 'e906-gat2.fnal.gov')\n"
		"-u: user (optional: default 'seaguest')\n"
		"-p: password (optional: default password)\n"
		"-d: schema (optional: default 'test_coda')\n"
		"-s: sampling mode with X events sampled\n"
		"--force: force overwrite any pre-existing run data\n\n");	
	}
  }

  // If every int in the array is a 1, then all necessary information has been entered
  for (i=0;i<2;i++){
	if (required[i] != check[i]) {
  	// If not, print help info and exit
  	printf("Some required options were not included. Exiting!\n");
    	printf("\n\n-f: file to decode -- Required\n"
		"-m: mode {0: offline, 1: online} -- Required\n"
		"-h: server host (optional: default 'e906-gat2.fnal.gov')\n"
		"-u: user (optional: default 'seaguest')\n"
		"-p: password (optional: default password)\n"
		"-d: schema (optional: default 'test_coda')\n"
		"--force: force overwrite any pre-existing run data\n\n");
   	return 1;
  	}
  }
  return 0;
}

int retry(FILE *fp, int codaEventCount, unsigned int physicsEvent[100000]){
// ================================================================
//
// This function is used in the case that a file is being decoded
//	while it's still being written.  When/if an EOF is reached
//	before an End Event is processed, evRead will have an aneurism.
//	This function handles this problem, which
//	closes the CODA file, waits a couple seconds (specified by the
//	the WAIT_TIME constant in the header), opens the file,
//	and loops evRead enough times to get to the desired event.
//
// Returns:	the status of the last evRead
//		0 (SUCCESS) on success
//		!0 for a myriad of other errors, including
//		-1 (EOF) (ERROR) on error

  int status, k;

  // File handling variables
  //FILE *fp;
  long int fileSize;
  long int fileSize2;

  // Close the CODA file
  //close_coda_file();	
  
  // Get file size
  //fp = fopen(fileName, "r");
  fseek(fp, 0L, SEEK_END);
  fileSize = ftell(fp);
  fseek(fp, 0L, SEEK_SET);
  //fclose(fp);
  //printf("Size: %i\n",fileSize);
  
  // Wait 5 seconds
  sleep(WAIT_TIME);

  // Get more recent file size
  //fp = fopen(fileName, "r");
  fseek(fp, 0L, SEEK_END);
  fileSize2 = ftell(fp);
  fseek(fp, 0L, SEEK_SET);
  //fclose(fp);

  //printf("Size: %i\n",fileSize2);

  // If the file grows by at least a single block size, get the next new event
  if ( fileSize2 > (fileSize + 32768) ) {

	//printf("NEW DATA!\n");

	close_coda_file();
	// Open up the CODA file again
	status = open_coda_file(fileName);
	if (status != SUCCESS) {
	  printf("Open File Status: %x\n", status);
	  return status;
	}
	
	// If closed and opened successfully, loop through to the desired event.
	for (k=0 ; k <= codaEventCount ; k++) 
	{ status = read_coda_file(physicsEvent,100000); }

  } else {
	status = ERROR;
  }

  //printf("Status: %.8x\n", status);

  return status;

}

int createSQL(MYSQL *conn, char *schema){
// ================================================================
//
//
// This function creates the schema and tables required to dump the decoded data
//
// Returns:	0 on success
//		1 on error

	char qryString[10000];
	int i;
	int k;

	char runInsert[1000];
	char codaEvInsert[1000];
	char spillInsert[1000];
	char v1495Insert[100000];
	char hitInsert[100000];

	
	
	sprintf(runInsert, "INSERT INTO Run (runID, runType, runTime) VALUES (?,?,?)");
	sprintf(spillInsert,"INSERT INTO Spill (spillID, runID, eventID, "
		"spillType, rocID, vmeTime) VALUES (?,?,?,?,?,?)");
	sprintf(codaEvInsert,"INSERT INTO Event (eventID, runID, spillID, "
		"eventType, triggerBits, dataQuality) VALUES (?,?,?,?,?,?)");
	sprintf(hitInsert,"INSERT INTO tempTDC (runID, spillID, eventID, rocID,"
		"boardID, channelID, tdcTime, signalWidth, vmeTime) "
		"VALUES (?,?,?,?,?,?,?,?,?)");
	/*
	for(i=1;i<max_tdc_rows;i++){
		sprintf(hitInsert,"%s, (?,?,?,?,?,?,?,?,?)",hitInsert);
	}
	sprintf(v1495Insert,"INSERT INTO tempv1495 (runID, spillID, eventID, rocID,"
		"boardID, channelID, tdcTime, vmeTime) "
		"VALUES (?,?,?,?,?,?,?,?)");
	for(i=1;i<max_v1495_rows;i++){
		sprintf(v1495Insert,"%s, (?,?,?,?,?,?,?,?)",v1495Insert);
	}
	*/
	
	
	memset(runBind, 0, sizeof(runBind));
	memset(spillBind, 0, sizeof(spillBind));
	memset(codaEvBind, 0, sizeof(codaEvBind));
	//memset(hitBind, 0, sizeof(hitBind));
	//memset(v1495Bind, 0, sizeof(v1495Bind));

	for (i=0;i<3;i++) runBind[i].buffer_type= MYSQL_TYPE_LONG;
	runBind[0].buffer= (char *)&runNum;
	runBind[1].buffer= (char *)&runType;
	runBind[2].buffer= (char *)&runTime;
	for (i=0;i<3;i++) runBind[i].is_null= 0;
	for (i=0;i<3;i++) runBind[i].length= 0; 

	for (i=0;i<6;i++) spillBind[i].buffer_type= MYSQL_TYPE_LONG;
	spillBind[0].buffer= (char *)&spillID;
	spillBind[1].buffer= (char *)&runNum;
	spillBind[2].buffer= (char *)&codaEventNum;
	spillBind[3].buffer= (char *)&eventType;
	spillBind[4].buffer= (char *)&ROCID;
	spillBind[5].buffer= (char *)&spillVmeTime;
	for (i=0;i<6;i++) spillBind[i].is_null= 0;
	for (i=0;i<6;i++) spillBind[i].length= 0; 

	for (i=0;i<6;i++) codaEvBind[i].buffer_type= MYSQL_TYPE_LONG;
	codaEvBind[0].buffer= (char *)&codaEventNum;
	codaEvBind[1].buffer= (char *)&runNum;
	codaEvBind[2].buffer= (char *)&spillID;
	codaEvBind[3].buffer= (char *)&eventType;
	codaEvBind[4].buffer= (char *)&triggerBits;
	codaEvBind[5].buffer= (char *)&dataQuality;
	for (i=0;i<6;i++) codaEvBind[i].is_null= 0;
	for (i=0;i<6;i++) codaEvBind[i].length= 0;
	/*
	for(k=0;k<max_tdc_rows;k++){
		for (i=0;i<9;i++) hitBind[(k*9+i)].buffer_type= MYSQL_TYPE_LONG;
		hitBind[(k*9+0)].buffer= (char *)&tdcRunID[k];
		hitBind[(k*9+1)].buffer= (char *)&tdcSpillID[k];
		hitBind[(k*9+2)].buffer= (char *)&tdcCodaID[k];
		hitBind[(k*9+3)].buffer= (char *)&tdcROC[k];
		hitBind[(k*9+4)].buffer= (char *)&tdcBoardID[k];
		hitBind[(k*9+5)].buffer= (char *)&tdcChannelID[k];
		hitBind[(k*9+6)].buffer= (char *)&tdcStopTime[k];
		hitBind[(k*9+7)].buffer= (char *)&tdcSignalWidth[k];
		hitBind[(k*9+8)].buffer= (char *)&tdcVmeTime[k];
		for (i=0;i<9;i++) hitBind[(k*9+i)].is_null= 0;
		for (i=0;i<9;i++) hitBind[(k*9+i)].length= 0;
	}

	for(k=0;k<max_v1495_rows;k++){
		for (i=0;i<8;i++) v1495Bind[(k*9+i)].buffer_type= MYSQL_TYPE_LONG;
		v1495Bind[(k*8+0)].buffer= (char *)&v1495RunID[k];
		v1495Bind[(k*8+1)].buffer= (char *)&v1495SpillID[k];
		v1495Bind[(k*8+2)].buffer= (char *)&v1495CodaID[k];
		v1495Bind[(k*8+3)].buffer= (char *)&v1495ROC[k];
		v1495Bind[(k*8+4)].buffer= (char *)&v1495BoardID[k];
		v1495Bind[(k*8+5)].buffer= (char *)&v1495ChannelID[k];
		v1495Bind[(k*8+6)].buffer= (char *)&v1495StopTime[k];
		v1495Bind[(k*8+7)].buffer= (char *)&v1495VmeTime[k];
		for (i=0;i<8;i++) v1495Bind[(k*8+i)].is_null= 0;
		for (i=0;i<8;i++) v1495Bind[(k*8+i)].length= 0;
	}
	*/
	sprintf(qryString, "CREATE DATABASE IF NOT EXISTS %s", schema);
	
	if( mysql_query(conn, qryString) )
    	{
		printf("Database creation error: %s\n", mysql_error(conn));
		return 1;
	}

	sprintf(qryString,"USE %s",schema);

	if( mysql_query(conn,qryString) )
	{
		printf("Database selection error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Run ("
		"`runID` INT NOT NULL PRIMARY KEY, `runType` INT NOT NULL, "
		"`runTime` DOUBLE NOT NULL, `runDescriptor` LONGTEXT)") )
	{
		printf("Run Table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	if ( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Spill ("
		"`spillID` INT NOT NULL, `runID` INT NOT NULL, "
		"`eventID` INT NOT NULL, `spillType` INT NOT NULL, "
		"`rocID` INT NOT NULL, `vmeTime` INT NOT NULL)") )
	{
		printf("Spill table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Event ("
		"`eventID` INT NOT NULL, `runID` INT NOT NULL, `spillID` INT NOT NULL, "
		"`eventType` INT NOT NULL, `triggerBits` INT NOT NULL, `dataQuality` INT NOT NULL, "
		"INDEX USING BTREE (eventID) )") )
    	{
		printf("Event table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Hit ("
		"`hitID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
		"`runID` INT UNSIGNED NOT NULL, `spillID` INT NOT NULL, "
		"`eventID` INT NOT NULL, `rocID` INT UNSIGNED NOT NULL, "
		"`boardID` INT NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, `detectorID` INT, "
		"`detectorName` CHAR(6), "
		"`elementID` INT UNSIGNED, `tdcTime` DOUBLE, `driftDistance` DOUBLE, `signalWidth` DOUBLE, "
		"`vmeTime` DOUBLE, INDEX USING BTREE (eventID), "
		"INDEX USING BTREE (detectorName), INDEX USING BTREE (elementID))") )
	{
		printf("Hit Table Creation Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS TriggerHit ("
		"`hitID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
		"`runID` INT UNSIGNED NOT NULL, `spillID` INT NOT NULL, "
		"`eventID` INT NOT NULL, `rocID` INT UNSIGNED NOT NULL, "
		"`boardID` INT UNSIGNED NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, `detectorID` INT, "
		"`detectorName` CHAR(6), "
		"`elementID` INT UNSIGNED, `tdcTime` DOUBLE, "
		"`vmeTime` DOUBLE, INDEX USING BTREE (eventID), "
		"INDEX USING BTREE (detectorName), INDEX USING BTREE (elementID))") )
	{
		printf("TriggerHit Table Creation Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Scaler ("
		"`scalerID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
		"`runID` INT NOT NULL, `spillID` INT NOT NULL, `eventID` INT NOT NULL, "
		"`rocID` TINYINT UNSIGNED NOT NULL, `boardID` INT, "
		"`channelID` TINYINT UNSIGNED NOT NULL, `value` INT NOT NULL, "
		"`detectorID` INT, `detectorName` CHAR(6), `elementID` INT UNSIGNED, "
		"`vmeTime` INT NOT NULL, "
		"INDEX USING BTREE (detectorName), INDEX USING BTREE (elementID), "
		"INDEX USING BTREE (eventID))") )
	{
		printf("Scaler Table Creation Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TEMPORARY TABLE IF NOT EXISTS tempScaler ("
		"`scalerID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
		"`runID` INT NOT NULL, `spillID` INT NOT NULL, `eventID` INT NOT NULL, "
		"`rocID` INT NOT NULL, `boardID` INT, "
		"`channelID` TINYINT UNSIGNED NOT NULL, `value` INT NOT NULL, "
		"`detectorID` INT, `detectorName` CHAR(6), "
		"`elementID` INT UNSIGNED, `vmeTime` INT NOT NULL)") )
	{
		printf("Create tempScaler table error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TEMPORARY TABLE IF NOT EXISTS tempTDC ("
		"`runID` INT NOT NULL, `spillID` INT NOT NULL, `eventID` INT NOT NULL, "
		"`rocID` INT NOT NULL, `boardID` INT NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, `detectorID` INT, `detectorName` CHAR(6), "
		"`elementID` INT UNSIGNED, `tdcTime` DOUBLE NOT NULL, `driftDistance` DOUBLE, `signalWidth` DOUBLE, "
		"`vmeTime` DOUBLE NOT NULL, INDEX USING BTREE (rocID, boardID))") )
	{
		printf("Create tempTDC error: %s\n", mysql_error(conn));
		return 1;
	}
	
	if( mysql_query(conn, "CREATE TEMPORARY TABLE IF NOT EXISTS tempv1495 ("
		"`runID` INT NOT NULL, `spillID` INT NOT NULL, `eventID` INT NOT NULL, "
		"`rocID` INT NOT NULL, `boardID` INT NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, `detectorID` INT, `detectorName` CHAR(6), "
		"`elementID` INT UNSIGNED, `tdcTime` DOUBLE NOT NULL, `vmeTime` DOUBLE NOT NULL)") )
	{
		printf("Create tempv1495 table error: %s\n", mysql_error(conn));
		return 1;
	}

	if ( mysql_query(conn, "CREATE TEMPORARY TABLE IF NOT EXISTS tempLatch ("
		"`runID` INT NOT NULL, `spillID` INT NOT NULL, `eventID` INT NOT NULL, "
		"`vmeTime` INT NOT NULL, "
		"`rocID` INT NOT NULL, `boardID` INT NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, `detectorID` INT, `detectorName` CHAR(6), "
		"`elementID` INT UNSIGNED)") )
	{
		printf("Create tempLatch table error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS SlowControl("
		"`time` DATETIME NOT NULL, `name` VARCHAR(128) NOT NULL, "
		"`value` DOUBLE NOT NULL, `type` VARCHAR(128))"))
	{
		printf("SlowControl table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	sprintf(qryString, "CREATE TABLE IF NOT EXISTS `HV` (\n"
  		"`crateID` INT(11) NOT NULL, \n"
		"`channelID` int(11), \n"
		"`setVoltage` INT,\n"
		"`voltage` INT\n"
		") ENGINE=MyISAM DEFAULT CHARSET=latin1");
	
	if( mysql_query(conn, qryString) )
	{
		printf("Create HV Table Error: %s\n", mysql_error(conn));
		return 1;
	}

	runStmt = mysql_stmt_init(conn);
	spillStmt = mysql_stmt_init(conn);
	codaEvStmt = mysql_stmt_init(conn);
	//hitStmt = mysql_stmt_init(conn);
	//v1495Stmt = mysql_stmt_init(conn);

	mysql_stmt_prepare(runStmt, runInsert, (unsigned int)strlen(runInsert));
	mysql_stmt_prepare(codaEvStmt, codaEvInsert, (unsigned int)strlen(codaEvInsert));
	mysql_stmt_prepare(spillStmt, spillInsert, (unsigned int)strlen(spillInsert));
	//mysql_stmt_prepare(hitStmt, hitInsert, (unsigned int)strlen(hitInsert));
	//mysql_stmt_prepare(v1495Stmt, v1495Insert, (unsigned int)strlen(v1495Insert));

	return 0;
}

int runExists(MYSQL *conn, int runNumber){
	
	MYSQL_RES *res;
	MYSQL_ROW row;
	int exists = 0;
	char qryString[10000];

	sprintf(qryString, "SELECT COUNT(*) FROM Run WHERE runID=%i", runNumber);
	
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

	mysql_free_result(res);

	return exists;

}

int prestartSQL(MYSQL *conn, unsigned int physicsEvent[100000]){
// ================================================================
//
//
// This function hadles the 'Go Event' CODA Events, storing its
//	values appropriately.
//
// Returns: 0 on success
//	    1 on MySQL error
//	   

	int evLength = 0;
	int prestartCode = 0;
	char overwrite_flag[32];
	char qryString[10000];
	runTime = 0;
	runNum = 0;
	runType = 0;

	evLength = physicsEvent[0];
	prestartCode = physicsEvent[1];
	runTime = physicsEvent[2];
	runNum = physicsEvent[3];
	runType = physicsEvent[4];

	// Check if this run already exists in the data
	if (runExists(conn, runNum)){
		if(force){
			printf(">> Clearing data regarding RunID = %i\n>> This may "
				"take a few moments...\n",runNum);
			sprintf(qryString,"DELETE FROM Run WHERE runID=%i", runNum);
			if(mysql_query(conn, qryString)){
				printf("%s\n", qryString);
				printf(">> Error clearing out run data: %s\n", mysql_error(conn));
				return 1;
			}
			sprintf(qryString,"DELETE FROM Event WHERE runID=%i", runNum);
			if(mysql_query(conn, qryString)){
				printf("%s\n", qryString);
				printf(">> Error clearing out run data: %s\n", mysql_error(conn));
				return 1;
			}
			sprintf(qryString,"DELETE FROM Hit WHERE runID=%i", runNum);
			if(mysql_query(conn, qryString)){
				printf("%s\n", qryString);
				printf(">> Error clearing out run data: %s\n", mysql_error(conn));
				return 1;
			}
			sprintf(qryString,"DELETE FROM Scaler WHERE runID=%i", runNum);
			if(mysql_query(conn, qryString)){
				printf("%s\n", qryString);
				printf(">> Error clearing out run data: %s\n", mysql_error(conn));
				return 1;
			}
			sprintf(qryString,"DELETE FROM Spill WHERE runID=%i", runNum);
			if(mysql_query(conn, qryString)){
				printf("%s\n", qryString);
				printf(">> Error clearing out run data: %s\n", mysql_error(conn));
				return 1;
			}
			printf(">> RunID = %i data has been cleared.\n>> Decoding...\n", runNum);
		} else {
			printf("WARNING: This run already exists here. Overwrite? (y/n)\n>> ");
			scanf("%s",&overwrite_flag);
			if ( !strcmp(overwrite_flag,"y") ){
				printf(">> Clearing data regarding RunID = %i\n>> This may "
					"take a few moments...\n",runNum);
				sprintf(qryString,"DELETE FROM Run WHERE runID=%i", runNum);
				if(mysql_query(conn, qryString)){
					printf("%s\n", qryString);
					printf(">> Error clearing out run data: %s\n", mysql_error(conn));
					return 1;
				}
				sprintf(qryString,"DELETE FROM Event WHERE runID=%i", runNum);
				if(mysql_query(conn, qryString)){
					printf("%s\n", qryString);
					printf(">> Error clearing out run data: %s\n", mysql_error(conn));
					return 1;
				}
				sprintf(qryString,"DELETE FROM Hit WHERE runID=%i", runNum);
				if(mysql_query(conn, qryString)){
					printf("%s\n", qryString);
					printf(">> Error clearing out run data: %s\n", mysql_error(conn));
					return 1;
				}
				sprintf(qryString,"DELETE FROM Scaler WHERE runID=%i", runNum);
				if(mysql_query(conn, qryString)){
					printf("%s\n", qryString);
					printf(">> Error clearing out run data: %s\n", mysql_error(conn));
					return 1;
				}
				sprintf(qryString,"DELETE FROM Spill WHERE runID=%i", runNum);
				if(mysql_query(conn, qryString)){
					printf("%s\n", qryString);
					printf(">> Error clearing out run data: %s\n", mysql_error(conn));
					return 1;
				}
				printf(">> RunID = %i data has been cleared.\n>> Decoding...\n", runNum);
			} else {
				printf("Exiting...\n\n");
				return 1;
			}
		}
  	}

	sprintf(end_file_name, "/data2/e906daq/coda/data/END/%i.end", runNum);

	mysql_stmt_bind_param(runStmt, runBind);
	
	if( mysql_stmt_execute(runStmt) ) {
		printf("Run Insert Error: %s\n",mysql_error(conn));
		return 1;
	}

	return 0;

}

int goEventSQL(MYSQL *conn, unsigned int physicsEvent[100000]){
// ================================================================
//
// This function hadles the 'Go Event' CODA Events, storing its
//	values appropriately.
//
// Returns: 0

	int evLength = 0;
	int goEventCode = 0;
	goEventTime = 0;
	numEventsInRunThusFar = 0;
	
	// evLength is the important field to read, as it tells us 
	// 	when to stop reading the codaEvents
	evLength = physicsEvent[0];
	goEventCode = physicsEvent[1];
	goEventTime = physicsEvent[2];
	// physicsEvent[3] is reserved as 0
	numEventsInRunThusFar = physicsEvent[4];

	return 0;
}

void showWarnings(MYSQL *conn){
	MYSQL_RES *res;
	MYSQL_ROW row;
	int num_cols, i;
	int num_warning;

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

	//free(sqlString);
}

int eventRunDescriptorSQL(MYSQL *conn, unsigned int physicsEvent[100000]){

	char text [10000]="";
	char remnant [10000]="";
	char descriptor [10000]="";
	char j;
	char qryString [5000]="";
	int value[4];
	int i, k, t;
	
	evLength = physicsEvent[0];

	// This puts the entirety of the ASCII event into one string
	for (i=4;i<(evLength+2);i++) {
		// The %.8lx ensures that no leading zeros are left off
		sprintf(text, "%s%.8lx", text, physicsEvent[i]);		
	}

	i = 0;
	
	// sscanf grabs the first two hex digits from the string until it gets 4 of em.
	// Since the values are given parsed and reversed, ("HELLO" -> "LLEHO")
	// 	This then reverses every four characters and writes them to string.
	while ( sscanf(text, "%2x%s", &value[i%4], remnant) == 2 )
	{
		strcpy(text, remnant);
		if((i%4)==3){
			sprintf(descriptor, "%s%c%c%c%c", descriptor, value[3], value[2], value[1], value[0]);
		}
		i++;
	}

	sprintf(qryString, "UPDATE Run SET runDescriptor = \"%s\" WHERE runID = %i", descriptor, runNum);	
	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}

	return 0;

}

int eventSlowSQL(MYSQL *conn, unsigned int physicsEvent[100000]){
// ================================================================
//
// This function takes the Slow Control Physics Event, which is purely
//	a blob of ASCII presented in the form of parsed and reversed
//	unsigned int's.  It slaps together the whole event into one
//	big string of hex values, parses it into 2-digit/1-char chunks, 
//	flips every group of four characters, and writes them to file.
//	After being written to file, they are loaded up to the MySQL server
//	via the LOAD DATA LOCAL INFILE funcion.
//
// Returns:	0 if successful
//		1 on error


	char text[10000];
	char *ptr = NULL;
	char *j = NULL;
	char outputFileName[10000];
	char qryString[100];
	int value[4];
	int i, num_cols, pid;
	FILE *fp;
	int PTR_LEN;
	
	slowControlCount++;

	PTR_LEN = evLength*8;

	ptr = (char*) malloc (PTR_LEN+1);
	j = (char*) malloc (PTR_LEN+1);
	memset(ptr,'\0',PTR_LEN);
	memset(j,'\0',PTR_LEN);

	pid = getpid();

	sprintf(outputFileName,".codaSlowControl%i.temp", pid);
	
	// This puts the entirety of the ASCII event into one string
	for (i=0;i<(evLength-2);i++) {
		// The %.8lx ensures that no leading zeros are left off
		sprintf(ptr, "%s%.8x", ptr, physicsEvent[i+4]);
	}
	ptr[PTR_LEN]='\0';
	
	// Open the temp file 
	if (file_exists(outputFileName)) remove(outputFileName);
	fp = fopen(outputFileName,"w");

	if (fp == NULL) {
  		fprintf(stderr, "Can't open output file %s!\n",
          		outputFileName);
		// sprintf(text, "");
  		exit(1);
	}
	
	//ptr = text;
	i = 0;

	// sscanf grabs the first two hex digits from the string until it gets 4 of em.
	// Since the values are given parsed and reversed, ("HELLO" -> "LLEHO")
	// 	This then reverses every four characters and writes them to file.
	while ( sscanf(ptr, "%2x%s", &value[i%4], j) == 2 )
		{
		strcpy(ptr,j);
		if((i%4)==3){
			fprintf(fp, "%c%c%c%c", value[3], value[2], 
				value[1], value[0]);
			//printf("%c%c%c%c", value[3], value[2], 
			//	value[1], value[0]);
		}
		i++;
	}
	ptr[PTR_LEN]='\0';
	// File MUST be closed before it can be loaded into MySQL

	fclose(fp);
	
	// Assemble the MySQL query to load the file into the server.
	// 	The LOCAL means the file is on the local machine
	//	The IGNORE 2 LINES skips the date and list of field names and goes
	//		right to the values.
	sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' INTO TABLE SlowControl "
			"FIELDS TERMINATED BY ' '", outputFileName);
	
	// Submit the query to the server	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_warning_count(conn) > 0 )
	{
		printf("Warnings have occured from the slow control event upload:\n");
		showWarnings(conn);
	}
	
	remove(outputFileName);
	free(ptr);
	free(j);

	return 0;
}

int eventTDCSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
// ================================================================
// This edifice of computer programming handles the TDC Physics Events
// 	All appropriate values are stored, and pertinent values are
//	sent to the MySQL server.  I will attempt to display the TDC
//	format visually here in gorgeous ASCII art:
//	   _____________________________________________________________
//  Start: |P|1|1|1|-|-|-|-| 24-Bit Event Number			|
//	   _____________________________________________________________
//  Data:  |P|0|6-bit Chan#| 24-Bit Data (Stop Time -Time of Rising Edge|
//
//		....
//	   _____________________________________________________________
//StopCnt1:|P|1|0|0|-|-|-|-|-|-|-|-|-|-|-|-| 16 LSBs of Stop Count Time |
//	   _____________________________________________________________
//StopCnt2:|P|1|0|1| 12-Bit board address  | 16 MSBs of Stop Count Time |
//	   _____________________________________________________________
//   End:  |P|1|1|0|-|-|-|-|-|-|-|-|-|-|-|-|6-Bit Err| 10-Bit Word Cnt  |
//
//   	- All Data is 32-Bits
//	- All Times are in nanoseconds
//	- Error Flags:
//		--Bit 0: 32-Bit Time Counter Overflow
//		--Bit 1: Subtraction error overflow during data compaction
//		--Bits 2-5: Unused
//	- P: Even parity check bit
//
//
//	Returns: 0 if successful
//		 1 on error
//

	int tdcrocEvLength;
	stopCountTime = 0;
	boardID = 0;
	int vmeTime = 0;
	int errFlags = 0;
	int wordCount = 0;
	int i = 0;
	int k, max;
	unsigned length;	
	int channelID = 0;
	float tdcTime = 0;
	int exitSwitch = 0;

	boardID = physicsEvent[j];
	j++;
	tdcrocEvLength = physicsEvent[j];
	j++;

	for (i=j;i<(j+tdcrocEvLength);i++) {
	
	      switch (get_hex_bits(physicsEvent[j],7,1) % 8){

		case 7: // Start ROC event
			// rocEventNum = get_hex_bits(physicsEvent[j],5,6);
			break;
			
		case 0: // Data w/ channel += 0
			channelID = get_hex_bit(physicsEvent[j],6) + 0;
			tdcTime = get_hex_bits(physicsEvent[j],5,6);
			
			tdcCodaID[tdcCount] = codaEventNum;
			tdcRunID[tdcCount] = runNum;
			tdcSpillID[tdcCount] = spillID;
			tdcROC[tdcCount] = ROCID;
			tdcBoardID[tdcCount] = boardID;
			tdcChannelID[tdcCount] = channelID;
			tdcStopTime[tdcCount] = tdcTime;
			tdcVmeTime[tdcCount] = vmeTime;

			tdcCount++;
			break;
		
		case 1: // Data w/ channel += 12
			channelID = get_hex_bit(physicsEvent[j],6) + 16;
			tdcTime = get_hex_bits(physicsEvent[j],5,6);

			tdcCodaID[tdcCount] = codaEventNum;
			tdcRunID[tdcCount] = runNum;
			tdcSpillID[tdcCount] = spillID;
			tdcROC[tdcCount] = ROCID;
			tdcBoardID[tdcCount] = boardID;
			tdcChannelID[tdcCount] = channelID;
			tdcStopTime[tdcCount] = tdcTime;
			tdcVmeTime[tdcCount] = vmeTime;
			tdcCount++;
			break;
		
		case 2: // Data w/ channel += 24
			channelID = get_hex_bit(physicsEvent[j],6) + 32;
			tdcTime = get_hex_bits(physicsEvent[j],5,6);

			tdcCodaID[tdcCount] = codaEventNum;
			tdcRunID[tdcCount] = runNum;
			tdcSpillID[tdcCount] = spillID;
			tdcROC[tdcCount] = ROCID;
			tdcBoardID[tdcCount] = boardID;
			tdcChannelID[tdcCount] = channelID;
			tdcStopTime[tdcCount] = tdcTime;
			tdcVmeTime[tdcCount] = vmeTime;
			
			tdcCount++;
			break;

		case 3: // Data w/ channel += 36
			channelID = get_hex_bit(physicsEvent[j],6) + 48;
			tdcTime = get_hex_bits(physicsEvent[j],5,6);

			tdcCodaID[tdcCount] = codaEventNum;
			tdcRunID[tdcCount] = runNum;
			tdcSpillID[tdcCount] = spillID;
			tdcROC[tdcCount] = ROCID;
			tdcBoardID[tdcCount] = boardID;
			tdcChannelID[tdcCount] = channelID;
			tdcStopTime[tdcCount] = tdcTime;
			tdcVmeTime[tdcCount] = vmeTime;
						
			tdcCount++;
			break;

		case 4: // Stop count 1 (LSB)
			stopCountTime = get_hex_bits(physicsEvent[j],3,4);
			break;

		case 5: // Stop count 2 (MSB) and board address
			// boardID = get_hex_bits(physicsEvent[j],6,3);
			// These are the MSB, so they must be multiplied by 16^4 and then added to the LSB
			stopCountTime += (get_hex_bits(physicsEvent[j],3,4)*16*16*16*16);
			break;

		case 6: // End ROC event
			errFlags = get_bin_bits(physicsEvent[j],11,2);
			wordCount = get_bin_bits(physicsEvent[j],9,10);
			
			if (errFlags==0){
				sprintf(errString,"");
			} else if (errFlags==1) {
				sprintf(errString,"Subtraction error overflow during Data Compaction.");
			} else if (errFlags==2) {
				sprintf(errString,"32 Bit time Counter Overflow.");
			} else if (errFlags==3) {
				sprintf(errString,"32 Bit time Counter Overflow and Subtraction overflow "
					"during Data Compaction.");
			}
			
			break;

		default: // Shouldn't happen
			printf("Illegal Old TDC Event entry: %lx\n", physicsEvent[j]);
			printf("%x\n", get_hex_bits(physicsEvent[j],7,1));
			return 1;
			break;
	      }

	      // Send the Data information to the database if there was data in the rocEvent

	      if (tdcCount == max_tdc_rows) {
		make_tdc_query(conn);
		tdcCount = 0;
	      }

	      j++;
		
	}

	return j;

}

int eventNewTDCSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	int channel = 0;
	int channelVal = 0;
	double Td, Ts, Tt;
	int val, k, l, m, n;
	double signalWidth[64];
	double tdcTime[64];
	boardID = 0;
	int i = 0;
	int channelFired[64];
	int channelFiredBefore[64];

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
	// printf("ROC: %i, boardID: %.8x\n", ROCID, boardID);
	j++;
	
	// Word 2 contains Td and signal window width
	Td = get_hex_bits(physicsEvent[j],3,2);
	Td = Td*2*10; // x2 (resolution) x10 (width of clock)
	if(get_bin_bit(physicsEvent[j],1)){
		windows = 64;
	} else {
		windows = 32;
	}
	// printf("#Windows: %i\n",windows);
	j++;

	// Word 3 contains Tt
	Tt = 0.0;
	for(k=0;k<3;k++){
		val = get_hex_bit(physicsEvent[j],k);
		Tt += trigger_pattern[val];
	}
	j++;

	// Initialize the ridiculous constants required for reading channel information
	for (k=0;k<64;k++){ 
		// This flags when a channel fires
		channelFired[k]=0;

		// This flags when a channel has fired previously
		channelFiredBefore[k]=0;

		// This keeps track of the signal width
		signalWidth[k]=0;
	}

	// 8*windows of words to follow for each channel
	for(k=0;k<windows;k++){
		channel = 0;
		for(m=0;m<8;m++){
		    // If all channels are not fired, mark them as such
		    //	If they were previously fired, register them as an INSERT row
		    if (physicsEvent[j] == 0){
			j++;
			for(n=0;n<8;n++){
			    if(channelFiredBefore[channel]){
				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }

			    channelFired[channel]=0;
			    channelFiredBefore[channel]=0;
			    channel++;
			}
		    // If there is a signal, store all pertinent information.
		    //	If there's a signal and it's the last window, add the info to the INSERT
		    } else {
			for(l=0;l<8;l++){
			    val = get_hex_bit(physicsEvent[j],l);
			    if(val!=0) { channelFired[channel]=1; }

			    // If there is a signal, but there was no signal before, we have a new hit
			    if(channelFired[channel] && !(channelFiredBefore[channel]) && (k<(windows-1) ) ) {
				// Compute TDC time
				Ts = k*10+channel_pattern[val];
				tdcTime[channel] = 1280-Td-Ts+Tt;
				// Establish first value of signal width
				signalWidth[channel] = (10 - channel_pattern[val]);
				// Mark that this channel has fired
				channelFiredBefore[channel]=1;
			    } 
			    
			    // If there is a signal, and it fired previously, simply add to the signal width
			    else if (channelFired[channel] && channelFiredBefore[channel] && (k<(windows-1) ) ) {
				// Add to signal width
				signalWidth[channel] += (10 - channel_pattern[val]);
				// Redundant, but keeps things straight
				channelFiredBefore[channel] = 1;
			    }
			    // If a signal has fired, fired before, and it's the last window, make an INSERT row
			    else if (channelFired[channel] && channelFiredBefore[channel] && k==(windows-1) ) {
				signalWidth[channel] += (10 - channel_pattern[val]);

				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				channelFired[channel]=1;
				channelFiredBefore[channel]=1;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }
			    // If a signal fires, but has not fired before, and it is the last window, 
			    //	calc TDC time, signal width, and make INSERT row
			    else if (channelFired[channel] && !(channelFiredBefore[channel]) && k==(windows-1) ) {
				// Compute TDC time
				Ts = k*10+channel_pattern[val];
				tdcTime[channel] = 1280-(Td+Ts)+Tt;
				// Establish first value of signal width
				signalWidth[channel] = (10 - channel_pattern[val]);

				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				channelFiredBefore[channel]=1;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }
			    // If it did not fire, and it fired previously, make an INSERT row
			    else if(!(channelFired[channel]) && channelFiredBefore[channel]) {
				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				channelFiredBefore[channel]=0;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }

			    channel++;
			}
			j++;
		    }
		}
	}
	
	return j;

}

int eventWCTDCSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	int channel = 0;
	int channelVal = 0;
	double Td, Ts, Tt;
	int val, k, l, m, n;
	double signalWidth[64];
	double tdcTime[64];
	boardID = 0;
	int i = 0;
	int channelFired[64];
	int channelFiredBefore[64];

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
	// printf("ROC: %i, boardID: %.8x\n", ROCID, boardID);
	j++;
	
	// Word 2 contains Td and signal window width
	Td = get_hex_bits(physicsEvent[j],3,2);
	Td = Td*40; // x2 (resolution) x10 (width of clock)
	if(get_bin_bit(physicsEvent[j],1)){
		windows = 64;
	} else {
		windows = 32;
	}
	// printf("#Windows: %i\n",windows);
	j++;

	// Word 3 contains Tt
	Tt = 0.0;
	for(k=0;k<3;k++){
		val = get_hex_bit(physicsEvent[j],k);
		Tt += wc_trigger_pattern[val];
	}
	j++;

	// Initialize the ridiculous constants required for reading channel information
	for (k=0;k<64;k++){ 
		// This flags when a channel fires
		channelFired[k]=0;

		// This flags when a channel has fired previously
		channelFiredBefore[k]=0;

		// This keeps track of the signal width
		signalWidth[k]=0;
	}

	// 8*windows of words to follow for each channel
	for(k=0;k<windows;k++){
		channel = 0;
		for(m=0;m<8;m++){
		    // If all channels are not fired, mark them as such
		    //	If they were previously fired, register them as an INSERT row
		    if (physicsEvent[j] == 0){
			j++;
			for(n=0;n<8;n++){
			    if(channelFiredBefore[channel]){
				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }

			    channelFired[channel]=0;
			    channelFiredBefore[channel]=0;
			    channel++;
			}
		    // If there is a signal, store all pertinent information.
		    //	If there's a signal and it's the last window, add the info to the INSERT
		    } else {
			for(l=0;l<8;l++){
			    val = get_hex_bit(physicsEvent[j],l);
			    if(val!=0) { channelFired[channel]=1; }

			    // If there is a signal, but there was no signal before, we have a new hit
			    if(channelFired[channel] && !(channelFiredBefore[channel]) && (k<(windows-1) ) ) {
				// Compute TDC time
				Ts = k*40+channel_pattern[val];
				tdcTime[channel] = (1280*4)-Td-Ts+Tt;
				// Establish first value of signal width
				signalWidth[channel] = (40 - wc_channel_pattern[val]);
				// Mark that this channel has fired
				channelFiredBefore[channel]=1;
			    } 
			    
			    // If there is a signal, and it fired previously, simply add to the signal width
			    else if (channelFired[channel] && channelFiredBefore[channel] && (k<(windows-1) ) ) {
				// Add to signal width
				signalWidth[channel] += (40 - wc_channel_pattern[val]);
				// Redundant, but keeps things straight
				channelFiredBefore[channel] = 1;
			    }
			    // If a signal has fired, fired before, and it's the last window, make an INSERT row
			    else if (channelFired[channel] && channelFiredBefore[channel] && k==(windows-1) ) {
				signalWidth[channel] += (40 - wc_channel_pattern[val]);

				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				channelFired[channel]=1;
				channelFiredBefore[channel]=1;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }
			    // If a signal fires, but has not fired before, and it is the last window, 
			    //	calc TDC time, signal width, and make INSERT row
			    else if (channelFired[channel] && !(channelFiredBefore[channel]) && k==(windows-1) ) {
				// Compute TDC time
				Ts = k*40+wc_channel_pattern[val];
				tdcTime[channel] = (1280*4)-(Td+Ts)+Tt;
				// Establish first value of signal width
				signalWidth[channel] = (40 - wc_channel_pattern[val]);

				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				channelFiredBefore[channel]=1;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }
			    // If it did not fire, and it fired previously, make an INSERT row
			    else if(!(channelFired[channel]) && channelFiredBefore[channel]) {
				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = signalWidth[channel];
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				channelFiredBefore[channel]=0;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }

			    channel++;
			}
			j++;
		    }
		}
	}
	
	return j;

}

int eventZSWCTDCSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	int channel = 0;
	int channelVal = 0;
	double Td, Ts, Tt;
	int val, k, l, m, n;
	double signalWidth[64];
	double tdcTime;
	int window;
	int word;
	boardID = 0;
	int i = 0;
	int channelFiredBefore[64];
	
	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
	// printf("boardID: %.8x\n",boardID);
	j++;
	
	// Word 2 contains Td and signal window width
	// x2 (resolution) x10 (width of clock)
	Td = (get_hex_bits(physicsEvent[j],3,2))*40;
	if(get_bin_bit(physicsEvent[j],1)){
		windows = 64;
	} else {
		windows = 32;
	}
	j++;

	// Word 3 contains Tt
	Tt = 0.0;
	for(k=0;k<3;k++){
		val = get_hex_bit(physicsEvent[j],k);
		Tt += trigger_pattern[val];
	}
	j++;

	for (k=0;k<64;k++){ 
		// This flags the window in which a channel has fired previously
		channelFiredBefore[k]=0xBD;
	}

	window = 0;
	word = 0;
	channel = 0;
	// 0xe906c0da is the 'end' word in this format
	while(physicsEvent[j]!=0xe906c0da){
		
		if(get_hex_bits(physicsEvent[j],7,5)==0xe906d){
			word=(get_hex_bits(physicsEvent[j],2,3));
			// printf("physicsEvent[%i]=%.8x\n",j,physicsEvent[j]);
			window=floor(word/8.0);
			channel=(word%8)*8;
			j++;
		} else {
			for(n=0;n<8;n++){
			  // If a channel fires off and did not fire off in the previous window, create a hit entry
			  val=get_hex_bit(physicsEvent[j],n);
			  if(val!=0x0){
			    if(window!=channelFiredBefore[channel]+1){
				channelFiredBefore[channel]=window;
				Ts = window*40+channel_pattern[val];
				tdcTime = 5120-Td-Ts+Tt;
				
				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime;
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = 0;
				tdcCount++;

				if (tdcCount == max_tdc_rows) {
				    // printf("Submitting Reimer Data!\n");
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
			    }
			    channelFiredBefore[channel]=window;
			  }
			  channel++;
			}
			word++;
			window=floor(word/8.0);
			channel=(word%8)*8;
			j++;
		}
	}

	j++;

	return j;

}

int eventv1495TDCSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	unsigned int numChannels = 0;
	unsigned int stopTime, channel, channelTime, tdcTime;
	unsigned int error;
	unsigned int ignore = 0;
	int i = 0;
	int k = 0;
	

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],3,4);
	j++;

	// Check to see if there is an error in next word
	// If so, don't submit the information from this board for this event
	error = get_hex_bit(physicsEvent[j+1],3);
	if (error != 0x1) {
		// printf("Coda Event Number: %i\n", codaEventNum);
		// printf("v1495 BoardID: %.4x\n", boardID);
		// printf("StopTime: %.8x\n", physicsEvent[j+1]);
		// printf("#DataWords: %.8x\n", physicsEvent[j]);
		if ((get_hex_bits(physicsEvent[j],3,4))>255){
			if (get_hex_bits(physicsEvent[j],3,4)==0xdead){
				case4++;
			} else {
				case6++;
			}
		} else {
			case2++;
		}	
		ignore = 1;
	} else {

		if ((get_hex_bits(physicsEvent[j],3,4))>255){
			if (get_hex_bits(physicsEvent[j],3,4)==0xdead){
				case3++;
			} else {
				case5++;
			}
		} else {
			case1++;
		}
	}

	// Contains 0x000000xx where xx is number of channel entries
	numChannels = get_hex_bits(physicsEvent[j],3,4);
	if (numChannels > 255) { 
		printf("Event: %i, Board: %.4x, Channel exceeds 255 in v1495 format: %.8x\n", codaEventNum, boardID, physicsEvent[j]);
		return j+2;
	}
	j++;

	// Next contains 0x00001xxx where xxx is the stop time
	stopTime = get_hex_bits(physicsEvent[j],2,3);
	j++;

	k = 0;
	while(k<numChannels){
		channel = get_hex_bits(physicsEvent[j],3,2);
		channelTime = get_hex_bits(physicsEvent[j],1,2);
		tdcTime = (stopTime - channelTime);
		j++;

		if (!ignore){
			v1495CodaID[v1495Count] = codaEventNum;
			v1495RunID[v1495Count] = runNum;
			v1495SpillID[v1495Count] = spillID;
			v1495ROC[v1495Count] = ROCID;
			v1495BoardID[v1495Count] = boardID;
			v1495ChannelID[v1495Count] = channel;
			v1495StopTime[v1495Count] = tdcTime;
			v1495VmeTime[v1495Count] = vmeTime;
			v1495Count++;
		}
		k++;

		if (v1495Count == max_v1495_rows) {
		    make_v1495_query(conn);
		    v1495Count = 0;
		}

	}
	i++;
	
	return j;

}

int eventReimerTDCSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	int channel = 0;
	int channelVal = 0;
	int Td, Ts, Tt;
	int val, k, l, m, n;
	int signalWidth[64];
	int tdcTime;
	int window;
	int word;
	boardID = 0;
	int i = 0;
	int channelFiredBefore[64];

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
	// printf("boardID: %.8x\n",boardID);
	j++;
	
	// Word 2 contains Td and signal window width
	// x2 (resolution) x10 (width of clock)
	Td = (get_hex_bits(physicsEvent[j],3,2))*2*10;
	if(get_bin_bit(physicsEvent[j],1)){
		windows = 64;
	} else {
		windows = 32;
	}
	j++;

	// Word 3 contains Tt
	Tt = 0.0;
	for(k=0;k<3;k++){
		val = get_hex_bit(physicsEvent[j],k);
		Tt += trigger_pattern[val];
	}
	j++;

	for (k=0;k<64;k++){ 
		// This flags the window in which a channel has fired previously
		channelFiredBefore[k]=0xBD;
	}

	window = 0;
	word = 0;
	channel = 0;
	// 0xe906c0da is the 'end' word in this format
	while(physicsEvent[j]!=0xe906c0da){
		
		if(get_hex_bits(physicsEvent[j],7,5)==0xe906d){
			word=(get_hex_bits(physicsEvent[j],2,3));
			// printf("physicsEvent[%i]=%.8x\n",j,physicsEvent[j]);
			window=floor(word/8.0);
			channel=(word%8)*8;
			j++;
		} else {
			for(n=0;n<8;n++){
			
			  val=get_hex_bit(physicsEvent[j],n);
			  if(val!=0x0){
			    if(window!=channelFiredBefore[channel]+1){
				channelFiredBefore[channel]=window;
				Ts = window*10+channel_pattern[val];
				tdcTime = 1280-Td-Ts+Tt;

				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime;
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = 0;
				tdcCount++;
				
				if (tdcCount == max_tdc_rows) {
				    // printf("Submitting Reimer Data!\n");
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
			    }
			    channelFiredBefore[channel]=window;
			    
			  }
			  channel++;
			}
			word++;
			window=floor(word/8.0);
			channel=(word%8)*8;
			j++;
		}
		
	}

	j++;

	return j;

}

int eventJyTDCSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int windows = 0;
	int channel = 0;
	int Td, Ts, Tt;
	int val;
	int tdcTime;
	int window;
	int word, zsword;
	boardID = 0;
	int i = 0;
	int k;
	int buf = 0;
	int channelFiredBefore[64];

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
	// printf("boardID: %.8x\n",boardID);
	j++;
	
	// Word 2 contains Td and signal window width
	// x2 (resolution) x10 (width of clock)
	Td = (get_hex_bits(physicsEvent[j],3,2))*2*10;
	if(get_bin_bit(physicsEvent[j],1)){
		windows = 64;
	} else {
		windows = 32;
	}
	j++;

	buf = (0xffff & physicsEvent[j]);
	j++;

	// Word 3 contains Tt
	Tt = 0.0;
	for(k=0;k<3;k++){
		val = get_hex_bit(physicsEvent[j],k);
		Tt += trigger_pattern[val];
	}
	j++;

	for (k=0;k<64;k++){ 
		// This flags the window in which a channel has fired previously
		channelFiredBefore[k]=0xBD;
	}

	window = 0;
	zsword = 2;
	word = 0;
	channel = 0;
	word = 0;
	
	while(zsword <= buf){
		
		if((physicsEvent[j] & 0xFF000000)==0x88000000){
			
			for(k=1;k<(buf-zsword)+1;k++){
			    // Look for the next 0x88 word, see where the next set
			    // of zeroes starts, and count backwards from there
			    if ((physicsEvent[j+k] & 0xFF000000)==0x88000000){
				word = (physicsEvent[j+k] & 0x00000FFF)-(k-1);
				// Set k to exit the for loop
				k = buf;
			    } else if(zsword+k==buf){
				word = (windows*8)-(k-1);
			    }
			}
			// Figure out which window and channel to start with
			window=floor(word/8.0);
			channel=(word%8)*8;
		} else {
			for(k=0;k<8;k++){
			
			  val=get_hex_bit(physicsEvent[j],k);
			  if(val!=0x0){
			    if(window!=channelFiredBefore[channel]+1){
				channelFiredBefore[channel]=window;
				Ts = window*10+channel_pattern[val];
				tdcTime = 1280-Td-Ts+Tt;

				tdcCodaID[tdcCount] = codaEventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime;
				tdcVmeTime[tdcCount] = vmeTime;
				tdcSignalWidth[tdcCount] = 0;
				tdcCount++;
				
				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
			    }
			    channelFiredBefore[channel]=window;
			  }
			  channel++;
			}
			word++;
			window=floor(word/8.0);
			channel=(word%8)*8;
		}
		j++;
		zsword++;
		
	}
	return j;

}

int eventLatchSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
    int i, k;
    unsigned int num;


	boardID = get_hex_bits(physicsEvent[j],7,4);
	j++;

	num = physicsEvent[j];

	for (k=0;k<32;k++) {
		if (num%2){
			
			latchCodaID[latchCount] = codaEventNum;
			latchRunID[latchCount] = runNum;
			latchSpillID[latchCount] = spillID;
			latchROC[latchCount] = ROCID;
			latchBoardID[latchCount] = boardID;
			latchChannelID[latchCount] = k;
			latchVmeTime[latchCount] = vmeTime;
			latchCount++;
		}
		num/=2.0;

		if (latchCount>=100){
			make_latch_query(conn);
			latchCount = 0;
		}
	}

	j++;
	

	num = physicsEvent[j];

	for (k=0;k<32;k++) {
		if (num%2){
			latchCodaID[latchCount] = codaEventNum;
			latchRunID[latchCount] = runNum;
			latchSpillID[latchCount] = spillID;
			latchROC[latchCount] = ROCID;
			latchBoardID[latchCount] = boardID;
			latchChannelID[latchCount] = (32+k);
			latchVmeTime[latchCount] = vmeTime;
			latchCount++;
		}
		num/=2.0;

		if (latchCount>=100){
			make_latch_query(conn);
			latchCount = 0;
		}
	}
	j++;

   	return j;

}

int eventScalerSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j){
    int i, k;
    unsigned int num;

    for (i=0;i<32;i++) {
	
	scalerCodaID[scalerCount] = codaEventNum;
	scalerRunID[scalerCount] = runNum;
	scalerSpillID[scalerCount] = spillID;
	scalerROC[scalerCount] = ROCID;
	scalerChannelID[scalerCount] = i;
	scalerValue[scalerCount] = physicsEvent[j];
	scalerVmeTime[scalerCount] = vmeTime;
	scalerCount++;

	if (scalerCount>=200){
	    make_scaler_query(conn);
	    scalerCount = 0;
	}

	j++;

    }

    return j;

}

int eventTriggerSQL(MYSQL *conn, unsigned int physicsEvent[100000], int j) {
	
	triggerBits = physicsEvent[j];
	j++;

	return j;
}

int eventSQL(MYSQL *conn, unsigned int physicsEvent[100000]){
// ================================================================
// Handles Physics Events.  Stores general header information and 
//	sends the different types of Physics Events to their 
//	respective functions.
// Returns: 	0 on success
//		1 on error

	int eventCode = 0;
	int headerLength = 0;
	int headerCode = 0;
	char sqlCodaEventQuery[256];
	int i, j, k, temproc, v, x;
	evLength = 0;
	eventType = 0;
	dataQuality = 0;
	triggerBits = 0;
	codaEventNum = 0;
	
	evLength = physicsEvent[0];
	eventCode = physicsEvent[1];
	headerLength = physicsEvent[2];
	headerCode = physicsEvent[3];
	codaEventNum = physicsEvent[4];
	eventType = physicsEvent[5];
	// physicsEvent[6] = 0 (reserved value)
	//printf("Begin Physics Event Processing\n");
	//printf("SpillID: %i, codaEventNum: %i\n", spillID, codaEventNum);
	switch (get_hex_bits(physicsEvent[1],7,4)){
		
		case 14:
		  j = 7;
		  while (j<evLength){
		    rocEvLength = physicsEvent[j];
		    v = j+rocEvLength;
		    if (v > evLength) { 
			printf("ERROR: ROC Event Length exceeds event length\n"
			"Event: %i, EventLength = %i, position = %i, rocEvLength = %.8x\n", 
			codaEventNum, evLength, j, physicsEvent[j]);
			for(x=j-20;x<j+20;x++){
				printf("physicsEvent[%i] == %.8x\n",x,physicsEvent[x]);
			} 
			return 1; 
		    }
		    // Variable v denotes the end of this ROC entry
		    j++;
		    ROCID = get_hex_bits(physicsEvent[j], 5, 2);
		    j++;
		    // physicsEvent[j]=0x11111111; IGNORE
		    j++;
		    // physicsEvent[j]=0x11111111; IGNORE
		    j++;
		    vmeTime = physicsEvent[j];
		    j++;
		    e906flag = physicsEvent[j];
		    j++;
		    if ( rocEvLength < 5 ) { j = v+1; }
		    while(j<=v){
			j = format(conn, physicsEvent, j, v);
		    }
		  }
		  break;

		case 130:
		    // Slow Control Type
		    if ( eventSlowSQL(conn, physicsEvent) ) {
		    	printf("Slow Control Event Processing Error\n");
		    	return 1;
		    }
		    break;

		case 140:
		    // Start of run descriptor
		    if ( eventRunDescriptorSQL(conn, physicsEvent) ) {
		    	printf("Start of Run Desctiptor Event Processing Error\n");
		    	return 1;
		    }
		    break;

		case 11:
		    // Beginning of spill
		    j = 7;
		    spillID++;
		    //printf("Begin Spill Event\n");
			
		    while((j<evLength) && (physicsEvent[j] & 0xfffff000)!=0xe906f000){
				
			rocEvLength = physicsEvent[j];
			v = j+rocEvLength;
			if ((rocEvLength+j) > evLength) { 
				printf("This error: rocEvLength = %i, j = %i, evLength = %i\n", 
				rocEvLength, j, evLength);
				for(x=0;x<evLength;x++){
					printf("physicsEvent[%i]=%.8x\n",x,physicsEvent[x]);
				}
				return 1;
			}
			j++;
			ROCID = get_hex_bits(physicsEvent[j], 7, 4);
			j++;
			//GARBAGE = physicsEvent[9]; (3 words to follow)
			j++;
			//latchFlag = physicsEvent[11] = 1; (1 word to follow)
			j++;
			spillVmeTime = physicsEvent[j];
			j++;
				
			mysql_stmt_bind_param(spillStmt, spillBind);
				
			if (mysql_stmt_execute(spillStmt))
			{
				printf("Error: %s\n", mysql_error(conn));
				return 1;
			}
			
			e906flag = physicsEvent[j];
			j++;
			if ( rocEvLength < 5 ) { j = v+1; }
			while(j<=v){
				j = format(conn, physicsEvent, j, v);
			}
		    }
		    break;

		case 12:
		    // End of spill.
		    j = 7;
 		    //printf("End Spill Event\n");
		    while((j<evLength) && (physicsEvent[j] & 0xfffff000)!=0xe906f000){
				
			rocEvLength = physicsEvent[j];
			v = j+rocEvLength;
			if ((rocEvLength+j) > evLength) { 
				printf("This error: rocEvLength = %i, j = %i, evLength = %i\n", 
				rocEvLength, j, evLength);
				for(x=0;x<evLength;x++){
					printf("physicsEvent[%i]=%.8x\n",x,physicsEvent[x]);
				}
				return 1;
			}
			j++;
			ROCID = get_hex_bits(physicsEvent[j], 7, 4);
			j++;
			//GARBAGE = physicsEvent[9]; (3 words to follow)
			j++;
			//latchFlag = physicsEvent[11] = 1; (1 word to follow)
			j++;
			spillVmeTime = physicsEvent[j];
			j++;
			
			mysql_stmt_bind_param(spillStmt, spillBind);
				
			if (mysql_stmt_execute(spillStmt))
			{
				printf("Error: %s\n", mysql_error(conn));
				return 1;
			}
			
			e906flag = physicsEvent[j];
			j++;
			if ( rocEvLength < 5 ) { j = v+1; }
			    while(j<=v){
				j = format(conn, physicsEvent, j, v);
			    }
			}
		    break;

		default:
		    // Awaiting further event types
		    printf("Unknown event type: %x\n", eventType);
		    break;
	}	

	evNum++;

	mysql_stmt_bind_param(codaEvStmt, codaEvBind);

	if (mysql_stmt_execute(codaEvStmt))
    	{
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}
	
	return 0;

}

int format(MYSQL *conn, unsigned int physicsEvent[100000], int j, int v) {

	int x;

	if        ( e906flag == 0xe906f002 ) {
		// TDC event type
		j = eventTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f001 ) {
		// Latch event type
		j = eventLatchSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f003 ) {
		// Scaler event type
		j = eventScalerSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f004 ) {
		// New TDC (non zero-suppressed) event type
		j = eventNewTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f005 ) {
		// v1495 TDC type
		j = eventv1495TDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f006 ) {
		// Reimer TDC type
		j = eventReimerTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f007 ) {
		// Wide-Channel TDC type
		j = eventWCTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f008 ) {
		// Zero-Suppressed Wide-Channel TDC type
		j = eventZSWCTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f009 ) {
		// Zero-Suppressed Jy TDC type
		j = eventJyTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f00f ) {
		// Trigger type
		j = eventTriggerSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xe906f000 ) {
		// Ignore Board
		while((j<=v) && ((physicsEvent[j] & 0xFFFFF000)!=0xE906F000)){j++;}
		if(j==v){
			// The end of the ROC data has been reached, move on
			j++;
		}
	} else {
		printf("Unexpected flag in CODA Event %i, ROC %i, after BoardID %.4x:\n"
			"e906flag = physicsEvent[%i] == %.8x\n\n",
		codaEventNum, ROCID, boardID,j,physicsEvent[j-1]);
		
		for(x=0;x<j+20;x++){
			printf("physicsEvent[%i] == %.8x\n",x,physicsEvent[x]);
		}
		
		return j;
	}
	if (j<v){ e906flag = physicsEvent[j]; j++; }

	return j;

}

int endEventSQL(MYSQL *conn, unsigned int physicsEvent[100000]){
// ================================================================
//
// Handles End Event CODA Events.
// Returns: 0

	evLength = 0;
	int endEventCode = 0;
	endEventTime = 0.00;
	int numEventsInRunAtEndOfRun = 0;

	evLength = physicsEvent[0];
	endEventCode = physicsEvent[1];
	endEventTime = physicsEvent[2];
	// physicsEvent[3] = 0 is a reserved value;
	numEventsInRunAtEndOfRun = physicsEvent [4];

	return 0;
}

int make_tdc_query(MYSQL* conn){

	char outputFileName[10000];
	char qryString[1000];
	int pid, k;
	FILE *fp;

	if( mysql_query(conn, "DELETE FROM tempTDC") ) {
		printf("Error: %s\n", mysql_error(conn));
   	}
	
	
	// This method prints the data out to file and then uploads directly via LOAD DATA INFILE
	pid = getpid();

	sprintf(outputFileName,".tdc.%i.temp", pid);
	
	// Open the temp file 
	if (file_exists(outputFileName)) remove(outputFileName);
	fp = fopen(outputFileName,"w");

	if (fp == NULL) {
  		fprintf(stderr, "Can't open output file %s!\n",
          		outputFileName);
		// sprintf(text, "");
  		exit(1);
	}

	for(k=0;k<tdcCount;k++){
		fprintf(fp, "%i %i %i %i %i %i \\N \\N \\N %i \\N %i %i\n", tdcRunID[k], tdcSpillID[k], tdcCodaID[k], tdcROC[k], 
			tdcBoardID[k], tdcChannelID[k], tdcStopTime[k], tdcSignalWidth[k], tdcVmeTime[k]);
	}

	// File MUST be closed before it can be loaded into MySQL
	fclose(fp);

	sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' INTO TABLE tempTDC "
			"FIELDS TERMINATED BY ' ' LINES TERMINATED BY '\\n'", outputFileName);
	
	// Submit the query to the server	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_warning_count(conn) > 0 )
	{
		printf("Warnings have occured from the Hit upload:\n");
		showWarnings(conn);
	}
	
	remove(outputFileName);
	
	
	
	/*
	// This method uses prepared statements to insert the data	
	mysql_stmt_bind_param(hitStmt, hitBind);

	if (mysql_stmt_execute(hitStmt))
    	{
		printf("INSERT Error: %s\n", mysql_stmt_error(hitStmt));
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}
	*/
	
	
	// Clear the arrays
	memset((void*)&tdcChannelID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcCodaID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcRunID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcSpillID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcROC, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcBoardID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcStopTime, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcSignalWidth, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcVmeTime, 0, sizeof(int)*max_tdc_rows);

	// Clear the query string
	//sprintf(sqlTDCQuery,"");


	if (mysql_query(conn, "UPDATE tempTDC t, Mapping m\n"
                "SET t.detectorName = m.detectorName, "
                "t.elementID = m.elementID\n"
                "WHERE t.rocID = m.rocID AND "
                "t.boardID = m.boardID AND t.channelID = m.channelID") )
        {
                printf("Error (101): %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "UPDATE tempTDC\n"
                "SET driftDistance = tdcTime*0.005") )
        {
                printf("Error (101): %s\n", mysql_error(conn));
                return 1;
        }
	
	if (mysql_query(conn, "INSERT INTO Hit (eventID, runID, spillID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, driftDistance, signalWidth, vmeTime) "
		"SELECT eventID, runID, spillID, rocID, boardID, channelID, "
		"detectorName, elementID, tdcTime, driftDistance, signalWidth, vmeTime FROM tempTDC") )
	{
		printf("Error (102): %s\n", mysql_error(conn));
                return 1;
        }
	
	if (mysql_query(conn, "DELETE FROM tempTDC") )
	{
		printf("Error (103): %s\n", mysql_error(conn));
                return 1;
        }
	
	return 0;

}

int make_v1495_query(MYSQL* conn){

	char outputFileName[10000];
	char qryString[1000];
	int pid, k;
	FILE *fp;
	
	if( mysql_query(conn, "DELETE FROM tempv1495") ) {
		printf("Error: %s\n", mysql_error(conn));
   	}
	
	/*
	// This method uses a prepared statement to insert the data
	mysql_stmt_bind_param(v1495Stmt, v1495Bind);

	if (mysql_stmt_execute(v1495Stmt))
    	{
		printf("INSERT Error: %s\n", mysql_stmt_error(v1495Stmt));
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}
	*/
	
	// This method prints the data out to file and then uploads directly via LOAD DATA INFILE
	pid = getpid();

	sprintf(outputFileName,".v1495.%i.temp", pid);
	
	// Open the temp file 
	if (file_exists(outputFileName)) remove(outputFileName);
	fp = fopen(outputFileName,"w");

	if (fp == NULL) {
		fprintf(stderr, "Can't open output file %s!\n",
			outputFileName);
		exit(1);
	}

	for(k=0;k<v1495Count;k++){
		fprintf(fp, "%i %i %i %i %i %i \\N \\N \\N %i %i\n", v1495RunID[k], v1495SpillID[k], v1495CodaID[k], v1495ROC[k], 
			v1495BoardID[k], v1495ChannelID[k], v1495StopTime[k], v1495VmeTime[k]);
	}

	// File MUST be closed before it can be loaded into MySQL
	fclose(fp);

	sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' INTO TABLE tempv1495 "
			"FIELDS TERMINATED BY ' ' LINES TERMINATED BY '\\n'", outputFileName);
	
	// Submit the query to the server	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_warning_count(conn) > 0 )
	{
		printf("Warnings have occured from the TriggerHit upload:\n");
		showWarnings(conn);
	}

	remove(outputFileName);
	
	// Clear the arrays
	memset((void*)&v1495ChannelID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495CodaID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495RunID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495SpillID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495ROC, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495BoardID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495StopTime, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495VmeTime, 0, sizeof(int)*max_v1495_rows);

	// Clear the query string
	// sprintf(sqlv1495Query,"");
	
	if (mysql_query(conn, "UPDATE tempv1495 t, Mapping m\n"
                "SET t.detectorName = m.detectorName, "
		"t.elementID = m.elementID\n"
                "WHERE t.rocID = m.rocID AND "
                "t.boardID = m.boardID AND t.channelID = m.channelID") )
        {
                printf("Error (101): %s\n", mysql_error(conn));
                return 1;
        }
	
	if (mysql_query(conn, "INSERT INTO TriggerHit (eventID, runID, spillID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, vmeTime) "
		"SELECT eventID, runID, spillID, rocID, boardID, channelID, "
		"detectorName, elementID, tdcTime, vmeTime FROM tempv1495") )
	{
		printf("Error (102): %s\n", mysql_error(conn));
                return 1;
        }
	
	if (mysql_query(conn, "DELETE FROM tempv1495") )
	{
		printf("Error (103): %s\n", mysql_error(conn));
                return 1;
        }

	return 0;

}

int make_latch_query(MYSQL *conn){
	
	char sqlLatchQuery[100000];

	if( mysql_query(conn, "DELETE FROM tempLatch") ) {
		printf("Error: %s\n", mysql_error(conn));
		return 1;
   	}
	
	sprintf(sqlLatchQuery, "INSERT INTO tempLatch (eventID, runID, rocID, boardID, channelID, vmeTime) "
	"VALUES (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i)",
	latchCodaID[0], latchRunID[0], latchSpillID[0], latchROC[0], latchBoardID[0], latchChannelID[0], latchVmeTime[0],
	latchCodaID[1], latchRunID[1], latchSpillID[1], latchROC[1], latchBoardID[1], latchChannelID[1], latchVmeTime[1],
	latchCodaID[2], latchRunID[2], latchSpillID[2], latchROC[2], latchBoardID[2], latchChannelID[2], latchVmeTime[2],
	latchCodaID[3], latchRunID[3], latchSpillID[3], latchROC[3], latchBoardID[3], latchChannelID[3], latchVmeTime[3],
	latchCodaID[4], latchRunID[4], latchSpillID[4], latchROC[4], latchBoardID[4], latchChannelID[4], latchVmeTime[4],
	latchCodaID[5], latchRunID[5], latchSpillID[5], latchROC[5], latchBoardID[5], latchChannelID[5], latchVmeTime[5],
	latchCodaID[6], latchRunID[6], latchSpillID[6], latchROC[6], latchBoardID[6], latchChannelID[6], latchVmeTime[6],
	latchCodaID[7], latchRunID[7], latchSpillID[7], latchROC[7], latchBoardID[7], latchChannelID[7], latchVmeTime[7],
	latchCodaID[8], latchRunID[8], latchSpillID[8], latchROC[8], latchBoardID[8], latchChannelID[8], latchVmeTime[8],
	latchCodaID[9], latchRunID[9], latchSpillID[9], latchROC[9], latchBoardID[9], latchChannelID[9], latchVmeTime[9],
	latchCodaID[10], latchRunID[10], latchSpillID[10], latchROC[10], latchBoardID[10], latchChannelID[10], latchVmeTime[10],
	latchCodaID[11], latchRunID[11], latchSpillID[11], latchROC[11], latchBoardID[11], latchChannelID[11], latchVmeTime[11],
	latchCodaID[12], latchRunID[12], latchSpillID[12], latchROC[12], latchBoardID[12], latchChannelID[12], latchVmeTime[12],
	latchCodaID[13], latchRunID[13], latchSpillID[13], latchROC[13], latchBoardID[13], latchChannelID[13], latchVmeTime[13],
	latchCodaID[14], latchRunID[14], latchSpillID[14], latchROC[14], latchBoardID[14], latchChannelID[14], latchVmeTime[14],
	latchCodaID[15], latchRunID[15], latchSpillID[15], latchROC[15], latchBoardID[15], latchChannelID[15], latchVmeTime[15],
	latchCodaID[16], latchRunID[16], latchSpillID[16], latchROC[16], latchBoardID[16], latchChannelID[16], latchVmeTime[16],
	latchCodaID[17], latchRunID[17], latchSpillID[17], latchROC[17], latchBoardID[17], latchChannelID[17], latchVmeTime[17],
	latchCodaID[18], latchRunID[18], latchSpillID[18], latchROC[18], latchBoardID[18], latchChannelID[18], latchVmeTime[18],
	latchCodaID[19], latchRunID[19], latchSpillID[19], latchROC[19], latchBoardID[19], latchChannelID[19], latchVmeTime[19],
	latchCodaID[20], latchRunID[20], latchSpillID[20], latchROC[20], latchBoardID[20], latchChannelID[20], latchVmeTime[20],
	latchCodaID[21], latchRunID[21], latchSpillID[21], latchROC[21], latchBoardID[21], latchChannelID[21], latchVmeTime[21],
	latchCodaID[22], latchRunID[22], latchSpillID[22], latchROC[22], latchBoardID[22], latchChannelID[22], latchVmeTime[22],
	latchCodaID[23], latchRunID[23], latchSpillID[23], latchROC[23], latchBoardID[23], latchChannelID[23], latchVmeTime[23],
	latchCodaID[24], latchRunID[24], latchSpillID[24], latchROC[24], latchBoardID[24], latchChannelID[24], latchVmeTime[24],
	latchCodaID[25], latchRunID[25], latchSpillID[25], latchROC[25], latchBoardID[25], latchChannelID[25], latchVmeTime[25],
	latchCodaID[26], latchRunID[26], latchSpillID[26], latchROC[26], latchBoardID[26], latchChannelID[26], latchVmeTime[26],
	latchCodaID[27], latchRunID[27], latchSpillID[27], latchROC[27], latchBoardID[27], latchChannelID[27], latchVmeTime[27],
	latchCodaID[28], latchRunID[28], latchSpillID[28], latchROC[28], latchBoardID[28], latchChannelID[28], latchVmeTime[28],
	latchCodaID[29], latchRunID[29], latchSpillID[29], latchROC[29], latchBoardID[29], latchChannelID[29], latchVmeTime[29],
	latchCodaID[30], latchRunID[30], latchSpillID[30], latchROC[30], latchBoardID[30], latchChannelID[30], latchVmeTime[30],
	latchCodaID[31], latchRunID[31], latchSpillID[31], latchROC[31], latchBoardID[31], latchChannelID[31], latchVmeTime[31],
	latchCodaID[32], latchRunID[32], latchSpillID[32], latchROC[32], latchBoardID[32], latchChannelID[32], latchVmeTime[32],
	latchCodaID[33], latchRunID[33], latchSpillID[33], latchROC[33], latchBoardID[33], latchChannelID[33], latchVmeTime[33],
	latchCodaID[34], latchRunID[34], latchSpillID[34], latchROC[34], latchBoardID[34], latchChannelID[34], latchVmeTime[34],
	latchCodaID[35], latchRunID[35], latchSpillID[35], latchROC[35], latchBoardID[35], latchChannelID[35], latchVmeTime[35],
	latchCodaID[36], latchRunID[36], latchSpillID[36], latchROC[36], latchBoardID[36], latchChannelID[36], latchVmeTime[36],
	latchCodaID[37], latchRunID[37], latchSpillID[37], latchROC[37], latchBoardID[37], latchChannelID[37], latchVmeTime[37],
	latchCodaID[38], latchRunID[38], latchSpillID[38], latchROC[38], latchBoardID[38], latchChannelID[38], latchVmeTime[38],
	latchCodaID[39], latchRunID[39], latchSpillID[39], latchROC[39], latchBoardID[39], latchChannelID[39], latchVmeTime[39],
	latchCodaID[40], latchRunID[40], latchSpillID[40], latchROC[40], latchBoardID[40], latchChannelID[40], latchVmeTime[40],
	latchCodaID[41], latchRunID[41], latchSpillID[41], latchROC[41], latchBoardID[41], latchChannelID[41], latchVmeTime[41],
	latchCodaID[42], latchRunID[42], latchSpillID[42], latchROC[42], latchBoardID[42], latchChannelID[42], latchVmeTime[42],
	latchCodaID[43], latchRunID[43], latchSpillID[43], latchROC[43], latchBoardID[43], latchChannelID[43], latchVmeTime[43],
	latchCodaID[44], latchRunID[44], latchSpillID[44], latchROC[44], latchBoardID[44], latchChannelID[44], latchVmeTime[44],
	latchCodaID[45], latchRunID[45], latchSpillID[45], latchROC[45], latchBoardID[45], latchChannelID[45], latchVmeTime[45],
	latchCodaID[46], latchRunID[46], latchSpillID[46], latchROC[46], latchBoardID[46], latchChannelID[46], latchVmeTime[46],
	latchCodaID[47], latchRunID[47], latchSpillID[47], latchROC[47], latchBoardID[47], latchChannelID[47], latchVmeTime[47],
	latchCodaID[48], latchRunID[48], latchSpillID[48], latchROC[48], latchBoardID[48], latchChannelID[48], latchVmeTime[48],
	latchCodaID[49], latchRunID[49], latchSpillID[49], latchROC[49], latchBoardID[49], latchChannelID[49], latchVmeTime[49],
	latchCodaID[50], latchRunID[50], latchSpillID[50], latchROC[50], latchBoardID[50], latchChannelID[50], latchVmeTime[50],
	latchCodaID[51], latchRunID[51], latchSpillID[51], latchROC[51], latchBoardID[51], latchChannelID[51], latchVmeTime[51],
	latchCodaID[52], latchRunID[52], latchSpillID[52], latchROC[52], latchBoardID[52], latchChannelID[52], latchVmeTime[52],
	latchCodaID[53], latchRunID[53], latchSpillID[53], latchROC[53], latchBoardID[53], latchChannelID[53], latchVmeTime[53],
	latchCodaID[54], latchRunID[54], latchSpillID[54], latchROC[54], latchBoardID[54], latchChannelID[54], latchVmeTime[54],
	latchCodaID[55], latchRunID[55], latchSpillID[55], latchROC[55], latchBoardID[55], latchChannelID[55], latchVmeTime[55],
	latchCodaID[56], latchRunID[56], latchSpillID[56], latchROC[56], latchBoardID[56], latchChannelID[56], latchVmeTime[56],
	latchCodaID[57], latchRunID[57], latchSpillID[57], latchROC[57], latchBoardID[57], latchChannelID[57], latchVmeTime[57],
	latchCodaID[58], latchRunID[58], latchSpillID[58], latchROC[58], latchBoardID[58], latchChannelID[58], latchVmeTime[58],
	latchCodaID[59], latchRunID[59], latchSpillID[59], latchROC[59], latchBoardID[59], latchChannelID[59], latchVmeTime[59],
	latchCodaID[60], latchRunID[60], latchSpillID[60], latchROC[60], latchBoardID[60], latchChannelID[60], latchVmeTime[60],
	latchCodaID[61], latchRunID[61], latchSpillID[61], latchROC[61], latchBoardID[61], latchChannelID[61], latchVmeTime[61],
	latchCodaID[62], latchRunID[62], latchSpillID[62], latchROC[62], latchBoardID[62], latchChannelID[62], latchVmeTime[62],
	latchCodaID[63], latchRunID[63], latchSpillID[63], latchROC[63], latchBoardID[63], latchChannelID[63], latchVmeTime[63],
	latchCodaID[64], latchRunID[64], latchSpillID[64], latchROC[64], latchBoardID[64], latchChannelID[64], latchVmeTime[64],
	latchCodaID[65], latchRunID[65], latchSpillID[65], latchROC[65], latchBoardID[65], latchChannelID[65], latchVmeTime[65],
	latchCodaID[66], latchRunID[66], latchSpillID[66], latchROC[66], latchBoardID[66], latchChannelID[66], latchVmeTime[66],
	latchCodaID[67], latchRunID[67], latchSpillID[67], latchROC[67], latchBoardID[67], latchChannelID[67], latchVmeTime[67],
	latchCodaID[68], latchRunID[68], latchSpillID[68], latchROC[68], latchBoardID[68], latchChannelID[68], latchVmeTime[68],
	latchCodaID[69], latchRunID[69], latchSpillID[69], latchROC[69], latchBoardID[69], latchChannelID[69], latchVmeTime[69],
	latchCodaID[70], latchRunID[70], latchSpillID[70], latchROC[70], latchBoardID[70], latchChannelID[70], latchVmeTime[70],
	latchCodaID[71], latchRunID[71], latchSpillID[71], latchROC[71], latchBoardID[71], latchChannelID[71], latchVmeTime[71],
	latchCodaID[72], latchRunID[72], latchSpillID[72], latchROC[72], latchBoardID[72], latchChannelID[72], latchVmeTime[72],
	latchCodaID[73], latchRunID[73], latchSpillID[73], latchROC[73], latchBoardID[73], latchChannelID[73], latchVmeTime[73],
	latchCodaID[74], latchRunID[74], latchSpillID[74], latchROC[74], latchBoardID[74], latchChannelID[74], latchVmeTime[74],
	latchCodaID[75], latchRunID[75], latchSpillID[75], latchROC[75], latchBoardID[75], latchChannelID[75], latchVmeTime[75],
	latchCodaID[76], latchRunID[76], latchSpillID[76], latchROC[76], latchBoardID[76], latchChannelID[76], latchVmeTime[76],
	latchCodaID[77], latchRunID[77], latchSpillID[77], latchROC[77], latchBoardID[77], latchChannelID[77], latchVmeTime[77],
	latchCodaID[78], latchRunID[78], latchSpillID[78], latchROC[78], latchBoardID[78], latchChannelID[78], latchVmeTime[78],
	latchCodaID[79], latchRunID[79], latchSpillID[79], latchROC[79], latchBoardID[79], latchChannelID[79], latchVmeTime[79],
	latchCodaID[80], latchRunID[80], latchSpillID[80], latchROC[80], latchBoardID[80], latchChannelID[80], latchVmeTime[80],
	latchCodaID[81], latchRunID[81], latchSpillID[81], latchROC[81], latchBoardID[81], latchChannelID[81], latchVmeTime[81],
	latchCodaID[82], latchRunID[82], latchSpillID[82], latchROC[82], latchBoardID[82], latchChannelID[82], latchVmeTime[82],
	latchCodaID[83], latchRunID[83], latchSpillID[83], latchROC[83], latchBoardID[83], latchChannelID[83], latchVmeTime[83],
	latchCodaID[84], latchRunID[84], latchSpillID[84], latchROC[84], latchBoardID[84], latchChannelID[84], latchVmeTime[84],
	latchCodaID[85], latchRunID[85], latchSpillID[85], latchROC[85], latchBoardID[85], latchChannelID[85], latchVmeTime[85],
	latchCodaID[86], latchRunID[86], latchSpillID[86], latchROC[86], latchBoardID[86], latchChannelID[86], latchVmeTime[86],
	latchCodaID[87], latchRunID[87], latchSpillID[87], latchROC[87], latchBoardID[87], latchChannelID[87], latchVmeTime[87],
	latchCodaID[88], latchRunID[88], latchSpillID[88], latchROC[88], latchBoardID[88], latchChannelID[88], latchVmeTime[88],
	latchCodaID[89], latchRunID[89], latchSpillID[89], latchROC[89], latchBoardID[89], latchChannelID[89], latchVmeTime[89],
	latchCodaID[90], latchRunID[90], latchSpillID[90], latchROC[90], latchBoardID[90], latchChannelID[90], latchVmeTime[90],
	latchCodaID[91], latchRunID[91], latchSpillID[91], latchROC[91], latchBoardID[91], latchChannelID[91], latchVmeTime[91],
	latchCodaID[92], latchRunID[92], latchSpillID[92], latchROC[92], latchBoardID[92], latchChannelID[92], latchVmeTime[92],
	latchCodaID[93], latchRunID[93], latchSpillID[93], latchROC[93], latchBoardID[93], latchChannelID[93], latchVmeTime[93],
	latchCodaID[94], latchRunID[94], latchSpillID[94], latchROC[94], latchBoardID[94], latchChannelID[94], latchVmeTime[94],
	latchCodaID[95], latchRunID[95], latchSpillID[95], latchROC[95], latchBoardID[95], latchChannelID[95], latchVmeTime[95],
	latchCodaID[96], latchRunID[96], latchSpillID[96], latchROC[96], latchBoardID[96], latchChannelID[96], latchVmeTime[96],
	latchCodaID[97], latchRunID[97], latchSpillID[97], latchROC[97], latchBoardID[97], latchChannelID[97], latchVmeTime[97],
	latchCodaID[98], latchRunID[98], latchSpillID[98], latchROC[98], latchBoardID[98], latchChannelID[98], latchVmeTime[98],
	latchCodaID[99], latchRunID[99], latchSpillID[99], latchROC[99], latchBoardID[99], latchChannelID[99], latchVmeTime[99]);

	memset((void*)&latchChannelID, 0, sizeof(int)*100);
	memset((void*)&latchRocID, 0, sizeof(int)*100);
	memset((void*)&latchCodaID, 0, sizeof(int)*100);
	memset((void*)&latchRunID, 0, sizeof(int)*100);
	memset((void*)&latchSpillID, 0, sizeof(int)*100);
	memset((void*)&latchROC, 0, sizeof(int)*100);
	memset((void*)&latchBoardID, 0, sizeof(int)*100);
	memset((void*)&latchVmeTime, 0, sizeof(int)*100);

	if (mysql_query(conn, sqlLatchQuery))
	{
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}

	sprintf(sqlLatchQuery,"");
	
	if (mysql_query(conn, "UPDATE tempLatch l, Mapping m\n"
                "SET l.detectorName = m.detectorName, l.elementID = m.elementID\n"
                "WHERE l.detectorName IS NULL AND l.rocID = m.rocID AND "
                "l.boardID = m.boardID AND l.channelID = m.channelID") )
        {
                printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO Hit (runID, eventID, spillID, vmeTime, "
		"rocID, boardID, channelID, detectorName, elementID) "
		"SELECT runID, eventID, spillID, vmeTime, rocID, boardID, channelID, "
		"detectorName, elementID FROM tempLatch") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempLatch") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	return 0;

}

int make_scaler_query(MYSQL* conn){

	char sqlScalerQuery[100000];

	if( mysql_query(conn, "DELETE FROM tempScaler") ) {
		printf("Error: %s\n", mysql_error(conn));
   	}

	sprintf(sqlScalerQuery, "INSERT INTO tempScaler (eventID, runID, spillID, rocID, value, channelID, vmeTime) "
	"VALUES (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), (%i, %i, %i, %i, %i, %i, %i), "
	"(%i, %i, %i, %i, %i, %i, %i)",
	scalerCodaID[0], scalerRunID[0], scalerSpillID[0], scalerROC[0], scalerValue[0], scalerChannelID[0], scalerVmeTime[0],
	scalerCodaID[1], scalerRunID[1], scalerSpillID[1], scalerROC[1], scalerValue[1], scalerChannelID[1], scalerVmeTime[1],
	scalerCodaID[2], scalerRunID[2], scalerSpillID[2], scalerROC[2], scalerValue[2], scalerChannelID[2], scalerVmeTime[2],
	scalerCodaID[3], scalerRunID[3], scalerSpillID[3], scalerROC[3], scalerValue[3], scalerChannelID[3], scalerVmeTime[3],
	scalerCodaID[4], scalerRunID[4], scalerSpillID[4], scalerROC[4], scalerValue[4], scalerChannelID[4], scalerVmeTime[4],
	scalerCodaID[5], scalerRunID[5], scalerSpillID[5], scalerROC[5], scalerValue[5], scalerChannelID[5], scalerVmeTime[5],
	scalerCodaID[6], scalerRunID[6], scalerSpillID[6], scalerROC[6], scalerValue[6], scalerChannelID[6], scalerVmeTime[6],
	scalerCodaID[7], scalerRunID[7], scalerSpillID[7], scalerROC[7], scalerValue[7], scalerChannelID[7], scalerVmeTime[7],
	scalerCodaID[8], scalerRunID[8], scalerSpillID[8], scalerROC[8], scalerValue[8], scalerChannelID[8], scalerVmeTime[8],
	scalerCodaID[9], scalerRunID[9], scalerSpillID[9], scalerROC[9], scalerValue[9], scalerChannelID[9], scalerVmeTime[9],
	scalerCodaID[10], scalerRunID[10], scalerSpillID[10], scalerROC[10], scalerValue[10], scalerChannelID[10], scalerVmeTime[10],
	scalerCodaID[11], scalerRunID[11], scalerSpillID[11], scalerROC[11], scalerValue[11], scalerChannelID[11], scalerVmeTime[11],
	scalerCodaID[12], scalerRunID[12], scalerSpillID[12], scalerROC[12], scalerValue[12], scalerChannelID[12], scalerVmeTime[12],
	scalerCodaID[13], scalerRunID[13], scalerSpillID[13], scalerROC[13], scalerValue[13], scalerChannelID[13], scalerVmeTime[13],
	scalerCodaID[14], scalerRunID[14], scalerSpillID[14], scalerROC[14], scalerValue[14], scalerChannelID[14], scalerVmeTime[14],
	scalerCodaID[15], scalerRunID[15], scalerSpillID[15], scalerROC[15], scalerValue[15], scalerChannelID[15], scalerVmeTime[15],
	scalerCodaID[16], scalerRunID[16], scalerSpillID[16], scalerROC[16], scalerValue[16], scalerChannelID[16], scalerVmeTime[16],
	scalerCodaID[17], scalerRunID[17], scalerSpillID[17], scalerROC[17], scalerValue[17], scalerChannelID[17], scalerVmeTime[17],
	scalerCodaID[18], scalerRunID[18], scalerSpillID[18], scalerROC[18], scalerValue[18], scalerChannelID[18], scalerVmeTime[18],
	scalerCodaID[19], scalerRunID[19], scalerSpillID[19], scalerROC[19], scalerValue[19], scalerChannelID[19], scalerVmeTime[19],
	scalerCodaID[20], scalerRunID[20], scalerSpillID[20], scalerROC[20], scalerValue[20], scalerChannelID[20], scalerVmeTime[20],
	scalerCodaID[21], scalerRunID[21], scalerSpillID[21], scalerROC[21], scalerValue[21], scalerChannelID[21], scalerVmeTime[21],
	scalerCodaID[22], scalerRunID[22], scalerSpillID[22], scalerROC[22], scalerValue[22], scalerChannelID[22], scalerVmeTime[22],
	scalerCodaID[23], scalerRunID[23], scalerSpillID[23], scalerROC[23], scalerValue[23], scalerChannelID[23], scalerVmeTime[23],
	scalerCodaID[24], scalerRunID[24], scalerSpillID[24], scalerROC[24], scalerValue[24], scalerChannelID[24], scalerVmeTime[24],
	scalerCodaID[25], scalerRunID[25], scalerSpillID[25], scalerROC[25], scalerValue[25], scalerChannelID[25], scalerVmeTime[25],
	scalerCodaID[26], scalerRunID[26], scalerSpillID[26], scalerROC[26], scalerValue[26], scalerChannelID[26], scalerVmeTime[26],
	scalerCodaID[27], scalerRunID[27], scalerSpillID[27], scalerROC[27], scalerValue[27], scalerChannelID[27], scalerVmeTime[27],
	scalerCodaID[28], scalerRunID[28], scalerSpillID[28], scalerROC[28], scalerValue[28], scalerChannelID[28], scalerVmeTime[28],
	scalerCodaID[29], scalerRunID[29], scalerSpillID[29], scalerROC[29], scalerValue[29], scalerChannelID[29], scalerVmeTime[29],
	scalerCodaID[30], scalerRunID[30], scalerSpillID[30], scalerROC[30], scalerValue[30], scalerChannelID[30], scalerVmeTime[30],
	scalerCodaID[31], scalerRunID[31], scalerSpillID[31], scalerROC[31], scalerValue[31], scalerChannelID[31], scalerVmeTime[31],
	scalerCodaID[32], scalerRunID[32], scalerSpillID[32], scalerROC[32], scalerValue[32], scalerChannelID[32], scalerVmeTime[32],
	scalerCodaID[33], scalerRunID[33], scalerSpillID[33], scalerROC[33], scalerValue[33], scalerChannelID[33], scalerVmeTime[33],
	scalerCodaID[34], scalerRunID[34], scalerSpillID[34], scalerROC[34], scalerValue[34], scalerChannelID[34], scalerVmeTime[34],
	scalerCodaID[35], scalerRunID[35], scalerSpillID[35], scalerROC[35], scalerValue[35], scalerChannelID[35], scalerVmeTime[35],
	scalerCodaID[36], scalerRunID[36], scalerSpillID[36], scalerROC[36], scalerValue[36], scalerChannelID[36], scalerVmeTime[36],
	scalerCodaID[37], scalerRunID[37], scalerSpillID[37], scalerROC[37], scalerValue[37], scalerChannelID[37], scalerVmeTime[37],
	scalerCodaID[38], scalerRunID[38], scalerSpillID[38], scalerROC[38], scalerValue[38], scalerChannelID[38], scalerVmeTime[38],
	scalerCodaID[39], scalerRunID[39], scalerSpillID[39], scalerROC[39], scalerValue[39], scalerChannelID[39], scalerVmeTime[39],
	scalerCodaID[40], scalerRunID[40], scalerSpillID[40], scalerROC[40], scalerValue[40], scalerChannelID[40], scalerVmeTime[40],
	scalerCodaID[41], scalerRunID[41], scalerSpillID[41], scalerROC[41], scalerValue[41], scalerChannelID[41], scalerVmeTime[41],
	scalerCodaID[42], scalerRunID[42], scalerSpillID[42], scalerROC[42], scalerValue[42], scalerChannelID[42], scalerVmeTime[42],
	scalerCodaID[43], scalerRunID[43], scalerSpillID[43], scalerROC[43], scalerValue[43], scalerChannelID[43], scalerVmeTime[43],
	scalerCodaID[44], scalerRunID[44], scalerSpillID[44], scalerROC[44], scalerValue[44], scalerChannelID[44], scalerVmeTime[44],
	scalerCodaID[45], scalerRunID[45], scalerSpillID[45], scalerROC[45], scalerValue[45], scalerChannelID[45], scalerVmeTime[45],
	scalerCodaID[46], scalerRunID[46], scalerSpillID[46], scalerROC[46], scalerValue[46], scalerChannelID[46], scalerVmeTime[46],
	scalerCodaID[47], scalerRunID[47], scalerSpillID[47], scalerROC[47], scalerValue[47], scalerChannelID[47], scalerVmeTime[47],
	scalerCodaID[48], scalerRunID[48], scalerSpillID[48], scalerROC[48], scalerValue[48], scalerChannelID[48], scalerVmeTime[48],
	scalerCodaID[49], scalerRunID[49], scalerSpillID[49], scalerROC[49], scalerValue[49], scalerChannelID[49], scalerVmeTime[49],
	scalerCodaID[50], scalerRunID[50], scalerSpillID[50], scalerROC[50], scalerValue[50], scalerChannelID[50], scalerVmeTime[50],
	scalerCodaID[51], scalerRunID[51], scalerSpillID[51], scalerROC[51], scalerValue[51], scalerChannelID[51], scalerVmeTime[51],
	scalerCodaID[52], scalerRunID[52], scalerSpillID[52], scalerROC[52], scalerValue[52], scalerChannelID[52], scalerVmeTime[52],
	scalerCodaID[53], scalerRunID[53], scalerSpillID[53], scalerROC[53], scalerValue[53], scalerChannelID[53], scalerVmeTime[53],
	scalerCodaID[54], scalerRunID[54], scalerSpillID[54], scalerROC[54], scalerValue[54], scalerChannelID[54], scalerVmeTime[54],
	scalerCodaID[55], scalerRunID[55], scalerSpillID[55], scalerROC[55], scalerValue[55], scalerChannelID[55], scalerVmeTime[55],
	scalerCodaID[56], scalerRunID[56], scalerSpillID[56], scalerROC[56], scalerValue[56], scalerChannelID[56], scalerVmeTime[56],
	scalerCodaID[57], scalerRunID[57], scalerSpillID[57], scalerROC[57], scalerValue[57], scalerChannelID[57], scalerVmeTime[57],
	scalerCodaID[58], scalerRunID[58], scalerSpillID[58], scalerROC[58], scalerValue[58], scalerChannelID[58], scalerVmeTime[58],
	scalerCodaID[59], scalerRunID[59], scalerSpillID[59], scalerROC[59], scalerValue[59], scalerChannelID[59], scalerVmeTime[59],
	scalerCodaID[60], scalerRunID[60], scalerSpillID[60], scalerROC[60], scalerValue[60], scalerChannelID[60], scalerVmeTime[60],
	scalerCodaID[61], scalerRunID[61], scalerSpillID[61], scalerROC[61], scalerValue[61], scalerChannelID[61], scalerVmeTime[61],
	scalerCodaID[62], scalerRunID[62], scalerSpillID[62], scalerROC[62], scalerValue[62], scalerChannelID[62], scalerVmeTime[62],
	scalerCodaID[63], scalerRunID[63], scalerSpillID[63], scalerROC[63], scalerValue[63], scalerChannelID[63], scalerVmeTime[63],
	scalerCodaID[64], scalerRunID[64], scalerSpillID[64], scalerROC[64], scalerValue[64], scalerChannelID[64], scalerVmeTime[64],
	scalerCodaID[65], scalerRunID[65], scalerSpillID[65], scalerROC[65], scalerValue[65], scalerChannelID[65], scalerVmeTime[65],
	scalerCodaID[66], scalerRunID[66], scalerSpillID[66], scalerROC[66], scalerValue[66], scalerChannelID[66], scalerVmeTime[66],
	scalerCodaID[67], scalerRunID[67], scalerSpillID[67], scalerROC[67], scalerValue[67], scalerChannelID[67], scalerVmeTime[67],
	scalerCodaID[68], scalerRunID[68], scalerSpillID[68], scalerROC[68], scalerValue[68], scalerChannelID[68], scalerVmeTime[68],
	scalerCodaID[69], scalerRunID[69], scalerSpillID[69], scalerROC[69], scalerValue[69], scalerChannelID[69], scalerVmeTime[69],
	scalerCodaID[70], scalerRunID[70], scalerSpillID[70], scalerROC[70], scalerValue[70], scalerChannelID[70], scalerVmeTime[70],
	scalerCodaID[71], scalerRunID[71], scalerSpillID[71], scalerROC[71], scalerValue[71], scalerChannelID[71], scalerVmeTime[71],
	scalerCodaID[72], scalerRunID[72], scalerSpillID[72], scalerROC[72], scalerValue[72], scalerChannelID[72], scalerVmeTime[72],
	scalerCodaID[73], scalerRunID[73], scalerSpillID[73], scalerROC[73], scalerValue[73], scalerChannelID[73], scalerVmeTime[73],
	scalerCodaID[74], scalerRunID[74], scalerSpillID[74], scalerROC[74], scalerValue[74], scalerChannelID[74], scalerVmeTime[74],
	scalerCodaID[75], scalerRunID[75], scalerSpillID[75], scalerROC[75], scalerValue[75], scalerChannelID[75], scalerVmeTime[75],
	scalerCodaID[76], scalerRunID[76], scalerSpillID[76], scalerROC[76], scalerValue[76], scalerChannelID[76], scalerVmeTime[76],
	scalerCodaID[77], scalerRunID[77], scalerSpillID[77], scalerROC[77], scalerValue[77], scalerChannelID[77], scalerVmeTime[77],
	scalerCodaID[78], scalerRunID[78], scalerSpillID[78], scalerROC[78], scalerValue[78], scalerChannelID[78], scalerVmeTime[78],
	scalerCodaID[79], scalerRunID[79], scalerSpillID[79], scalerROC[79], scalerValue[79], scalerChannelID[79], scalerVmeTime[79],
	scalerCodaID[80], scalerRunID[80], scalerSpillID[80], scalerROC[80], scalerValue[80], scalerChannelID[80], scalerVmeTime[80],
	scalerCodaID[81], scalerRunID[81], scalerSpillID[81], scalerROC[81], scalerValue[81], scalerChannelID[81], scalerVmeTime[81],
	scalerCodaID[82], scalerRunID[82], scalerSpillID[82], scalerROC[82], scalerValue[82], scalerChannelID[82], scalerVmeTime[82],
	scalerCodaID[83], scalerRunID[83], scalerSpillID[83], scalerROC[83], scalerValue[83], scalerChannelID[83], scalerVmeTime[83],
	scalerCodaID[84], scalerRunID[84], scalerSpillID[84], scalerROC[84], scalerValue[84], scalerChannelID[84], scalerVmeTime[84],
	scalerCodaID[85], scalerRunID[85], scalerSpillID[85], scalerROC[85], scalerValue[85], scalerChannelID[85], scalerVmeTime[85],
	scalerCodaID[86], scalerRunID[86], scalerSpillID[86], scalerROC[86], scalerValue[86], scalerChannelID[86], scalerVmeTime[86],
	scalerCodaID[87], scalerRunID[87], scalerSpillID[87], scalerROC[87], scalerValue[87], scalerChannelID[87], scalerVmeTime[87],
	scalerCodaID[88], scalerRunID[88], scalerSpillID[88], scalerROC[88], scalerValue[88], scalerChannelID[88], scalerVmeTime[88],
	scalerCodaID[89], scalerRunID[89], scalerSpillID[89], scalerROC[89], scalerValue[89], scalerChannelID[89], scalerVmeTime[89],
	scalerCodaID[90], scalerRunID[90], scalerSpillID[90], scalerROC[90], scalerValue[90], scalerChannelID[90], scalerVmeTime[90],
	scalerCodaID[91], scalerRunID[91], scalerSpillID[91], scalerROC[91], scalerValue[91], scalerChannelID[91], scalerVmeTime[91],
	scalerCodaID[92], scalerRunID[92], scalerSpillID[92], scalerROC[92], scalerValue[92], scalerChannelID[92], scalerVmeTime[92],
	scalerCodaID[93], scalerRunID[93], scalerSpillID[93], scalerROC[93], scalerValue[93], scalerChannelID[93], scalerVmeTime[93],
	scalerCodaID[94], scalerRunID[94], scalerSpillID[94], scalerROC[94], scalerValue[94], scalerChannelID[94], scalerVmeTime[94],
	scalerCodaID[95], scalerRunID[95], scalerSpillID[95], scalerROC[95], scalerValue[95], scalerChannelID[95], scalerVmeTime[95],
	scalerCodaID[96], scalerRunID[96], scalerSpillID[96], scalerROC[96], scalerValue[96], scalerChannelID[96], scalerVmeTime[96],
	scalerCodaID[97], scalerRunID[97], scalerSpillID[97], scalerROC[97], scalerValue[97], scalerChannelID[97], scalerVmeTime[97],
	scalerCodaID[98], scalerRunID[98], scalerSpillID[98], scalerROC[98], scalerValue[98], scalerChannelID[98], scalerVmeTime[98],
	scalerCodaID[99], scalerRunID[99], scalerSpillID[99], scalerROC[99], scalerValue[99], scalerChannelID[99], scalerVmeTime[99],
	scalerCodaID[100], scalerRunID[100], scalerSpillID[100], scalerROC[100], scalerValue[100], scalerChannelID[100], scalerVmeTime[100],
	scalerCodaID[101], scalerRunID[101], scalerSpillID[101], scalerROC[101], scalerValue[101], scalerChannelID[101], scalerVmeTime[101],
	scalerCodaID[102], scalerRunID[102], scalerSpillID[102], scalerROC[102], scalerValue[102], scalerChannelID[102], scalerVmeTime[102],
	scalerCodaID[103], scalerRunID[103], scalerSpillID[103], scalerROC[103], scalerValue[103], scalerChannelID[103], scalerVmeTime[103],
	scalerCodaID[104], scalerRunID[104], scalerSpillID[104], scalerROC[104], scalerValue[104], scalerChannelID[104], scalerVmeTime[104],
	scalerCodaID[105], scalerRunID[105], scalerSpillID[105], scalerROC[105], scalerValue[105], scalerChannelID[105], scalerVmeTime[105],
	scalerCodaID[106], scalerRunID[106], scalerSpillID[106], scalerROC[106], scalerValue[106], scalerChannelID[106], scalerVmeTime[106],
	scalerCodaID[107], scalerRunID[107], scalerSpillID[107], scalerROC[107], scalerValue[107], scalerChannelID[107], scalerVmeTime[107],
	scalerCodaID[108], scalerRunID[108], scalerSpillID[108], scalerROC[108], scalerValue[108], scalerChannelID[108], scalerVmeTime[108],
	scalerCodaID[109], scalerRunID[109], scalerSpillID[109], scalerROC[109], scalerValue[109], scalerChannelID[109], scalerVmeTime[109],
	scalerCodaID[110], scalerRunID[110], scalerSpillID[110], scalerROC[110], scalerValue[110], scalerChannelID[110], scalerVmeTime[110],
	scalerCodaID[111], scalerRunID[111], scalerSpillID[111], scalerROC[111], scalerValue[111], scalerChannelID[111], scalerVmeTime[111],
	scalerCodaID[112], scalerRunID[112], scalerSpillID[112], scalerROC[112], scalerValue[112], scalerChannelID[112], scalerVmeTime[112],
	scalerCodaID[113], scalerRunID[113], scalerSpillID[113], scalerROC[113], scalerValue[113], scalerChannelID[113], scalerVmeTime[113],
	scalerCodaID[114], scalerRunID[114], scalerSpillID[114], scalerROC[114], scalerValue[114], scalerChannelID[114], scalerVmeTime[114],
	scalerCodaID[115], scalerRunID[115], scalerSpillID[115], scalerROC[115], scalerValue[115], scalerChannelID[115], scalerVmeTime[115],
	scalerCodaID[116], scalerRunID[116], scalerSpillID[116], scalerROC[116], scalerValue[116], scalerChannelID[116], scalerVmeTime[116],
	scalerCodaID[117], scalerRunID[117], scalerSpillID[117], scalerROC[117], scalerValue[117], scalerChannelID[117], scalerVmeTime[117],
	scalerCodaID[118], scalerRunID[118], scalerSpillID[118], scalerROC[118], scalerValue[118], scalerChannelID[118], scalerVmeTime[118],
	scalerCodaID[119], scalerRunID[119], scalerSpillID[119], scalerROC[119], scalerValue[119], scalerChannelID[119], scalerVmeTime[119],
	scalerCodaID[120], scalerRunID[120], scalerSpillID[120], scalerROC[120], scalerValue[120], scalerChannelID[120], scalerVmeTime[120],
	scalerCodaID[121], scalerRunID[121], scalerSpillID[121], scalerROC[121], scalerValue[121], scalerChannelID[121], scalerVmeTime[121],
	scalerCodaID[122], scalerRunID[122], scalerSpillID[122], scalerROC[122], scalerValue[122], scalerChannelID[122], scalerVmeTime[122],
	scalerCodaID[123], scalerRunID[123], scalerSpillID[123], scalerROC[123], scalerValue[123], scalerChannelID[123], scalerVmeTime[123],
	scalerCodaID[124], scalerRunID[124], scalerSpillID[124], scalerROC[124], scalerValue[124], scalerChannelID[124], scalerVmeTime[124],
	scalerCodaID[125], scalerRunID[125], scalerSpillID[125], scalerROC[125], scalerValue[125], scalerChannelID[125], scalerVmeTime[125],
	scalerCodaID[126], scalerRunID[126], scalerSpillID[126], scalerROC[126], scalerValue[126], scalerChannelID[126], scalerVmeTime[126],
	scalerCodaID[127], scalerRunID[127], scalerSpillID[127], scalerROC[127], scalerValue[127], scalerChannelID[127], scalerVmeTime[127],
	scalerCodaID[128], scalerRunID[128], scalerSpillID[128], scalerROC[128], scalerValue[128], scalerChannelID[128], scalerVmeTime[128],
	scalerCodaID[129], scalerRunID[129], scalerSpillID[129], scalerROC[129], scalerValue[129], scalerChannelID[129], scalerVmeTime[129],
	scalerCodaID[130], scalerRunID[130], scalerSpillID[130], scalerROC[130], scalerValue[130], scalerChannelID[130], scalerVmeTime[130],
	scalerCodaID[131], scalerRunID[131], scalerSpillID[131], scalerROC[131], scalerValue[131], scalerChannelID[131], scalerVmeTime[131],
	scalerCodaID[132], scalerRunID[132], scalerSpillID[132], scalerROC[132], scalerValue[132], scalerChannelID[132], scalerVmeTime[132],
	scalerCodaID[133], scalerRunID[133], scalerSpillID[133], scalerROC[133], scalerValue[133], scalerChannelID[133], scalerVmeTime[133],
	scalerCodaID[134], scalerRunID[134], scalerSpillID[134], scalerROC[134], scalerValue[134], scalerChannelID[134], scalerVmeTime[134],
	scalerCodaID[135], scalerRunID[135], scalerSpillID[135], scalerROC[135], scalerValue[135], scalerChannelID[135], scalerVmeTime[135],
	scalerCodaID[136], scalerRunID[136], scalerSpillID[136], scalerROC[136], scalerValue[136], scalerChannelID[136], scalerVmeTime[136],
	scalerCodaID[137], scalerRunID[137], scalerSpillID[137], scalerROC[137], scalerValue[137], scalerChannelID[137], scalerVmeTime[137],
	scalerCodaID[138], scalerRunID[138], scalerSpillID[138], scalerROC[138], scalerValue[138], scalerChannelID[138], scalerVmeTime[138],
	scalerCodaID[139], scalerRunID[139], scalerSpillID[139], scalerROC[139], scalerValue[139], scalerChannelID[139], scalerVmeTime[139],
	scalerCodaID[140], scalerRunID[140], scalerSpillID[140], scalerROC[140], scalerValue[140], scalerChannelID[140], scalerVmeTime[140],
	scalerCodaID[141], scalerRunID[141], scalerSpillID[141], scalerROC[141], scalerValue[141], scalerChannelID[141], scalerVmeTime[141],
	scalerCodaID[142], scalerRunID[142], scalerSpillID[142], scalerROC[142], scalerValue[142], scalerChannelID[142], scalerVmeTime[142],
	scalerCodaID[143], scalerRunID[143], scalerSpillID[143], scalerROC[143], scalerValue[143], scalerChannelID[143], scalerVmeTime[143],
	scalerCodaID[144], scalerRunID[144], scalerSpillID[144], scalerROC[144], scalerValue[144], scalerChannelID[144], scalerVmeTime[144],
	scalerCodaID[145], scalerRunID[145], scalerSpillID[145], scalerROC[145], scalerValue[145], scalerChannelID[145], scalerVmeTime[145],
	scalerCodaID[146], scalerRunID[146], scalerSpillID[146], scalerROC[146], scalerValue[146], scalerChannelID[146], scalerVmeTime[146],
	scalerCodaID[147], scalerRunID[147], scalerSpillID[147], scalerROC[147], scalerValue[147], scalerChannelID[147], scalerVmeTime[147],
	scalerCodaID[148], scalerRunID[148], scalerSpillID[148], scalerROC[148], scalerValue[148], scalerChannelID[148], scalerVmeTime[148],
	scalerCodaID[149], scalerRunID[149], scalerSpillID[149], scalerROC[149], scalerValue[149], scalerChannelID[149], scalerVmeTime[149],
	scalerCodaID[150], scalerRunID[150], scalerSpillID[150], scalerROC[150], scalerValue[150], scalerChannelID[150], scalerVmeTime[150],
	scalerCodaID[151], scalerRunID[151], scalerSpillID[151], scalerROC[151], scalerValue[151], scalerChannelID[151], scalerVmeTime[151],
	scalerCodaID[152], scalerRunID[152], scalerSpillID[152], scalerROC[152], scalerValue[152], scalerChannelID[152], scalerVmeTime[152],
	scalerCodaID[153], scalerRunID[153], scalerSpillID[153], scalerROC[153], scalerValue[153], scalerChannelID[153], scalerVmeTime[153],
	scalerCodaID[154], scalerRunID[154], scalerSpillID[154], scalerROC[154], scalerValue[154], scalerChannelID[154], scalerVmeTime[154],
	scalerCodaID[155], scalerRunID[155], scalerSpillID[155], scalerROC[155], scalerValue[155], scalerChannelID[155], scalerVmeTime[155],
	scalerCodaID[156], scalerRunID[156], scalerSpillID[156], scalerROC[156], scalerValue[156], scalerChannelID[156], scalerVmeTime[156],
	scalerCodaID[157], scalerRunID[157], scalerSpillID[157], scalerROC[157], scalerValue[157], scalerChannelID[157], scalerVmeTime[157],
	scalerCodaID[158], scalerRunID[158], scalerSpillID[158], scalerROC[158], scalerValue[158], scalerChannelID[158], scalerVmeTime[158],
	scalerCodaID[159], scalerRunID[159], scalerSpillID[159], scalerROC[159], scalerValue[159], scalerChannelID[159], scalerVmeTime[159],
	scalerCodaID[160], scalerRunID[160], scalerSpillID[160], scalerROC[160], scalerValue[160], scalerChannelID[160], scalerVmeTime[160],
	scalerCodaID[161], scalerRunID[161], scalerSpillID[161], scalerROC[161], scalerValue[161], scalerChannelID[161], scalerVmeTime[161],
	scalerCodaID[162], scalerRunID[162], scalerSpillID[162], scalerROC[162], scalerValue[162], scalerChannelID[162], scalerVmeTime[162],
	scalerCodaID[163], scalerRunID[163], scalerSpillID[163], scalerROC[163], scalerValue[163], scalerChannelID[163], scalerVmeTime[163],
	scalerCodaID[164], scalerRunID[164], scalerSpillID[164], scalerROC[164], scalerValue[164], scalerChannelID[164], scalerVmeTime[164],
	scalerCodaID[165], scalerRunID[165], scalerSpillID[165], scalerROC[165], scalerValue[165], scalerChannelID[165], scalerVmeTime[165],
	scalerCodaID[166], scalerRunID[166], scalerSpillID[166], scalerROC[166], scalerValue[166], scalerChannelID[166], scalerVmeTime[166],
	scalerCodaID[167], scalerRunID[167], scalerSpillID[167], scalerROC[167], scalerValue[167], scalerChannelID[167], scalerVmeTime[167],
	scalerCodaID[168], scalerRunID[168], scalerSpillID[168], scalerROC[168], scalerValue[168], scalerChannelID[168], scalerVmeTime[168],
	scalerCodaID[169], scalerRunID[169], scalerSpillID[169], scalerROC[169], scalerValue[169], scalerChannelID[169], scalerVmeTime[169],
	scalerCodaID[170], scalerRunID[170], scalerSpillID[170], scalerROC[170], scalerValue[170], scalerChannelID[170], scalerVmeTime[170],
	scalerCodaID[171], scalerRunID[171], scalerSpillID[171], scalerROC[171], scalerValue[171], scalerChannelID[171], scalerVmeTime[171],
	scalerCodaID[172], scalerRunID[172], scalerSpillID[172], scalerROC[172], scalerValue[172], scalerChannelID[172], scalerVmeTime[172],
	scalerCodaID[173], scalerRunID[173], scalerSpillID[173], scalerROC[173], scalerValue[173], scalerChannelID[173], scalerVmeTime[173],
	scalerCodaID[174], scalerRunID[174], scalerSpillID[174], scalerROC[174], scalerValue[174], scalerChannelID[174], scalerVmeTime[174],
	scalerCodaID[175], scalerRunID[175], scalerSpillID[175], scalerROC[175], scalerValue[175], scalerChannelID[175], scalerVmeTime[175],
	scalerCodaID[176], scalerRunID[176], scalerSpillID[176], scalerROC[176], scalerValue[176], scalerChannelID[176], scalerVmeTime[176],
	scalerCodaID[177], scalerRunID[177], scalerSpillID[177], scalerROC[177], scalerValue[177], scalerChannelID[177], scalerVmeTime[177],
	scalerCodaID[178], scalerRunID[178], scalerSpillID[178], scalerROC[178], scalerValue[178], scalerChannelID[178], scalerVmeTime[178],
	scalerCodaID[179], scalerRunID[179], scalerSpillID[179], scalerROC[179], scalerValue[179], scalerChannelID[179], scalerVmeTime[179],
	scalerCodaID[180], scalerRunID[180], scalerSpillID[180], scalerROC[180], scalerValue[180], scalerChannelID[180], scalerVmeTime[180],
	scalerCodaID[181], scalerRunID[181], scalerSpillID[181], scalerROC[181], scalerValue[181], scalerChannelID[181], scalerVmeTime[181],
	scalerCodaID[182], scalerRunID[182], scalerSpillID[182], scalerROC[182], scalerValue[182], scalerChannelID[182], scalerVmeTime[182],
	scalerCodaID[183], scalerRunID[183], scalerSpillID[183], scalerROC[183], scalerValue[183], scalerChannelID[183], scalerVmeTime[183],
	scalerCodaID[184], scalerRunID[184], scalerSpillID[184], scalerROC[184], scalerValue[184], scalerChannelID[184], scalerVmeTime[184],
	scalerCodaID[185], scalerRunID[185], scalerSpillID[185], scalerROC[185], scalerValue[185], scalerChannelID[185], scalerVmeTime[185],
	scalerCodaID[186], scalerRunID[186], scalerSpillID[186], scalerROC[186], scalerValue[186], scalerChannelID[186], scalerVmeTime[186],
	scalerCodaID[187], scalerRunID[187], scalerSpillID[187], scalerROC[187], scalerValue[187], scalerChannelID[187], scalerVmeTime[187],
	scalerCodaID[188], scalerRunID[188], scalerSpillID[188], scalerROC[188], scalerValue[188], scalerChannelID[188], scalerVmeTime[188],
	scalerCodaID[189], scalerRunID[189], scalerSpillID[189], scalerROC[189], scalerValue[189], scalerChannelID[189], scalerVmeTime[189],
	scalerCodaID[190], scalerRunID[190], scalerSpillID[190], scalerROC[190], scalerValue[190], scalerChannelID[190], scalerVmeTime[190],
	scalerCodaID[191], scalerRunID[191], scalerSpillID[191], scalerROC[191], scalerValue[191], scalerChannelID[191], scalerVmeTime[191],
	scalerCodaID[192], scalerRunID[192], scalerSpillID[192], scalerROC[192], scalerValue[192], scalerChannelID[192], scalerVmeTime[192],
	scalerCodaID[193], scalerRunID[193], scalerSpillID[193], scalerROC[193], scalerValue[193], scalerChannelID[193], scalerVmeTime[193],
	scalerCodaID[194], scalerRunID[194], scalerSpillID[194], scalerROC[194], scalerValue[194], scalerChannelID[194], scalerVmeTime[194],
	scalerCodaID[195], scalerRunID[195], scalerSpillID[195], scalerROC[195], scalerValue[195], scalerChannelID[195], scalerVmeTime[195],
	scalerCodaID[196], scalerRunID[196], scalerSpillID[196], scalerROC[196], scalerValue[196], scalerChannelID[196], scalerVmeTime[196],
	scalerCodaID[197], scalerRunID[197], scalerSpillID[197], scalerROC[197], scalerValue[197], scalerChannelID[197], scalerVmeTime[197],
	scalerCodaID[198], scalerRunID[198], scalerSpillID[198], scalerROC[198], scalerValue[198], scalerChannelID[198], scalerVmeTime[198],
	scalerCodaID[199], scalerRunID[199], scalerSpillID[199], scalerROC[199], scalerValue[199], scalerChannelID[199], scalerVmeTime[199]);

	
	if (mysql_query(conn, sqlScalerQuery))
	{
		printf("Error: %s\n", mysql_error(conn));
		return 1;
	}
	
	
	// Clear the arrays
	memset((void*)&scalerChannelID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerCodaID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerRunID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerSpillID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerROC, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerValue, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerVmeTime, 0, sizeof(int)*max_scaler_rows);

	// Clear the query string
	sprintf(sqlScalerQuery,"");
	
	if (mysql_query(conn, "UPDATE tempScaler t, Mapping m\n"
                "SET t.detectorName = m.detectorName, t.elementID = m.elementID\n"
                "WHERE t.detectorName IS NULL AND t.rocID = m.rocID AND "
                "t.channelID = m.channelID") )
        {
                printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO Scaler (eventID, runID, spillID, "
		"rocID, channelID, detectorName, elementID, value, vmeTime) "
		"SELECT eventID, runID, spillID, rocID, channelID, "
		"detectorName, elementID, value, vmeTime FROM tempScaler") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempScaler") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }
	
	return 0;

}

int send_final_tdc(MYSQL *conn){
	
	int i;
	char sqlTDCQuery[10000000];

	if (mysql_query(conn, "DELETE FROM tempTDC") )
	{
		printf("Prior Delete tempTDC Error: %s\n", mysql_error(conn));
                return 1;
        }

	sprintf(sqlTDCQuery, "INSERT INTO tempTDC (eventID, runID, spillID, rocID, boardID, "
		"channelID, tdcTime, signalWidth, vmeTime) "
		"VALUES (%i, %i, %i, %i, %i, %i, %f, %f, %f)",
		tdcCodaID[0], tdcRunID[0], tdcSpillID[0], tdcROC[0],
		tdcBoardID[0], tdcChannelID[0], tdcStopTime[0], tdcSignalWidth[0],
		tdcVmeTime[0]);

	for (i=1;i<tdcCount;i++) {
		sprintf(sqlTDCQuery,"%s, (%i, %i, %i, %i, %i, %i, %f, %f, %f)", sqlTDCQuery, 
			tdcCodaID[i], tdcRunID[i], tdcSpillID[i], tdcROC[i], 
			tdcBoardID[i], tdcChannelID[i], tdcStopTime[i], 
			tdcSignalWidth[i], tdcVmeTime[i]);
	}

	if( mysql_query(conn, sqlTDCQuery) ) {
		printf("Final TDC Submit Error: %s\n", mysql_error(conn));
		return 1;
   	}

	memset((void*)&tdcChannelID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcCodaID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcRunID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcSpillID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcROC, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcBoardID, 0, sizeof(int)*max_tdc_rows);
	memset((void*)&tdcStopTime, 0, sizeof(double)*max_tdc_rows);
	memset((void*)&tdcSignalWidth, 0, sizeof(double)*max_tdc_rows);
	memset((void*)&tdcVmeTime, 0, sizeof(double)*max_tdc_rows);

	if (mysql_query(conn, "UPDATE tempTDC t, Mapping m\n"
                "SET t.detectorName = m.detectorName, t.elementID = m.elementID\n"
                "WHERE t.detectorName IS NULL AND t.rocID = m.rocID AND "
                "t.boardID = m.boardID AND t.channelID = m.channelID") )
        {
                printf("tempTDC Mapping Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "UPDATE tempTDC\n"
                "SET driftDistance = tdcTime*0.005") )
        {
                printf("Error (101): %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO Hit (eventID, runID, spillID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, driftDistance, signalWidth, vmeTime) "
		"SELECT eventID, runID, spillID, rocID, boardID, channelID, "
		"detectorName, elementID, tdcTime, driftDistance, signalWidth, vmeTime FROM tempTDC") )
	{
		printf("tempTDC to Hit Insert Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempTDC") )
	{
		printf("Delete tempTDC Error: %s\n", mysql_error(conn));
                return 1;
        }

	return;
}

int send_final_v1495(MYSQL *conn){
	
	int i;
	char sqlv1495Query[10000000];

	if (mysql_query(conn, "DELETE FROM tempv1495") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	sprintf(sqlv1495Query, "INSERT INTO tempv1495 (eventID, "
		"runID, spillID, rocID, boardID, channelID, tdcTime, vmeTime) "
		"VALUES (%i, %i, %i, %i, %i, %i, %f, %f)", 
		v1495CodaID[0], v1495RunID[0], v1495SpillID[0], v1495ROC[0],
		v1495BoardID[0], v1495ChannelID[0], v1495StopTime[0], v1495VmeTime[0]);

	for (i=1;i<v1495Count;i++) {
		sprintf(sqlv1495Query,"%s, (%i, %i, %i, %i, %i, %i, %f, %f)", sqlv1495Query, 
			v1495CodaID[i], v1495RunID[i], v1495SpillID[i], v1495ROC[i], 
			v1495BoardID[i], v1495ChannelID[i], v1495StopTime[i], 
			v1495VmeTime[i]);
	}

	if( mysql_query(conn, sqlv1495Query) ) {
		printf("%s\n", sqlv1495Query);
		printf("Error: %s\n", mysql_error(conn));
		return 1;
   	}

	memset((void*)&v1495ChannelID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495CodaID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495RunID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495SpillID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495ROC, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495BoardID, 0, sizeof(int)*max_v1495_rows);
	memset((void*)&v1495StopTime, 0, sizeof(double)*max_v1495_rows);
	memset((void*)&v1495VmeTime, 0, sizeof(double)*max_v1495_rows);

	if (mysql_query(conn, "UPDATE tempv1495 t, Mapping m\n"
                "SET t.detectorName = m.detectorName, t.elementID = m.elementID\n"
                "WHERE t.detectorName IS NULL AND t.rocID = m.rocID AND "
                "t.boardID = m.boardID AND t.channelID = m.channelID") )
        {
                printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO TriggerHit (eventID, runID, spillID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, vmeTime) "
		"SELECT eventID, runID, spillID, rocID, boardID, channelID, "
		"detectorName, elementID, tdcTime, vmeTime FROM tempv1495") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempv1495") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	return 0;
}

int send_final_latch(MYSQL *conn){

	int i;
	char sqlLatchQuery[100000];

	if( mysql_query(conn, "DELETE FROM tempLatch") ) {
		printf("Error: %s\n", mysql_error(conn));
		return 1;
   	}

	sprintf(sqlLatchQuery, "INSERT INTO tempLatch (runID, spillID, eventID, vmeTime, "
		"rocID, boardID, channelID, detectorName, elementID) VALUES "
		"(%i, %i, %i, %i, %i, %i, %i, NULL, NULL)",
		latchRunID[0], latchSpillID[0], latchCodaID[0], latchVmeTime[0], latchROC[0], latchBoardID[0],
		latchChannelID[0]);

	for (i=1;i<latchCount;i++) {
		sprintf(sqlLatchQuery,"%s, (%i, %i, %i, %i, %i, %i, %i, NULL, NULL)",
			sqlLatchQuery, latchRunID[i], latchSpillID[i], latchCodaID[i], latchVmeTime[i], 
			latchROC[i], latchBoardID[i], latchChannelID[i]);
	}

	if( mysql_query(conn, sqlLatchQuery) ) {
		printf("Latch Final Insert Error: %s\n", mysql_error(conn));
		return 1;
   	}

	sprintf(sqlLatchQuery,"");

	memset((void*)&latchChannelID, 0, sizeof(int)*100);
	memset((void*)&latchRocID, 0, sizeof(int)*100);
	memset((void*)&latchCodaID, 0, sizeof(int)*100);
	memset((void*)&latchRunID, 0, sizeof(int)*100);
	memset((void*)&latchSpillID, 0, sizeof(int)*100);
	memset((void*)&latchROC, 0, sizeof(int)*100);
	memset((void*)&latchBoardID, 0, sizeof(int)*100);
	memset((void*)&latchVmeTime, 0, sizeof(int)*100);

	if (mysql_query(conn, "UPDATE tempLatch l, Mapping m\n"
                "SET l.detectorName = m.detectorName, l.elementID = m.elementID\n"
                "WHERE l.detectorName IS NULL AND l.rocID = m.rocID AND "
                "l.boardID = m.boardID AND l.channelID = m.channelID") )
        {
                printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO Hit (runID, spillID, eventID, vmeTime, "
		"rocID, boardID, channelID, detectorName, elementID) "
		"SELECT runID, spillID, eventID, vmeTime, rocID, boardID, channelID, "
		"detectorName, elementID FROM tempLatch") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempLatch") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	return 0;
}

int send_final_scaler(MYSQL *conn){
	
	char sqlScalerQuery[100000];
	int i;

	if (mysql_query(conn, "DELETE FROM tempScaler") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	sprintf(sqlScalerQuery, "INSERT INTO tempScaler (eventID, runID, spillID, rocID, "
		"channelID, value, vmeTime) VALUES (%i, %i, %i, %i, %i, %i, %i)", 
		scalerCodaID[0], scalerRunID[0], scalerSpillID[0], scalerROC[0], scalerChannelID[0], 
		scalerValue[0], scalerVmeTime[0]);

	for (i=1;i<scalerCount;i++) {
		sprintf(sqlScalerQuery,"%s, (%i, %i, %i, %i, %i, %i, %i)", sqlScalerQuery, 
			scalerCodaID[i], scalerRunID[i], scalerSpillID[i], scalerROC[i], 
			scalerChannelID[i], scalerValue[i], scalerVmeTime[i]);
	}

	if( mysql_query(conn, sqlScalerQuery) ) {
		printf("%s\n", sqlScalerQuery);
		printf("Error: %s\n", mysql_error(conn));
		return 1;
   	}

	memset((void*)&scalerChannelID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerCodaID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerRunID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerSpillID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerROC, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerValue, 0, sizeof(double)*max_scaler_rows);
	memset((void*)&scalerVmeTime, 0, sizeof(double)*max_scaler_rows);

	if (mysql_query(conn, "UPDATE tempScaler t, Mapping m\n"
                "SET t.detectorName = m.detectorName, t.elementID = m.elementID\n"
                "WHERE t.detectorName IS NULL AND t.rocID = m.rocID AND "
                "t.channelID = m.channelID") )
        {
                printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO Scaler (eventID, runID, spillID, "
		"rocID, channelID, detectorName, elementID, value, vmeTime) "
		"SELECT eventID, runID, spillID, rocID, channelID, "
		"detectorName, elementID, value, vmeTime FROM tempScaler") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempScaler") )
	{
		printf("Error: %s\n", mysql_error(conn));
                return 1;
        }

	return 0;
}

// ================================================================
// END OF FUNCTION DEFINITIONS
// ================================================================
// ================================================================
// END OF CODE
// ================================================================



/*
	sprintf(sqlTDCQuery, "INSERT INTO tempTDC (eventID, runID, spillID, rocID, boardID, channelID, "
	"tdcTime, signalWidth, vmeTime) "
	"VALUES (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f, %f)",
	tdcCodaID[0], tdcRunID[0], tdcSpillID[0], tdcROC[0], tdcBoardID[0], tdcChannelID[0], tdcStopTime[0], tdcSignalWidth[0], tdcVmeTime[0],
	tdcCodaID[1], tdcRunID[1], tdcSpillID[1], tdcROC[1], tdcBoardID[1], tdcChannelID[1], tdcStopTime[1], tdcSignalWidth[1], tdcVmeTime[1],
	tdcCodaID[2], tdcRunID[2], tdcSpillID[2], tdcROC[2], tdcBoardID[2], tdcChannelID[2], tdcStopTime[2], tdcSignalWidth[2], tdcVmeTime[2],
	tdcCodaID[3], tdcRunID[3], tdcSpillID[3], tdcROC[3], tdcBoardID[3], tdcChannelID[3], tdcStopTime[3], tdcSignalWidth[3], tdcVmeTime[3],
	tdcCodaID[4], tdcRunID[4], tdcSpillID[4], tdcROC[4], tdcBoardID[4], tdcChannelID[4], tdcStopTime[4], tdcSignalWidth[4], tdcVmeTime[4],
	tdcCodaID[5], tdcRunID[5], tdcSpillID[5], tdcROC[5], tdcBoardID[5], tdcChannelID[5], tdcStopTime[5], tdcSignalWidth[5], tdcVmeTime[5],
	tdcCodaID[6], tdcRunID[6], tdcSpillID[6], tdcROC[6], tdcBoardID[6], tdcChannelID[6], tdcStopTime[6], tdcSignalWidth[6], tdcVmeTime[6],
	tdcCodaID[7], tdcRunID[7], tdcSpillID[7], tdcROC[7], tdcBoardID[7], tdcChannelID[7], tdcStopTime[7], tdcSignalWidth[7], tdcVmeTime[7],
	tdcCodaID[8], tdcRunID[8], tdcSpillID[8], tdcROC[8], tdcBoardID[8], tdcChannelID[8], tdcStopTime[8], tdcSignalWidth[8], tdcVmeTime[8],
	tdcCodaID[9], tdcRunID[9], tdcSpillID[9], tdcROC[9], tdcBoardID[9], tdcChannelID[9], tdcStopTime[9], tdcSignalWidth[9], tdcVmeTime[9],
	tdcCodaID[10], tdcRunID[10], tdcSpillID[10], tdcROC[10], tdcBoardID[10], tdcChannelID[10], tdcStopTime[10], tdcSignalWidth[10], tdcVmeTime[10],
	tdcCodaID[11], tdcRunID[11], tdcSpillID[11], tdcROC[11], tdcBoardID[11], tdcChannelID[11], tdcStopTime[11], tdcSignalWidth[11], tdcVmeTime[11],
	tdcCodaID[12], tdcRunID[12], tdcSpillID[12], tdcROC[12], tdcBoardID[12], tdcChannelID[12], tdcStopTime[12], tdcSignalWidth[12], tdcVmeTime[12],
	tdcCodaID[13], tdcRunID[13], tdcSpillID[13], tdcROC[13], tdcBoardID[13], tdcChannelID[13], tdcStopTime[13], tdcSignalWidth[13], tdcVmeTime[13],
	tdcCodaID[14], tdcRunID[14], tdcSpillID[14], tdcROC[14], tdcBoardID[14], tdcChannelID[14], tdcStopTime[14], tdcSignalWidth[14], tdcVmeTime[14],
	tdcCodaID[15], tdcRunID[15], tdcSpillID[15], tdcROC[15], tdcBoardID[15], tdcChannelID[15], tdcStopTime[15], tdcSignalWidth[15], tdcVmeTime[15],
	tdcCodaID[16], tdcRunID[16], tdcSpillID[16], tdcROC[16], tdcBoardID[16], tdcChannelID[16], tdcStopTime[16], tdcSignalWidth[16], tdcVmeTime[16],
	tdcCodaID[17], tdcRunID[17], tdcSpillID[17], tdcROC[17], tdcBoardID[17], tdcChannelID[17], tdcStopTime[17], tdcSignalWidth[17], tdcVmeTime[17],
	tdcCodaID[18], tdcRunID[18], tdcSpillID[18], tdcROC[18], tdcBoardID[18], tdcChannelID[18], tdcStopTime[18], tdcSignalWidth[18], tdcVmeTime[18],
	tdcCodaID[19], tdcRunID[19], tdcSpillID[19], tdcROC[19], tdcBoardID[19], tdcChannelID[19], tdcStopTime[19], tdcSignalWidth[19], tdcVmeTime[19],
	tdcCodaID[20], tdcRunID[20], tdcSpillID[20], tdcROC[20], tdcBoardID[20], tdcChannelID[20], tdcStopTime[20], tdcSignalWidth[20], tdcVmeTime[20],
	tdcCodaID[21], tdcRunID[21], tdcSpillID[21], tdcROC[21], tdcBoardID[21], tdcChannelID[21], tdcStopTime[21], tdcSignalWidth[21], tdcVmeTime[21],
	tdcCodaID[22], tdcRunID[22], tdcSpillID[22], tdcROC[22], tdcBoardID[22], tdcChannelID[22], tdcStopTime[22], tdcSignalWidth[22], tdcVmeTime[22],
	tdcCodaID[23], tdcRunID[23], tdcSpillID[23], tdcROC[23], tdcBoardID[23], tdcChannelID[23], tdcStopTime[23], tdcSignalWidth[23], tdcVmeTime[23],
	tdcCodaID[24], tdcRunID[24], tdcSpillID[24], tdcROC[24], tdcBoardID[24], tdcChannelID[24], tdcStopTime[24], tdcSignalWidth[24], tdcVmeTime[24],
	tdcCodaID[25], tdcRunID[25], tdcSpillID[25], tdcROC[25], tdcBoardID[25], tdcChannelID[25], tdcStopTime[25], tdcSignalWidth[25], tdcVmeTime[25],
	tdcCodaID[26], tdcRunID[26], tdcSpillID[26], tdcROC[26], tdcBoardID[26], tdcChannelID[26], tdcStopTime[26], tdcSignalWidth[26], tdcVmeTime[26],
	tdcCodaID[27], tdcRunID[27], tdcSpillID[27], tdcROC[27], tdcBoardID[27], tdcChannelID[27], tdcStopTime[27], tdcSignalWidth[27], tdcVmeTime[27],
	tdcCodaID[28], tdcRunID[28], tdcSpillID[28], tdcROC[28], tdcBoardID[28], tdcChannelID[28], tdcStopTime[28], tdcSignalWidth[28], tdcVmeTime[28],
	tdcCodaID[29], tdcRunID[29], tdcSpillID[29], tdcROC[29], tdcBoardID[29], tdcChannelID[29], tdcStopTime[29], tdcSignalWidth[29], tdcVmeTime[29],
	tdcCodaID[30], tdcRunID[30], tdcSpillID[30], tdcROC[30], tdcBoardID[30], tdcChannelID[30], tdcStopTime[30], tdcSignalWidth[30], tdcVmeTime[30],
	tdcCodaID[31], tdcRunID[31], tdcSpillID[31], tdcROC[31], tdcBoardID[31], tdcChannelID[31], tdcStopTime[31], tdcSignalWidth[31], tdcVmeTime[31],
	tdcCodaID[32], tdcRunID[32], tdcSpillID[32], tdcROC[32], tdcBoardID[32], tdcChannelID[32], tdcStopTime[32], tdcSignalWidth[32], tdcVmeTime[32],
	tdcCodaID[33], tdcRunID[33], tdcSpillID[33], tdcROC[33], tdcBoardID[33], tdcChannelID[33], tdcStopTime[33], tdcSignalWidth[33], tdcVmeTime[33],
	tdcCodaID[34], tdcRunID[34], tdcSpillID[34], tdcROC[34], tdcBoardID[34], tdcChannelID[34], tdcStopTime[34], tdcSignalWidth[34], tdcVmeTime[34],
	tdcCodaID[35], tdcRunID[35], tdcSpillID[35], tdcROC[35], tdcBoardID[35], tdcChannelID[35], tdcStopTime[35], tdcSignalWidth[35], tdcVmeTime[35],
	tdcCodaID[36], tdcRunID[36], tdcSpillID[36], tdcROC[36], tdcBoardID[36], tdcChannelID[36], tdcStopTime[36], tdcSignalWidth[36], tdcVmeTime[36],
	tdcCodaID[37], tdcRunID[37], tdcSpillID[37], tdcROC[37], tdcBoardID[37], tdcChannelID[37], tdcStopTime[37], tdcSignalWidth[37], tdcVmeTime[37],
	tdcCodaID[38], tdcRunID[38], tdcSpillID[38], tdcROC[38], tdcBoardID[38], tdcChannelID[38], tdcStopTime[38], tdcSignalWidth[38], tdcVmeTime[38],
	tdcCodaID[39], tdcRunID[39], tdcSpillID[39], tdcROC[39], tdcBoardID[39], tdcChannelID[39], tdcStopTime[39], tdcSignalWidth[39], tdcVmeTime[39],
	tdcCodaID[40], tdcRunID[40], tdcSpillID[40], tdcROC[40], tdcBoardID[40], tdcChannelID[40], tdcStopTime[40], tdcSignalWidth[40], tdcVmeTime[40],
	tdcCodaID[41], tdcRunID[41], tdcSpillID[41], tdcROC[41], tdcBoardID[41], tdcChannelID[41], tdcStopTime[41], tdcSignalWidth[41], tdcVmeTime[41],
	tdcCodaID[42], tdcRunID[42], tdcSpillID[42], tdcROC[42], tdcBoardID[42], tdcChannelID[42], tdcStopTime[42], tdcSignalWidth[42], tdcVmeTime[42],
	tdcCodaID[43], tdcRunID[43], tdcSpillID[43], tdcROC[43], tdcBoardID[43], tdcChannelID[43], tdcStopTime[43], tdcSignalWidth[43], tdcVmeTime[43],
	tdcCodaID[44], tdcRunID[44], tdcSpillID[44], tdcROC[44], tdcBoardID[44], tdcChannelID[44], tdcStopTime[44], tdcSignalWidth[44], tdcVmeTime[44],
	tdcCodaID[45], tdcRunID[45], tdcSpillID[45], tdcROC[45], tdcBoardID[45], tdcChannelID[45], tdcStopTime[45], tdcSignalWidth[45], tdcVmeTime[45],
	tdcCodaID[46], tdcRunID[46], tdcSpillID[46], tdcROC[46], tdcBoardID[46], tdcChannelID[46], tdcStopTime[46], tdcSignalWidth[46], tdcVmeTime[46],
	tdcCodaID[47], tdcRunID[47], tdcSpillID[47], tdcROC[47], tdcBoardID[47], tdcChannelID[47], tdcStopTime[47], tdcSignalWidth[47], tdcVmeTime[47],
	tdcCodaID[48], tdcRunID[48], tdcSpillID[48], tdcROC[48], tdcBoardID[48], tdcChannelID[48], tdcStopTime[48], tdcSignalWidth[48], tdcVmeTime[48],
	tdcCodaID[49], tdcRunID[49], tdcSpillID[49], tdcROC[49], tdcBoardID[49], tdcChannelID[49], tdcStopTime[49], tdcSignalWidth[49], tdcVmeTime[49],
	tdcCodaID[50], tdcRunID[50], tdcSpillID[50], tdcROC[50], tdcBoardID[50], tdcChannelID[50], tdcStopTime[50], tdcSignalWidth[50], tdcVmeTime[50],
	tdcCodaID[51], tdcRunID[51], tdcSpillID[51], tdcROC[51], tdcBoardID[51], tdcChannelID[51], tdcStopTime[51], tdcSignalWidth[51], tdcVmeTime[51],
	tdcCodaID[52], tdcRunID[52], tdcSpillID[52], tdcROC[52], tdcBoardID[52], tdcChannelID[52], tdcStopTime[52], tdcSignalWidth[52], tdcVmeTime[52],
	tdcCodaID[53], tdcRunID[53], tdcSpillID[53], tdcROC[53], tdcBoardID[53], tdcChannelID[53], tdcStopTime[53], tdcSignalWidth[53], tdcVmeTime[53],
	tdcCodaID[54], tdcRunID[54], tdcSpillID[54], tdcROC[54], tdcBoardID[54], tdcChannelID[54], tdcStopTime[54], tdcSignalWidth[54], tdcVmeTime[54],
	tdcCodaID[55], tdcRunID[55], tdcSpillID[55], tdcROC[55], tdcBoardID[55], tdcChannelID[55], tdcStopTime[55], tdcSignalWidth[55], tdcVmeTime[55],
	tdcCodaID[56], tdcRunID[56], tdcSpillID[56], tdcROC[56], tdcBoardID[56], tdcChannelID[56], tdcStopTime[56], tdcSignalWidth[56], tdcVmeTime[56],
	tdcCodaID[57], tdcRunID[57], tdcSpillID[57], tdcROC[57], tdcBoardID[57], tdcChannelID[57], tdcStopTime[57], tdcSignalWidth[57], tdcVmeTime[57],
	tdcCodaID[58], tdcRunID[58], tdcSpillID[58], tdcROC[58], tdcBoardID[58], tdcChannelID[58], tdcStopTime[58], tdcSignalWidth[58], tdcVmeTime[58],
	tdcCodaID[59], tdcRunID[59], tdcSpillID[59], tdcROC[59], tdcBoardID[59], tdcChannelID[59], tdcStopTime[59], tdcSignalWidth[59], tdcVmeTime[59],
	tdcCodaID[60], tdcRunID[60], tdcSpillID[60], tdcROC[60], tdcBoardID[60], tdcChannelID[60], tdcStopTime[60], tdcSignalWidth[60], tdcVmeTime[60],
	tdcCodaID[61], tdcRunID[61], tdcSpillID[61], tdcROC[61], tdcBoardID[61], tdcChannelID[61], tdcStopTime[61], tdcSignalWidth[61], tdcVmeTime[61],
	tdcCodaID[62], tdcRunID[62], tdcSpillID[62], tdcROC[62], tdcBoardID[62], tdcChannelID[62], tdcStopTime[62], tdcSignalWidth[62], tdcVmeTime[62],
	tdcCodaID[63], tdcRunID[63], tdcSpillID[63], tdcROC[63], tdcBoardID[63], tdcChannelID[63], tdcStopTime[63], tdcSignalWidth[63], tdcVmeTime[63],
	tdcCodaID[64], tdcRunID[64], tdcSpillID[64], tdcROC[64], tdcBoardID[64], tdcChannelID[64], tdcStopTime[64], tdcSignalWidth[64], tdcVmeTime[64],
	tdcCodaID[65], tdcRunID[65], tdcSpillID[65], tdcROC[65], tdcBoardID[65], tdcChannelID[65], tdcStopTime[65], tdcSignalWidth[65], tdcVmeTime[65],
	tdcCodaID[66], tdcRunID[66], tdcSpillID[66], tdcROC[66], tdcBoardID[66], tdcChannelID[66], tdcStopTime[66], tdcSignalWidth[66], tdcVmeTime[66],
	tdcCodaID[67], tdcRunID[67], tdcSpillID[67], tdcROC[67], tdcBoardID[67], tdcChannelID[67], tdcStopTime[67], tdcSignalWidth[67], tdcVmeTime[67],
	tdcCodaID[68], tdcRunID[68], tdcSpillID[68], tdcROC[68], tdcBoardID[68], tdcChannelID[68], tdcStopTime[68], tdcSignalWidth[68], tdcVmeTime[68],
	tdcCodaID[69], tdcRunID[69], tdcSpillID[69], tdcROC[69], tdcBoardID[69], tdcChannelID[69], tdcStopTime[69], tdcSignalWidth[69], tdcVmeTime[69],
	tdcCodaID[70], tdcRunID[70], tdcSpillID[70], tdcROC[70], tdcBoardID[70], tdcChannelID[70], tdcStopTime[70], tdcSignalWidth[70], tdcVmeTime[70],
	tdcCodaID[71], tdcRunID[71], tdcSpillID[71], tdcROC[71], tdcBoardID[71], tdcChannelID[71], tdcStopTime[71], tdcSignalWidth[71], tdcVmeTime[71],
	tdcCodaID[72], tdcRunID[72], tdcSpillID[72], tdcROC[72], tdcBoardID[72], tdcChannelID[72], tdcStopTime[72], tdcSignalWidth[72], tdcVmeTime[72],
	tdcCodaID[73], tdcRunID[73], tdcSpillID[73], tdcROC[73], tdcBoardID[73], tdcChannelID[73], tdcStopTime[73], tdcSignalWidth[73], tdcVmeTime[73],
	tdcCodaID[74], tdcRunID[74], tdcSpillID[74], tdcROC[74], tdcBoardID[74], tdcChannelID[74], tdcStopTime[74], tdcSignalWidth[74], tdcVmeTime[74],
	tdcCodaID[75], tdcRunID[75], tdcSpillID[75], tdcROC[75], tdcBoardID[75], tdcChannelID[75], tdcStopTime[75], tdcSignalWidth[75], tdcVmeTime[75],
	tdcCodaID[76], tdcRunID[76], tdcSpillID[76], tdcROC[76], tdcBoardID[76], tdcChannelID[76], tdcStopTime[76], tdcSignalWidth[76], tdcVmeTime[76],
	tdcCodaID[77], tdcRunID[77], tdcSpillID[77], tdcROC[77], tdcBoardID[77], tdcChannelID[77], tdcStopTime[77], tdcSignalWidth[77], tdcVmeTime[77],
	tdcCodaID[78], tdcRunID[78], tdcSpillID[78], tdcROC[78], tdcBoardID[78], tdcChannelID[78], tdcStopTime[78], tdcSignalWidth[78], tdcVmeTime[78],
	tdcCodaID[79], tdcRunID[79], tdcSpillID[79], tdcROC[79], tdcBoardID[79], tdcChannelID[79], tdcStopTime[79], tdcSignalWidth[79], tdcVmeTime[79],
	tdcCodaID[80], tdcRunID[80], tdcSpillID[80], tdcROC[80], tdcBoardID[80], tdcChannelID[80], tdcStopTime[80], tdcSignalWidth[80], tdcVmeTime[80],
	tdcCodaID[81], tdcRunID[81], tdcSpillID[81], tdcROC[81], tdcBoardID[81], tdcChannelID[81], tdcStopTime[81], tdcSignalWidth[81], tdcVmeTime[81],
	tdcCodaID[82], tdcRunID[82], tdcSpillID[82], tdcROC[82], tdcBoardID[82], tdcChannelID[82], tdcStopTime[82], tdcSignalWidth[82], tdcVmeTime[82],
	tdcCodaID[83], tdcRunID[83], tdcSpillID[83], tdcROC[83], tdcBoardID[83], tdcChannelID[83], tdcStopTime[83], tdcSignalWidth[83], tdcVmeTime[83],
	tdcCodaID[84], tdcRunID[84], tdcSpillID[84], tdcROC[84], tdcBoardID[84], tdcChannelID[84], tdcStopTime[84], tdcSignalWidth[84], tdcVmeTime[84],
	tdcCodaID[85], tdcRunID[85], tdcSpillID[85], tdcROC[85], tdcBoardID[85], tdcChannelID[85], tdcStopTime[85], tdcSignalWidth[85], tdcVmeTime[85],
	tdcCodaID[86], tdcRunID[86], tdcSpillID[86], tdcROC[86], tdcBoardID[86], tdcChannelID[86], tdcStopTime[86], tdcSignalWidth[86], tdcVmeTime[86],
	tdcCodaID[87], tdcRunID[87], tdcSpillID[87], tdcROC[87], tdcBoardID[87], tdcChannelID[87], tdcStopTime[87], tdcSignalWidth[87], tdcVmeTime[87],
	tdcCodaID[88], tdcRunID[88], tdcSpillID[88], tdcROC[88], tdcBoardID[88], tdcChannelID[88], tdcStopTime[88], tdcSignalWidth[88], tdcVmeTime[88],
	tdcCodaID[89], tdcRunID[89], tdcSpillID[89], tdcROC[89], tdcBoardID[89], tdcChannelID[89], tdcStopTime[89], tdcSignalWidth[89], tdcVmeTime[89],
	tdcCodaID[90], tdcRunID[90], tdcSpillID[90], tdcROC[90], tdcBoardID[90], tdcChannelID[90], tdcStopTime[90], tdcSignalWidth[90], tdcVmeTime[90],
	tdcCodaID[91], tdcRunID[91], tdcSpillID[91], tdcROC[91], tdcBoardID[91], tdcChannelID[91], tdcStopTime[91], tdcSignalWidth[91], tdcVmeTime[91],
	tdcCodaID[92], tdcRunID[92], tdcSpillID[92], tdcROC[92], tdcBoardID[92], tdcChannelID[92], tdcStopTime[92], tdcSignalWidth[92], tdcVmeTime[92],
	tdcCodaID[93], tdcRunID[93], tdcSpillID[93], tdcROC[93], tdcBoardID[93], tdcChannelID[93], tdcStopTime[93], tdcSignalWidth[93], tdcVmeTime[93],
	tdcCodaID[94], tdcRunID[94], tdcSpillID[94], tdcROC[94], tdcBoardID[94], tdcChannelID[94], tdcStopTime[94], tdcSignalWidth[94], tdcVmeTime[94],
	tdcCodaID[95], tdcRunID[95], tdcSpillID[95], tdcROC[95], tdcBoardID[95], tdcChannelID[95], tdcStopTime[95], tdcSignalWidth[95], tdcVmeTime[95],
	tdcCodaID[96], tdcRunID[96], tdcSpillID[96], tdcROC[96], tdcBoardID[96], tdcChannelID[96], tdcStopTime[96], tdcSignalWidth[96], tdcVmeTime[96],
	tdcCodaID[97], tdcRunID[97], tdcSpillID[97], tdcROC[97], tdcBoardID[97], tdcChannelID[97], tdcStopTime[97], tdcSignalWidth[97], tdcVmeTime[97],
	tdcCodaID[98], tdcRunID[98], tdcSpillID[98], tdcROC[98], tdcBoardID[98], tdcChannelID[98], tdcStopTime[98], tdcSignalWidth[98], tdcVmeTime[98],
	tdcCodaID[99], tdcRunID[99], tdcSpillID[99], tdcROC[99], tdcBoardID[99], tdcChannelID[99], tdcStopTime[99], tdcSignalWidth[99], tdcVmeTime[99],
	tdcCodaID[100], tdcRunID[100], tdcSpillID[100], tdcROC[100], tdcBoardID[100], tdcChannelID[100], tdcStopTime[100], tdcSignalWidth[100], tdcVmeTime[100],
	tdcCodaID[101], tdcRunID[101], tdcSpillID[101], tdcROC[101], tdcBoardID[101], tdcChannelID[101], tdcStopTime[101], tdcSignalWidth[101], tdcVmeTime[101],
	tdcCodaID[102], tdcRunID[102], tdcSpillID[102], tdcROC[102], tdcBoardID[102], tdcChannelID[102], tdcStopTime[102], tdcSignalWidth[102], tdcVmeTime[102],
	tdcCodaID[103], tdcRunID[103], tdcSpillID[103], tdcROC[103], tdcBoardID[103], tdcChannelID[103], tdcStopTime[103], tdcSignalWidth[103], tdcVmeTime[103],
	tdcCodaID[104], tdcRunID[104], tdcSpillID[104], tdcROC[104], tdcBoardID[104], tdcChannelID[104], tdcStopTime[104], tdcSignalWidth[104], tdcVmeTime[104],
	tdcCodaID[105], tdcRunID[105], tdcSpillID[105], tdcROC[105], tdcBoardID[105], tdcChannelID[105], tdcStopTime[105], tdcSignalWidth[105], tdcVmeTime[105],
	tdcCodaID[106], tdcRunID[106], tdcSpillID[106], tdcROC[106], tdcBoardID[106], tdcChannelID[106], tdcStopTime[106], tdcSignalWidth[106], tdcVmeTime[106],
	tdcCodaID[107], tdcRunID[107], tdcSpillID[107], tdcROC[107], tdcBoardID[107], tdcChannelID[107], tdcStopTime[107], tdcSignalWidth[107], tdcVmeTime[107],
	tdcCodaID[108], tdcRunID[108], tdcSpillID[108], tdcROC[108], tdcBoardID[108], tdcChannelID[108], tdcStopTime[108], tdcSignalWidth[108], tdcVmeTime[108],
	tdcCodaID[109], tdcRunID[109], tdcSpillID[109], tdcROC[109], tdcBoardID[109], tdcChannelID[109], tdcStopTime[109], tdcSignalWidth[109], tdcVmeTime[109],
	tdcCodaID[110], tdcRunID[110], tdcSpillID[110], tdcROC[110], tdcBoardID[110], tdcChannelID[110], tdcStopTime[110], tdcSignalWidth[110], tdcVmeTime[110],
	tdcCodaID[111], tdcRunID[111], tdcSpillID[111], tdcROC[111], tdcBoardID[111], tdcChannelID[111], tdcStopTime[111], tdcSignalWidth[111], tdcVmeTime[111],
	tdcCodaID[112], tdcRunID[112], tdcSpillID[112], tdcROC[112], tdcBoardID[112], tdcChannelID[112], tdcStopTime[112], tdcSignalWidth[112], tdcVmeTime[112],
	tdcCodaID[113], tdcRunID[113], tdcSpillID[113], tdcROC[113], tdcBoardID[113], tdcChannelID[113], tdcStopTime[113], tdcSignalWidth[113], tdcVmeTime[113],
	tdcCodaID[114], tdcRunID[114], tdcSpillID[114], tdcROC[114], tdcBoardID[114], tdcChannelID[114], tdcStopTime[114], tdcSignalWidth[114], tdcVmeTime[114],
	tdcCodaID[115], tdcRunID[115], tdcSpillID[115], tdcROC[115], tdcBoardID[115], tdcChannelID[115], tdcStopTime[115], tdcSignalWidth[115], tdcVmeTime[115],
	tdcCodaID[116], tdcRunID[116], tdcSpillID[116], tdcROC[116], tdcBoardID[116], tdcChannelID[116], tdcStopTime[116], tdcSignalWidth[116], tdcVmeTime[116],
	tdcCodaID[117], tdcRunID[117], tdcSpillID[117], tdcROC[117], tdcBoardID[117], tdcChannelID[117], tdcStopTime[117], tdcSignalWidth[117], tdcVmeTime[117],
	tdcCodaID[118], tdcRunID[118], tdcSpillID[118], tdcROC[118], tdcBoardID[118], tdcChannelID[118], tdcStopTime[118], tdcSignalWidth[118], tdcVmeTime[118],
	tdcCodaID[119], tdcRunID[119], tdcSpillID[119], tdcROC[119], tdcBoardID[119], tdcChannelID[119], tdcStopTime[119], tdcSignalWidth[119], tdcVmeTime[119],
	tdcCodaID[120], tdcRunID[120], tdcSpillID[120], tdcROC[120], tdcBoardID[120], tdcChannelID[120], tdcStopTime[120], tdcSignalWidth[120], tdcVmeTime[120],
	tdcCodaID[121], tdcRunID[121], tdcSpillID[121], tdcROC[121], tdcBoardID[121], tdcChannelID[121], tdcStopTime[121], tdcSignalWidth[121], tdcVmeTime[121],
	tdcCodaID[122], tdcRunID[122], tdcSpillID[122], tdcROC[122], tdcBoardID[122], tdcChannelID[122], tdcStopTime[122], tdcSignalWidth[122], tdcVmeTime[122],
	tdcCodaID[123], tdcRunID[123], tdcSpillID[123], tdcROC[123], tdcBoardID[123], tdcChannelID[123], tdcStopTime[123], tdcSignalWidth[123], tdcVmeTime[123],
	tdcCodaID[124], tdcRunID[124], tdcSpillID[124], tdcROC[124], tdcBoardID[124], tdcChannelID[124], tdcStopTime[124], tdcSignalWidth[124], tdcVmeTime[124],
	tdcCodaID[125], tdcRunID[125], tdcSpillID[125], tdcROC[125], tdcBoardID[125], tdcChannelID[125], tdcStopTime[125], tdcSignalWidth[125], tdcVmeTime[125],
	tdcCodaID[126], tdcRunID[126], tdcSpillID[126], tdcROC[126], tdcBoardID[126], tdcChannelID[126], tdcStopTime[126], tdcSignalWidth[126], tdcVmeTime[126],
	tdcCodaID[127], tdcRunID[127], tdcSpillID[127], tdcROC[127], tdcBoardID[127], tdcChannelID[127], tdcStopTime[127], tdcSignalWidth[127], tdcVmeTime[127],
	tdcCodaID[128], tdcRunID[128], tdcSpillID[128], tdcROC[128], tdcBoardID[128], tdcChannelID[128], tdcStopTime[128], tdcSignalWidth[128], tdcVmeTime[128],
	tdcCodaID[129], tdcRunID[129], tdcSpillID[129], tdcROC[129], tdcBoardID[129], tdcChannelID[129], tdcStopTime[129], tdcSignalWidth[129], tdcVmeTime[129],
	tdcCodaID[130], tdcRunID[130], tdcSpillID[130], tdcROC[130], tdcBoardID[130], tdcChannelID[130], tdcStopTime[130], tdcSignalWidth[130], tdcVmeTime[130],
	tdcCodaID[131], tdcRunID[131], tdcSpillID[131], tdcROC[131], tdcBoardID[131], tdcChannelID[131], tdcStopTime[131], tdcSignalWidth[131], tdcVmeTime[131],
	tdcCodaID[132], tdcRunID[132], tdcSpillID[132], tdcROC[132], tdcBoardID[132], tdcChannelID[132], tdcStopTime[132], tdcSignalWidth[132], tdcVmeTime[132],
	tdcCodaID[133], tdcRunID[133], tdcSpillID[133], tdcROC[133], tdcBoardID[133], tdcChannelID[133], tdcStopTime[133], tdcSignalWidth[133], tdcVmeTime[133],
	tdcCodaID[134], tdcRunID[134], tdcSpillID[134], tdcROC[134], tdcBoardID[134], tdcChannelID[134], tdcStopTime[134], tdcSignalWidth[134], tdcVmeTime[134],
	tdcCodaID[135], tdcRunID[135], tdcSpillID[135], tdcROC[135], tdcBoardID[135], tdcChannelID[135], tdcStopTime[135], tdcSignalWidth[135], tdcVmeTime[135],
	tdcCodaID[136], tdcRunID[136], tdcSpillID[136], tdcROC[136], tdcBoardID[136], tdcChannelID[136], tdcStopTime[136], tdcSignalWidth[136], tdcVmeTime[136],
	tdcCodaID[137], tdcRunID[137], tdcSpillID[137], tdcROC[137], tdcBoardID[137], tdcChannelID[137], tdcStopTime[137], tdcSignalWidth[137], tdcVmeTime[137],
	tdcCodaID[138], tdcRunID[138], tdcSpillID[138], tdcROC[138], tdcBoardID[138], tdcChannelID[138], tdcStopTime[138], tdcSignalWidth[138], tdcVmeTime[138],
	tdcCodaID[139], tdcRunID[139], tdcSpillID[139], tdcROC[139], tdcBoardID[139], tdcChannelID[139], tdcStopTime[139], tdcSignalWidth[139], tdcVmeTime[139],
	tdcCodaID[140], tdcRunID[140], tdcSpillID[140], tdcROC[140], tdcBoardID[140], tdcChannelID[140], tdcStopTime[140], tdcSignalWidth[140], tdcVmeTime[140],
	tdcCodaID[141], tdcRunID[141], tdcSpillID[141], tdcROC[141], tdcBoardID[141], tdcChannelID[141], tdcStopTime[141], tdcSignalWidth[141], tdcVmeTime[141],
	tdcCodaID[142], tdcRunID[142], tdcSpillID[142], tdcROC[142], tdcBoardID[142], tdcChannelID[142], tdcStopTime[142], tdcSignalWidth[142], tdcVmeTime[142],
	tdcCodaID[143], tdcRunID[143], tdcSpillID[143], tdcROC[143], tdcBoardID[143], tdcChannelID[143], tdcStopTime[143], tdcSignalWidth[143], tdcVmeTime[143],
	tdcCodaID[144], tdcRunID[144], tdcSpillID[144], tdcROC[144], tdcBoardID[144], tdcChannelID[144], tdcStopTime[144], tdcSignalWidth[144], tdcVmeTime[144],
	tdcCodaID[145], tdcRunID[145], tdcSpillID[145], tdcROC[145], tdcBoardID[145], tdcChannelID[145], tdcStopTime[145], tdcSignalWidth[145], tdcVmeTime[145],
	tdcCodaID[146], tdcRunID[146], tdcSpillID[146], tdcROC[146], tdcBoardID[146], tdcChannelID[146], tdcStopTime[146], tdcSignalWidth[146], tdcVmeTime[146],
	tdcCodaID[147], tdcRunID[147], tdcSpillID[147], tdcROC[147], tdcBoardID[147], tdcChannelID[147], tdcStopTime[147], tdcSignalWidth[147], tdcVmeTime[147],
	tdcCodaID[148], tdcRunID[148], tdcSpillID[148], tdcROC[148], tdcBoardID[148], tdcChannelID[148], tdcStopTime[148], tdcSignalWidth[148], tdcVmeTime[148],
	tdcCodaID[149], tdcRunID[149], tdcSpillID[149], tdcROC[149], tdcBoardID[149], tdcChannelID[149], tdcStopTime[149], tdcSignalWidth[149], tdcVmeTime[149],
	tdcCodaID[150], tdcRunID[150], tdcSpillID[150], tdcROC[150], tdcBoardID[150], tdcChannelID[150], tdcStopTime[150], tdcSignalWidth[150], tdcVmeTime[150],
	tdcCodaID[151], tdcRunID[151], tdcSpillID[151], tdcROC[151], tdcBoardID[151], tdcChannelID[151], tdcStopTime[151], tdcSignalWidth[151], tdcVmeTime[151],
	tdcCodaID[152], tdcRunID[152], tdcSpillID[152], tdcROC[152], tdcBoardID[152], tdcChannelID[152], tdcStopTime[152], tdcSignalWidth[152], tdcVmeTime[152],
	tdcCodaID[153], tdcRunID[153], tdcSpillID[153], tdcROC[153], tdcBoardID[153], tdcChannelID[153], tdcStopTime[153], tdcSignalWidth[153], tdcVmeTime[153],
	tdcCodaID[154], tdcRunID[154], tdcSpillID[154], tdcROC[154], tdcBoardID[154], tdcChannelID[154], tdcStopTime[154], tdcSignalWidth[154], tdcVmeTime[154],
	tdcCodaID[155], tdcRunID[155], tdcSpillID[155], tdcROC[155], tdcBoardID[155], tdcChannelID[155], tdcStopTime[155], tdcSignalWidth[155], tdcVmeTime[155],
	tdcCodaID[156], tdcRunID[156], tdcSpillID[156], tdcROC[156], tdcBoardID[156], tdcChannelID[156], tdcStopTime[156], tdcSignalWidth[156], tdcVmeTime[156],
	tdcCodaID[157], tdcRunID[157], tdcSpillID[157], tdcROC[157], tdcBoardID[157], tdcChannelID[157], tdcStopTime[157], tdcSignalWidth[157], tdcVmeTime[157],
	tdcCodaID[158], tdcRunID[158], tdcSpillID[158], tdcROC[158], tdcBoardID[158], tdcChannelID[158], tdcStopTime[158], tdcSignalWidth[158], tdcVmeTime[158],
	tdcCodaID[159], tdcRunID[159], tdcSpillID[159], tdcROC[159], tdcBoardID[159], tdcChannelID[159], tdcStopTime[159], tdcSignalWidth[159], tdcVmeTime[159],
	tdcCodaID[160], tdcRunID[160], tdcSpillID[160], tdcROC[160], tdcBoardID[160], tdcChannelID[160], tdcStopTime[160], tdcSignalWidth[160], tdcVmeTime[160],
	tdcCodaID[161], tdcRunID[161], tdcSpillID[161], tdcROC[161], tdcBoardID[161], tdcChannelID[161], tdcStopTime[161], tdcSignalWidth[161], tdcVmeTime[161],
	tdcCodaID[162], tdcRunID[162], tdcSpillID[162], tdcROC[162], tdcBoardID[162], tdcChannelID[162], tdcStopTime[162], tdcSignalWidth[162], tdcVmeTime[162],
	tdcCodaID[163], tdcRunID[163], tdcSpillID[163], tdcROC[163], tdcBoardID[163], tdcChannelID[163], tdcStopTime[163], tdcSignalWidth[163], tdcVmeTime[163],
	tdcCodaID[164], tdcRunID[164], tdcSpillID[164], tdcROC[164], tdcBoardID[164], tdcChannelID[164], tdcStopTime[164], tdcSignalWidth[164], tdcVmeTime[164],
	tdcCodaID[165], tdcRunID[165], tdcSpillID[165], tdcROC[165], tdcBoardID[165], tdcChannelID[165], tdcStopTime[165], tdcSignalWidth[165], tdcVmeTime[165],
	tdcCodaID[166], tdcRunID[166], tdcSpillID[166], tdcROC[166], tdcBoardID[166], tdcChannelID[166], tdcStopTime[166], tdcSignalWidth[166], tdcVmeTime[166],
	tdcCodaID[167], tdcRunID[167], tdcSpillID[167], tdcROC[167], tdcBoardID[167], tdcChannelID[167], tdcStopTime[167], tdcSignalWidth[167], tdcVmeTime[167],
	tdcCodaID[168], tdcRunID[168], tdcSpillID[168], tdcROC[168], tdcBoardID[168], tdcChannelID[168], tdcStopTime[168], tdcSignalWidth[168], tdcVmeTime[168],
	tdcCodaID[169], tdcRunID[169], tdcSpillID[169], tdcROC[169], tdcBoardID[169], tdcChannelID[169], tdcStopTime[169], tdcSignalWidth[169], tdcVmeTime[169],
	tdcCodaID[170], tdcRunID[170], tdcSpillID[170], tdcROC[170], tdcBoardID[170], tdcChannelID[170], tdcStopTime[170], tdcSignalWidth[170], tdcVmeTime[170],
	tdcCodaID[171], tdcRunID[171], tdcSpillID[171], tdcROC[171], tdcBoardID[171], tdcChannelID[171], tdcStopTime[171], tdcSignalWidth[171], tdcVmeTime[171],
	tdcCodaID[172], tdcRunID[172], tdcSpillID[172], tdcROC[172], tdcBoardID[172], tdcChannelID[172], tdcStopTime[172], tdcSignalWidth[172], tdcVmeTime[172],
	tdcCodaID[173], tdcRunID[173], tdcSpillID[173], tdcROC[173], tdcBoardID[173], tdcChannelID[173], tdcStopTime[173], tdcSignalWidth[173], tdcVmeTime[173],
	tdcCodaID[174], tdcRunID[174], tdcSpillID[174], tdcROC[174], tdcBoardID[174], tdcChannelID[174], tdcStopTime[174], tdcSignalWidth[174], tdcVmeTime[174],
	tdcCodaID[175], tdcRunID[175], tdcSpillID[175], tdcROC[175], tdcBoardID[175], tdcChannelID[175], tdcStopTime[175], tdcSignalWidth[175], tdcVmeTime[175],
	tdcCodaID[176], tdcRunID[176], tdcSpillID[176], tdcROC[176], tdcBoardID[176], tdcChannelID[176], tdcStopTime[176], tdcSignalWidth[176], tdcVmeTime[176],
	tdcCodaID[177], tdcRunID[177], tdcSpillID[177], tdcROC[177], tdcBoardID[177], tdcChannelID[177], tdcStopTime[177], tdcSignalWidth[177], tdcVmeTime[177],
	tdcCodaID[178], tdcRunID[178], tdcSpillID[178], tdcROC[178], tdcBoardID[178], tdcChannelID[178], tdcStopTime[178], tdcSignalWidth[178], tdcVmeTime[178],
	tdcCodaID[179], tdcRunID[179], tdcSpillID[179], tdcROC[179], tdcBoardID[179], tdcChannelID[179], tdcStopTime[179], tdcSignalWidth[179], tdcVmeTime[179],
	tdcCodaID[180], tdcRunID[180], tdcSpillID[180], tdcROC[180], tdcBoardID[180], tdcChannelID[180], tdcStopTime[180], tdcSignalWidth[180], tdcVmeTime[180],
	tdcCodaID[181], tdcRunID[181], tdcSpillID[181], tdcROC[181], tdcBoardID[181], tdcChannelID[181], tdcStopTime[181], tdcSignalWidth[181], tdcVmeTime[181],
	tdcCodaID[182], tdcRunID[182], tdcSpillID[182], tdcROC[182], tdcBoardID[182], tdcChannelID[182], tdcStopTime[182], tdcSignalWidth[182], tdcVmeTime[182],
	tdcCodaID[183], tdcRunID[183], tdcSpillID[183], tdcROC[183], tdcBoardID[183], tdcChannelID[183], tdcStopTime[183], tdcSignalWidth[183], tdcVmeTime[183],
	tdcCodaID[184], tdcRunID[184], tdcSpillID[184], tdcROC[184], tdcBoardID[184], tdcChannelID[184], tdcStopTime[184], tdcSignalWidth[184], tdcVmeTime[184],
	tdcCodaID[185], tdcRunID[185], tdcSpillID[185], tdcROC[185], tdcBoardID[185], tdcChannelID[185], tdcStopTime[185], tdcSignalWidth[185], tdcVmeTime[185],
	tdcCodaID[186], tdcRunID[186], tdcSpillID[186], tdcROC[186], tdcBoardID[186], tdcChannelID[186], tdcStopTime[186], tdcSignalWidth[186], tdcVmeTime[186],
	tdcCodaID[187], tdcRunID[187], tdcSpillID[187], tdcROC[187], tdcBoardID[187], tdcChannelID[187], tdcStopTime[187], tdcSignalWidth[187], tdcVmeTime[187],
	tdcCodaID[188], tdcRunID[188], tdcSpillID[188], tdcROC[188], tdcBoardID[188], tdcChannelID[188], tdcStopTime[188], tdcSignalWidth[188], tdcVmeTime[188],
	tdcCodaID[189], tdcRunID[189], tdcSpillID[189], tdcROC[189], tdcBoardID[189], tdcChannelID[189], tdcStopTime[189], tdcSignalWidth[189], tdcVmeTime[189],
	tdcCodaID[190], tdcRunID[190], tdcSpillID[190], tdcROC[190], tdcBoardID[190], tdcChannelID[190], tdcStopTime[190], tdcSignalWidth[190], tdcVmeTime[190],
	tdcCodaID[191], tdcRunID[191], tdcSpillID[191], tdcROC[191], tdcBoardID[191], tdcChannelID[191], tdcStopTime[191], tdcSignalWidth[191], tdcVmeTime[191],
	tdcCodaID[192], tdcRunID[192], tdcSpillID[192], tdcROC[192], tdcBoardID[192], tdcChannelID[192], tdcStopTime[192], tdcSignalWidth[192], tdcVmeTime[192],
	tdcCodaID[193], tdcRunID[193], tdcSpillID[193], tdcROC[193], tdcBoardID[193], tdcChannelID[193], tdcStopTime[193], tdcSignalWidth[193], tdcVmeTime[193],
	tdcCodaID[194], tdcRunID[194], tdcSpillID[194], tdcROC[194], tdcBoardID[194], tdcChannelID[194], tdcStopTime[194], tdcSignalWidth[194], tdcVmeTime[194],
	tdcCodaID[195], tdcRunID[195], tdcSpillID[195], tdcROC[195], tdcBoardID[195], tdcChannelID[195], tdcStopTime[195], tdcSignalWidth[195], tdcVmeTime[195],
	tdcCodaID[196], tdcRunID[196], tdcSpillID[196], tdcROC[196], tdcBoardID[196], tdcChannelID[196], tdcStopTime[196], tdcSignalWidth[196], tdcVmeTime[196],
	tdcCodaID[197], tdcRunID[197], tdcSpillID[197], tdcROC[197], tdcBoardID[197], tdcChannelID[197], tdcStopTime[197], tdcSignalWidth[197], tdcVmeTime[197],
	tdcCodaID[198], tdcRunID[198], tdcSpillID[198], tdcROC[198], tdcBoardID[198], tdcChannelID[198], tdcStopTime[198], tdcSignalWidth[198], tdcVmeTime[198],
	tdcCodaID[199], tdcRunID[199], tdcSpillID[199], tdcROC[199], tdcBoardID[199], tdcChannelID[199], tdcStopTime[199], tdcSignalWidth[199], tdcVmeTime[199]);

	if (mysql_query(conn, sqlTDCQuery))
	{
		printf("Error (100): %s\n%s", mysql_error(conn), sqlTDCQuery);
		return 1;
	}
*/

/*
	char sqlv1495Query[100000];

	if( mysql_query(conn, "DELETE FROM tempv1495") ) {
		printf("Error: %s\n", mysql_error(conn));
   	}

	sprintf(sqlv1495Query, "INSERT INTO tempv1495 (eventID, runID, spillID, rocID, boardID, channelID, tdcTime, vmeTime) "
	"VALUES (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), (%i, %i, %i, %i, %i, %i, %f, %f), "
	"(%i, %i, %i, %i, %i, %i, %f, %f)",
	v1495CodaID[0], v1495RunID[0], v1495SpillID[0], v1495ROC[0], v1495BoardID[0], v1495ChannelID[0], v1495StopTime[0], v1495VmeTime[0],
	v1495CodaID[1], v1495RunID[1], v1495SpillID[1], v1495ROC[1], v1495BoardID[1], v1495ChannelID[1], v1495StopTime[1], v1495VmeTime[1],
	v1495CodaID[2], v1495RunID[2], v1495SpillID[2], v1495ROC[2], v1495BoardID[2], v1495ChannelID[2], v1495StopTime[2], v1495VmeTime[2],
	v1495CodaID[3], v1495RunID[3], v1495SpillID[3], v1495ROC[3], v1495BoardID[3], v1495ChannelID[3], v1495StopTime[3], v1495VmeTime[3],
	v1495CodaID[4], v1495RunID[4], v1495SpillID[4], v1495ROC[4], v1495BoardID[4], v1495ChannelID[4], v1495StopTime[4], v1495VmeTime[4],
	v1495CodaID[5], v1495RunID[5], v1495SpillID[5], v1495ROC[5], v1495BoardID[5], v1495ChannelID[5], v1495StopTime[5], v1495VmeTime[5],
	v1495CodaID[6], v1495RunID[6], v1495SpillID[6], v1495ROC[6], v1495BoardID[6], v1495ChannelID[6], v1495StopTime[6], v1495VmeTime[6],
	v1495CodaID[7], v1495RunID[7], v1495SpillID[7], v1495ROC[7], v1495BoardID[7], v1495ChannelID[7], v1495StopTime[7], v1495VmeTime[7],
	v1495CodaID[8], v1495RunID[8], v1495SpillID[8], v1495ROC[8], v1495BoardID[8], v1495ChannelID[8], v1495StopTime[8], v1495VmeTime[8],
	v1495CodaID[9], v1495RunID[9], v1495SpillID[9], v1495ROC[9], v1495BoardID[9], v1495ChannelID[9], v1495StopTime[9], v1495VmeTime[9],
	v1495CodaID[10], v1495RunID[10], v1495SpillID[10], v1495ROC[10], v1495BoardID[10], v1495ChannelID[10], v1495StopTime[10], v1495VmeTime[10],
	v1495CodaID[11], v1495RunID[11], v1495SpillID[11], v1495ROC[11], v1495BoardID[11], v1495ChannelID[11], v1495StopTime[11], v1495VmeTime[11],
	v1495CodaID[12], v1495RunID[12], v1495SpillID[12], v1495ROC[12], v1495BoardID[12], v1495ChannelID[12], v1495StopTime[12], v1495VmeTime[12],
	v1495CodaID[13], v1495RunID[13], v1495SpillID[13], v1495ROC[13], v1495BoardID[13], v1495ChannelID[13], v1495StopTime[13], v1495VmeTime[13],
	v1495CodaID[14], v1495RunID[14], v1495SpillID[14], v1495ROC[14], v1495BoardID[14], v1495ChannelID[14], v1495StopTime[14], v1495VmeTime[14],
	v1495CodaID[15], v1495RunID[15], v1495SpillID[15], v1495ROC[15], v1495BoardID[15], v1495ChannelID[15], v1495StopTime[15], v1495VmeTime[15],
	v1495CodaID[16], v1495RunID[16], v1495SpillID[16], v1495ROC[16], v1495BoardID[16], v1495ChannelID[16], v1495StopTime[16], v1495VmeTime[16],
	v1495CodaID[17], v1495RunID[17], v1495SpillID[17], v1495ROC[17], v1495BoardID[17], v1495ChannelID[17], v1495StopTime[17], v1495VmeTime[17],
	v1495CodaID[18], v1495RunID[18], v1495SpillID[18], v1495ROC[18], v1495BoardID[18], v1495ChannelID[18], v1495StopTime[18], v1495VmeTime[18],
	v1495CodaID[19], v1495RunID[19], v1495SpillID[19], v1495ROC[19], v1495BoardID[19], v1495ChannelID[19], v1495StopTime[19], v1495VmeTime[19],
	v1495CodaID[20], v1495RunID[20], v1495SpillID[20], v1495ROC[20], v1495BoardID[20], v1495ChannelID[20], v1495StopTime[20], v1495VmeTime[20],
	v1495CodaID[21], v1495RunID[21], v1495SpillID[21], v1495ROC[21], v1495BoardID[21], v1495ChannelID[21], v1495StopTime[21], v1495VmeTime[21],
	v1495CodaID[22], v1495RunID[22], v1495SpillID[22], v1495ROC[22], v1495BoardID[22], v1495ChannelID[22], v1495StopTime[22], v1495VmeTime[22],
	v1495CodaID[23], v1495RunID[23], v1495SpillID[23], v1495ROC[23], v1495BoardID[23], v1495ChannelID[23], v1495StopTime[23], v1495VmeTime[23],
	v1495CodaID[24], v1495RunID[24], v1495SpillID[24], v1495ROC[24], v1495BoardID[24], v1495ChannelID[24], v1495StopTime[24], v1495VmeTime[24],
	v1495CodaID[25], v1495RunID[25], v1495SpillID[25], v1495ROC[25], v1495BoardID[25], v1495ChannelID[25], v1495StopTime[25], v1495VmeTime[25],
	v1495CodaID[26], v1495RunID[26], v1495SpillID[26], v1495ROC[26], v1495BoardID[26], v1495ChannelID[26], v1495StopTime[26], v1495VmeTime[26],
	v1495CodaID[27], v1495RunID[27], v1495SpillID[27], v1495ROC[27], v1495BoardID[27], v1495ChannelID[27], v1495StopTime[27], v1495VmeTime[27],
	v1495CodaID[28], v1495RunID[28], v1495SpillID[28], v1495ROC[28], v1495BoardID[28], v1495ChannelID[28], v1495StopTime[28], v1495VmeTime[28],
	v1495CodaID[29], v1495RunID[29], v1495SpillID[29], v1495ROC[29], v1495BoardID[29], v1495ChannelID[29], v1495StopTime[29], v1495VmeTime[29],
	v1495CodaID[30], v1495RunID[30], v1495SpillID[30], v1495ROC[30], v1495BoardID[30], v1495ChannelID[30], v1495StopTime[30], v1495VmeTime[30],
	v1495CodaID[31], v1495RunID[31], v1495SpillID[31], v1495ROC[31], v1495BoardID[31], v1495ChannelID[31], v1495StopTime[31], v1495VmeTime[31],
	v1495CodaID[32], v1495RunID[32], v1495SpillID[32], v1495ROC[32], v1495BoardID[32], v1495ChannelID[32], v1495StopTime[32], v1495VmeTime[32],
	v1495CodaID[33], v1495RunID[33], v1495SpillID[33], v1495ROC[33], v1495BoardID[33], v1495ChannelID[33], v1495StopTime[33], v1495VmeTime[33],
	v1495CodaID[34], v1495RunID[34], v1495SpillID[34], v1495ROC[34], v1495BoardID[34], v1495ChannelID[34], v1495StopTime[34], v1495VmeTime[34],
	v1495CodaID[35], v1495RunID[35], v1495SpillID[35], v1495ROC[35], v1495BoardID[35], v1495ChannelID[35], v1495StopTime[35], v1495VmeTime[35],
	v1495CodaID[36], v1495RunID[36], v1495SpillID[36], v1495ROC[36], v1495BoardID[36], v1495ChannelID[36], v1495StopTime[36], v1495VmeTime[36],
	v1495CodaID[37], v1495RunID[37], v1495SpillID[37], v1495ROC[37], v1495BoardID[37], v1495ChannelID[37], v1495StopTime[37], v1495VmeTime[37],
	v1495CodaID[38], v1495RunID[38], v1495SpillID[38], v1495ROC[38], v1495BoardID[38], v1495ChannelID[38], v1495StopTime[38], v1495VmeTime[38],
	v1495CodaID[39], v1495RunID[39], v1495SpillID[39], v1495ROC[39], v1495BoardID[39], v1495ChannelID[39], v1495StopTime[39], v1495VmeTime[39],
	v1495CodaID[40], v1495RunID[40], v1495SpillID[40], v1495ROC[40], v1495BoardID[40], v1495ChannelID[40], v1495StopTime[40], v1495VmeTime[40],
	v1495CodaID[41], v1495RunID[41], v1495SpillID[41], v1495ROC[41], v1495BoardID[41], v1495ChannelID[41], v1495StopTime[41], v1495VmeTime[41],
	v1495CodaID[42], v1495RunID[42], v1495SpillID[42], v1495ROC[42], v1495BoardID[42], v1495ChannelID[42], v1495StopTime[42], v1495VmeTime[42],
	v1495CodaID[43], v1495RunID[43], v1495SpillID[43], v1495ROC[43], v1495BoardID[43], v1495ChannelID[43], v1495StopTime[43], v1495VmeTime[43],
	v1495CodaID[44], v1495RunID[44], v1495SpillID[44], v1495ROC[44], v1495BoardID[44], v1495ChannelID[44], v1495StopTime[44], v1495VmeTime[44],
	v1495CodaID[45], v1495RunID[45], v1495SpillID[45], v1495ROC[45], v1495BoardID[45], v1495ChannelID[45], v1495StopTime[45], v1495VmeTime[45],
	v1495CodaID[46], v1495RunID[46], v1495SpillID[46], v1495ROC[46], v1495BoardID[46], v1495ChannelID[46], v1495StopTime[46], v1495VmeTime[46],
	v1495CodaID[47], v1495RunID[47], v1495SpillID[47], v1495ROC[47], v1495BoardID[47], v1495ChannelID[47], v1495StopTime[47], v1495VmeTime[47],
	v1495CodaID[48], v1495RunID[48], v1495SpillID[48], v1495ROC[48], v1495BoardID[48], v1495ChannelID[48], v1495StopTime[48], v1495VmeTime[48],
	v1495CodaID[49], v1495RunID[49], v1495SpillID[49], v1495ROC[49], v1495BoardID[49], v1495ChannelID[49], v1495StopTime[49], v1495VmeTime[49],
	v1495CodaID[50], v1495RunID[50], v1495SpillID[50], v1495ROC[50], v1495BoardID[50], v1495ChannelID[50], v1495StopTime[50], v1495VmeTime[50],
	v1495CodaID[51], v1495RunID[51], v1495SpillID[51], v1495ROC[51], v1495BoardID[51], v1495ChannelID[51], v1495StopTime[51], v1495VmeTime[51],
	v1495CodaID[52], v1495RunID[52], v1495SpillID[52], v1495ROC[52], v1495BoardID[52], v1495ChannelID[52], v1495StopTime[52], v1495VmeTime[52],
	v1495CodaID[53], v1495RunID[53], v1495SpillID[53], v1495ROC[53], v1495BoardID[53], v1495ChannelID[53], v1495StopTime[53], v1495VmeTime[53],
	v1495CodaID[54], v1495RunID[54], v1495SpillID[54], v1495ROC[54], v1495BoardID[54], v1495ChannelID[54], v1495StopTime[54], v1495VmeTime[54],
	v1495CodaID[55], v1495RunID[55], v1495SpillID[55], v1495ROC[55], v1495BoardID[55], v1495ChannelID[55], v1495StopTime[55], v1495VmeTime[55],
	v1495CodaID[56], v1495RunID[56], v1495SpillID[56], v1495ROC[56], v1495BoardID[56], v1495ChannelID[56], v1495StopTime[56], v1495VmeTime[56],
	v1495CodaID[57], v1495RunID[57], v1495SpillID[57], v1495ROC[57], v1495BoardID[57], v1495ChannelID[57], v1495StopTime[57], v1495VmeTime[57],
	v1495CodaID[58], v1495RunID[58], v1495SpillID[58], v1495ROC[58], v1495BoardID[58], v1495ChannelID[58], v1495StopTime[58], v1495VmeTime[58],
	v1495CodaID[59], v1495RunID[59], v1495SpillID[59], v1495ROC[59], v1495BoardID[59], v1495ChannelID[59], v1495StopTime[59], v1495VmeTime[59],
	v1495CodaID[60], v1495RunID[60], v1495SpillID[60], v1495ROC[60], v1495BoardID[60], v1495ChannelID[60], v1495StopTime[60], v1495VmeTime[60],
	v1495CodaID[61], v1495RunID[61], v1495SpillID[61], v1495ROC[61], v1495BoardID[61], v1495ChannelID[61], v1495StopTime[61], v1495VmeTime[61],
	v1495CodaID[62], v1495RunID[62], v1495SpillID[62], v1495ROC[62], v1495BoardID[62], v1495ChannelID[62], v1495StopTime[62], v1495VmeTime[62],
	v1495CodaID[63], v1495RunID[63], v1495SpillID[63], v1495ROC[63], v1495BoardID[63], v1495ChannelID[63], v1495StopTime[63], v1495VmeTime[63],
	v1495CodaID[64], v1495RunID[64], v1495SpillID[64], v1495ROC[64], v1495BoardID[64], v1495ChannelID[64], v1495StopTime[64], v1495VmeTime[64],
	v1495CodaID[65], v1495RunID[65], v1495SpillID[65], v1495ROC[65], v1495BoardID[65], v1495ChannelID[65], v1495StopTime[65], v1495VmeTime[65],
	v1495CodaID[66], v1495RunID[66], v1495SpillID[66], v1495ROC[66], v1495BoardID[66], v1495ChannelID[66], v1495StopTime[66], v1495VmeTime[66],
	v1495CodaID[67], v1495RunID[67], v1495SpillID[67], v1495ROC[67], v1495BoardID[67], v1495ChannelID[67], v1495StopTime[67], v1495VmeTime[67],
	v1495CodaID[68], v1495RunID[68], v1495SpillID[68], v1495ROC[68], v1495BoardID[68], v1495ChannelID[68], v1495StopTime[68], v1495VmeTime[68],
	v1495CodaID[69], v1495RunID[69], v1495SpillID[69], v1495ROC[69], v1495BoardID[69], v1495ChannelID[69], v1495StopTime[69], v1495VmeTime[69],
	v1495CodaID[70], v1495RunID[70], v1495SpillID[70], v1495ROC[70], v1495BoardID[70], v1495ChannelID[70], v1495StopTime[70], v1495VmeTime[70],
	v1495CodaID[71], v1495RunID[71], v1495SpillID[71], v1495ROC[71], v1495BoardID[71], v1495ChannelID[71], v1495StopTime[71], v1495VmeTime[71],
	v1495CodaID[72], v1495RunID[72], v1495SpillID[72], v1495ROC[72], v1495BoardID[72], v1495ChannelID[72], v1495StopTime[72], v1495VmeTime[72],
	v1495CodaID[73], v1495RunID[73], v1495SpillID[73], v1495ROC[73], v1495BoardID[73], v1495ChannelID[73], v1495StopTime[73], v1495VmeTime[73],
	v1495CodaID[74], v1495RunID[74], v1495SpillID[74], v1495ROC[74], v1495BoardID[74], v1495ChannelID[74], v1495StopTime[74], v1495VmeTime[74],
	v1495CodaID[75], v1495RunID[75], v1495SpillID[75], v1495ROC[75], v1495BoardID[75], v1495ChannelID[75], v1495StopTime[75], v1495VmeTime[75],
	v1495CodaID[76], v1495RunID[76], v1495SpillID[76], v1495ROC[76], v1495BoardID[76], v1495ChannelID[76], v1495StopTime[76], v1495VmeTime[76],
	v1495CodaID[77], v1495RunID[77], v1495SpillID[77], v1495ROC[77], v1495BoardID[77], v1495ChannelID[77], v1495StopTime[77], v1495VmeTime[77],
	v1495CodaID[78], v1495RunID[78], v1495SpillID[78], v1495ROC[78], v1495BoardID[78], v1495ChannelID[78], v1495StopTime[78], v1495VmeTime[78],
	v1495CodaID[79], v1495RunID[79], v1495SpillID[79], v1495ROC[79], v1495BoardID[79], v1495ChannelID[79], v1495StopTime[79], v1495VmeTime[79],
	v1495CodaID[80], v1495RunID[80], v1495SpillID[80], v1495ROC[80], v1495BoardID[80], v1495ChannelID[80], v1495StopTime[80], v1495VmeTime[80],
	v1495CodaID[81], v1495RunID[81], v1495SpillID[81], v1495ROC[81], v1495BoardID[81], v1495ChannelID[81], v1495StopTime[81], v1495VmeTime[81],
	v1495CodaID[82], v1495RunID[82], v1495SpillID[82], v1495ROC[82], v1495BoardID[82], v1495ChannelID[82], v1495StopTime[82], v1495VmeTime[82],
	v1495CodaID[83], v1495RunID[83], v1495SpillID[83], v1495ROC[83], v1495BoardID[83], v1495ChannelID[83], v1495StopTime[83], v1495VmeTime[83],
	v1495CodaID[84], v1495RunID[84], v1495SpillID[84], v1495ROC[84], v1495BoardID[84], v1495ChannelID[84], v1495StopTime[84], v1495VmeTime[84],
	v1495CodaID[85], v1495RunID[85], v1495SpillID[85], v1495ROC[85], v1495BoardID[85], v1495ChannelID[85], v1495StopTime[85], v1495VmeTime[85],
	v1495CodaID[86], v1495RunID[86], v1495SpillID[86], v1495ROC[86], v1495BoardID[86], v1495ChannelID[86], v1495StopTime[86], v1495VmeTime[86],
	v1495CodaID[87], v1495RunID[87], v1495SpillID[87], v1495ROC[87], v1495BoardID[87], v1495ChannelID[87], v1495StopTime[87], v1495VmeTime[87],
	v1495CodaID[88], v1495RunID[88], v1495SpillID[88], v1495ROC[88], v1495BoardID[88], v1495ChannelID[88], v1495StopTime[88], v1495VmeTime[88],
	v1495CodaID[89], v1495RunID[89], v1495SpillID[89], v1495ROC[89], v1495BoardID[89], v1495ChannelID[89], v1495StopTime[89], v1495VmeTime[89],
	v1495CodaID[90], v1495RunID[90], v1495SpillID[90], v1495ROC[90], v1495BoardID[90], v1495ChannelID[90], v1495StopTime[90], v1495VmeTime[90],
	v1495CodaID[91], v1495RunID[91], v1495SpillID[91], v1495ROC[91], v1495BoardID[91], v1495ChannelID[91], v1495StopTime[91], v1495VmeTime[91],
	v1495CodaID[92], v1495RunID[92], v1495SpillID[92], v1495ROC[92], v1495BoardID[92], v1495ChannelID[92], v1495StopTime[92], v1495VmeTime[92],
	v1495CodaID[93], v1495RunID[93], v1495SpillID[93], v1495ROC[93], v1495BoardID[93], v1495ChannelID[93], v1495StopTime[93], v1495VmeTime[93],
	v1495CodaID[94], v1495RunID[94], v1495SpillID[94], v1495ROC[94], v1495BoardID[94], v1495ChannelID[94], v1495StopTime[94], v1495VmeTime[94],
	v1495CodaID[95], v1495RunID[95], v1495SpillID[95], v1495ROC[95], v1495BoardID[95], v1495ChannelID[95], v1495StopTime[95], v1495VmeTime[95],
	v1495CodaID[96], v1495RunID[96], v1495SpillID[96], v1495ROC[96], v1495BoardID[96], v1495ChannelID[96], v1495StopTime[96], v1495VmeTime[96],
	v1495CodaID[97], v1495RunID[97], v1495SpillID[97], v1495ROC[97], v1495BoardID[97], v1495ChannelID[97], v1495StopTime[97], v1495VmeTime[97],
	v1495CodaID[98], v1495RunID[98], v1495SpillID[98], v1495ROC[98], v1495BoardID[98], v1495ChannelID[98], v1495StopTime[98], v1495VmeTime[98],
	v1495CodaID[99], v1495RunID[99], v1495SpillID[99], v1495ROC[99], v1495BoardID[99], v1495ChannelID[99], v1495StopTime[99], v1495VmeTime[99],
	v1495CodaID[100], v1495RunID[100], v1495SpillID[100], v1495ROC[100], v1495BoardID[100], v1495ChannelID[100], v1495StopTime[100], v1495VmeTime[100],
	v1495CodaID[101], v1495RunID[101], v1495SpillID[101], v1495ROC[101], v1495BoardID[101], v1495ChannelID[101], v1495StopTime[101], v1495VmeTime[101],
	v1495CodaID[102], v1495RunID[102], v1495SpillID[102], v1495ROC[102], v1495BoardID[102], v1495ChannelID[102], v1495StopTime[102], v1495VmeTime[102],
	v1495CodaID[103], v1495RunID[103], v1495SpillID[103], v1495ROC[103], v1495BoardID[103], v1495ChannelID[103], v1495StopTime[103], v1495VmeTime[103],
	v1495CodaID[104], v1495RunID[104], v1495SpillID[104], v1495ROC[104], v1495BoardID[104], v1495ChannelID[104], v1495StopTime[104], v1495VmeTime[104],
	v1495CodaID[105], v1495RunID[105], v1495SpillID[105], v1495ROC[105], v1495BoardID[105], v1495ChannelID[105], v1495StopTime[105], v1495VmeTime[105],
	v1495CodaID[106], v1495RunID[106], v1495SpillID[106], v1495ROC[106], v1495BoardID[106], v1495ChannelID[106], v1495StopTime[106], v1495VmeTime[106],
	v1495CodaID[107], v1495RunID[107], v1495SpillID[107], v1495ROC[107], v1495BoardID[107], v1495ChannelID[107], v1495StopTime[107], v1495VmeTime[107],
	v1495CodaID[108], v1495RunID[108], v1495SpillID[108], v1495ROC[108], v1495BoardID[108], v1495ChannelID[108], v1495StopTime[108], v1495VmeTime[108],
	v1495CodaID[109], v1495RunID[109], v1495SpillID[109], v1495ROC[109], v1495BoardID[109], v1495ChannelID[109], v1495StopTime[109], v1495VmeTime[109],
	v1495CodaID[110], v1495RunID[110], v1495SpillID[110], v1495ROC[110], v1495BoardID[110], v1495ChannelID[110], v1495StopTime[110], v1495VmeTime[110],
	v1495CodaID[111], v1495RunID[111], v1495SpillID[111], v1495ROC[111], v1495BoardID[111], v1495ChannelID[111], v1495StopTime[111], v1495VmeTime[111],
	v1495CodaID[112], v1495RunID[112], v1495SpillID[112], v1495ROC[112], v1495BoardID[112], v1495ChannelID[112], v1495StopTime[112], v1495VmeTime[112],
	v1495CodaID[113], v1495RunID[113], v1495SpillID[113], v1495ROC[113], v1495BoardID[113], v1495ChannelID[113], v1495StopTime[113], v1495VmeTime[113],
	v1495CodaID[114], v1495RunID[114], v1495SpillID[114], v1495ROC[114], v1495BoardID[114], v1495ChannelID[114], v1495StopTime[114], v1495VmeTime[114],
	v1495CodaID[115], v1495RunID[115], v1495SpillID[115], v1495ROC[115], v1495BoardID[115], v1495ChannelID[115], v1495StopTime[115], v1495VmeTime[115],
	v1495CodaID[116], v1495RunID[116], v1495SpillID[116], v1495ROC[116], v1495BoardID[116], v1495ChannelID[116], v1495StopTime[116], v1495VmeTime[116],
	v1495CodaID[117], v1495RunID[117], v1495SpillID[117], v1495ROC[117], v1495BoardID[117], v1495ChannelID[117], v1495StopTime[117], v1495VmeTime[117],
	v1495CodaID[118], v1495RunID[118], v1495SpillID[118], v1495ROC[118], v1495BoardID[118], v1495ChannelID[118], v1495StopTime[118], v1495VmeTime[118],
	v1495CodaID[119], v1495RunID[119], v1495SpillID[119], v1495ROC[119], v1495BoardID[119], v1495ChannelID[119], v1495StopTime[119], v1495VmeTime[119],
	v1495CodaID[120], v1495RunID[120], v1495SpillID[120], v1495ROC[120], v1495BoardID[120], v1495ChannelID[120], v1495StopTime[120], v1495VmeTime[120],
	v1495CodaID[121], v1495RunID[121], v1495SpillID[121], v1495ROC[121], v1495BoardID[121], v1495ChannelID[121], v1495StopTime[121], v1495VmeTime[121],
	v1495CodaID[122], v1495RunID[122], v1495SpillID[122], v1495ROC[122], v1495BoardID[122], v1495ChannelID[122], v1495StopTime[122], v1495VmeTime[122],
	v1495CodaID[123], v1495RunID[123], v1495SpillID[123], v1495ROC[123], v1495BoardID[123], v1495ChannelID[123], v1495StopTime[123], v1495VmeTime[123],
	v1495CodaID[124], v1495RunID[124], v1495SpillID[124], v1495ROC[124], v1495BoardID[124], v1495ChannelID[124], v1495StopTime[124], v1495VmeTime[124],
	v1495CodaID[125], v1495RunID[125], v1495SpillID[125], v1495ROC[125], v1495BoardID[125], v1495ChannelID[125], v1495StopTime[125], v1495VmeTime[125],
	v1495CodaID[126], v1495RunID[126], v1495SpillID[126], v1495ROC[126], v1495BoardID[126], v1495ChannelID[126], v1495StopTime[126], v1495VmeTime[126],
	v1495CodaID[127], v1495RunID[127], v1495SpillID[127], v1495ROC[127], v1495BoardID[127], v1495ChannelID[127], v1495StopTime[127], v1495VmeTime[127],
	v1495CodaID[128], v1495RunID[128], v1495SpillID[128], v1495ROC[128], v1495BoardID[128], v1495ChannelID[128], v1495StopTime[128], v1495VmeTime[128],
	v1495CodaID[129], v1495RunID[129], v1495SpillID[129], v1495ROC[129], v1495BoardID[129], v1495ChannelID[129], v1495StopTime[129], v1495VmeTime[129],
	v1495CodaID[130], v1495RunID[130], v1495SpillID[130], v1495ROC[130], v1495BoardID[130], v1495ChannelID[130], v1495StopTime[130], v1495VmeTime[130],
	v1495CodaID[131], v1495RunID[131], v1495SpillID[131], v1495ROC[131], v1495BoardID[131], v1495ChannelID[131], v1495StopTime[131], v1495VmeTime[131],
	v1495CodaID[132], v1495RunID[132], v1495SpillID[132], v1495ROC[132], v1495BoardID[132], v1495ChannelID[132], v1495StopTime[132], v1495VmeTime[132],
	v1495CodaID[133], v1495RunID[133], v1495SpillID[133], v1495ROC[133], v1495BoardID[133], v1495ChannelID[133], v1495StopTime[133], v1495VmeTime[133],
	v1495CodaID[134], v1495RunID[134], v1495SpillID[134], v1495ROC[134], v1495BoardID[134], v1495ChannelID[134], v1495StopTime[134], v1495VmeTime[134],
	v1495CodaID[135], v1495RunID[135], v1495SpillID[135], v1495ROC[135], v1495BoardID[135], v1495ChannelID[135], v1495StopTime[135], v1495VmeTime[135],
	v1495CodaID[136], v1495RunID[136], v1495SpillID[136], v1495ROC[136], v1495BoardID[136], v1495ChannelID[136], v1495StopTime[136], v1495VmeTime[136],
	v1495CodaID[137], v1495RunID[137], v1495SpillID[137], v1495ROC[137], v1495BoardID[137], v1495ChannelID[137], v1495StopTime[137], v1495VmeTime[137],
	v1495CodaID[138], v1495RunID[138], v1495SpillID[138], v1495ROC[138], v1495BoardID[138], v1495ChannelID[138], v1495StopTime[138], v1495VmeTime[138],
	v1495CodaID[139], v1495RunID[139], v1495SpillID[139], v1495ROC[139], v1495BoardID[139], v1495ChannelID[139], v1495StopTime[139], v1495VmeTime[139],
	v1495CodaID[140], v1495RunID[140], v1495SpillID[140], v1495ROC[140], v1495BoardID[140], v1495ChannelID[140], v1495StopTime[140], v1495VmeTime[140],
	v1495CodaID[141], v1495RunID[141], v1495SpillID[141], v1495ROC[141], v1495BoardID[141], v1495ChannelID[141], v1495StopTime[141], v1495VmeTime[141],
	v1495CodaID[142], v1495RunID[142], v1495SpillID[142], v1495ROC[142], v1495BoardID[142], v1495ChannelID[142], v1495StopTime[142], v1495VmeTime[142],
	v1495CodaID[143], v1495RunID[143], v1495SpillID[143], v1495ROC[143], v1495BoardID[143], v1495ChannelID[143], v1495StopTime[143], v1495VmeTime[143],
	v1495CodaID[144], v1495RunID[144], v1495SpillID[144], v1495ROC[144], v1495BoardID[144], v1495ChannelID[144], v1495StopTime[144], v1495VmeTime[144],
	v1495CodaID[145], v1495RunID[145], v1495SpillID[145], v1495ROC[145], v1495BoardID[145], v1495ChannelID[145], v1495StopTime[145], v1495VmeTime[145],
	v1495CodaID[146], v1495RunID[146], v1495SpillID[146], v1495ROC[146], v1495BoardID[146], v1495ChannelID[146], v1495StopTime[146], v1495VmeTime[146],
	v1495CodaID[147], v1495RunID[147], v1495SpillID[147], v1495ROC[147], v1495BoardID[147], v1495ChannelID[147], v1495StopTime[147], v1495VmeTime[147],
	v1495CodaID[148], v1495RunID[148], v1495SpillID[148], v1495ROC[148], v1495BoardID[148], v1495ChannelID[148], v1495StopTime[148], v1495VmeTime[148],
	v1495CodaID[149], v1495RunID[149], v1495SpillID[149], v1495ROC[149], v1495BoardID[149], v1495ChannelID[149], v1495StopTime[149], v1495VmeTime[149],
	v1495CodaID[150], v1495RunID[150], v1495SpillID[150], v1495ROC[150], v1495BoardID[150], v1495ChannelID[150], v1495StopTime[150], v1495VmeTime[150],
	v1495CodaID[151], v1495RunID[151], v1495SpillID[151], v1495ROC[151], v1495BoardID[151], v1495ChannelID[151], v1495StopTime[151], v1495VmeTime[151],
	v1495CodaID[152], v1495RunID[152], v1495SpillID[152], v1495ROC[152], v1495BoardID[152], v1495ChannelID[152], v1495StopTime[152], v1495VmeTime[152],
	v1495CodaID[153], v1495RunID[153], v1495SpillID[153], v1495ROC[153], v1495BoardID[153], v1495ChannelID[153], v1495StopTime[153], v1495VmeTime[153],
	v1495CodaID[154], v1495RunID[154], v1495SpillID[154], v1495ROC[154], v1495BoardID[154], v1495ChannelID[154], v1495StopTime[154], v1495VmeTime[154],
	v1495CodaID[155], v1495RunID[155], v1495SpillID[155], v1495ROC[155], v1495BoardID[155], v1495ChannelID[155], v1495StopTime[155], v1495VmeTime[155],
	v1495CodaID[156], v1495RunID[156], v1495SpillID[156], v1495ROC[156], v1495BoardID[156], v1495ChannelID[156], v1495StopTime[156], v1495VmeTime[156],
	v1495CodaID[157], v1495RunID[157], v1495SpillID[157], v1495ROC[157], v1495BoardID[157], v1495ChannelID[157], v1495StopTime[157], v1495VmeTime[157],
	v1495CodaID[158], v1495RunID[158], v1495SpillID[158], v1495ROC[158], v1495BoardID[158], v1495ChannelID[158], v1495StopTime[158], v1495VmeTime[158],
	v1495CodaID[159], v1495RunID[159], v1495SpillID[159], v1495ROC[159], v1495BoardID[159], v1495ChannelID[159], v1495StopTime[159], v1495VmeTime[159],
	v1495CodaID[160], v1495RunID[160], v1495SpillID[160], v1495ROC[160], v1495BoardID[160], v1495ChannelID[160], v1495StopTime[160], v1495VmeTime[160],
	v1495CodaID[161], v1495RunID[161], v1495SpillID[161], v1495ROC[161], v1495BoardID[161], v1495ChannelID[161], v1495StopTime[161], v1495VmeTime[161],
	v1495CodaID[162], v1495RunID[162], v1495SpillID[162], v1495ROC[162], v1495BoardID[162], v1495ChannelID[162], v1495StopTime[162], v1495VmeTime[162],
	v1495CodaID[163], v1495RunID[163], v1495SpillID[163], v1495ROC[163], v1495BoardID[163], v1495ChannelID[163], v1495StopTime[163], v1495VmeTime[163],
	v1495CodaID[164], v1495RunID[164], v1495SpillID[164], v1495ROC[164], v1495BoardID[164], v1495ChannelID[164], v1495StopTime[164], v1495VmeTime[164],
	v1495CodaID[165], v1495RunID[165], v1495SpillID[165], v1495ROC[165], v1495BoardID[165], v1495ChannelID[165], v1495StopTime[165], v1495VmeTime[165],
	v1495CodaID[166], v1495RunID[166], v1495SpillID[166], v1495ROC[166], v1495BoardID[166], v1495ChannelID[166], v1495StopTime[166], v1495VmeTime[166],
	v1495CodaID[167], v1495RunID[167], v1495SpillID[167], v1495ROC[167], v1495BoardID[167], v1495ChannelID[167], v1495StopTime[167], v1495VmeTime[167],
	v1495CodaID[168], v1495RunID[168], v1495SpillID[168], v1495ROC[168], v1495BoardID[168], v1495ChannelID[168], v1495StopTime[168], v1495VmeTime[168],
	v1495CodaID[169], v1495RunID[169], v1495SpillID[169], v1495ROC[169], v1495BoardID[169], v1495ChannelID[169], v1495StopTime[169], v1495VmeTime[169],
	v1495CodaID[170], v1495RunID[170], v1495SpillID[170], v1495ROC[170], v1495BoardID[170], v1495ChannelID[170], v1495StopTime[170], v1495VmeTime[170],
	v1495CodaID[171], v1495RunID[171], v1495SpillID[171], v1495ROC[171], v1495BoardID[171], v1495ChannelID[171], v1495StopTime[171], v1495VmeTime[171],
	v1495CodaID[172], v1495RunID[172], v1495SpillID[172], v1495ROC[172], v1495BoardID[172], v1495ChannelID[172], v1495StopTime[172], v1495VmeTime[172],
	v1495CodaID[173], v1495RunID[173], v1495SpillID[173], v1495ROC[173], v1495BoardID[173], v1495ChannelID[173], v1495StopTime[173], v1495VmeTime[173],
	v1495CodaID[174], v1495RunID[174], v1495SpillID[174], v1495ROC[174], v1495BoardID[174], v1495ChannelID[174], v1495StopTime[174], v1495VmeTime[174],
	v1495CodaID[175], v1495RunID[175], v1495SpillID[175], v1495ROC[175], v1495BoardID[175], v1495ChannelID[175], v1495StopTime[175], v1495VmeTime[175],
	v1495CodaID[176], v1495RunID[176], v1495SpillID[176], v1495ROC[176], v1495BoardID[176], v1495ChannelID[176], v1495StopTime[176], v1495VmeTime[176],
	v1495CodaID[177], v1495RunID[177], v1495SpillID[177], v1495ROC[177], v1495BoardID[177], v1495ChannelID[177], v1495StopTime[177], v1495VmeTime[177],
	v1495CodaID[178], v1495RunID[178], v1495SpillID[178], v1495ROC[178], v1495BoardID[178], v1495ChannelID[178], v1495StopTime[178], v1495VmeTime[178],
	v1495CodaID[179], v1495RunID[179], v1495SpillID[179], v1495ROC[179], v1495BoardID[179], v1495ChannelID[179], v1495StopTime[179], v1495VmeTime[179],
	v1495CodaID[180], v1495RunID[180], v1495SpillID[180], v1495ROC[180], v1495BoardID[180], v1495ChannelID[180], v1495StopTime[180], v1495VmeTime[180],
	v1495CodaID[181], v1495RunID[181], v1495SpillID[181], v1495ROC[181], v1495BoardID[181], v1495ChannelID[181], v1495StopTime[181], v1495VmeTime[181],
	v1495CodaID[182], v1495RunID[182], v1495SpillID[182], v1495ROC[182], v1495BoardID[182], v1495ChannelID[182], v1495StopTime[182], v1495VmeTime[182],
	v1495CodaID[183], v1495RunID[183], v1495SpillID[183], v1495ROC[183], v1495BoardID[183], v1495ChannelID[183], v1495StopTime[183], v1495VmeTime[183],
	v1495CodaID[184], v1495RunID[184], v1495SpillID[184], v1495ROC[184], v1495BoardID[184], v1495ChannelID[184], v1495StopTime[184], v1495VmeTime[184],
	v1495CodaID[185], v1495RunID[185], v1495SpillID[185], v1495ROC[185], v1495BoardID[185], v1495ChannelID[185], v1495StopTime[185], v1495VmeTime[185],
	v1495CodaID[186], v1495RunID[186], v1495SpillID[186], v1495ROC[186], v1495BoardID[186], v1495ChannelID[186], v1495StopTime[186], v1495VmeTime[186],
	v1495CodaID[187], v1495RunID[187], v1495SpillID[187], v1495ROC[187], v1495BoardID[187], v1495ChannelID[187], v1495StopTime[187], v1495VmeTime[187],
	v1495CodaID[188], v1495RunID[188], v1495SpillID[188], v1495ROC[188], v1495BoardID[188], v1495ChannelID[188], v1495StopTime[188], v1495VmeTime[188],
	v1495CodaID[189], v1495RunID[189], v1495SpillID[189], v1495ROC[189], v1495BoardID[189], v1495ChannelID[189], v1495StopTime[189], v1495VmeTime[189],
	v1495CodaID[190], v1495RunID[190], v1495SpillID[190], v1495ROC[190], v1495BoardID[190], v1495ChannelID[190], v1495StopTime[190], v1495VmeTime[190],
	v1495CodaID[191], v1495RunID[191], v1495SpillID[191], v1495ROC[191], v1495BoardID[191], v1495ChannelID[191], v1495StopTime[191], v1495VmeTime[191],
	v1495CodaID[192], v1495RunID[192], v1495SpillID[192], v1495ROC[192], v1495BoardID[192], v1495ChannelID[192], v1495StopTime[192], v1495VmeTime[192],
	v1495CodaID[193], v1495RunID[193], v1495SpillID[193], v1495ROC[193], v1495BoardID[193], v1495ChannelID[193], v1495StopTime[193], v1495VmeTime[193],
	v1495CodaID[194], v1495RunID[194], v1495SpillID[194], v1495ROC[194], v1495BoardID[194], v1495ChannelID[194], v1495StopTime[194], v1495VmeTime[194],
	v1495CodaID[195], v1495RunID[195], v1495SpillID[195], v1495ROC[195], v1495BoardID[195], v1495ChannelID[195], v1495StopTime[195], v1495VmeTime[195],
	v1495CodaID[196], v1495RunID[196], v1495SpillID[196], v1495ROC[196], v1495BoardID[196], v1495ChannelID[196], v1495StopTime[196], v1495VmeTime[196],
	v1495CodaID[197], v1495RunID[197], v1495SpillID[197], v1495ROC[197], v1495BoardID[197], v1495ChannelID[197], v1495StopTime[197], v1495VmeTime[197],
	v1495CodaID[198], v1495RunID[198], v1495SpillID[198], v1495ROC[198], v1495BoardID[198], v1495ChannelID[198], v1495StopTime[198], v1495VmeTime[198],
	v1495CodaID[199], v1495RunID[199], v1495SpillID[199], v1495ROC[199], v1495BoardID[199], v1495ChannelID[199], v1495StopTime[199], v1495VmeTime[199]);

	if (mysql_query(conn, sqlv1495Query))
	{
		printf("Error (100): %s\n%s", mysql_error(conn), sqlv1495Query);
		return 1;
	}
*/

