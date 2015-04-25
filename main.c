/*
 * File: main.c
 * Description:	TCP/IP server for i-RIS
 */

//
// Standard include files
//
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/timeb.h>

#include <time.h>
#include <string.h>
#include <malloc.h>

//
// Project include files
//
#include "ws_util.h"

#ifdef USE_MYSQL
	#include <mysql.h>
	#define	db_error(mysql, res)					mysql_error(mysql)
void * get_thread_dbh();
void * set_thread_dbh();
#endif

#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
static pthread_mutex_t logMutex;
static pthread_mutex_t counterMutex;
static pthread_mutex_t hsmMutex;

//
// Local include files
//
#include "zlib.h"
#include "macro.h"
#include "3des.h"

typedef struct
{
	SOCKET sd;
	struct sockaddr_in sinRemote;
} T_THREAD_DATA;

//MYSQL * mysql;
int ris_amount = 20000;
int triggerb = 20000;
int rewardb = 2000;
int stan = 0;
int order = 243574;

const unsigned char master[16] =	{0x02, 0x04, 0x08, 0x10, 0x02, 0x04, 0x08, 0x10, 0x02, 0x04, 0x08, 0x10, 0x02, 0x04, 0x08, 0x10};
const unsigned char master_hsm[2][16] =	{{0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
					{0x6B, 0xA1, 0x89, 0xF1, 0x5E, 0xCE, 0x0B, 0x3E, 0xAB, 0x08, 0xB6, 0xEA, 0xB5, 0x1F, 0x0D, 0x25}
										};
const unsigned char ppasn[8] =	{0x07, 0x25, 0x43, 0x61, 0x8F, 0xAB, 0xCD, 0xE9};

static FILE * stream = NULL;
int debug = 1;

int sleepTime = 0;
char * deviceGatewayIPAddress = "localhost";
int deviceGatewayPortNumber = 20301;
#define C_DATAWIRE_RX_BUF_SIZE	3000

char * logFile = "./irisLog";
int strictSerialNumber = 1;
int noTrace = 0;
int running = 1;
int test = 0;
int scan = 0;
int hsm = 0;
int hsm_no = 0;
int hsm_sd = -1;
int ignoreHSM = 0;
int iRewards = 0;
int iScan = 0;
int dispMessage = 0;
int maxZipPacketSize = 5000;
int minZipPacketSize = 1000;

#define NORMAL_SOCKET_WAITTIME	1800
#define	END_SOCKET_WAITTIME		0
int waitTime = NORMAL_SOCKET_WAITTIME;

int counter = 0;

int background_update = 0;

//// Constants /////////////////////////////////////////////////////////

/*
**-------------------------------------------------------------------------------------------
** FUNCTION   : UtilHexToString
**
** DESCRIPTION:	Transforms hex byte array to a string. Each byte is simply split in half.
**				The minimum size required is double that of the hex byte array
**
** PARAMETERS:	hex			<=	Array to store the converted hex data
**				string		=>	The number string.
**				length		<=	Length of hex byte array
**
** RETURNS:		The converted string
**-------------------------------------------------------------------------------------------
*/
char * UtilHexToString(unsigned char * hex, int length, char * string)
{
	int i;

	if (string)
	{
		string[0] = '\0';

		if (hex)
		{
			for (i = 0; i < length; i++)
				sprintf(&string[i*2], "%02X", hex[i]);
		}
	}

	return string;
}

static void counterIncrement(void)
{
	// Counter critical section
	pthread_mutex_lock(&counterMutex);
	counter++;
	pthread_mutex_unlock(&counterMutex);
}

static void counterDecrement(void)
{
	// Counter critical section
	pthread_mutex_lock(&counterMutex);
	counter--;
	pthread_mutex_unlock(&counterMutex);
}

static void logStart(void)
{
	// Initialise
	stream = stdout;

#ifndef WIN32
	// Open the appropriate stream
	if ((stream = fopen(logFile, "a+")) == NULL)
		stream = stdout;
#endif

	// Initialise the log mutex
	pthread_mutex_init(&logMutex, NULL);
}

void logEnd(void)
{
	fclose(stream);

	pthread_mutex_destroy(&logMutex);
}

static void dbStart(void)
{
	// Start of log critical section
	//pthread_mutex_lock(&dbMutex);
}

static void dbEnd(void)
{
	// free log mutex and unlock other threads
	//pthread_mutex_unlock(&dbMutex);
}

static char * timeString(char * string, int len)
{
	struct tm *newtime;
	struct tm temp;
	struct timeb tb;

	ftime(&tb);
	newtime = localtime_r(&tb.time, &temp);
	strftime(string, len, "%a, %d/%m/%Y %H:%M:%S", newtime);
	sprintf(&string[strlen(string)], ".%03ld", tb.millitm);

	return string;
}

int logArchive(FILE **stream, long maxSize)
{
	int result;

	if (stream && *stream != NULL && *stream != stderr && *stream != stdout && ftell(*stream) > maxSize)
	{
		char cmd[400];

		// Log file too large - gzip the current file and start over.
		// Archive name: <path>"<logfile>-DDMMYY.gz"
		if ((result = snprintf(cmd, sizeof(cmd), "gzip -f -S -`date +%%y%%m%%d%%H%%M`.gz %s", logFile)) < 0)
			return -1;

		fclose(*stream);

		system(cmd);

		/* Old log file discarded by gzip */
		if ((*stream = fopen(logFile, "a+")) == NULL)
			*stream = stdout;
	}

	return 1;
}

void logNow(const char * format, ...)
{
	va_list args;
	va_start( args, format );

	// Start of log critical section
	pthread_mutex_lock(&logMutex);

	// Print formatted message
	vfprintf(stream, format, args);
	fflush(stream);

	// check and archive
	logArchive(&stream, (10000*1024L));

	// free log mutex and unlock other threads
	pthread_mutex_unlock(&logMutex);

	va_end(args);
}

static void displayComms(char * header, char * data, int len)
{
	int i, j, k;
	char * line;

	if (dispMessage == 0) return;

	line = malloc(strlen(header) + (4 * len) + (len / 16 * 4) + 200);
	if (line == NULL) return;

	strcpy(line, header);
	strcat(line, "\n");

	for (i = 0, k = strlen(line); i < len;)
	{
		for (j = 0; j < 16; j++, i++)
		{
			if (i < len)
				sprintf(&line[k], "%02.2X ", (BYTE) data[i]);
			else
				strcat(line, "   ");
			k += 3;
		}

		strcat(line, "   ");
		k += 3;

		for (j = 0, i -= 16; j < 16; j++, i++)
		{
			if (i < len)
			{
				if (data[i] == '%')
				{
					line[k++] = '%';
					line[k++] = '%';
				}
				else if (data[i] >= ' ' && data[i] <= '~')
					line[k++] = data[i];
				else
					line[k++] = '.';
			}
			else line[k++] = ' ';
		}

		line[k++] = '\n';
		line[k] = '\0';
	}

	strcat(line, "\n");
	logNow(line);
	free(line);
}


static unsigned long shrink(char * objects)
{
    z_stream c_stream; /* compression stream */
    int err;
    uLong len = (uLong)strlen(objects)+1;
	unsigned long comprLen = len + 200;		// It should be smaller but just in case it actually grows. Small data can possibly cause this.
	unsigned char * compr = malloc(comprLen);	

    c_stream.zalloc = (alloc_func)0;
    c_stream.zfree = (free_func)0;
    c_stream.opaque = (voidpf)0;

    err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);

    c_stream.next_in  = (Bytef*)objects;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen)
	{
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = deflate(&c_stream, Z_NO_FLUSH);
    }

    /* Finish the stream, still forcing small buffers: */
	for (;;)
	{
		c_stream.avail_out = 1;
		err = deflate(&c_stream, Z_FINISH);
		if (err == Z_STREAM_END) break;
	}

	err = deflateEnd(&c_stream);

	// Update the objects with the compressed objects data
	memcpy(objects, compr, c_stream.total_out);
	free(compr);

	return c_stream.total_out;
}

static int databaseInsert(char * query, char * errorMsg)
{
	int result;

	//  Add the object
	dbStart();
	if (mysql_real_query(get_thread_dbh(), query, strlen(query)) == 0) // success
		result = TRUE;
	else
	{
		if (errorMsg) strcpy(errorMsg, db_error(get_thread_dbh(), res));
		result = FALSE;
	}

	dbEnd();

	return result;
}

static long databaseCount(char * query)
{
	long count = -1;
	MYSQL_RES * res;

	dbStart();

	if (mysql_real_query(get_thread_dbh(), query, strlen(query)) == 0) // success
	{
		MYSQL_ROW row;

		if (res = mysql_store_result(get_thread_dbh()))
		{
			if (row = mysql_fetch_row(res))
			{
				if(strlen(row[0])) count = atol(row[0]);
			}
			mysql_free_result(res);
		}
	}

	dbEnd();

	return count;
}

int getFileData(char **pfileData,char *filename,int *len)
{
	FILE *fp = fopen(filename,"rb");
	int fileLen = 0;
	int nRead = 0;
	char *dataBuffer = NULL;

	*len = 0;
	*pfileData = NULL;
	
	if(fp!= NULL ) {
		fseek (fp , 0 , SEEK_END);
		fileLen = ftell (fp);
		rewind (fp);
		*pfileData = malloc( fileLen + 1 );
		nRead = fread(*pfileData,1,fileLen,fp);
		*len = nRead;
		fclose(fp);
	}
	return(*len);
}

int getObjectField(char * data, int count, char * field, char ** srcPtr, const char * tag)
{
	char * ptr;
	int i = 0;
	int j = 0;

	if (srcPtr)
	{
		if ((*srcPtr = strstr(data, tag)) == NULL)
			return 0;
		else
			return (strlen(*srcPtr));
	}

	// Extract the TYPE
	if ((ptr = strstr(data, tag)) != NULL)
	{
		for (--count; ptr[i+strlen(tag)] != ',' && ptr[i+strlen(tag)] != ']' && ptr[i+strlen(tag)] != '}' ; i++)
		{
			for(; count; count--, i++)
			{
				for(; ptr[i+strlen(tag)] != ','; i++);
			}
			field[j++] = ptr[i+strlen(tag)];
		}
	}

	field[j] = '\0';

	return strlen(field);
}

int getNextObject(unsigned char * request, unsigned int requestLength, unsigned int * offset,
				  char * type, char * name, char * version, char * event, char * value, char * object, char * json,
				  unsigned char * iv, unsigned char * MKr, unsigned int * currIVByte, char * serialnumber)
{
	unsigned int i;
	char data;
	unsigned int length = 0;
	int marker = 0;
	char temp[50];

	if (debug) logNow(	"\n%s:: Next OBJECT:: ", timeString(temp, sizeof(temp)));

	for (i = *offset; i < requestLength; i++)
	{
		data = request[i];

#ifndef NO_ENC
		// If currentIV at the beginning of an 8-byte block, encrypt IV using MKr
		if (iv)
		{
			if (*currIVByte % 8 == 0)
				Des3Encrypt(MKr, iv, 8);

			// Get the data in the clear
			data ^= iv[(*currIVByte)%8];
			(*currIVByte)++;
		}
#endif

		// If the marker is detected, start storing the JSON object
		if (marker || data == '{')
			json[length++] = data;

		// If we detect the start of an object, increment the object marker
		if (data == '{')
			marker++;

		// If we detect graphic characters, do not process any further - corrupt message
		else if (((unsigned char) data) > 0x7F)
		{
			logNow("Invalid JSON object: %02X. No further processing....\n", data);
			return -1;
		}

		// IF we detect the end of an object, decrement the marker
		else if (data == '}')
		{
			marker--;
			if (marker < 0)
			{
				logNow("Incorrectly formated JSON object. Found end of object without finding beginning of object\n");
				return -1;
			}
			if (marker == 0)
			{
				i++;
				break;
			}
		}
	}

	// Set the start of the next object search
	*offset = i;

	// If the object exist...
	if (length)
	{
		// Terminate the "json" string
		json[length] = '\0';

		// Extract the type field
		getObjectField(json, 1, type, NULL, "TYPE:");

		if (strcmp(type, "IDENTITY") == 0)
		{
			// Extract the serial number field
			getObjectField(json, 1, name, NULL, "SERIALNUMBER:");

			// Extract the manufacturer field
			getObjectField(json, 1, version, NULL, "MANUFACTURER:");

			// Extract the model field
			getObjectField(json, 1, event, NULL, "MODEL:");

			// Output the data in the clear
			if (debug) logNow("%s\n", json);

			return 1;
		}
		else if (strcmp(type, "GETFILE") == 0)
		{
			// Extract the file name
			getObjectField(json, 1, name, NULL, "NAME:");

			// Output the data in the clear
			if (debug) logNow("%s\n", json);

			return 2;
		}
		else
		{
			// Extract the name field
			getObjectField(json, 1, name, NULL, "NAME:");

			// Extract the version field
			getObjectField(json, 1, version, NULL, "VERSION:");

			// Extract the event field
			getObjectField(json, 1, event, NULL, "EVENT:");

			// Extract the value field
			getObjectField(json, 1, value, NULL, "VALUE:");

			// Extract the object field
			getObjectField(json, 1, object, NULL, "OBJECT:");

			// Output the data in the clear
			if (debug)
			{
				logNow(":+:+:%s:+:+:%s\n", serialnumber, json);
			}

			return 0;
		}
	}
	else logNow("No more...\n");

	return -99;
}

void OFBObjects(unsigned char * response, unsigned long length, char * serialnumber, unsigned char * ivTx)
{
	unsigned int i;
	unsigned char block[16];
	unsigned int currIVTxByte = 0;

	// Get the MKs encryption key to use for OFB encryption
	FILE * fp = fopen(serialnumber, "r+b");

	// If only we can find the file, and we should normally...
	if (fp)
	{
		// Read the encryption key..
		fseek(fp, 32, SEEK_SET);
		fread(block, 1, 16, fp);
		fclose(fp);

		// XOR the data
		for (i = 0; i < length; i++)
		{
			if (currIVTxByte % 8 == 0)
				Des3Encrypt(block, ivTx, 8);

			response[i] ^= ivTx[currIVTxByte%8];
			currIVTxByte++;
		}
	}
}

static void addObject(unsigned char ** response, char * data, int ofb, unsigned int offset, unsigned int maskLength)
{
	char temp[50];

	// If empty additional data, do not bother adding....
	if (data[0] == '\0')
		return;

	my_malloc_max( response, 10240, strlen(data)+1);

	// Output object to be sent
	if (debug)
	{
		if (maskLength)
			logNow("\n%s:: Sending Object:: %.*s **** OBJECT LOGGING TRUNCATED ****\n", timeString(temp, sizeof(temp)), maskLength, data);
		else
			logNow("\n%s:: Sending Object:: %s\n", timeString(temp, sizeof(temp)), data);
	}

	strcat(*response, data);

#ifndef NO_ENC
	if (ofb)
		(*response)[offset-9] = '1';
#endif
}

int getObject(char * query, unsigned char ** response, char * serialnumber, int force)
{
	int retVal = 0;

	MYSQL *dbh = (MYSQL *)get_thread_dbh();

	dbStart();

	#ifdef USE_MYSQL
		if (mysql_real_query(dbh, query, strlen(query)) == 0) // success
		{
			MYSQL_RES * res;
			MYSQL_ROW row;

			if (res = mysql_store_result(dbh))
			{
				if (row = mysql_fetch_row(res))
				{
					int exists = 0;

					// If this is a "no trace" server, just add the object and return
					if (noTrace)
					{
						addObject(response, row[1], 0, 0, 0);
						retVal = 1;
					}
				}
				mysql_free_result(res);
			}
		}
	#endif

	dbEnd();

	return retVal;
}

static void newRandomKey(FILE * fp, char * label, char * authMsg, unsigned char * key, unsigned char * variant)
{
	int i;
	unsigned char block[16];
	unsigned char varKey[16];

	// Prepare a Master variant of 0x82 to encrypt
	for (i = 0; i < 16; i++)
	{
		varKey[i] = key[i] ^ variant[0]; i++;
		varKey[i] = key[i] ^ variant[1];
	}

	// Generate a new random key for the terminal
	for (i = 0; i < 16; i++)
		block[i] = (unsigned char) rand();

	// Store new key for terminal
	fwrite(block, 1, 16, fp);

	// Encypt it for sending to the terminal
	Des3Encrypt(varKey, block, 16);

	// Add it to the authentication response object
	strcat(authMsg, label);
	for (i = 0; i < 16; i++)
		sprintf(&authMsg[strlen(authMsg)], "%0.2X", block[i]);
}

static int my_isblank(char data)
{
	if (data == ' ' || data == '\t' || data == '\r' || data == '\n')
		return TRUE;
	else
		return FALSE;
}

int my_malloc_max(unsigned char **buff, int maxlen, int currlen)
{
	int ilen = 0;

	if(*buff==NULL) {
		*buff = calloc( maxlen,1 );
	} else {
		ilen = strlen(*buff) + currlen + 1;
		if(ilen > maxlen)
			*buff = realloc(*buff, ilen);
	}

	return(0);
}

static FLAG examineAuth(unsigned char ** response, unsigned int * offset, char * json, char * serialnumber, unsigned char * iv)
{
	int i;
	FILE * fp;
	FLAG send = FALSE;
	FLAG newKEK = FALSE;
	char authMsg[300];
	char temp[100];
	char extra[100];
	unsigned char block[16];
	unsigned char key[16];
	unsigned char zero[8];

	// Prepare the authentication message
	strcpy(authMsg, "{TYPE:AUTH,RESULT:");

	if (json)
	{
		// Get the KVC string's value
		getObjectField(json, 1, extra, NULL, "KVC:");
		memset(zero, 0, sizeof(zero));

		temp[0] = '\0';
		if ((fp = fopen(serialnumber, "r+b")) != NULL)
		{
			fseek(fp, 16, SEEK_SET);
			fread(block, 1, 16, fp);
			Des3Encrypt(block, zero, 8);
			sprintf(temp, "%0.2X%0.2X%0.2X", zero[0], zero[1], zero[2]);
			fclose(fp);
		}

		// Lose the iPAY_CFG file if the terminal has been swapped out
		if (strcmp(extra, "000000") == 0)
		{
			FILE * fp;
			char fname[100];

			sprintf(fname, "%s.iPAY_CFG", serialnumber);

			if ((fp = fopen(fname, "rb")))
			{
				char cmd[300];
				struct tm *newtime;
#ifndef WIN32
				struct tm temp;
#endif
				struct timeb tb;

				logNow("\n%s:: ***SWAP OUT DETECTED*** for %s\n", timeString(cmd, sizeof(cmd)), serialnumber);

				fclose(fp);
				ftime(&tb);
				newtime = localtime_r(&tb.time, &temp);

				sprintf(cmd, "mv %s.iPAY_CFG keep/%s.iPAY_CFG.", serialnumber, serialnumber);
				strftime(&cmd[strlen(cmd)], 50, "%Y%m%d%H", newtime);
				system(cmd);
			}
		}
	}

	// If KVC does not match, update KEK and send along with PPASN to the terminal
	if (json == NULL || fp == NULL || strcmp(temp, extra))
	{
		// Tell the terminal that a new session is required
		strcat(authMsg, "NEW SESSION");
		send = newKEK = TRUE;

		// Store new KEK for terminal for use later on when exchanging session keys (MKr, MKs, and other for other applications....)
		if ((fp = fopen(serialnumber, "w+b")) == NULL) return TRUE;

		// Get a new random KEK, update auth message response and close the KEK storage file for the terminal
		newRandomKey(fp, ",KEK:", authMsg, (char *) (hsm? master_hsm[hsm_no]:master), "\x82\xC0");
		fclose(fp);

		// Prepare an encrypted PPASN but with varinat \x88\x88
		memcpy(block, ppasn, 8);
		for (i = 0; i < 16; i++) key[i] = (hsm? master_hsm[hsm_no][i]:master[i]) ^ 0x88;

		// Encrypt PPASN
		Des3Encrypt(key, block, 8);

		// Add encrypted PPASN to the authentication response object
		strcat(authMsg, ",PPASN:");
		for (i = 0; i < 8; i++)
			sprintf(&authMsg[strlen(authMsg)], "%0.2X", block[i]);				
	}

	if (send == FALSE)
	{
		// Get the PROOF string's value
		getObjectField(json, 1, extra, NULL, "PROOF:");
		for (i = 0; i < 16; i++)
		{
			temp[i] = (extra[i*2] >= 'A'? (extra[i*2] - 'A' + 0x0A):(extra[i*2] - '0')) << 4;
			temp[i] |= (extra[i*2+1] >= 'A'? (extra[i*2+1] - 'A' + 0x0A):(extra[i*2+1] - '0'));
		}

		// Open the key file for reading...Change to HSM later
		if ((fp = fopen(serialnumber, "r+b")) == NULL) return TRUE;
		fseek(fp, 48, SEEK_SET);
		fread(block, 1, 16, fp);
		fclose(fp);

		// Decrypt using MKr
		Des3Decrypt(block, &temp[8]);
		for (i = 0; i < 8; i++)
			temp[8+i] ^= temp[i];
		Des3Decrypt(block, temp);

		// Update the IV used for OFB'ing the rest of the objects from the terminal
		if (iv) memcpy(iv, temp, 8);

		if (memcmp(&temp[8], "\xCA\xFE\xBA\xBE\xDE\xAF\xF0\x01", 8))
			send = TRUE;
	}

	if (send == TRUE)
	{
		unsigned char ppasn_16[16];
		unsigned char ppasn_16_k[16];
		unsigned char ppasn_16_k2[16];

		// Open the key file for reading...Change to HSM later
		if ((fp = fopen(serialnumber, "r+b")) == NULL) return TRUE;

		// If a new KEK was added, then vary using KEK to get KEK1
		if (newKEK)
		{
			// Read KEK from the file
			fread(block, 1, 16, fp);

			// Vary KEK using PPASN|PPASN
			for (i = 0; i < 8; i++)
			{
				block[i] ^= ppasn[i];
				block[i+8] ^= ppasn[i];
			}
		}
		else
		{
			strcat(authMsg, "NEW SESSION");

			// Read the current KEK1 from the file
			fseek(fp, 16, SEEK_SET);
			fread(block, 1, 16, fp);
		}

		// Prepare some temporary PPASN blocks for use during OWF operation
		for (i = 0; i < 8; i++)
		{
			ppasn_16[i] = ppasn_16_k[i] = ppasn_16_k2[i] = ppasn[i];
			ppasn_16[i+8] = ppasn_16_k[i+8] = ppasn_16_k2[i+8] = ppasn[i];
		}

		// OWF, find the MAB
		Des3Encrypt(block, ppasn_16, 16);

		// Use MAB of PPASN|PPASN as an initial vector to encrypting PPASN again
		for (i = 0; i < 8; i++)
			ppasn_16_k[i] ^= ppasn_16[i+8];

		// Encrypt PPASN|PPASN again using the MAB of the first encryption as an IV.
		Des3Encrypt(block, ppasn_16_k, 16);

		// The result is the new KEK1 to use after XORING it with PPASN|PPASN
		for (i = 0; i < 16; i++)
			block[i] = ppasn_16_k[i] ^ ppasn_16_k2[i];

		// Read the new KEK1 int the file. Update within HSM later.
		fseek(fp, 16, SEEK_SET);
		fwrite(block, 1, 16, fp);

		// Send a new MKs session key
		newRandomKey(fp, ",MKs:", authMsg, block, "\x22\xC0");

		// Send a new MKr session key
		newRandomKey(fp, ",MKr:", authMsg, block, "\x44\xC0");

		// Close the authentication object
		strcat(authMsg, "}000000000");

		fclose(fp);

		my_malloc_max( response, 10240, strlen(authMsg)+200+1);

		strcat(*response, authMsg);
		*offset = strlen(*response);

		return TRUE;
	}

	// Return TRUE if a new session is required AND the server should not send further objects
	strcat(authMsg, "YES GRANTED}000000000");

	my_malloc_max( response, 10240, strlen(authMsg)+1);
	strcat(*response, authMsg);
	*offset = strlen(*response);

	return FALSE;
}

static void addQuotes(char * src, char * dest)
{
	unsigned int i,j;
	int tagValue = 0;

	for (i = 0, j = 0; i < strlen(src); i++)
	{
		if (tagValue == 0)
		{
			// Add an preceding quotes if required
			if (src[i] == ':')
			{
				dest[j++] = '"';
				tagValue = 1;
			}

			// Add the character from the identity object
			if (src[i] == '"' || src[i] == '\\')
				dest[j++] = '\\';
			dest[j++] = src[i];

			// Add a quote at the end if required
			if (src[i] == '{' || src[i] == ':')
				dest[j++] = '"';
		}
		else
		{
			// Add an preceding quotes if required
			if (src[i] == '}' || src[i] == ',')
			{
				dest[j++] = '"';
				tagValue = 0;
			}

			// Add the character from the identity object
			if (src[i] == '"' || src[i] == '\\')
				dest[j++] = '\\';
			dest[j++] = src[i];

			// Add a quote at the end if required
			if (src[i] == ',')
				dest[j++] = '"';
		}

	}

	dest[j] = '\0';
}

static void stripQuotes(char * src, char * dest)
{
	int withinQuotes = 0;
	unsigned int i,j;

	for (i = 0, j = 0; i < strlen(src); i++)
	{
		// Detect any quotes
		if (src[i] == '"')
		{
			withinQuotes = !withinQuotes;
			continue;
		}

		if (!withinQuotes && (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' || src[i] == '\r'))
			continue;

		// Add the character
		dest[j++] = src[i];
	}

	dest[j] = '\0';
}

static int new_tagValue(FILE * fp1, FILE * fp2, char * tag, char * value, int replace)
{
	int i, j;
	int size;
	unsigned char data;

	// Get the original tag
	for (size = fread(&data, 1, 1, fp1), i = 0; size == 1 && data != ':' && data != '}'; i++, size = fread(&data, 1, 1, fp1))
	{
		if (data != '{' && data != ',')
			tag[i] = data;
		else i = -1;
	}
	if (i == 0 || size != 1) return 0;
	tag[i++] = ':';
	tag[i] = '\0';

	// Get the original value
	for (size = fread(&data, 1, 1, fp1), i = 0; data != ',' && data != '}'; i++, size = fread(&data, 1, 1, fp1))
		value[i] = data;
	if (size != 1) return 0;
	value[i] = '\0';

	// Search for the tag in the other file...
	for (fseek(fp2, 0, SEEK_SET), size = fread(&data, 1, 1, fp2), i = 0, j = 0; size == 1; i++, size = fread(&data, 1, 1, fp2))
	{
		if (tag[j] == data)
			j++;
		else j = 0;

		if (tag[j] == '\0') break;
	}

	// If found, replace the original value with the new value found in the other file, but only if requested
	if (j && tag[j] == '\0')
	{
		if (!replace) return 2;

		for (size = fread(&data, 1, 1, fp2), i = 0; size == 1 && data != ','  && data != '}'; i++, size = fread(&data, 1, 1, fp2))
			value[i] = data;
		if (size != 1) return 0;
		value[i] = '\0';
	}

	return 1;
}

void send_out_object(char * fileName, char * inFileName, char * outFileName, unsigned char ** response, unsigned int offset)
{
	FILE * fp;
	FILE * in_fp;
	FILE * out_fp;
	int first = 1;

	// Make sure the in file is available
	if ((in_fp = fopen(inFileName, "rb")) != NULL)
	{
		// Make sure we can create an out file
		if ((out_fp = fopen(outFileName, "w+b")) != NULL)
		{
			// Make sure we can read the original file
			if ((fp = fopen(fileName, "rb")) != NULL)
			{
				unsigned char tag[100];
				unsigned char value[1000];
				unsigned char tagValue[1150];

				// Replace original tag values with new ones
				for(;;)
				{
					if (new_tagValue(fp, in_fp, tag, value, 1) == 0)
						break;

					// Write the "out" tag:value pair
					sprintf(tagValue, "%c%s%s", first?'{':',', tag, value);
					fwrite(tagValue, 1, strlen(tagValue), out_fp);
					first = 0;
				}
							
				// Add new tag:value pairs
				for(fseek(in_fp, 0, SEEK_SET);;)
				{
					int result = new_tagValue(in_fp, fp, tag, value, 0);
					if (result == 0)
						break;

					if (result == 1)
					{
						// Write the "out" tag:value pair
						sprintf(tagValue, ",%s%s", tag, value);
						fwrite(tagValue, 1, strlen(tagValue), out_fp);
					}
				}

				// Close the "out" object
				fwrite("}", 1, 1, out_fp);

				fclose(fp);
			}

			fclose(out_fp);

			// Send the new file
			if ((out_fp = fopen(outFileName, "rb")) != NULL)
			{
				char line[300];

				while (fgets(line, 300, out_fp) != NULL)
					addObject(response, line, 1, offset, 0);

				fclose(out_fp);
				remove(outFileName);
			}
		}

		fclose(in_fp);
		remove(inFileName);
	}
}

void get_mid_tid(char * serialnumber, char * __mid, char * __tid)
{
	FILE * fp;
	char tmp[1000];
	int i;

	// Get the current terminal TID and MID
	sprintf(tmp, "%s.iPAY_CFG", serialnumber);
	if ((fp = fopen(tmp, "r")) != NULL)
	{
		if (fgets(tmp, sizeof(tmp), fp))
		{
			char * tid = strstr(tmp, "TID:");
			char * mid = strstr(tmp, "MID:");
			if (__tid)
			{
				if (tid)
					sprintf(__tid, "%8.8s", &tid[4]);
				else strcpy(__tid, "");
			}
			if (__mid)
			{
				if (mid)
				{
					for (i = 4; mid[i] != ',' && mid[i] != '}' && mid[i]; i++)
						__mid[i-4] = mid[i];
					__mid[i-4] = '\0';
				}
				else strcpy(__mid, "");
			}
		}
		fclose(fp);
	}
}

int get_checkbit(char *cardnum)
{
            int sum = 0;
            int alt = 1;
	    int len = strlen(cardnum) ;
	    int i = 0;

            for (i = len - 1; i >= 0; i--)
            {
                int curDigit = cardnum[i]-'0';
                if (alt)
                {
                    curDigit *= 2;
                    if (curDigit > 9)
                        curDigit -= 9;
                }
                sum += curDigit;
                alt = 1-alt;
            }
            if ((sum % 10) == 0 )
            {
                return 0;
            }
            return (10 - (sum % 10));
}  

int processRequest(SOCKET sd, unsigned char * request, unsigned int requestLength, char * serialnumber, int * unauthorised)
{
	char type[100];
	union
	{
		char name[100];
		char serialnumber[100];
	}u;
	union
	{
		char version[100];
		char manufacturer[100];
	}u2;
	union
	{
		char event[100];
		char model[100];
	}u3;
	char value[100];
	char object[100];
	char json[4000];
	char query[5000];
	unsigned int requestOffset = 0;
	unsigned char * response = NULL;
	unsigned int offset = 0;
	unsigned long length = 0;
	int objectType, lastObjectType;
	char model[20];
	unsigned char iv[8];
	unsigned char ivTx[8];
	unsigned char MKr[16];
	unsigned int currIVByte = 0;
	FLAG ofb = FALSE;
	char identity[500];
	int update = 0;
	int sd_dg = -1;
	int iPAY_CFG_RECEIVED = 0;
	int iTAXI_batchno = 0;
	int iSCAN_SAF_RECEIVED = 0;
	int iFUEL_SAF_RECEIVED = 0;
	//MessageMCA mcamsg;
	MYSQL *dbh = (MYSQL *)get_thread_dbh();
	int nextmsg = 0;
	T_WEBREQUEST xmlreq;
	T_WEBRESP xmlresp;
	char tid[30]="";
	char temp[50]="";
	int dldexist = 0;
	char appversion[30]="";

	// Increment the unauthorised flag
	(*unauthorised)++;

	// Examine request an object at a time for trigger to download objects
	while((objectType = getNextObject(request, requestLength, &requestOffset, type, u.name, u2.version, u3.event, value, object, json, ofb?iv:NULL, MKr, &currIVByte, serialnumber)) >= 0)
	{
		int id = 0;

		// Process the device identity
		lastObjectType = objectType;
		if (objectType == 1)
		{
			// Add it in. If a duplicate, it does not matter but just get the ID back later
			strcpy(serialnumber, u.serialnumber);
			strcpy(model, u3.model);

			// Display merchant details TID, MID and ADDRESS for debugging purposes if available
			{
				FILE * fp;
				char tmp[1000];

				sprintf(tmp, "%s.iPAY_CFG", serialnumber);
				if ((fp = fopen(tmp, "r")) != NULL)
				{
					if (fgets(tmp, sizeof(tmp), fp))
					{
						char * tid = strstr(tmp, "TID:");
						char * addr2 = strstr(tmp, "ADDR2:");
						char * addr3 = strstr(tmp, "ADDR3:");

						logNow("*+*+*+*+*+*+*+*+*+*+*+*+* %12.12s,%26.26s,%26.26s +*+*+*+*+*+*+*+*+*+*+*+*\n", tid?tid:"oOoOoOoO", addr2?addr2:"", addr3?addr3:"");
					}
					fclose(fp);
				}
			}

			strcpy(identity, json);

			continue;
		}

		// Do not allow the object download to continue if the device has not identified itself
		//if (serialnumber[0] == '\0') continue;

		// If this is an authorisation, then examine the proof
		if (strcmp(type, "AUTH") == 0 )
		{
			FILE * fp;
			char temp[300];
			int position = 0;

			// If a new session is required, stop here and send the new session details to the terminal
#ifndef NO_ENC
			if (examineAuth(&response, &offset, json, serialnumber, iv) == TRUE)
				break;
#endif

			// Set the TX Initial Vector
			memcpy(ivTx, iv, sizeof(iv));

			// No new sesion required, so enable OFB'ing the rest of the message to get the clear objects out
			ofb = TRUE;

			// If there is a specific message to the terminal, add it now
			sprintf(temp, "%s.dld", serialnumber);
			if ((fp = fopen(temp, "rb")) != NULL)
			{
				char line[300];

				while (fgets(line, 300, fp) != NULL)
					addObject(&response, line, 1, offset, 0);

				fclose(fp);
				remove(temp);
				dldexist = 1;
			}

			// Get the MKr key to use for OFB decryption
			if ((fp = fopen(serialnumber, "r+b")) == NULL)
				break;
			fseek(fp, 48, SEEK_SET);
			fread(MKr, 1, 16, fp);
			fclose(fp);
			continue;
		}

		// From this point onward, OFB must have been enabled and objects received from terminal must have been OFB'd properly
#ifndef NO_ENC
		if (ofb == FALSE)
			continue;
#endif
		(*unauthorised) = 0;

		// If the type == DATA, then just store it in the object list for further processing at a later time
		if (strcmp(type, "DATA") == 0)
		{
			char extra[100];

			// Process transactions
			if (strcmp(u.name, "iPAY_CFG") == 0)
			{
				FILE * fp;
				char fname[100];
				char temp[100];

				sprintf(fname, "%s.iPAY_CFG", serialnumber);

				// Download any initial updates
				if ((fp = fopen(fname, "rb")) == NULL)
				{
					if ((fp = fopen("UPDATE.INI", "rb")) != NULL)
					{
						char line[300];

						while (fgets(line, 300, fp) != NULL)
							addObject(&response, line, 1, offset, 0);

						fclose(fp);
					}
				}
				else fclose(fp);

				fp = fopen(fname, "w+b");
				fwrite(json, 1, strlen(json), fp);
				fclose(fp);

				iPAY_CFG_RECEIVED = 1;
				getObjectField(json, 1, tid, NULL, "TID:");

				// If there is a specific message to the terminal, add it now
				sprintf(temp, "T%s.dld", tid);
				if ((fp = fopen(temp, "rb")) != NULL)
				{
					char line[1024];
					while (fgets(line, 1024, fp) != NULL)
						addObject(&response, line, 1, offset, 0);
					fclose(fp);
					remove(temp);
					dldexist = 1;
				}

			}

			else if (strcmp(u.name, "iRIS_POWERON") == 0)
			{
				FILE * fp;

				if ((fp = fopen("BOOT.INI", "rb")) != NULL)
				{
					char line[300];

					while (fgets(line, 300, fp) != NULL)
						addObject(&response, line, 1, offset, 0);

					fclose(fp);
				}
			}
			else if (strcmp(u.name, "IRIS_CFG") == 0)
			{
				FILE * fp;
				char fname[100];
				char inFileName[100];
				char outFileName[100];

				sprintf(fname, "%s.iRIS_CFG", serialnumber);
				fp = fopen(fname, "w+b");
				fwrite(json, 1, strlen(json), fp);
				fclose(fp);

				// Send the out object if an "in" file object exists
				sprintf(inFileName, "%s.iRIS_CFG.in", serialnumber);
				sprintf(outFileName, "%s.iRIS_CFG.out", serialnumber);
				send_out_object(fname, inFileName, outFileName, &response, offset);
			}

			else if (strcmp(u.name, "iPAY_CFG") == 0)
			{
			}

			continue;
		}

		// If this is an initialisation, then clear the "downloaded" from the database
		if (strcmp(type, "INITIALISE") == 0)
		{
			#ifndef USE_MYSQL
				MYSQL_RES * res;
			#endif

			sprintf(query, "DELETE FROM DOWNLOADED WHERE SERIALNUMBER = '%s'", serialnumber);

			dbStart();
			#ifdef USE_MYSQL
				if (mysql_real_query(dbh, query, strlen(query)) == 0) // success
			#else
				if (PQresultStatus((res = PQexec(dbh, query))) == PGRES_COMMAND_OK)
			#endif
					logNow( "Device **INITIALISED**\n");
				else
					logNow( "Failed to delete downloaded rows.  Error: %s\n", db_error(dbh, res));

			#ifndef USE_MYSQL
				if (res) PQclear(res);
			#endif
			dbEnd();

			continue;
		}

		// If there is a specific request from the terminal for an object, honour it
		if (strcmp(type, "GETOBJECT") == 0)
		{
			sprintf(query, "SELECT id, json FROM object WHERE name='%s'", u.name);
			if (strcmp(u.name, "__MENU") == 0)
				sprintf(&query[strlen(query)], " AND type='CONFIG-%s'", serialnumber);
			if (getObject(query, &response, serialnumber, 1) == 0 && strcmp(u.name, "__MENU") == 0 && strictSerialNumber == 0)
			{
				sprintf(query, "SELECT id, json FROM object WHERE name='%s'  AND type='CONFIG-DEFAULT'", u.name);
				getObject(query, &response, serialnumber, 1);
			}

			continue;
		}

		// PREPAY
		if (strcmp(type, "Logon") == 0)
		{
			char line[128]="";
			/**
			sprintf(query, "");
			dbStart();
				if (mysql_real_query(dbh, query, strlen(query)) == 0) // success
					logNow( "Device **LOGON**\n");
				else
					logNow( "Failed to logon.  Error: %s\n", db_error(dbh, res));
			dbEnd();
			**/

			strcpy(line,"{TYPE:LogonResp,result:OK,code:0}");

			addObject(&response, line, 1, offset, 0);

			continue;

		}else if(strcmp(type,"IssueCard")==0) {
			char line[128]="";
			char iss_name[128]="";
			char iss_mobile[128]="";
			char iss_email[128]="";
			char iss_cardtype[128]="";
			int ok = 1;
			int err = 0;
			long icnt = 0;
			char query[1024]="";
			char cardnum[20]="";
			unsigned long holderid = 0;
			unsigned int chkbit = 0;
			char stmp[1024]="";

			getObjectField(json, 1, iss_name, NULL, "Name:");
			getObjectField(json, 1, iss_mobile, NULL, "Mobile:");
			getObjectField(json, 1, iss_email, NULL, "Email:");
			getObjectField(json, 1, iss_cardtype, NULL, "CardType:");

			if(ok && strlen(iss_name)==0) { ok = 0; err = 101; }
			if(ok && strlen(iss_mobile)==0) { ok = 0;  err = 102;}
			if(ok && strlen(iss_email)==0) { ok = 0;  err = 103;}
			if(ok && strlen(iss_cardtype)==0) { ok = 0;  err = 104;}
			if(ok) {

				/** 1 - CARD HOLDER **/
				sprintf(query, "insert into cardholder values (default,'',now(),'%s','%s','%s')", iss_name,iss_mobile,iss_email);
				dbStart();
				if (mysql_real_query(dbh, query, strlen(query)) == 0) { // success 
					logNow( "new cardholder created **OK**\n");
					holderid = mysql_insert_id(dbh);
				}
				else
					logNow( "Failed to create new customer.  Error: %s\n", db_error(dbh, res));

				
				/** 2 - SEQNO **/
				sprintf(query, "select seqnum from seqno where seqType='%02s'", iss_cardtype);
				icnt = databaseCount(query);
				if(icnt<=0) {
					icnt = 1;
					sprintf(query, "insert into seqno values( '%s',%d, now())", iss_cardtype,icnt);
				} else {
					icnt ++;
					sprintf(query, "update seqno set seqnum = seqnum+1,DateTime=now() where seqtype ='%s'", iss_cardtype);
				}
				if (mysql_real_query(dbh, query, strlen(query)) == 0) // success
					logNow( "new seqno updated **OK**\n");
				
				/** 3 - CARDACCOUNT **/
				sprintf(cardnum,"%4.4s%02s00000%04d","8888",iss_cardtype,icnt);
				chkbit = get_checkbit(cardnum);
				sprintf(stmp,"%s%d", cardnum,chkbit);
				strcpy(cardnum,stmp);

				sprintf(query, "insert into cardaccount values (default,'','%s','%s',%d,now(),%d)", iss_cardtype,cardnum,0,holderid);
				if (mysql_real_query(dbh, query, strlen(query)) == 0) // success
					logNow( "new card created **OK**\n");
				else
					logNow( "Failed to create new card.  Error: %s\n", db_error(dbh, res));
				dbEnd();
			}


			if(ok)
				sprintf(line,"{TYPE:IssueCardResp,result:OK,CARD:%s,code:0}",cardnum);
			else
				sprintf(line,"{TYPE:IssueCardResp,result:NOK,code:%d}",err);
			addObject(&response, line, 1, offset, 0);
			continue;

		}else if(strcmp(type,"Deposit")==0 || strcmp(type,"Purchase")==0 ) {
			char line[128]="";
			int ok = 1;
			int err = 0;
			char query[1024]="";
			char stmp[1024]="";
			char cardnum[20]="";
			char amount[20]="";
			long lAmt = 0;
			long lBal = 0;

			getObjectField(json, 1, cardnum, NULL, "Card:");
			getObjectField(json, 1, amount, NULL, "Amount:");

			if(ok && strlen(cardnum)==0) { ok = 0;  err = 110;}
			if(ok && strlen(amount)==0) { ok = 0;  err = 111;}
			if(ok) {
				dbStart();
				sprintf(query, "select balance from cardaccount where cardno = '%s'", cardnum);
				lBal = databaseCount(query);
				if(lBal<0) {
					ok = 0; err = 112;
				} else {
					lAmt =   atof(amount) * 100 ;
					lBal = lBal + lAmt;

					sprintf(query, "update cardaccount set balance = %ld where cardno = '%s'", lBal, cardnum);
					if (mysql_real_query(dbh, query, strlen(query)) == 0) // success
						logNow( " card updated **OK**\n");
					else
						logNow( "Failed to create card.  Error: %s\n", db_error(dbh, res));
				}
				dbEnd();
			}

			if(ok)
				sprintf(line,"{TYPE:%sResp,result:OK,balance:%.2f,code:0}",type,lBal/100.0);
			else
				sprintf(line,"{TYPE:%sResp,result:NOK,code:%d}",type,err);
			addObject(&response, line, 1, offset, 0);
			continue;

		}

	}

	if (iPAY_CFG_RECEIVED == 1 && dldexist == 0 ) { // local download
				FILE * fp;
				char fname[100];
				int downloadqueue = 0;
				char dq_id[32]="";
				char dq_fname[64]="";

				sprintf(fname,"%s.mdld", serialnumber);
				fp = fopen(fname, "rb");
				if(fp==NULL)  {
					sprintf(fname,"T%s.mdld", tid);
					fp = fopen(fname, "rb");
				}

				if(fp==NULL) {
					char queue[1024]="";

					// Check if we have the service provider URLs available
					sprintf(query, "select id,filename from downloadqueue where tid = '%s' and queueid = ( select min(queueid) from downloadqueue where tid = '%s' and endtime is null);", tid,tid);
					dbStart();
					#ifdef USE_MYSQL
					if (mysql_real_query(dbh, query, strlen(query)) == 0) // success
					{
						MYSQL_RES * res;
						MYSQL_ROW row;

						if (res = mysql_store_result(dbh))
						{
							if (row = mysql_fetch_row(res))
							{
								if (row[0])
								{
									strcpy(dq_id, row[0]);
									strcpy(dq_fname, row[1]);
								}
							}
							mysql_free_result(res);
						}
					}
					#endif
					dbEnd();

					if(strlen(dq_fname)) {
						downloadqueue = 1;
						strcpy( fname, dq_fname);
						fp = fopen(fname, "rb");
					}
				}

				if (fp!= NULL)
				{
					char line[2048];
					int filesend;
					struct timeval timeout;
					char * fp_line = NULL;

					timeout.tv_sec = 15;     // If the connection stays for more than 15 seconds, lose it.
					timeout.tv_usec = 100;
					setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

					while((fp_line = fgets(line,2048,fp))!=NULL) {
						char *filedata = NULL;
						char *data_type = NULL;
						char rfile[30] = "DATA:READFROMFILE";
						char sShutdown[30] = "TYPE:SHUTDOWN";
						char data_filename[30] = "";
						int fileLen = 0;

						// right trim
						while(line[strlen(line)-1] == '\n' || line[strlen(line)-1] == '\r' || line[strlen(line)-1] == ' ') line[strlen(line)-1] = '\0';
						if(strlen(line) == 0) continue;

						data_type = strstr(line,rfile);
						getObjectField(line, 1, data_filename, NULL, "NAME:");

						if( data_type != NULL && strlen(data_filename) ) {
							char fpath[100]="";
							char flocalpath[60]="";
							char findex[10]="";
							getObjectField(line, 1, findex, NULL, "INDEX:");
							getObjectField(line, 1, flocalpath, NULL, "LOCALPATH:");
							if(strlen(flocalpath)) strcat(flocalpath,"/");
							sprintf(fpath,"tmp/%s%s_%s",flocalpath,data_filename,findex);
							if(getFileData(&filedata,fpath,&fileLen)) {
								char filedata_h[10480];
								char msglen[10480];
								*data_type = 0;

								UtilHexToString(filedata, fileLen, filedata_h);
								sprintf(msglen,"%sDATA:%s%s", line,filedata_h,data_type+strlen(rfile));
								addObject(&response, msglen, 0, offset, 200);
								free(filedata);
							}
						} else if (line[0] == '{') {
							addObject(&response, line, 0, offset, 200);
						} else continue; // only send JSON files

						if(response&&strlen(response) < 10) continue;

						sendToTerminal( sd,&response, offset, length, 0);
						if(response!=NULL) { // for next sending
							strcpy( response, "00000000");
							offset = 8;
						}
						filesend = 1;
						//Must in last line {TYPE:SHUTDOWN}
						if(strstr(line,sShutdown)!=NULL) {
							fp_line = NULL;
							break;
						}

						unsigned char acReadBuffer[10];
						int nReadBytes = tcp_recv(sd, 3, acReadBuffer);
						if(nReadBytes <3) break;

						if( memcmp(acReadBuffer,"\x00\x01",2)) {
							int leftlen = acReadBuffer[0] * 256 + acReadBuffer[1] -1 ;

							memset(line,0,sizeof(line));
							nReadBytes = tcp_recv(sd, leftlen,&line[1]);
							line[0] = acReadBuffer[2];
							logNow("\n multiple downloading : received %d [%02x%02x %s]\n",nReadBytes, acReadBuffer[0],acReadBuffer[1],line);
						} 
						else logNow("\n multiple downloading : received ack [%02x%02x %02x]\n",acReadBuffer[0],acReadBuffer[1],acReadBuffer[2]);

					}

					// not completed

					fclose(fp);
					if(fp_line == NULL)
					{
						if(downloadqueue) {
							char DBError[200];
							sprintf(query, "UPDATE downloadqueue set endtime = now() where id = %s", dq_id);
							if (databaseInsert(query, DBError))
								logNow( "DOWNLOADQUEUE ==> ID:%s **RECORDED**\n", dq_id);
							else
							{
								logNow( "Failed to update 'DOWNLOADQUEUE' table.  Error: %s\n", DBError);
							}
						} else {
							char cmd[200];
							sprintf(cmd, "mv %s %s.done", fname, fname);
							system(cmd);
							logNow("\n mdld ok0 [%s]", cmd);
						}
					
					}
					if(response) free(response); 

					logNow("\n mdld ok " );
					return(0);
				}
	}

	if (response)
	{
		// If an empty response, add some dummy bytes to ensure the compressor does not complain
		if (strlen(&response[offset]) == 0)
			addObject(&response, "Empty!!", 1, offset, 0);

#ifndef NO_ENC
		// Store the original length of the objects to allow for better allocation of memory at the device
		sprintf(&response[offset-8], "%07ld", strlen(&response[offset]) + 1);

		// Compress the objects
		logNow("response=[%s] \n", &response[offset]);

		logNow("\nCompressing from %d ", strlen(&response[offset]));
		length = shrink(&response[offset]);
		logNow("to %d\n", length);
	//	length = strlen(&response[offset]);

		// OFB the objects
		if (response[offset-9] == '1')
			OFBObjects(&response[offset-8], length+8, serialnumber, ivTx);

		// Adjust the length
		length += offset;
#else
		length = strlen(&response[offset]);
#endif
	}

	if (length > 0)
	{
		char title[200];
		char temp[50];

		response = realloc(response, length+2);
		memmove(&response[2], response, length);
		response[0] = (unsigned char) (length / 256);
		response[1] = (unsigned char) (length % 256);

		sprintf(title,	"\n\n---------------------------"
						"\n%s"
						"\nSending updates to terminal:"
						"\n---------------------------\n", timeString(temp, sizeof(temp)));
		displayComms(title, response, length+2);

		if (send(sd, response, length+2, MSG_NOSIGNAL) == (int) (length+2))
		{
			// Advance the update zip file position if successful sending during slow upgrades

			logNow(	"\n%s:: ***SENT***", timeString(temp, sizeof(temp)));
		}
		else
			logNow(	"\n%s:: ***SEND FAILED***", timeString(temp, sizeof(temp)));

		free(response);
	}
	else
	{
		char title[200];
		char temp[50];
		unsigned char resp[2];

		resp[0] = 0;
		resp[1] = 0;

		sprintf(title,	"\n\n---------------------------"
						"\n%s"
						"\nSending updates to terminal:"
						"\n---------------------------\n", timeString(temp, sizeof(temp)));
		displayComms(title, resp, 2);

		send(sd, resp, 2, MSG_NOSIGNAL);
		logNow(	"\n%s:: ***SENT***", timeString(temp, sizeof(temp)));
	}

	return update;
}

int sendToTerminal( int sd,char **sendResponse, int offset, int length, int endflag)
{
	unsigned char *response = *sendResponse;

	if (response)
	{
		// If an empty response, add some dummy bytes to ensure the compressor does not complain
		if (strlen(&response[offset]) == 0)
			addObject(&response, "Empty!!", 1, offset, 0);

#ifndef NO_ENC
		// Store the original length of the objects to allow for better allocation of memory at the device
		sprintf(&response[offset-8], "%07ld", strlen(&response[offset]) + 1);

		// Compress the objects
		logNow("\nCompressing from %d ", strlen(&response[offset]));
		length = shrink(&response[offset]);
		logNow("to %d\n", length);

		// Adjust the length
		length += offset;
#else
		length = strlen(&response[offset]);
#endif
	}

	if (length > 0)
	{
		char title[200];
		char temp[50];

		memmove(&response[2], response, length);
		response[0] = (unsigned char) (length / 256);
		response[1] = (unsigned char) (length % 256);

		sprintf(title,	"\n\n---------------------------"
						"\n%s"
						"\nSending updates to terminal:"
						"\n---------------------------\n", timeString(temp, sizeof(temp)));
		displayComms(title, response, length+2);

		if (send(sd, response, length+2, MSG_NOSIGNAL) == (int) (length+2))
		{
			logNow(	"\n%s:: ***SENT***", timeString(temp, sizeof(temp)));
		}
		else
			logNow(	"\n%s:: ***SEND FAILED***", timeString(temp, sizeof(temp)));

		return(1);
	}
	return(0);
}

//// EchoIncomingPackets ///////////////////////////////////////////////
// Bounces any incoming packets back to the client.  We return false
// on errors, or true if the client closed the socket normally.
static int EchoIncomingPackets(SOCKET sd)
{
    // Read data from client
	unsigned char acReadBuffer[BUFFER_SIZE];
	int nReadBytes;
	int lengthBytes = 2;
	int length = 0;
	unsigned char * request = NULL;
	unsigned int requestLength = 0;
	char serialnumber[100];
	int update = 0;
	int unauthorised = 0;
	char temp[50];

	// Initialisation
	serialnumber[0] = '\0';

	logNow("\n%s:: Get thread DBHandler start\n", timeString(temp, sizeof(temp)));
	if( set_thread_dbh() == NULL) {
		logNow("\n%s:: Get DBHandler failed !!\n", timeString(temp, sizeof(temp)));
		return FALSE;
	}
	logNow("\n%s:: Get thread DBHandler ok!\n", timeString(temp, sizeof(temp)));

	while(1)
	{
#ifndef WIN32
		struct timeval timeout;
#endif

		// If we are unauthorised for more than 10 times, drop the session
		if (unauthorised >= 10)
		{
			logNow("\n%s:: Too many unauthorised sessions - possibly tampered!!\n", timeString(temp, sizeof(temp)));
			return FALSE;
		}

		// Get the length first
		do
		{
#ifndef WIN32
			//timeout.tv_sec = waitTime;	// If the connection stays for more than 30 minutes, lose it.
			timeout.tv_sec = 30;	// If the connection stays for more than 30 seconds, lose it.
			timeout.tv_usec = 100;
			setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
#endif
	
			nReadBytes = recv(sd, acReadBuffer, lengthBytes, 0);
			if (nReadBytes == 0)
			{
				logNow("\n%s:: Connection closed by peer (1).\n", timeString(temp, sizeof(temp)));
				return TRUE;
			}
			else if (nReadBytes > 0)
			{
				length = length * 256 + acReadBuffer[0];
				if (--lengthBytes && --nReadBytes)
				{
					length = length * 256 + acReadBuffer[1];
					--lengthBytes;
				}
			}
			else if (nReadBytes == SOCKET_ERROR  || nReadBytes < 0)
			{
				logNow("\n%s:: Connection closed by peer (socket - %d).\n", timeString(temp, sizeof(temp)), errno);
				return FALSE;
			}	
		} while (lengthBytes);

		logNow(	"\n--------------------------------"
				"\n%s"
				"\nReceiving request from terminal:"
				"\n--------------------------------"
				"\nExpected request length = %d bytes from client.\n", timeString(temp, sizeof(temp)), length);

		do
		{
			if (length <= 0)
			{
				logNow(	"\n---------------------------------"
						"\n%s"
						"\nProcessing request from terminal:"
						"\n---------------------------------\n", timeString(temp, sizeof(temp)));
				displayComms("Message Received:\n", request, requestLength);

				// Only update the upgrade counter from the second packet onwards until the last one. The first upgrade counter will be zero or wherever we left at the time of failure (see processRequest())...
				if (serialnumber[0] != '\0' && update == 2)
				{
					FILE * fp;
					char temp[300];
					long position = 0;
					int count = 0;

					// Update the counter only if an upgrade is required
					sprintf(temp, "%s.zip", serialnumber);
					if ((fp = fopen(temp, "rb")) != NULL)
					{
						fclose(fp);
					}
				}
				update = processRequest(sd, request, requestLength, serialnumber, &unauthorised );

				// Reinitialise for the next request
				lengthBytes = 2;
				free(request);
				request = NULL;
				requestLength = 0;

#ifndef WIN32
				// Flush the buffer
				timeout.tv_sec = 0;	// If the connection stays for more than 30 minutes, lose it.
				timeout.tv_usec = 1;
				setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
				while (recv(sd, acReadBuffer, 1, 0) > 0);
#endif	
				break;
			}

#ifndef WIN32
			timeout.tv_sec = 5;
			timeout.tv_usec = 100;
			setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
#endif
	
			nReadBytes = recv(sd, acReadBuffer, length, 0);
			if (nReadBytes > 0)
			{
				length -= nReadBytes;

				acReadBuffer[nReadBytes] = '\0';

				if (request == NULL)
					request = malloc(nReadBytes);
				else
					request = realloc(request, requestLength + nReadBytes);

				memcpy(&request[requestLength], acReadBuffer, nReadBytes);
				requestLength += nReadBytes;
			}
			else if (nReadBytes == SOCKET_ERROR || nReadBytes < 0)
			{
				if (request) free(request);
				logNow("\n%s:: Connection closed by peer (socket2 - %d).\n", timeString(temp, sizeof(temp)), errno);
				return FALSE;
			}
		} while (nReadBytes != 0);
	}

	logNow("Connection closed by peer.\n");
	if (request) free(request);
	return TRUE;
}

//// EchoHandler ///////////////////////////////////////////////////////
// Handles the incoming data by reflecting it back to the sender.

DWORD WINAPI EchoHandler(void * threadData_)
{
	char temp[50];
	int result;
	int nRetval = 0;
	T_THREAD_DATA threadData = *((T_THREAD_DATA *)threadData_);
	SOCKET sd = threadData.sd;

	// Clean up
	free(threadData_);

//	logNow("sd = %d\n", sd);

	if (!(result = EchoIncomingPackets(sd)))
	{
		logNow("\n%s\n", WSAGetLastErrorMessage("Echo incoming packets failed"));
		nRetval = 3;
    	}
	
	close_thread_dbh();

	logNow("Shutting connection down...");
    	if (ShutdownConnection(sd, result))
		logNow("Connection is down.\n");
	else
	{
		logNow("\n%s\n", WSAGetLastErrorMessage("Connection shutdown failed"));
		nRetval = 3;
	}

	counterDecrement();
	logNow("\n%s:: Number of sessions left after closing (%ld.%ld.%ld.%ld:%d) = %d.\n", timeString(temp, sizeof(temp)), ntohl(threadData.sinRemote.sin_addr.s_addr) >> 24, (ntohl(threadData.sinRemote.sin_addr.s_addr) >> 16) & 0xff,
					(ntohl(threadData.sinRemote.sin_addr.s_addr) >> 8) & 0xff, ntohl(threadData.sinRemote.sin_addr.s_addr) & 0xff, ntohs(threadData.sinRemote.sin_port), counter);

	return nRetval;
}


//// AcceptConnections /////////////////////////////////////////////////
// Spins forever waiting for connections.  For each one that comes in, 
// we create a thread to handle it and go back to waiting for
// connections.  If an error occurs, we return.

static void AcceptConnections(SOCKET ListeningSocket)
{
	T_THREAD_DATA threadData;
    int nAddrSize = sizeof(struct sockaddr_in);


	//DO NOT create ping thread any more: 2013-02-11
	sleepTime = 0;
	// Create a thread that accesses the database continuously to stop MySQL timeout
#ifdef USE_MYSQL
	if (sleepTime)
	{
		T_THREAD_DATA myThreadData;
		pthread_t thread;
		pthread_attr_t tattr;
		int status;

		pthread_attr_init(&tattr);
		pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
		pthread_attr_destroy(&tattr);
	}
#endif

    while (running)
	{
		threadData.sd = accept(ListeningSocket, (struct sockaddr *)&threadData.sinRemote, &nAddrSize);
        if (threadData.sd != INVALID_SOCKET)
		{
			char temp[50];
			T_THREAD_DATA * threadDataCopy;
			pthread_t thread;
			pthread_attr_t tattr;
			int status;

			counterIncrement();
			if( counter > 300 ) {
				logNow(	"\nToomany sessions blocking...%d,"
				"\n**************************"
				"\nExiting program now..."
				"\n**************************\n\n", counter);
				sleep(2);
				exit(1);
			}
			logNow("\n%s:: Received TCP packet from %ld.%ld.%ld.%ld:%d - Number of Sessions = %d\n", timeString(temp, sizeof(temp)), ntohl(threadData.sinRemote.sin_addr.s_addr) >> 24, (ntohl(threadData.sinRemote.sin_addr.s_addr) >> 16) & 0xff,
					(ntohl(threadData.sinRemote.sin_addr.s_addr) >> 8) & 0xff, ntohl(threadData.sinRemote.sin_addr.s_addr) & 0xff, ntohs(threadData.sinRemote.sin_port), counter);
			threadDataCopy = malloc(sizeof(T_THREAD_DATA));
			*threadDataCopy = threadData;
#ifdef WIN32
            CreateThread(0, 0, EchoHandler, (void*)threadDataCopy, 0, &nThreadID);
#else
//			logNow("A thread is about to be created.....%d\n", threadData.sd);
			pthread_attr_init(&tattr);
			pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
			status = pthread_create(&thread, &tattr, (void *(*)(void*))EchoHandler, (void *) threadDataCopy);
			if (status)
			{
				logNow("pthread_create() failed with error number = %d", status);
				return;
			}
			pthread_attr_destroy(&tattr);
#endif

        }
        else
		{
            logNow("%s\n", WSAGetLastErrorMessage("accept() failed"));
            return;
        }
    }
}

//// SetUpListener /////////////////////////////////////////////////////
// Sets up a listener on the given interface and port, returning the
// listening socket if successful; if not, returns INVALID_SOCKET.
static SOCKET SetUpListener(const char * pcAddress, int nPort)
{
	u_long nInterfaceAddr = inet_addr(pcAddress);

	if (nInterfaceAddr != INADDR_NONE)
	{
		SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
		if (sd != INVALID_SOCKET)
		{
			struct sockaddr_in sinInterface;
			int reuse;

			reuse = 1;
			setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

			sinInterface.sin_family = AF_INET;
			sinInterface.sin_addr.s_addr = nInterfaceAddr;
			sinInterface.sin_port = nPort;
			if (bind(sd, (struct sockaddr *)&sinInterface, sizeof(struct sockaddr_in)) != SOCKET_ERROR)
			{
				listen(sd, SOMAXCONN);
				return sd;
			}
			else
			{
				logNow("%s\n", WSAGetLastErrorMessage("bind() failed"));
			}
		}
	}

	return INVALID_SOCKET;
}

#ifndef WIN32
void signalHandler(int s)
{
	logNow("%s\n", WSAGetLastErrorMessage(	"\n******************"
											"\ni-RIS stopping...."
											"\n******************\n"));
	waitTime = END_SOCKET_WAITTIME;
	running = 0;
	sleep(5);

	//mysql_close(mysql);

	//pthread_mutex_destroy(&dbMutex);
	pthread_mutex_destroy(&counterMutex);
	pthread_mutex_destroy(&hsmMutex);

	logEnd();

	exit(0);
}
#endif

//// DoWinsock /////////////////////////////////////////////////////////
// The module's driver function -- we just call other functions and
// interpret their results.

static int DoWinsock(const char * schema, const char * pcAddress, int nPort)
{
	char temp[50];
	SOCKET listeningSocket;

	logNow("\n\n%s:: Establishing the %s listener using port %d ...\n", timeString(temp, sizeof(temp)), schema, nPort);
	listeningSocket = SetUpListener(pcAddress, htons((u_short) nPort));
	if (listeningSocket == INVALID_SOCKET)
	{
		logNow("\n%s\n", WSAGetLastErrorMessage("establish listener"));
		return 3;
	}

#ifndef WIN32
	signal(SIGINT, signalHandler);
	signal(SIGUSR1, signalHandler);
#endif

	logNow("Waiting for connections...");
//	while (running)
//	{
		AcceptConnections(listeningSocket);
//		logNow("Acceptor restarting...\n");
//	}

	return 0;   // warning eater
}

//// main //////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	char * database = "i-ris";
	char * databaseIPAddress = "localhost";
	int databasePortNumber = 5432;
	char * serverIPAddress = "localhost";
	int serverPortNumber = 44555;
	char * user = "root";
	char * password = "password.01";
	int db_reconnect;
	time_t mytime;

	int arg;
	int nCode;
	int retval;
	char * unix_socket = NULL;

	// Initialisation
	pthread_mutex_init(&hsmMutex, NULL);
	pthread_mutex_init(&counterMutex, NULL);

	mytime = time(NULL);

	while((arg = getopt(argc, argv, "a:A:F:W:w:Q:q:z:Z:d:D:C:i:o:s:p:S:P:e:E:l:L:u:U:k:M:I:Y:x:X:BrRcmnNtHTh?")) != -1)
	{
		switch(arg)
		{
			case 'A':
				break;
			case 'B':
				break;
			case 'D':
				break;
			case 'F':
				break;
			case 'C':
				break;
			case 'c':
				break;
			case 'z':
				minZipPacketSize = atoi(optarg);
				break;
			case 'Z':
				maxZipPacketSize = atoi(optarg);
				break;
			case 'd':
				database = optarg;
				break;
			case 'i':
				databaseIPAddress = optarg;
				break;
			case 'o':
				databasePortNumber = atoi(optarg);
				break;
			case 'M':
				break;
			case 'I':
				break;
			case 's':
				serverIPAddress = optarg;
				break;
			case 'p':
				serverPortNumber = atoi(optarg);
				break;
			case 'S':
				deviceGatewayIPAddress = optarg;
				break;
			case 'P':
				deviceGatewayPortNumber = atoi(optarg);
				break;
			case 'e':
				break;
			case 'E':
				break;
			case 'l':
				sleepTime = atoi(optarg);
				break;
			case 'L':
				logFile = optarg;
				break;
			case 'r':
				strictSerialNumber = 0;
				break;
			case 'm':
				dispMessage = 1;
				break;
			case 'n':
				noTrace = 1;
				break;
			case 't':
				test = 1;
				break;
			case 'H':
				hsm = 1;
				break;
			case 'a':
				hsm_no = atoi(optarg);
				break;
			case 'T':
				ignoreHSM = 1;
				break;
			case 'u':
				user = optarg;
				break;
			case 'U':
				password = optarg;
				break;
			case 'k':
				unix_socket = optarg;
				break;
			case 'q':
				break;
			case 'Q':
				break;
			case 'w':
				break;
			case 'W':
				break;
			case 'R':
				break;
			case 'N':
				break;
			case 'x':
				break;
			case 'X':
				break;
			case 'h':
			case '?':
			default:
				printf(	"Usage: %s [-h=help] [-?=help] [-d database=i-ris] [-L logFileName] [-r=relaxSerialNumber]\n"
						"            [-n=no trace] [-i databaseIPAddress=localhost] [-o databasePortNumber=5432]\n"
						"            [-s serverIPAddress=localhost] [-p serverPortNumber=44555]\n"
						"            [-u databaseUserID=root] [-U databaseuserPassword=password.01]\n"
						"            [-q revIPAddress=localhost] [-Q revPortNumber=32001]\n"
						"            [-w rewardIPAddress=localhost] [-W rewardPortNumber=32002]\n"
						"            [-S deviceGatewayIPAddress=localhost] [-P deviceGatewayPortNumber]\n", argv[0]);
				exit(-1);
		}
	}

#if defined(USE_MYSQL)
	{
	MYSQL *dbh = NULL;
	// mysql initialisation
	if ((dbh = mysql_init(NULL)) == NULL)
	{
		printf("MySql Initialisation error. Exiting...\n");
		exit(-1);
	}

	// mysql database connection options
	db_reconnect = 1;
	mysql_options(dbh, MYSQL_OPT_RECONNECT, &db_reconnect);

	// mysql database connection
	if (!mysql_real_connect(dbh, databaseIPAddress, user, password, database, 0, unix_socket, 0))
	{
		fprintf(stderr, "%s\n", db_error(dbh, res));
		exit(-2);
	}
	set_db_connect_param( databaseIPAddress, user, password, database, unix_socket);


	mysql_close(dbh);
	}
#endif

	// Start the log & counter
	logStart();

    // Call the main example routine.
	retval = DoWinsock(database, serverIPAddress, serverPortNumber);

	// Shut Winsock back down and take off.
#ifdef WIN32
	WSACleanup();
#endif

	logNow(	"\n**************************"
		"\nExiting program now..."
		"\n**************************\n\n");

	pthread_mutex_destroy(&counterMutex);
	pthread_mutex_destroy(&hsmMutex);

	logEnd();

	return retval;
}
