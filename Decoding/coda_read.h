//using namespace std;
#ifndef __CODAREAD__
#define __CODAREAD__
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <mysql.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <time.h>

// EVIO Values
int handle;

//Decoder Values
int online, sampling;
char *fileName;
char end_file_name [1000];

// Run Values
char descriptor[10000];
unsigned short int runNum;
int runType, numEventsInRunThusFar;
unsigned int runTime;

// CODA Event Values
double goEventTime, endEventTime, stopCountTime;
unsigned int codaEventNum;
int eventType, evLength, evNum; 
unsigned short int triggerBits, dataQuality;

// Counters
int slowControlCount;

// ROC Values
unsigned char ROCID;
unsigned short int boardID; 

// TDC Values
int const max_tdc_rows = 5000;
unsigned int tdcCodaID[5000];
unsigned char tdcROC[5000], tdcChannelID[5000];
unsigned short int tdcBoardID[5000], tdcRunID[5000], tdcSpillID[5000], tdcVmeTime[5000];
float tdcStopTime[5000];
unsigned short int tdcCount;
char errString[1028];

// v1495 Values
int const max_v1495_rows = 5000;
int v1495RocID[5000], v1495CodaID[5000], v1495RunID[5000], v1495SpillID[5000];
int v1495ROC[5000], v1495ChannelID[5000];
unsigned short int v1495BoardID[5000];
float v1495StopTime[5000];
int v1495VmeTime[5000];
int v1495Count;

// Latch Values
int const max_latch_rows = 5000;
int latchRocID[5000], latchCodaID[5000], latchRunID[5000], latchSpillID[5000], latchROC[5000];
int latchBoardID[5000], latchChannelID[5000], latchVmeTime[5000];
int latchCount;

// Spill Values
unsigned short int spillID, spillVmeTime;
unsigned char spillType;

// Scaler Values
int const max_scaler_rows = 5000;
int scalerCodaID[5000], scalerRunID[5000], scalerSpillID[5000], scalerROC[5000];
int scalerBoardID[5000], scalerChannelID[5000], scalerValue[5000], scalerVmeTime[5000];
int scalerCount;

// New TDC Values
double const trigger_pattern[8] = { 0.0, 0.0, 0.0, 0.0, 10.0, 2.5, 5.0, 7.5 };
double const channel_pattern[8] = { 10.0, 2.5, 5.0, 7.5, 0.0, 0.0, 0.0, 0.0 };

// WC TDC Values
double const wc_trigger_pattern[8] = { 0.0, 0.0, 0.0, 0.0, 40.0, 10.0, 20.0, 30.0 };
double const wc_channel_pattern[8] = { 40.0, 10.0, 20.0, 30.0, 0.0, 0.0, 0.0, 0.0 };

// Some global variables
double vmeTime;
unsigned int rocEvLength, e906flag;
int force;

// MySQL Values and Statement Bind Objects
MYSQL_STMT *runStmt;
MYSQL_STMT *spillStmt;
MYSQL_STMT *codaEvStmt;
MYSQL_BIND runBind[2];
MYSQL_BIND spillBind[6];
MYSQL_BIND codaEvBind[5];
char *server;
char *user;
char *password;
char *database;
char *schema;


// Benchmarking Values

clock_t beginSQLcreate, endSQLcreate;
double totalSQLcreate;

time_t beginTimeIndexing, endTimeIndexing;
double totalTimeIndexing;

clock_t beginTDCfile, endTDCfile;
double totalTDCfile;
clock_t beginTDCload, endTDCload;
double totalTDCload;
clock_t beginTDCmap, endTDCmap;
double totalTDCmap;
clock_t beginTDCinsert, endTDCinsert;
double totalTDCinsert;
clock_t beginTDCwhole, endTDCwhole;
double totalTDCwhole;

time_t beginTimeTDCload, endTimeTDCload;
double totalTimeTDCload;
time_t beginTimeTDCmap, endTimeTDCmap;
double totalTimeTDCmap;
time_t beginTimeTDCinsert, endTimeTDCinsert;
double totalTimeTDCinsert;


clock_t beginv1495file, endv1495file;
double totalv1495file;
clock_t beginv1495load, endv1495load;
double totalv1495load;
clock_t beginv1495map, endv1495map;
double totalv1495map;
clock_t beginv1495insert, endv1495insert;
double totalv1495insert;

time_t beginTimev1495load, endTimev1495load;
double totalTimev1495load;
time_t beginTimev1495map, endTimev1495map;
double totalTimev1495map;
time_t beginTimev1495insert, endTimev1495insert;
double totalTimev1495insert;


clock_t beginSlowfile, endSlowfile;
double totalSlowfile;
clock_t beginSlowload, endSlowload;
double totalSlowload;
time_t beginTimeSlowload, endTimeSlowload;
double totalTimeSlowload;

clock_t beginCodaBind, endCodaBind;
double totalCodaBind;

clock_t beginSpillBind, endSpillBind;
double totalSpillBind;

clock_t beginJyTDC, endJyTDC;
double totalJyTDC;

clock_t beginReimerTDC, endReimerTDC;
double totalReimerTDC;

clock_t beginv1495, endv1495;
double totalv1495;

clock_t beginZSWC, endZSWC;
double totalZSWC;

clock_t beginWC, endWC;
double totalWC;

clock_t beginTDC, endTDC;
double totalTDC;

clock_t cpuStart, cpuEnd;
time_t timeStart, timeEnd; 
double timeTotal;
double cpuTotal; 

// v1495 debugging variables
int case1, case2, case3, case4, case5, case6, case7, case8;

// Some Constants used.
enum { SUCCESS = 0 };
enum { ERROR = -1 };
enum { CODA_EVENT = 0x01cc };
enum { PRESTART_EVENT = 0x001101cc };
enum { GO_EVENT = 0x001201cc };
enum { END_EVENT = 0x001401cc };
enum { PHYSICS_EVENT = 0x10cc };
enum { EXIT_CASE = 0x66666666 };
enum { ON = 1 };
enum { OFF = 0 };
enum { WAIT_TIME = 5 };


// =======================================================
// BEGIN FUNCTION DECLARATIONS:
// =======================================================
// Checks to see if the CODA file exists
int file_exists(char *fileName);
// -------------------------------------------------------
// Handles command-line options
int initialize(int argc, char* argv[]);
// -------------------------------------------------------
// Uses evOpen from evio library to open the CODA file
//int open_coda_file(char *filename);
// -------------------------------------------------------
// Uses evClose from evio library to close the CODA file 
int close_coda_file();
// -------------------------------------------------------
// Uses evRead from the evio library to fill the buffer with the next event
int read_coda_file(unsigned int physicsEvent[40000],int evnum);
// -------------------------------------------------------
// Retrieves a certain number of hex digits from an unsigned int starting
// 	a certain number of digits from the right of the number
int get_hex_bits(unsigned int hexNum, int numBitFromRight, int numBits);
// -------------------------------------------------------
// Retrieves a single hex digit from an unsigned int a certain number of digits
// 	from the right of the number
int get_hex_bit(unsigned int hexNum, int numBitFromRight);
// -------------------------------------------------------
// Does what get_hex_bits does, only for binary numbers
int get_bin_bits(unsigned int binNum, int numBitFromRight, int numBits);
// -------------------------------------------------------
// Does what get_hex_bit does, only for binary numbers
int get_bin_bit(unsigned int binNum, int numBitFromRight);
// -------------------------------------------------------
// Issues the commands to the SQL server to assemble the necessary schemas and 
//	tables if they do not yet exist
int createSQL(MYSQL *conn, char *schema);
// -------------------------------------------------------
// This function handles prestart-type CODA events
int prestartSQL(MYSQL *conn, unsigned int physicsEvent[40000]);
// -------------------------------------------------------
// This function handles goEvent-type CODA events
int goEventSQL(MYSQL *conn, unsigned int physicsEvent[40000]);
// -------------------------------------------------------
// This function handles physics-type CODA events
int eventSQL(MYSQL *conn, unsigned int physicsEvent[40000]);
// -------------------------------------------------------
// This function handles endEvent-type CODA events
int endEventSQL(MYSQL *conn, unsigned int physicsEvent[40000]);
// -------------------------------------------------------
// This function handles Spill Begin and Spill End events
int eventSpillSQL(MYSQL *conn, unsigned int physicsEvent[40000]);
// -------------------------------------------------------
// This function hanldes Scaler-type events
int eventScalerSQL(MYSQL *conn, unsigned int physicsEvent[40000], int j);
// -------------------------------------------------------
// This function, if reading while on-line, will wait and try again if
// 	an EOF is encountered before the End Event
int retry(char *file, long unsigned int oldfilesize, int codaEventCount, unsigned int physicsEvent[40000]);
// --------------------------------------------------------
// This will make a 200-row insert statement with TDC data
int make_data_query(MYSQL* conn);
// --------------------------------------------------------
// This will make a 200-row insert statement with Latch data
int make_latch_query(MYSQL* conn);
// --------------------------------------------------------

// ========================================================
// END FUNCTION DECLARATIONS
// ========================================================



int open_coda_file(char *filename)
{
//
// This function calls evio's evOpen function in order to open the
//	specified CODA file.
//
// Returns:	status of evOpen
//

  return(evOpen(filename, "r", &handle));
}

int get_hex_bit(unsigned int hexNum, int numBitFromRight)
{
// ================================================================
//
// This function takes an integer, grabs a certain number of hexadecimal digits
//	from a certain position in the hex representation of the number.
//
// For example, if number = 0x10e59c (or, 0d107356)
//		then get_hex_bit(number, 3) would return e (or 0d14), 
//		representing 0x10e59c <-- those parts of the number
//				 ^
    int shift;
    unsigned int hexBit;

    // Shift the number to get rid of the bits on the right that we want
    shift = numBitFromRight;
    hexBit = hexNum;
    hexBit = hexBit >> (4*shift);

    // Do the bitwise AND operation
    hexBit = hexBit & 0xF;

    return hexBit;
}

int get_hex_bits(unsigned int hexNum, int numBitFromRight, int numBits)
{
// ================================================================
//
// This function takes an integer, grabs a certain number of hexadecimal digits
//	from a certain position in the hex representation of the number.
//
// For example, if number = 0x10e59c (or, 0d107356)
//		then get_bin_bits(number, 3, 3) would return e59 (or 0d3673), 
//		representing 0x10e59c <-- those parts of the number
//				 ^^^

    unsigned int hexBits=0x0;
    int shift;
    unsigned int bitwiseand=0xF;
    unsigned int eff=0xF;
    int i;
    // Bitwise method.  Shift the bits, and use bitwise AND to get the bits we want

    // Shift the number to get rid of the bits on the right that we want
    shift = numBitFromRight - numBits + 1;
    hexBits = hexNum;
    hexBits = hexBits >> (4*shift);

    // Assemble the number that we will use with the above number in the bitwise AND operation
    //   so, if we want get_hex_bits(hexNum, 3, 2), it will make 0xFF
    for (i=1;i<numBits;i++) { bitwiseand+=(eff << (4*i)); }

    // Do the bitwise AND operation
    hexBits = hexBits & bitwiseand;

    return hexBits;
}

int get_bin_bit(unsigned int binNum, int numBitFromRight)
{
// ================================================================
//
// This function takes an integer, grabs a certain binary digit
//	from a certain position in the binary number.
//
// For example, if number = 11010001101011100 (or, 0d107356)
//		then get_bin_bit(number, 3) would return 1 (or 0d01), 
//		representing 11010001101011100 <-- this part of the number
//					  ^
    while (numBitFromRight--) {
        binNum /= 2;
    }
    return (binNum % 2);
}

int get_bin_bits(unsigned int binNum, int numBitFromRight, int numBits)
{
// ================================================================

// This function takes an integer, grabs a certain number of binary digits
//	from a certain position in the binary number.
//
// For example, if number = 11010001101011100,
//		then get_bin_bits(number, 3, 3) would return 110 (or 0d06), 
//		representing 11010001101011100 <-- those parts of the number
//			                  ^^^
    int binBit=0;
    int binBits=0;
    int n=0;
    float d=1.0;

    for (n=0;n<(numBits-1);n++) d*=2.0;
    for (n=0;n<(numBits) && n<=numBitFromRight;n++){
        binBit = get_bin_bit(binNum, numBitFromRight-n);
        binBits += binBit*d;
        d /= 2;
    }
    return binBits;
}

int read_coda_file(unsigned int physicsEvent[40000],int evnum)
{
// ================================================================
//
//
// Moves to the next entry in the CODA file
// Returns:	CODA status code
//		(See constants at top of file)	

  return evRead(handle,physicsEvent,evnum);

}

int close_coda_file()
{
// ================================================================
//
// Closes the CODA file when done
// Returns:	CODA status code
//		(See constants at top of file)

  return evClose(handle);
}

int file_exists(char *file)
{
// ================================================================
//
// Checks to see if the file specified exists
// Returns: 	1 if file exists, 
//		0 if it does not

    FILE * fp;

    if (fp = fopen(file, "r"))
    {
        fclose(fp);
        return 1;
    }
    return 0;
}

#endif




