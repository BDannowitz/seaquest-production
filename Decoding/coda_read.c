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
//	./decode -f /full/path/and/filename.dat -m 1 -d test_output
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
unsigned int physicsEvent[40000];

// MySQL variables
MYSQL *conn;
MYSQL_RES *res;
database = "";
const int PORT = 0;
char *UNIX_SOCKET = NULL;
const int CLIENT_FLAG = 0;
int sleepCounter = 0;
unsigned int* enableLoadDataInfile;

// File handling variables
FILE *fp;
long unsigned int fileSize;
int stopped = 0;

all1495=0; good1495=0; d1ad1495=0; d2ad1495=0; d3ad1495=0;

// Some other variables used
int i, j, physEvCount, status;
unsigned int eventIdentifier;

char *fileNameBak;

// Initialize some important values
cpuTotal 	= 0.0;
timeTotal 	= 0.0;
physEvCount 	= 0;
codaEventNum 	= 0;
eventNum 	= 0;
tdcCount 	= 0;
v1495Count 	= 0;
latchCount 	= 0;
force 		= 0;
spillID 	= 0;
slowControlCount= 0;
sampling_count 	= 1;
optimCounter 	= 1;
row_count	= 0;

// Changes here.

fileNameBak = malloc(10 * sizeof (*fileNameBak));
fileName = malloc(10 * sizeof (*fileName));
sprintf(end_file_name, "");

// Handle the command-line options
if (initialize(argc, argv)) return 1;
sprintf(fileNameBak,"%s",fileName);

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

memset((void*)&physicsEvent, 0, sizeof(unsigned int)*40000);

// Check if specified file exists
if (file_exists(fileNameBak)){
	// Get file size
	fp = fopen(fileNameBak, "r");
	fseek(fp, 0L, SEEK_END);
	fileSize = ftell(fp);

	// evOpen will return an error if the file is less than
	//	a certain size, so wait until the file is big enough.
	//	...if the file *is* currently being written
	while (fileSize < 32768 && sleepCounter<20){
		sleep(15);
		// Get file size again
		printf("File too small. Waiting for more...\n");
		fseek(fp, 0L, SEEK_END);
		fileSize = ftell(fp);
		sleepCounter++;
	}
	if (sleepCounter==20){
		printf("File not large enough; Wait timeout. Exiting...\n\n");
		return 1;
	}
	printf("Loading... \"%s\"\n", fileNameBak);
	fclose(fp);
	status = open_coda_file(fileNameBak);
	//printf("Open CODA File Status: %d\n",status);

	if (status == SUCCESS){
		printf("Opening CODA File Status: OK!\n\n");
	}
	else { 
		printf("Problem Opening CODA File\nStatus Code: %x\n\nExiting!\n\n", status); 
		return 1; 
	}

// If the file does NOT exist:
} else {
	printf("File \"%s\" does not exist.\nExiting...\n", fileNameBak);
	return 1;
}

cpuStart = clock();
timeStart = time(NULL);
// Read the first event
spillID = 0;
status = read_coda_file(physicsEvent,40000);
eventIdentifier = physicsEvent[1];

for(i=0; eventIdentifier != EXIT_CASE; i++){ 
	// If an error is hit while decoding an offline file, something's wrong
	while (status != SUCCESS){
	
	    if (file_exists(end_file_name) || file_exists(next_run_name)){
	
		// If there are still TDC data values waiting to be sent to the server, send them
		if (tdcCount > 0) 	make_tdc_query(conn); 
		if (latchCount > 0) 	make_latch_query(conn);
		if (scalerCount > 0) 	make_scaler_query(conn);
		if (v1495Count > 0)	make_v1495_query(conn);	
		
		cpuEnd = clock(); timeEnd = time(NULL);
	
		cpuTotal += (double)( cpuEnd - cpuStart ) / (double)CLOCKS_PER_SEC;
		timeTotal += (double)( timeEnd - timeStart );
	
		printf("End File Found: %s\nExiting...\n\n", end_file_name);
	
		beginTimeIndexing = time(NULL);

		if(!online){
			if( mysql_query(conn, "ALTER TABLE Hit ENABLE KEYS") )
			{
				printf("Hit Table Disable Keys Error: %s\n", mysql_error(conn));
				return 1;
			}
		
			if( mysql_query(conn, "ALTER TABLE TriggerHit ENABLE KEYS") )
			{
				printf("TriggerHit Table Disable Keys Error: %s\n", mysql_error(conn));
				return 1;
			}
		}

		if( mysql_query(conn, "OPTIMIZE LOCAL TABLE `Run`, `Spill`, `Event`, `Hit`, `TriggerHit`, `Scaler`, `SlowControl`") )
		{
			printf("Optimize Table Error: %s\n", mysql_error(conn));
			return 1;
		}

		endTimeIndexing = time(NULL);
		totalTimeIndexing = (double)( endTimeIndexing - beginTimeIndexing );
	
		mysql_close(conn);
		close_coda_file();
	
		printf("CPU Time: %fs\n\n", cpuTotal);
			
		printf("CPU Time in Creating MySQL Tables: %fs (%f%%)\n", totalSQLcreate, (totalSQLcreate/cpuTotal)*100);
		printf("Real Time in Creating MySQL Indexes: %fs (%f%%)\n\n", totalTimeIndexing, (totalTimeIndexing/timeTotal)*100);
			
		printf("CPU Time in JyTDC: %fs (%f%%)\n", totalJyTDC, (totalJyTDC/cpuTotal)*100);
		printf("CPU Time in ReimerTDC: %fs (%f%%)\n", totalReimerTDC, (totalReimerTDC/cpuTotal)*100);
		printf("CPU Time in WC TDC: %fs (%f%%)\n", totalWC, (totalWC/cpuTotal)*100);
		printf("CPU Time in ZS WC TDC: %fs (%f%%)\n", totalZSWC, (totalZSWC/cpuTotal)*100);
		printf("CPU Time in Non ZS TDC: %fs (%f%%)\n", totalTDC, (totalTDC/cpuTotal)*100);
		printf("CPU Time in v1495: %fs (%f%%)\n\n", totalv1495, (totalv1495/cpuTotal)*100);
		
		printf("CPU Time in TDC temp file writing: %fs (%f%%)\n", totalTDCfile, (totalTDCfile/cpuTotal)*100);
		printf("CPU Time in TDC Data Loading: %fs (%f%%)\n", totalTDCload, (totalTDCload/cpuTotal)*100);
		printf("CPU Time in TDC Mapping: %fs (%f%%)\n", totalTDCmap, (totalTDCmap/cpuTotal)*100);
		printf("CPU Time in TDC Inserting into Hit: %fs (%f%%)\n", totalTDCinsert, (totalTDCinsert/cpuTotal)*100);
		printf("CPU Time in Whole TDC Process: %fs (%f%%)\n", totalTDCwhole, (totalTDCwhole/cpuTotal)*100);
		printf("Real Time in TDC Data Loading: %fs (%f%%)\n", totalTimeTDCload, (totalTimeTDCload/timeTotal)*100);
		printf("Real Time in TDC Mapping: %fs (%f%%)\n", totalTimeTDCmap, (totalTimeTDCmap/timeTotal)*100);
		printf("Real Time in TDC Inserting into Hit: %fs (%f%%)\n\n", totalTimeTDCinsert, (totalTimeTDCinsert/timeTotal)*100);
			
		printf("CPU Time in v1495 temp file writing: %fs (%f%%)\n", totalv1495file, (totalv1495file/cpuTotal)*100);
		printf("CPU Time in v1495 Data Loading: %fs (%f%%)\n", totalv1495load, (totalv1495load/cpuTotal)*100);
		printf("CPU Time in v1495 Mapping: %fs (%f%%)\n", totalv1495map, (totalv1495map/cpuTotal)*100);
		printf("CPU Time in v1495 Inserting into Hit: %fs (%f%%)\n", totalv1495insert, (totalv1495insert/cpuTotal)*100);
		printf("Real Time in v1495 Data Loading: %fs (%f%%)\n", totalTimev1495load, (totalTimev1495load/timeTotal)*100);
		printf("Real Time in v1495 Mapping: %fs (%f%%)\n", totalTimev1495map, (totalTimev1495map/timeTotal)*100);
		printf("Real Time in v1495 Inserting into Hit: %fs (%f%%)\n\n", totalTimev1495insert, (totalTimev1495insert/timeTotal)*100);
		
		printf("CPU Time writing SlowControl temp files: %fs (%f%%)\n", totalSlowfile, (totalSlowfile/cpuTotal)*100);
		printf("CPU Time submitting SlowControl Data: %fs (%f%%)\n", totalSlowload, (totalSlowload/cpuTotal)*100);
		printf("Real Time submitting SlowControl Data: %fs (%f%%)\n\n", totalTimeSlowload, (totalTimeSlowload/timeTotal)*100);
		
		printf("CPU Time submitting CODA Events: %fs (%f%%)\n", totalCodaBind, (totalCodaBind/cpuTotal)*100);
		printf("CPU Time submitting Spill Events: %fs (%f%%)\n\n", totalSpillBind, (totalSpillBind/cpuTotal)*100);
	
		printf("Total v1495 events: %i, Good Events: %i, 0xD1AD Events: %i, 0xD2AD Events: %i, 0xD3AD Events: %i\n\n", all1495, good1495,d1ad1495,d2ad1495,d3ad1495);
	
		printf("SlowControl Count: %i\n", slowControlCount);
		printf("Real Time: %f\n", timeTotal);
		printf("%i Total CODA Events Decoded\n%i Physics Events Decoded\n", i, physEvCount);
		printf("Average Rate: %f Events/s\n", ((double)i)/(timeTotal));
		printf("Average Rate: %f Hit rows/s\n\n", ((double)row_count)/(timeTotal));
	
		return 0;

	    } else {
		// If an error is hit while decoding an online file, it likely just means
		// 	you need to wait for more events to be written.  Wait and try again.
		//if ( online == 0 ) { printf("ERROR: CODA Read Error in Offline Mode. No END file. Exiting...\n"); return 1; }
		
		// Submit whatever data you've got before you wait
		if (tdcCount > 0) { make_tdc_query(conn); tdcCount = 0; }
		if (latchCount > 0) { make_latch_query(conn); latchCount = 0; }
		if (scalerCount > 0) { make_scaler_query(conn); scalerCount = 0; }
		if (v1495Count > 0) { make_v1495_query(conn); v1495Count = 0; }

		// Get file size the first time through this while loop
		if ( stopped == 0 ) {
			fp = fopen(fileNameBak, "r");
			fseek(fp, 0L, SEEK_END);
			fileSize = ftell(fp);
			fclose(fp);
			stopped = 1;

			if( mysql_query(conn, "OPTIMIZE LOCAL TABLE Run, Spill, Event, Hit, TriggerHit, Scaler") )
			{
				printf("Optimize Table Error: %s\n", mysql_error(conn));
				return 1;
			}
			res = mysql_use_result(conn);
			mysql_free_result(res);

		}
		// Wait, then see if the file has grown by at least one block size
		status = retry(fileNameBak, fileSize, i, physicsEvent);
		if ( status == SUCCESS ) { cpuStart = clock(); timeStart = time(NULL); stopped = 0; }
	}
}

// The last 4 hex digits will be 0x01cc for Prestart, Go Event, 
//		and End Event, and 0x10cc for Physics Events
switch (eventIdentifier & 0xFFFF) {

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
				make_tdc_query(conn);
			}
			if (latchCount > 0) {
				make_latch_query(conn);
			}
			return 1;
			break;
		}
		break;

	case 0:
		// Special case which requires waiting and retrying
		status = retry(fileNameBak, 0, (i-1), physicsEvent);
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
	memset((void*)&physicsEvent, 0, sizeof(unsigned int)*40000);
	status = read_coda_file(physicsEvent,40000);
	eventIdentifier = physicsEvent[1];
}
}

if (tdcCount > 0) make_tdc_query(conn);
if (latchCount > 0) make_latch_query(conn);
if (scalerCount > 0) make_scaler_query(conn);
if (v1495Count > 0) make_v1495_query(conn);

cpuEnd = clock(); timeEnd = time(NULL);
cpuTotal += (double)( cpuEnd - cpuStart ) / (double)CLOCKS_PER_SEC;
timeTotal += (double)( timeEnd - timeStart );

beginTimeIndexing = time(NULL);

if(!online){
	if( mysql_query(conn, "ALTER TABLE Hit ENABLE KEYS") )
	{
		printf("Hit Table Disable Keys Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "ALTER TABLE TriggerHit ENABLE KEYS") )
	{
		printf("TriggerHit Table Disable Keys Error: %s\n", mysql_error(conn));
		return 1;
	}
}



if( mysql_query(conn, "OPTIMIZE LOCAL TABLE `Run`, `Spill`, `Event`, `Hit`, `TriggerHit`, `Scaler`, `SlowControl`") )
{
	printf("Optimize Table Error: %s\n", mysql_error(conn)); return 1;
}

endTimeIndexing = time(NULL);
totalTimeIndexing = (double)( endTimeIndexing - beginTimeIndexing );

mysql_close(conn);
close_coda_file();

printf("CPU Time: %fs\n\n", cpuTotal);

printf("CPU Time in Creating MySQL Tables: %fs (%f%%)\n", totalSQLcreate, (totalSQLcreate/cpuTotal)*100);
printf("Real Time in Creating MySQL Indexes: %fs (%f%%)\n\n", totalTimeIndexing, (totalTimeIndexing/timeTotal)*100);

printf("CPU Time in JyTDC: %fs (%f%%)\n", totalJyTDC, (totalJyTDC/cpuTotal)*100);
printf("CPU Time in ReimerTDC: %fs (%f%%)\n", totalReimerTDC, (totalReimerTDC/cpuTotal)*100);
printf("CPU Time in WC TDC: %fs (%f%%)\n", totalWC, (totalWC/cpuTotal)*100);
printf("CPU Time in ZS WC TDC: %fs (%f%%)\n", totalZSWC, (totalZSWC/cpuTotal)*100);
printf("CPU Time in Non ZS TDC: %fs (%f%%)\n", totalTDC, (totalTDC/cpuTotal)*100);
printf("CPU Time in v1495: %fs (%f%%)\n\n", totalv1495, (totalv1495/cpuTotal)*100);

printf("CPU Time in TDC temp file writing: %fs (%f%%)\n", totalTDCfile, (totalTDCfile/cpuTotal)*100);
printf("CPU Time in TDC Data Loading: %fs (%f%%)\n", totalTDCload, (totalTDCload/cpuTotal)*100);
printf("CPU Time in TDC Mapping: %fs (%f%%)\n", totalTDCmap, (totalTDCmap/cpuTotal)*100);
printf("CPU Time in TDC Inserting into Hit: %fs (%f%%)\n", totalTDCinsert, (totalTDCinsert/cpuTotal)*100);
printf("CPU Time in Whole TDC Process: %fs (%f%%)\n", totalTDCwhole, (totalTDCwhole/cpuTotal)*100);
printf("Real Time in TDC Data Loading: %fs (%f%%)\n", totalTimeTDCload, (totalTimeTDCload/timeTotal)*100);
printf("Real Time in TDC Mapping: %fs (%f%%)\n", totalTimeTDCmap, (totalTimeTDCmap/timeTotal)*100);
printf("Real Time in TDC Inserting into Hit: %fs (%f%%)\n\n", totalTimeTDCinsert, (totalTimeTDCinsert/timeTotal)*100);
	
printf("CPU Time in v1495 temp file writing: %fs (%f%%)\n", totalv1495file, (totalv1495file/cpuTotal)*100);
printf("CPU Time in v1495 Data Loading: %fs (%f%%)\n", totalv1495load, (totalv1495load/cpuTotal)*100);
printf("CPU Time in v1495 Mapping: %fs (%f%%)\n", totalv1495map, (totalv1495map/cpuTotal)*100);
printf("CPU Time in v1495 Inserting into Hit: %fs (%f%%)\n", totalv1495insert, (totalv1495insert/cpuTotal)*100);
printf("Real Time in v1495 Data Loading: %fs (%f%%)\n", totalTimev1495load, (totalTimev1495load/timeTotal)*100);
printf("Real Time in v1495 Mapping: %fs (%f%%)\n", totalTimev1495map, (totalTimev1495map/timeTotal)*100);
printf("Real Time in v1495 Inserting into Hit: %fs (%f%%)\n\n", totalTimev1495insert, (totalTimev1495insert/timeTotal)*100);

printf("CPU Time writing SlowControl temp files: %fs (%f%%)\n", totalSlowfile, (totalSlowfile/cpuTotal)*100);
printf("CPU Time submitting SlowControl Data: %fs (%f%%)\n", totalSlowload, (totalSlowload/cpuTotal)*100);
printf("Real Time submitting SlowControl Data: %fs (%f%%)\n\n", totalTimeSlowload, (totalTimeSlowload/timeTotal)*100);

printf("CPU Time submitting CODA Events: %fs (%f%%)\n", totalCodaBind, (totalCodaBind/cpuTotal)*100);
printf("CPU Time submitting Spill Events: %fs (%f%%)\n\n", totalSpillBind, (totalSpillBind/cpuTotal)*100);

printf("Total v1495 events: %i, Good Events: %i, 0xD1AD Events: %i, 0xD2AD Events: %i, 0xD3AD Events: %i\n\n", all1495,good1495,d1ad1495,d2ad1495,d3ad1495);

printf("SlowControl Count: %i\n", slowControlCount);
printf("Real Time: %f\n", timeTotal);
printf("%i Total CODA Events Decoded\n%i Physics Events Decoded\n", i, physEvCount);
printf("Average Rate: %f Events/s\n", ((double)i)/(timeTotal));
printf("Average Rate: %f Hit rows/s\n\n", ((double)row_count)/(timeTotal));

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
  sampling = 0;

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
	   sampling = 1;
	}
	// Print Help
	else if (!strcmp(argv[i],"help")){
	   printf("\n\n-f: file to decode -- Required\n"
		"-m: mode {0: offline, 1: online} -- Required\n"
		"-h: server host (optional: default 'e906-gat2.fnal.gov')\n"
		"-u: user (optional: default 'seaguest')\n"
		"-p: password (optional: default password)\n"
		"-d: schema (optional: default 'test_coda')\n"
		"-s: sampling mode with 1 in 10 physics events samples\n"
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
		"-s: sampling mode with 1 in 10 physics events samples\n"
		"--force: force overwrite any pre-existing run data\n\n");
   	return 1;
  	}
  }
  return 0;
}

int retry(char *file, long unsigned int oldfilesize, int codaEventCount, unsigned int physicsEvent[40000]){
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
  FILE *fp;
  long int newfilesize;

  // Wait N seconds
  sleep(WAIT_TIME);

  // Get more recent file size
  fp = fopen(file, "r");
  fseek(fp, 0L, SEEK_END);
  newfilesize = ftell(fp);

  // If the file grows by at least a single block size, get the next new event
  if ( newfilesize > (oldfilesize + 32768) ) {

	// Open up the CODA file again
	status = open_coda_file(file);
	if (status != SUCCESS) {
	  return status;
	}
	// If closed and opened successfully, loop through to the desired event.
	for (k=0 ; k <= codaEventCount ; k++) 
	{ status = read_coda_file(physicsEvent,40000); }

  } else {
	status = ERROR;
  }

  fclose(fp);
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
	char runInsert[1000];
	char codaEvInsert[1000];
	char spillInsert[1000];

	beginSQLcreate = clock();
		
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
		"`runID` SMALLINT UNSIGNED NOT NULL PRIMARY KEY, "
		"`runTime` FLOAT NOT NULL, "
		"`runDescriptor` TEXT, "
		"INDEX USING BTREE (runID))") )
	{
		printf("Run Table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	if ( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Spill ("
		"`spillID` MEDIUMINT UNSIGNED NOT NULL, "
		"`runID` MEDIUMINT UNSIGNED NOT NULL, "
		"`codaEventID` INT UNSIGNED NOT NULL, "
		"`spillType` ENUM('BOS', 'EOS', 'OTHER'), "
		"`rocID` TINYINT UNSIGNED NOT NULL, "
		"`vmeTime` MEDIUMINT UNSIGNED NOT NULL, "
		"INDEX USING BTREE (spillID), "
		"INDEX USING BTREE (runID), "
		"INDEX USING BTREE (rocID) )") )
	{
		printf("Spill table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Event ("
		"`eventID` INT UNSIGNED NOT NULL, "
		"`codaEventID` INT UNSIGNED NOT NULL, "
		"`runID` SMALLINT UNSIGNED NOT NULL, "
		"`spillID` MEDIUMINT UNSIGNED NOT NULL, "
		"`triggerBits` MEDIUMINT UNSIGNED, "
		"`dataQuality` MEDIUMINT UNSIGNED, " 
		"`vmeTime` MEDIUMINT UNSIGNED NOT NULL, "
		"INDEX USING BTREE (runID), "
		"INDEX USING BTREE (spillID), "
		"INDEX USING BTREE (eventID) )") )
    	{
		printf("Event table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	// This stores data from the Latches and TDC's
	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Hit ("
		"`hitID` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
		"`runID` MEDIUMINT UNSIGNED NOT NULL, "
		"`spillID` MEDIUMINT UNSIGNED NOT NULL, "
		"`eventID` INT UNSIGNED NOT NULL, "
		"`rocID` TINYINT UNSIGNED NOT NULL, "
		"`boardID` SMALLINT UNSIGNED NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, "
		"`detectorName` CHAR(16), "
		"`elementID` TINYINT UNSIGNED, "
		"`tdcTime` FLOAT, "
		"`inTime` TINYINT, "
		"`driftTime` FLOAT, "
		"`driftDistance` FLOAT, "
		"INDEX USING BTREE (runID), "
		"INDEX USING BTREE (spillID), "
		"INDEX USING BTREE (eventID), "
		"INDEX USING BTREE (rocID, boardID, channelID), "
		"INDEX USING BTREE (detectorName, elementID) )") )
	{
		printf("Hit Table Creation Error: %s\n", mysql_error(conn));
		return 1;
	}

	// This stores data that is processed by our v1495 trigger modules.
	//	They're kept separate in order to keep duplicate hits separated
	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS TriggerHit ("
		"`hitID` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
		"`runID` MEDIUMINT UNSIGNED NOT NULL, "
		"`spillID` MEDIUMINT UNSIGNED NOT NULL, "
		"`eventID` INT UNSIGNED NOT NULL, "
		"`rocID` TINYINT UNSIGNED NOT NULL, "
		"`boardID` SMALLINT UNSIGNED NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, "
		"`detectorName` CHAR(16), "
		"`elementID` TINYINT UNSIGNED, "
		"`tdcTime` FLOAT, "
		"`inTime` TINYINT, "
		"INDEX USING BTREE (runID), "
		"INDEX USING BTREE (spillID), "
		"INDEX USING BTREE (eventID), "
		"INDEX USING BTREE (rocID, boardID, channelID), "
		"INDEX USING BTREE (detectorName, elementID) )") )
	{
		printf("TriggerHit Table Creation Error: %s\n", mysql_error(conn));
		return 1;
	}

	// This is the temporary table into which the data is uploaded, mapped,
	//	calibrated, and then it is inserted into the Hit/TriggerHit tables
	if( mysql_query(conn, "CREATE TEMPORARY TABLE IF NOT EXISTS tempHit ("
		"`runID` MEDIUMINT UNSIGNED NOT NULL, "
		"`spillID` MEDIUMINT UNSIGNED NOT NULL, "
		"`eventID` INT UNSIGNED NOT NULL, "
		"`rocID` TINYINT UNSIGNED NOT NULL, "
		"`boardID` SMALLINT UNSIGNED NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, "
		"`detectorName` CHAR(16), "
		"`elementID` TINYINT UNSIGNED, "
		"`tdcTime` FLOAT, "
		"`inTime` TINYINT, "
		"`driftTime` FLOAT, "
		"`driftDistance` FLOAT, "
		"INDEX USING BTREE (rocID, boardID, channelID) )"
		"ENGINE=MEMORY") )
	{
		printf("Create tempHit error: %s\n", mysql_error(conn));
		return 1;
	}

	// Contains data from our Scalers
	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS Scaler ("
		"`scalerID` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
		"`runID` MEDIUMINT UNSIGNED NOT NULL, "
		"`spillID` MEDIUMINT UNSIGNED NOT NULL, "
		"`codaEventID` INT UNSIGNED NOT NULL, "
		"`rocID` TINYINT UNSIGNED NOT NULL, "
		"`boardID` SMALLINT UNSIGNED NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, "
		"`detectorName` CHAR(16), "
		"`elementID` TINYINT UNSIGNED, "
		"`value` INT NOT NULL, "
		"INDEX USING BTREE (runID), "
		"INDEX USING BTREE (spillID), "
		"INDEX USING BTREE (codaEventID), "
		"INDEX USING BTREE (rocID, boardID, channelID), "
		"INDEX USING BTREE (detectorName, elementID))") )
	{
		printf("Scaler Table Creation Error: %s\n", mysql_error(conn));
		return 1;
	}

	// Scaler data is uploaded to this table, is mapped, and then sent over to
	//	the Scaler table.
	if( mysql_query(conn, "CREATE TEMPORARY TABLE IF NOT EXISTS tempScaler ("
		"`runID` MEDIUMINT UNSIGNED NOT NULL, "
		"`spillID` MEDIUMINT UNSIGNED NOT NULL, "
		"`codaEventID` INT UNSIGNED NOT NULL, "
		"`rocID` TINYINT UNSIGNED NOT NULL, "
		"`boardID` SMALLINT UNSIGNED NOT NULL, "
		"`channelID` TINYINT UNSIGNED NOT NULL, "
		"`detectorName` CHAR(16), "
		"`elementID` TINYINT UNSIGNED, "
		"`value` INT NOT NULL, "
		"INDEX USING BTREE (rocID, boardID, channelID))") )
	{
		printf("Create tempScaler table error: %s\n", mysql_error(conn));
		return 1;
	}

	// The slow control data is directly uploaded to this table
	if( mysql_query(conn, "CREATE TABLE IF NOT EXISTS SlowControl("
		"`time` DATETIME NOT NULL, "
		"`name` VARCHAR(128) NOT NULL, "
		"`value` FLOAT NOT NULL, "
		"`type` VARCHAR(128))"))
	{
		printf("SlowControl table creation error: %s\n", mysql_error(conn));
		return 1;
	}

	// UNDER CONSTRUCTION -- HV data will be handled much like slow control
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

	if(!online){
		if( mysql_query(conn, "ALTER TABLE Hit DISABLE KEYS") )
		{
			printf("Hit Table Disable Keys Error: %s\n", mysql_error(conn));
			return 1;
		}
	
		if( mysql_query(conn, "ALTER TABLE TriggerHit DISABLE KEYS") )
		{
			printf("TriggerHit Table Disable Keys Error: %s\n", mysql_error(conn));
			return 1;
		}
	}

	// These are the strings that the prepared statements will use
	sprintf(runInsert, "INSERT INTO Run (runID, runTime) VALUES (?,?)");
	sprintf(spillInsert,"INSERT INTO Spill (spillID, runID, codaEventID, "
		"spillType, rocID, vmeTime) VALUES (?,?,?,?,?,?)");
	sprintf(codaEvInsert,"INSERT INTO Event (eventID, codaEventID, runID, spillID, "
		"triggerBits, dataQuality, vmeTime) VALUES (?,?,?,?,?,?,?)");
	
	memset(runBind, 0, sizeof(runBind));
	memset(spillBind, 0, sizeof(spillBind));
	memset(codaEvBind, 0, sizeof(codaEvBind));

	// Set up the bound variables for the prepared statements
	runBind[0].buffer_type= MYSQL_TYPE_LONG;
	runBind[0].is_unsigned= 1;
	runBind[0].buffer= 	(char *)&runNum;
	runBind[0].is_null= 	0;
	runBind[0].length= 	0;
	runBind[1].buffer_type= MYSQL_TYPE_LONG;
	runBind[1].is_unsigned= 1;
	runBind[1].buffer= 	(char *)&runTime;
	runBind[1].is_null= 	0;
	runBind[1].length= 	0; 

	spillBind[0].buffer_type= 	MYSQL_TYPE_SHORT;
	spillBind[0].buffer= 		(char *)&spillID;
	spillBind[0].is_unsigned= 	1;
	spillBind[1].buffer_type= 	MYSQL_TYPE_SHORT;
	spillBind[1].buffer= 		(char *)&runNum;
	spillBind[1].is_unsigned= 	1;
	spillBind[2].buffer_type= 	MYSQL_TYPE_LONG;
	spillBind[2].buffer= 		(char *)&codaEventNum;
	spillBind[2].is_unsigned= 	1;
	spillBind[3].buffer_type= 	MYSQL_TYPE_TINY;
	spillBind[3].buffer= 		(char *)&spillType;
	spillBind[4].buffer_type= 	MYSQL_TYPE_TINY;
	spillBind[4].buffer= 		(char *)&ROCID;
	spillBind[4].is_unsigned= 	1;
	spillBind[5].buffer_type= 	MYSQL_TYPE_LONG;
	spillBind[5].buffer= 		(char *)&spillVmeTime;
	spillBind[5].is_unsigned= 	1;
	for (i=0;i<6;i++) spillBind[i].is_null= 0;
	for (i=0;i<6;i++) spillBind[i].length= 0; 

	codaEvBind[0].buffer_type= 	MYSQL_TYPE_LONG;
	codaEvBind[0].is_unsigned= 	1;
	codaEvBind[0].buffer= 		(char *)&eventNum;
	codaEvBind[1].buffer_type= 	MYSQL_TYPE_LONG;
	codaEvBind[1].is_unsigned= 	1;
	codaEvBind[1].buffer= 		(char *)&codaEventNum;
	codaEvBind[2].buffer_type= 	MYSQL_TYPE_SHORT;
	codaEvBind[2].buffer= 		(char *)&runNum;
	codaEvBind[2].is_unsigned= 	1;
	codaEvBind[3].buffer_type= 	MYSQL_TYPE_LONG;
	codaEvBind[3].is_unsigned= 	1;
	codaEvBind[3].buffer= 		(char *)&spillID;
	codaEvBind[4].buffer_type= 	MYSQL_TYPE_SHORT;
	codaEvBind[4].is_unsigned= 	1;
	codaEvBind[4].buffer= 		(char *)&triggerBits;
	codaEvBind[5].buffer_type= 	MYSQL_TYPE_SHORT;
	codaEvBind[5].is_unsigned= 	1;
	codaEvBind[5].buffer= 		(char *)&dataQuality;
	codaEvBind[6].buffer_type= 	MYSQL_TYPE_LONG;
	codaEvBind[6].buffer= 		(char *)&codaEvVmeTime;
	codaEvBind[6].is_unsigned= 	1;
	for (i=0;i<7;i++) codaEvBind[i].is_null= 0;
	for (i=0;i<7;i++) codaEvBind[i].length= 0;

	runStmt = mysql_stmt_init(conn);
	spillStmt = mysql_stmt_init(conn);
	codaEvStmt = mysql_stmt_init(conn);

	mysql_stmt_prepare(runStmt, runInsert, (unsigned int)strlen(runInsert));
	mysql_stmt_prepare(codaEvStmt, codaEvInsert, (unsigned int)strlen(codaEvInsert));
	mysql_stmt_prepare(spillStmt, spillInsert, (unsigned int)strlen(spillInsert));

	endSQLcreate = clock();
	totalSQLcreate += ((double)( endSQLcreate - beginSQLcreate ) / (double)CLOCKS_PER_SEC);

	

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

int prestartSQL(MYSQL *conn, unsigned int physicsEvent[40000]){
// ================================================================
//
//
// This function hadles the 'Go Event' CODA Events, storing its
//	values appropriately.
//
// Returns: 0 on success
//	    1 on MySQL error
//	   

	MYSQL_RES *res;
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

	cpuEnd = clock();
	timeEnd = time(NULL);
	cpuTotal += (double)( cpuEnd - cpuStart ) / (double)CLOCKS_PER_SEC;
	timeTotal += (double)( timeEnd - timeStart );


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
			sprintf(qryString,"DELETE FROM TriggerHit WHERE runID=%i", runNum);
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
			
			if( mysql_query(conn, "OPTIMIZE LOCAL TABLE Run, Spill, Event, Hit, TriggerHit, Scaler") )
			{
				printf("Optimize Table Error: %s\n", mysql_error(conn));
				return 1;
			}
			res = mysql_use_result(conn);
			mysql_free_result(res);

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
				sprintf(qryString,"DELETE FROM TriggerHit WHERE runID=%i", runNum);
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
				
				if( mysql_query(conn, "OPTIMIZE LOCAL TABLE Run, Spill, Event, Hit, TriggerHit, Scaler") )
				{
					printf("Optimize Table Error: %s\n", mysql_error(conn));
					return 1;
				}
				res = mysql_use_result(conn);
				mysql_free_result(res);
		
				printf(">> RunID = %i data has been cleared.\n>> Decoding...\n", runNum);
			} else {
				printf("Exiting...\n\n");
				return 1;
			}
		}
  	}

	sprintf(end_file_name, "/data2/e906daq/coda/data/END/%i.end", runNum);
	sprintf(next_run_name, "/data2/e906daq/coda/data/run_%.6i.dat", (runNum+1));

	mysql_stmt_bind_param(runStmt, runBind);

	if( mysql_stmt_execute(runStmt) ) {
		printf("Run Insert Error: %s\n",mysql_error(conn));
		return 1;
	}

	cpuStart = clock();
	timeStart = time(NULL);

	return 0;

}

int goEventSQL(MYSQL *conn, unsigned int physicsEvent[40000]){
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

int eventRunDescriptorSQL(MYSQL *conn, unsigned int physicsEvent[40000]){

	char descriptor [10000]="";
	char qryString [10000]="";
	int i, x;
	unsigned int number;
	unsigned char letter;
	
	evLength = physicsEvent[0];
	
	// Assemble the descriptor from the hex
	for (x=4;x<(evLength+1);x++) {
		number = physicsEvent[x];
		for(i=0;i<4;i++){
			if((number&0xFF)==0x22) sprintf(descriptor,"%s'",descriptor);
			else sprintf(descriptor,"%s%c",descriptor,(number&0xFF));
			number = number>>8;
		}
	}

	// Create and execute the query that updates the descriptor field
	sprintf(qryString, "UPDATE Run SET runDescriptor = \"%s\" WHERE runID = %i", descriptor, runNum);	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Run Descriptor Update Error: %s\n", mysql_error(conn));
		return 1;
	}

	return 0;

}

int eventSlowSQL(MYSQL *conn, unsigned int physicsEvent[40000]){
// ================================================================
//
// This function takes the Slow Control Physics Event, which is purely
//	a blob of ASCII presented in the form of parsed and reversed
//	unsigned int's.  It slaps together the whole event into one
//	big string of hex values, parses it into 2-digit/1-char chunks, 
//	flips every group of four characters, and writes them to file.
//	After being written to file, they are loaded up to the MySQL server
//	via the LOAD DATA CONCURRENT LOCAL INFILE funcion.
//
// Returns:	0 if successful
//		1 on error


	char outputFileName[100];
	char qryString[100];
	int i, pid, x;
	FILE *fp;
	unsigned int number;

	slowControlCount++;

	beginSlowfile = clock();

	pid = getpid();

	sprintf(outputFileName,".codaSlowControl.%i.temp", pid);

	// Open the temp file 
	if (file_exists(outputFileName)) { remove(outputFileName); }
	fp = fopen(outputFileName,"w");

	if (fp == NULL) {
  		fprintf(stderr, "Can't open output file %s!\n",
          		outputFileName);
  		exit(1);
	}

	for (x=4;x<(evLength+1);x++) {
		number = physicsEvent[x];
		for(i=0;i<4;i++){
			fprintf(fp,"%c",(number&0xFF));
			number = number>>8;
		}
	}

	fclose(fp);

	endSlowfile = clock();
	totalSlowfile += ((double)( endSlowfile - beginSlowfile ) / (double)CLOCKS_PER_SEC);
	

	beginSlowload = clock();
	beginTimeSlowload = time(NULL);

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
		printf("Slow Control Load Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_warning_count(conn) > 0 )
	{
		printf("Warnings have occured from the slow control event upload:\n");
		showWarnings(conn);
	}

	endSlowload = clock();
	endTimeSlowload = time(NULL);
	totalSlowload += ((double)( endSlowload - beginSlowload ) / (double)CLOCKS_PER_SEC);
	totalTimeSlowload += (double)( endTimeSlowload - beginTimeSlowload );
	
	if (file_exists(outputFileName)) { remove(outputFileName); }

	return 0;
}

int eventNewTDCSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	int channel = 0;
	int channelVal = 0;
	float Td, Ts, Tt;
	int val, k, l, m, n;
	double signalWidth[64];
	float tdcTime[64];
	boardID = 0;
	int i = 0;
	int channelFired[64];
	int channelFiredBefore[64];

	beginTDC = clock();

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
				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				
				tdcCount++;

				endTDC = clock();
				totalTDC += ((double)( endTDC - beginTDC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginTDC = clock();
			
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

				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				
				tdcCount++;

				endTDC = clock();
				totalTDC += ((double)( endTDC - beginTDC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginTDC = clock();
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

				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				
				tdcCount++;

				endTDC = clock();
				totalTDC += ((double)( endTDC - beginTDC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginTDC = clock();
				channelFiredBefore[channel]=1;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }
			    // If it did not fire, and it fired previously, make an INSERT row
			    else if(!(channelFired[channel]) && channelFiredBefore[channel]) {
				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				
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

int eventWCTDCSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	int channel = 0;
	int channelVal = 0;
	float Td, Ts, Tt;
	int val, k, l, m, n;
	double signalWidth[64];
	float tdcTime[64];
	boardID = 0;
	int i = 0;
	int channelFired[64];
	int channelFiredBefore[64];

	beginWC = clock();

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
	// printf("ROC: %i, boardID: %.8x\n", ROCID, boardID);
	j++;
	
	// Word 2 contains Td and signal window width
	Td = get_hex_bits(physicsEvent[j],3,2);
	Td = Td*40; // x40 (width of clock)
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
				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				

				tdcCount++;

				endWC = clock();
				totalWC += ((double)( endWC - beginWC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginWC = clock();

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
				Ts = k*40+wc_channel_pattern[val];
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

				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				
				tdcCount++;

				endWC = clock();
				totalWC += ((double)( endWC - beginWC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginWC = clock();

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

				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				
				tdcCount++;

				endWC = clock();
				totalWC += ((double)( endWC - beginWC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginWC = clock();

				channelFiredBefore[channel]=1;
				tdcTime[channel]=0;
				signalWidth[channel]=0;
			    }
			    // If it did not fire, and it fired previously, make an INSERT row
			    else if(!(channelFired[channel]) && channelFiredBefore[channel]) {
				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime[channel];
				
				tdcCount++;

				endWC = clock();
				totalWC += ((double)( endWC - beginWC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginWC = clock();

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
	
	endWC = clock();

	return j;

}

int eventZSWCTDCSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	int channel = 0;
	int channelVal = 0;
	float Td, Ts, Tt;
	int val, k, l, m, n;
	double signalWidth[64];
	float tdcTime;
	int window;
	int word;
	boardID = 0;
	int i = 0;
	int channelFiredBefore[64];
	
	beginZSWC = clock();

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
	j++;
	
	// Word 2 contains Td and signal window width
	// x40 (width of clock)
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
		Tt += wc_trigger_pattern[val];
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
				Ts = window*40+wc_channel_pattern[val];
				tdcTime = 5120-Td-Ts+Tt;
				
				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime;
				

				tdcCount++;

				endZSWC = clock();
				totalZSWC += ((double)( endZSWC - beginZSWC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}

				beginZSWC = clock();
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

	endZSWC = clock();
	totalZSWC += ((double)( endZSWC - beginZSWC ) / (double)CLOCKS_PER_SEC);

	return j;

}

int eventv1495TDCSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	unsigned int numChannels = 0;
	unsigned int stopTime, channel, channelTime; 
	double tdcTime;
	unsigned int error;
	unsigned int ignore = 0;
	int i = 0;
	int k = 0;
	int x = 0;

	beginv1495 = clock();

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],3,4);
	j++;

	// Check to see if there is an error in next word
	// If so, don't submit the information from this board for this event
	error = get_hex_bit(physicsEvent[j+1],3);
	if (error != 0x1) { ignore = 1; }

	all1495++;
	if ((physicsEvent[j] & 0xFFFF) == 0xD1AD) d1ad1495++;
	if ((physicsEvent[j+1] & 0xFFFF) == 0xD2AD) d2ad1495++;
	if ((physicsEvent[j+1] & 0xFFFF) == 0xD3AD) d3ad1495++;
	if (((physicsEvent[j] & 0xFFFF) != 0xD1AD) && ((physicsEvent[j+1] & 0xFFFF) != 0xD2AD) && ((physicsEvent[j+1] & 0xFFFF) != 0xD3AD)) good1495++;

	// Contains 0x000000xx where xx is number of channel entries
	numChannels = (physicsEvent[j] & 0x0000FFFF);
	if (numChannels > 255) { 
		
		//for (x=0;x<j+20;x++) printf("%.8x\n", physicsEvent[x]);
		return j+2;
	}
	j++;

	// Next contains 0x00001xxx where xxx is the stop time
	stopTime = (physicsEvent[j] & 0x00000FFF);
	j++;

	k = 0;
	while(k<numChannels){
		channel = get_hex_bits(physicsEvent[j],3,2);
		channelTime = get_hex_bits(physicsEvent[j],1,2);
		tdcTime = (stopTime - channelTime);
		j++;

		if (!ignore){
			v1495CodaID[v1495Count] = eventNum;
			v1495RunID[v1495Count] = runNum;
			v1495SpillID[v1495Count] = spillID;
			v1495ROC[v1495Count] = ROCID;
			v1495BoardID[v1495Count] = boardID;
			v1495ChannelID[v1495Count] = channel;
			v1495StopTime[v1495Count] = tdcTime;
			
			v1495Count++;
		}
		k++;

		endv1495 = clock();
		totalv1495 += ((double)( endv1495 - beginv1495 ) / (double)CLOCKS_PER_SEC);	

		if (v1495Count == max_v1495_rows) {
		    make_v1495_query(conn);
		    v1495Count = 0;
		}
		beginv1495 = clock();
	}
	i++;
	
	endv1495 = clock();
	totalv1495 += ((double)( endv1495 - beginv1495 ) / (double)CLOCKS_PER_SEC);

	return j;

}

int eventReimerTDCSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int boardNum = 0;
	int windows = 0;
	unsigned char channel = 0;
	int channelVal = 0;
	float Td, Ts, Tt;
	int val, k, l, m, n;
	float tdcTime;
	int window;
	int word;
	boardID = 0;
	int i = 0;
	unsigned int channelFiredBefore[64];

	beginReimerTDC = clock();

	// Word 1 contains boardID only
	boardID = (physicsEvent[j] >> 16);
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

	memset((void*)&channelFiredBefore, 0xBD, sizeof(unsigned int)*64);
	
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

				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime;
				
				tdcCount++;
				
				endReimerTDC = clock();
				totalReimerTDC += ((double)( endReimerTDC - beginReimerTDC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				beginReimerTDC = clock();
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
	
	endReimerTDC = clock();
	totalReimerTDC += ((double)( endReimerTDC - beginReimerTDC ) / (double)CLOCKS_PER_SEC);

	return j;

}

int eventJyTDCSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
//
//	Returns: 0 if successful
//		 1 on error
//
	stopCountTime = 0;
	int windows = 0;
	int channel = 0;
	float Td, Ts, Tt;
	int val;
	float tdcTime;
	int window;
	int word, zsword;
	boardID = 0;
	int i = 0;
	int k;
	int buf = 0;
	int channelFiredBefore[64];

	beginJyTDC = clock();

	// Word 1 contains boardID only
	boardID = get_hex_bits(physicsEvent[j],7,4);
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

	buf = (0xFFFF & physicsEvent[j]);
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

				tdcCodaID[tdcCount] = eventNum;
				tdcRunID[tdcCount] = runNum;
				tdcSpillID[tdcCount] = spillID;
				tdcROC[tdcCount] = ROCID;
				tdcBoardID[tdcCount] = boardID;
				tdcChannelID[tdcCount] = channel;
				tdcStopTime[tdcCount] = tdcTime;
				
				tdcCount++;
				
				endJyTDC = clock();
				totalJyTDC += ((double)( endJyTDC - beginJyTDC ) / (double)CLOCKS_PER_SEC);

				if (tdcCount == max_tdc_rows) {
				    make_tdc_query(conn);
				    tdcCount = 0;
				}
				beginJyTDC = clock();
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

	endJyTDC = clock();
	totalJyTDC += ((double)( endJyTDC - beginJyTDC ) / (double)CLOCKS_PER_SEC);

	return j;

}

int eventLatchSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
    int i, k;
    unsigned int num;


	boardID = get_hex_bits(physicsEvent[j],7,4);
	j++;

	num = physicsEvent[j];

	for (k=0;k<32;k++) {
		if (num%2){
			
			latchCodaID[latchCount] = eventNum;
			latchRunID[latchCount] = runNum;
			latchSpillID[latchCount] = spillID;
			latchROC[latchCount] = ROCID;
			latchBoardID[latchCount] = boardID;
			latchChannelID[latchCount] = k;
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
			latchCodaID[latchCount] = eventNum;
			latchRunID[latchCount] = runNum;
			latchSpillID[latchCount] = spillID;
			latchROC[latchCount] = ROCID;
			latchBoardID[latchCount] = boardID;
			latchChannelID[latchCount] = (32+k);
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

int eventScalerSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j){
    int i, k;
    unsigned int num;

    boardID = (0xFFFF & physicsEvent[j]);
    j++;
    for (i=0;i<32;i++) {
	
	scalerCodaID[scalerCount] = codaEventNum;
	scalerRunID[scalerCount] = runNum;
	scalerSpillID[scalerCount] = spillID;
	scalerROC[scalerCount] = ROCID;
	scalerBoardID[scalerCount] = boardID;
	scalerChannelID[scalerCount] = i;
	scalerValue[scalerCount] = physicsEvent[j];
	scalerCount++;

	if (scalerCount==max_scaler_rows){
	    make_scaler_query(conn);
	    scalerCount = 0;
	}

	j++;

    }

    return j;

}

int eventTriggerSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j) {
	
	triggerBits = physicsEvent[j];
	//printf("Event: %i, RocID: %i, TriggerBits: %.8x\n",codaEventNum, ROCID, physicsEvent[j]);
	if((triggerBits & 0x1F)>0){
		triggerType = NIM;
	} else if ((triggerBits & 0x3E0)>0){
		triggerType = FPGA;
	}	

	j++;

	return j;
}

int eventSQL(MYSQL *conn, unsigned int physicsEvent[40000]){
// ================================================================
// Handles Physics Events.  Stores general header information and 
//	sends the different types of Physics Events to their 
//	respective functions.
// Returns: 	0 on success
//		1 on error

	MYSQL_RES* res;
	int eventCode = 0;
	int headerLength = 0;
	int headerCode = 0;
	char sqlCodaEventQuery[256];
	int i, j, k, temproc, v, x, submit;
	evLength = 0;
	eventType = 0;
	dataQuality = 0;
	triggerBits = 0;
	codaEventNum = 0;
	vmeTime = 0;
	submit=0;
	
	evLength = physicsEvent[0];
	eventCode = physicsEvent[1];
	headerLength = physicsEvent[2];
	headerCode = physicsEvent[3];
	codaEventNum = physicsEvent[4]+4;
	eventType = physicsEvent[5];
	// physicsEvent[6] = 0 (reserved value)

	switch (get_hex_bits(physicsEvent[1],7,4)){
		
		case 14:
		  j = 7;
		  if(sampling && sampling_count<20){ sampling_count++; return 0; }
		  else if (sampling && sampling_count==20) { sampling_count=1; }

		  eventNum++;

		  while (j<evLength){
		    rocEvLength = physicsEvent[j];
		    v = j+rocEvLength;
		    if (v > evLength) { 
			printf("ERROR: ROC Event Length exceeds event length\n"
			"Event: %i, EventLength = %i, position = %i, rocEvLength = %.8x\n", 
			codaEventNum, evLength, j, physicsEvent[j]);
			for(x=j-48;x<j+50;x++){
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
		    if(ROCID==14)codaEvVmeTime = vmeTime;
		    j++;
		    e906flag = physicsEvent[j];
		    j++;
		    if ( rocEvLength < 5 ) { j = v+1; }
		    while(j<=v){
			j = format(conn, physicsEvent, j, v);
		    }
		    submit = 1;
		  }
		  break;

		case 130:
		    // Slow Control Type
		    if ( eventSlowSQL(conn, physicsEvent) ) {
		    	printf("Slow Control Event Processing Error\n");
		    	return 1;
		    }
		    submit = 0;
		    break;

		case 140:
		    // Start of run descriptor
		    if ( eventRunDescriptorSQL(conn, physicsEvent) ) {
		    	printf("Start of Run Desctiptor Event Processing Error\n");
		    	return 1;
		    }
		    submit = 0;
		    break;

		case 11:
		    // Beginning of spill
		    j = 7;
		    spillID++;
		    //sprintf(spillType,"BEGIN");
		    spillType = 1;
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
				

			beginSpillBind = clock();

			if (mysql_stmt_execute(spillStmt))
			{
				printf("Spill Insert Error: %s\n", mysql_error(conn));
				return 1;
			}
			
			endSpillBind = clock();

        		totalSpillBind += ((double)( endSpillBind - beginSpillBind ) / (double)CLOCKS_PER_SEC);			

			e906flag = physicsEvent[j];
			j++;
			if ( rocEvLength < 5 ) { j = v+1; }
			while(j<=v){
				j = format(conn, physicsEvent, j, v);
			}

			if (scalerCount > 0) {
				make_scaler_query(conn);
				scalerCount = 0;
			}
		    }
		    submit = 0;
		    break;

		case 12:
		    // End of spill.
		    j = 7;
 		    //sprintf(spillType,"END");
		    spillType = 2;
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
			
			beginSpillBind = clock();

			mysql_stmt_bind_param(spillStmt, spillBind);
				
			if (mysql_stmt_execute(spillStmt))
			{
				printf("Spill Insert Error: %s\n", mysql_error(conn));
				return 1;
			}

			endSpillBind = clock();

        		totalSpillBind += ((double)( endSpillBind - beginSpillBind ) / (double)CLOCKS_PER_SEC);

			if (optimCounter == 20 && online){
				if( mysql_query(conn, "OPTIMIZE LOCAL TABLE Run, Spill, Event, Hit, TriggerHit, Scaler") )
				{
					printf("Optimize Table Error: %s\n", mysql_error(conn));
					return 1;
				}
				res = mysql_use_result(conn);
				mysql_free_result(res);
				optimCounter = 1;
			} else if (optimCounter<20 && online) { optimCounter++; }

			e906flag = physicsEvent[j];
			j++;
			if ( rocEvLength < 5 ) { j = v+1; }
			while(j<=v){
				j = format(conn, physicsEvent, j, v);
			}
			if (scalerCount > 0) {
				make_scaler_query(conn);
				scalerCount = 0;
			}

		    }
		    submit = 0;
		    break;

		default:
		    // Awaiting further event types
		    printf("Unknown event type: %x\n", eventType);
		    submit = 0;
		    break;
	}	

	if(submit){

		beginCodaBind = clock();

		mysql_stmt_bind_param(codaEvStmt, codaEvBind);

		if (mysql_stmt_execute(codaEvStmt))
    		{
			printf("Event Insert Error: %s\n", mysql_error(conn));
			return 1;
		}

		endCodaBind = clock();

        	totalCodaBind += ((double)( endCodaBind - beginCodaBind ) / (double)CLOCKS_PER_SEC);
	}
	return 0;

}

int format(MYSQL *conn, unsigned int physicsEvent[40000], int j, int v) {

	int x;

	if        ( e906flag == 0xe906f002 ) {
		// TDC event type -- HISTORICAL
		//j = eventTDCSQL(conn, physicsEvent, j);
		// Ignore Board
		while((j<=v) && ((physicsEvent[j] & 0xFFFFF000)!=0xE906F000)){j++;}
		if(j==v){
			// The end of the ROC data has been reached, move on
			j++;
		}
	} else if ( e906flag == 0xE906F001 ) {
		// Latch event type
		j = eventLatchSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F003 ) {
		// Scaler event type
		j = eventScalerSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F004 ) {
		// New TDC (non zero-suppressed) event type
		j = eventNewTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F005 ) {
		// v1495 TDC type
		j = eventv1495TDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F006 ) {
		// Reimer TDC type
		j = eventReimerTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F007 ) {
		// Wide-Channel TDC type
		j = eventWCTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F008 ) {
		// Zero-Suppressed Wide-Channel TDC type
		j = eventZSWCTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906f009 ) {
		// Zero-Suppressed Jy TDC type
		j = eventJyTDCSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F00F ) {
		// Trigger type
		j = eventTriggerSQL(conn, physicsEvent, j);
	} else if ( e906flag == 0xE906F000 ) {
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
		
		for(x=j-48;x<j+48;x++){
			printf("physicsEvent[%i] == %.8x\n",x,physicsEvent[x]);
		}
		
		return j;
	}
	if (j<v){ e906flag = physicsEvent[j]; j++; }

	return j;

}

int endEventSQL(MYSQL *conn, unsigned int physicsEvent[40000]){
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

	MYSQL_RES* res;
	MYSQL_ROW row;
	char outputFileName[10000];
	//char insertQuery[1000000];
	char qryString[1000];
	int pid, k;
	FILE *fp;

	beginTDCwhole = clock();
	
	if( mysql_query(conn, "DELETE FROM tempHit") ) {
		printf("Deleting tempHit Data Error: %s\n", mysql_error(conn));
   	}
	
	beginTDCfile = clock();

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
		fprintf(fp, "%i\t%i\t%i\t%i\t%i\t%i\t\\N\t\\N\t%f\t0\t\\N\t\\N\n", tdcRunID[k], tdcSpillID[k],
			tdcCodaID[k], tdcROC[k], tdcBoardID[k], tdcChannelID[k], tdcStopTime[k]);
	}

	// File MUST be closed before it can be loaded into MySQL
	fclose(fp);

	endTDCfile = clock();
        totalTDCfile += ((double)( endTDCfile - beginTDCfile ) / (double)CLOCKS_PER_SEC);


	sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' INTO TABLE tempHit", outputFileName);
	
	beginTDCload = clock();
	beginTimeTDCload = time(NULL);

	// Submit the query to the server	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Load Hit Data Error: %s\n", mysql_error(conn));
		return 1;
	}

	endTDCload = clock();
	endTimeTDCload = time(NULL);
        totalTDCload += ((double)( endTDCload - beginTDCload ) / (double)CLOCKS_PER_SEC);
	totalTimeTDCload += (double)( endTimeTDCload - beginTimeTDCload );

	if( mysql_warning_count(conn) > 0 )
	{
		printf("Warnings have occured from the Hit upload:\n");
		showWarnings(conn);
	}

	remove(outputFileName);

	// Clear the arrays
	memset((void*)&tdcChannelID, 0, sizeof(unsigned char)*max_tdc_rows);
	memset((void*)&tdcCodaID, 0, sizeof(unsigned int)*max_tdc_rows);
	memset((void*)&tdcRunID, 0, sizeof(unsigned short int)*max_tdc_rows);
	memset((void*)&tdcSpillID, 0, sizeof(unsigned short int)*max_tdc_rows);
	memset((void*)&tdcROC, 0, sizeof(unsigned char)*max_tdc_rows);
	memset((void*)&tdcBoardID, 0, sizeof(unsigned short int)*max_tdc_rows);
	memset((void*)&tdcStopTime, 0, sizeof(float)*max_tdc_rows);

	beginTDCmap = clock();
	beginTimeTDCmap = time(NULL);

	if (mysql_query(conn, 
		"UPDATE tempHit h LEFT JOIN \n" 
		"(Calibration c LEFT JOIN Mapping m USING(rocID, boardID, channelID)) \n"
   		"USING(rocID, boardID, channelID)\n"
		"SET h.detectorName = m.detectorName, \n"
    		"h.elementID = m.elementID, \n"
    		"h.driftTime=IF((c.t0-h.tdcTime<0),0.0,c.t0-h.tdcTime), \n"
    		"h.driftDistance=IF((c.t0-h.tdcTime<0.0),0.0,c.t0-h.tdcTime)*0.005, \n"
		"h.inTime=IF((h.tdcTime<c.tMax AND h.tdcTime>c.tMin),1,0)") )
        {
                printf("Mapping and Calibration Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, 
		"UPDATE tempHit h LEFT JOIN \n" 
		"( Mapping m LEFT JOIN Timing t USING(rocID, boardID, channelID) ) \n"
   		"USING(rocID, boardID, channelID)\n"
		"SET h.detectorName = m.detectorName, \n"
    		"h.elementID = m.elementID, \n"
    		"h.driftTime=NULL, \n"
    		"h.driftDistance=NULL, \n"
		"h.inTime=IF((h.tdcTime<(t.tPeak+0.5*t.width) AND h.tdcTime>(t.tPeak-0.5*t.width)),1,0)"
		"WHERE h.detectorName IS NULL") )
        {
                printf("Mapping and timing Error: %s\n", mysql_error(conn));
                return 1;
        }

	endTDCmap = clock();
        totalTDCmap += ((double)( endTDCmap - beginTDCmap ) / (double)CLOCKS_PER_SEC);
	endTimeTDCmap = time(NULL);
        totalTimeTDCmap += (double)( endTimeTDCmap - beginTimeTDCmap );
	

	beginTDCinsert = clock();
	beginTimeTDCinsert = time(NULL);

	if (mysql_query(conn, "INSERT INTO Hit (eventID, runID, spillID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, driftTime, driftDistance, inTime) "
		"SELECT eventID, runID, spillID, rocID, boardID, channelID, "
		"detectorName, elementID, tdcTime, driftTime, driftDistance, inTime FROM tempHit") )
	{
		printf("Error (102): %s\n", mysql_error(conn));
                return 1;
        }
/*
	if( mysql_query(conn, "SELECT * FROM tempHit") )
	{
		printf("Select * From tempHit Table Error: %s\n", mysql_error(conn));
		return 1;
	}
	res = mysql_store_result(conn);

	if (row = mysql_fetch_row(res)){
		sprintf(insertQuery, "INSERT DELAYED INTO Hit (runID, spillID, eventID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, inTime, driftTime, driftDistance) "
		"VALUES (%s, %s, %s, %s, %s, %s, '%s', %s, %s, %s, %s, %s)",
		row[0],row[1],row[2],row[3],row[4],row[5],row[6],row[7],row[8],row[9],row[10],row[11]);
	}
	while (row = mysql_fetch_row(res)) {
		sprintf(insertQuery, "%s, (%s, %s, %s, %s, %s, %s, '%s', %s, %s, %s, %s, %s)", insertQuery,
		row[0],row[1],row[2],row[3],row[4],row[5],row[6],row[7],row[8],row[9],row[10],row[11]);
	}
	mysql_free_result(res);
	if( mysql_query(conn, insertQuery) )
	{
		printf("Hit Insert Error: %s\n%s", mysql_error(conn),insertQuery);
		return 1;
	}
*/
	endTDCinsert = clock();
        totalTDCinsert += ((double)( endTDCinsert - beginTDCinsert ) / (double)CLOCKS_PER_SEC);
	endTimeTDCinsert = time(NULL);
        totalTimeTDCinsert += (double)( endTimeTDCinsert - beginTimeTDCinsert );

	row_count+=tdcCount;

	if (mysql_query(conn, "DELETE FROM tempHit") )
	{
		printf("Error (103): %s\n", mysql_error(conn));
                return 1;
        }

	endTDCwhole = clock();
        totalTDCwhole += ((double)( endTDCwhole - beginTDCwhole ) / (double)CLOCKS_PER_SEC);
	
	return 0;

}

int make_v1495_query(MYSQL* conn){


	MYSQL_RES* res;
	MYSQL_ROW row;
	char insertQuery[1000000];
	char outputFileName[10000];
	char qryString[1000];
	int pid, k;
	FILE *fp;
	
	if( mysql_query(conn, "DELETE FROM tempHit") ) {
		printf("Deleting tempHit Table Error: %s\n", mysql_error(conn));
   	}
	

	beginv1495file = clock();

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
		fprintf(fp, "%i\t%i\t%i\t%i\t%i\t%i\t\\N\t\\N\t%f\t0\t\\N\t\\N\n", v1495RunID[k], v1495SpillID[k],
			v1495CodaID[k], v1495ROC[k], v1495BoardID[k], v1495ChannelID[k], v1495StopTime[k]);
	}

	// File MUST be closed before it can be loaded into MySQL
	fclose(fp);

	endv1495file = clock();
        totalv1495file += ((double)( endv1495file - beginv1495file ) / (double)CLOCKS_PER_SEC);


	beginv1495load = clock();
	beginTimev1495load = time(NULL);

	sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' INTO TABLE tempHit", outputFileName);
	
	// Submit the query to the server	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Load v1495 Data Error: %s\n", mysql_error(conn));
		return 1;
	}

	endv1495load = clock();
        totalv1495load += ((double)( endv1495load - beginv1495load ) / (double)CLOCKS_PER_SEC);
	endTimev1495load = time(NULL);
        totalTimev1495load += (double)( endTimev1495load - beginTimev1495load );


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
	memset((void*)&v1495StopTime, 0, sizeof(double)*max_v1495_rows);

	beginv1495map = clock();
	beginTimev1495map = time(NULL);

	if (mysql_query(conn, 
		"UPDATE tempHit h LEFT JOIN \n" 
		"( Mapping m LEFT JOIN Timing t USING(rocID, boardID, channelID) ) \n"
   		"USING(rocID, boardID, channelID)\n"
		"SET h.detectorName = m.detectorName, \n"
    		"h.elementID = m.elementID, \n"
    		"h.driftTime=NULL, \n"
    		"h.driftDistance=NULL, \n"
		"h.inTime=IF((h.tdcTime<(t.tPeak+0.5*t.width) AND h.tdcTime>(t.tPeak-0.5*t.width)),1,0)"
		"WHERE h.detectorName IS NULL") )
        {
                printf("Mapping and Calibration Error: %s\n", mysql_error(conn));
                return 1;
        }

	endv1495map = clock();
        totalv1495map += ((double)( endv1495map - beginv1495map ) / (double)CLOCKS_PER_SEC);
	endTimev1495map = time(NULL);
        totalTimev1495map += (double)( endTimev1495map - beginTimev1495map );

	beginv1495insert = clock();
	beginTimev1495insert = time(NULL);

	if (mysql_query(conn, "INSERT INTO TriggerHit (eventID, runID, spillID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, inTime) "
		"SELECT eventID, runID, spillID, rocID, boardID, channelID, "
		"detectorName, elementID, tdcTime, inTime FROM tempHit") )
	{
		printf("Insert into TriggerHit Error: %s\n", mysql_error(conn));
                return 1;
        }
/*

	if( mysql_query(conn, "SELECT * FROM tempHit") )
	{
		printf("Select * From tempHit Table Error: %s\n", mysql_error(conn));
		return 1;
	}
	res = mysql_store_result(conn);

	if (row = mysql_fetch_row(res)){
		sprintf(insertQuery, "INSERT DELAYED INTO TriggerHit (runID, spillID, eventID, "
		"rocID, boardID, channelID, detectorName, elementID, tdcTime, inTime) "
		"VALUES (%s, %s, %s, %s, %s, %s, '%s', %s, %s, %s)",
		row[0],row[1],row[2],row[3],row[4],row[5],row[6],row[7],row[8],row[9]);
	}
	while (row = mysql_fetch_row(res)) {
		sprintf(insertQuery, "%s, (%s, %s, %s, %s, %s, %s, '%s', %s, %s, %s)", insertQuery,
		row[0],row[1],row[2],row[3],row[4],row[5],row[6],row[7],row[8],row[9]);
	}
	mysql_free_result(res);
	if( mysql_query(conn, insertQuery) )
	{
		printf("Hit Insert Error: %s\n%s", mysql_error(conn),insertQuery);
		return 1;
	}

*/

	endv1495insert = clock();
        totalv1495insert += ((double)( endv1495insert - beginv1495insert ) / (double)CLOCKS_PER_SEC);
	endTimev1495insert = time(NULL);
        totalTimev1495insert += (double)( endTimev1495insert - beginTimev1495insert );

	if (mysql_query(conn, "DELETE FROM tempHit") )
	{
		printf("Delete from tempHit Error: %s\n", mysql_error(conn));
                return 1;
        }

	return 0;

}

int make_latch_query(MYSQL *conn){
	
	char outputFileName[10000];
	char qryString[1000];
	int pid, k;
	FILE *fp;
	
	if( mysql_query(conn, "DELETE FROM tempHit") ) {
		printf("Deleting tempHit Table Error: %s\n", mysql_error(conn));
	}
	
	// This method prints the data out to file and then uploads directly via LOAD DATA INFILE
	pid = getpid();

	sprintf(outputFileName,".latch.%i.temp", pid);
	
	// Open the temp file 
	if (file_exists(outputFileName)) remove(outputFileName);
	fp = fopen(outputFileName,"w");

	if (fp == NULL) {
		fprintf(stderr, "Can't open output file %s!\n",
			outputFileName);
		exit(1);
	}

	for(k=0;k<latchCount;k++){
		fprintf(fp, "%i\t%i\t%i\t%i\t%i\t%i\t\\N\t\\N\t\\N\t0\t\\N\t\\N\n", latchRunID[k], latchSpillID[k], latchCodaID[k],
			latchROC[k], latchBoardID[k], latchChannelID[k]);
	}

	// File MUST be closed before it can be loaded into MySQL
	fclose(fp);

	sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' INTO TABLE tempHit", outputFileName);
	
	// Submit the query to the server	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Load Latch Data Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_warning_count(conn) > 0 )
	{
		printf("Warnings have occured from the Latch upload:\n");
		showWarnings(conn);
	}

	remove(outputFileName);
	
	memset((void*)&latchChannelID, 0, sizeof(int)*max_latch_rows);
	memset((void*)&latchRocID, 0, sizeof(int)*max_latch_rows);
	memset((void*)&latchCodaID, 0, sizeof(int)*max_latch_rows);
	memset((void*)&latchRunID, 0, sizeof(int)*max_latch_rows);
	memset((void*)&latchSpillID, 0, sizeof(int)*max_latch_rows);
	memset((void*)&latchROC, 0, sizeof(int)*max_latch_rows);
	memset((void*)&latchBoardID, 0, sizeof(int)*max_latch_rows);
	
	if (mysql_query(conn, 
		"UPDATE tempHit h LEFT JOIN \n" 
		"(Mapping m LEFT JOIN Calibration c USING(rocID, boardID, channelID)) \n"
   		"USING(rocID, boardID, channelID)\n"
		"SET h.detectorName = m.detectorName, \n"
    		"h.elementID = m.elementID, \n"
    		"h.driftTime=IF((c.t0-h.tdcTime<0),0.0,c.t0-h.tdcTime), \n"
    		"h.driftDistance=IF((c.t0-h.tdcTime<0.0),0.0,c.t0-h.tdcTime)*0.005, \n"
		"h.inTime=IF((h.tdcTime<c.tMax AND h.tdcTime>c.tMin),1,0)") )
        {
                printf("Mapping and Calibration Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO Hit (runID, eventID, spillID, "
		"rocID, boardID, channelID, detectorName, elementID, driftTime, driftDistance, inTime) "
		"SELECT runID, eventID, spillID, rocID, boardID, channelID, "
		"detectorName, elementID, driftTime, driftDistance, inTime FROM tempHit") )
	{
		printf("Inserting Latch Data to Hit Table Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempHit") )
	{
		printf("Deleting tempHit Table Error: %s\n", mysql_error(conn));
                return 1;
        }

	return 0;

}

int make_scaler_query(MYSQL* conn){

	char outputFileName[10000];
	char qryString[1000];
	int pid, k;
	FILE *fp;
	
	if( mysql_query(conn, "DELETE FROM tempScaler") ) {
		printf("Deleting tempScaler Table Error: %s\n", mysql_error(conn));
	}
	
	// This method prints the data out to file and then uploads directly via LOAD DATA INFILE
	pid = getpid();

	sprintf(outputFileName,".scalar.%i.temp", pid);
	
	// Open the temp file 
	if (file_exists(outputFileName)) remove(outputFileName);
	fp = fopen(outputFileName,"w");

	if (fp == NULL) {
		fprintf(stderr, "Can't open output file %s!\n",
			outputFileName);
		exit(1);
	}

	for(k=0;k<scalerCount;k++){
		fprintf(fp, "%i\t%i\t%i\t%i\t%i\t%i\t\\N\t\\N\t%i\n", runNum, scalerSpillID[k], 
			scalerCodaID[k], scalerROC[k], scalerBoardID[k], scalerChannelID[k], 
			scalerValue[k]);
	}

	// File MUST be closed before it can be loaded into MySQL
	fclose(fp);

	sprintf(qryString,"LOAD DATA LOCAL INFILE '%s' INTO TABLE tempScaler", outputFileName);
	
	// Submit the query to the server	
	if(mysql_query(conn, qryString))
	{
		printf("%s\n", qryString);
		printf("Load Scaler Data Error: %s\n", mysql_error(conn));
		return 1;
	}

	if( mysql_warning_count(conn) > 0 )
	{
		printf("Warnings have occured from the Scaler upload:\n");
		showWarnings(conn);
	}

	remove(outputFileName);
	
	// Clear the arrays
	memset((void*)&scalerChannelID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerCodaID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerRunID, 0, sizeof(unsigned short int)*max_scaler_rows);
	memset((void*)&scalerSpillID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerROC, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerBoardID, 0, sizeof(int)*max_scaler_rows);
	memset((void*)&scalerValue, 0, sizeof(int)*max_scaler_rows);
	
	if (mysql_query(conn, "UPDATE tempScaler t, Mapping m\n"
                "SET t.detectorName = m.detectorName, t.elementID = m.elementID\n"
                "WHERE t.detectorName IS NULL AND t.rocID = m.rocID AND "
                "t.boardID = m.boardID AND t.channelID = m.channelID") )
        {
                printf("Mapping Scaler Data Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "INSERT INTO Scaler (runID, spillID, codaEventID, "
		"rocID, boardID, channelID, detectorName, elementID, value) "
		"SELECT runID, spillID, codaEventID, rocID, boardID, channelID, "
		"detectorName, elementID, value FROM tempScaler") )
	{
		printf("Inserting from tempScaler to Scaler Error: %s\n", mysql_error(conn));
                return 1;
        }

	if (mysql_query(conn, "DELETE FROM tempScaler") )
	{
		printf("Deleting tempScaler Table Error: %s\n", mysql_error(conn));
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
