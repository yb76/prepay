
/*
 * Summary: revprepay message definination
 * Description: 
 * Copy: 
 * Author: 
 */

#ifndef __REVPREPAY_MESSAGE_H__
#define __REVPREPAY_MESSAGE_H__


typedef struct MessageRev MessageRev ;
struct MessageRev
{
//request
char storeid[5];
char opid[9];
char transtype[2];
char devicetime[26];
long value;
char cardtr1[80];
char oppass[20];
char customref[40];

//response
char systemtime[26];
char msgcode[5];
char msgtext[50];

//internal
char transrc[2];
char bin[7];
char prn[14];
char upc[12];
char storecode[40];
char opcode[40];
char opfname[20];
char oplname[20];
char storename[20];

char title[10];
char fname[50];
char mname[10];
char lname[50];
char birdate[20];
char address1[40];
char address2[40];
char suburb[20];
char city[20];
char postcode[10];
char state[10];
char homenumber[20];
char mobile[20];
char email[40];
char placeofbirth[20];
char driverlicence[20];
char pkgtr1[80];
char barcode[40];

char xsdtime[26];
char svalue[7];

};
typedef MessageRev *pMessageRev;

#endif /* ! __REVPREPAY_MESSAGE_H__ */
