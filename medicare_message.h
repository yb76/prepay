
/*
 * Summary: medicare message definination
 * Description: 
 * Copy: 
 * Author: 
 */

#ifndef __MEDICARE_MESSAGE_H__
#define __MEDICARE_MESSAGE_H__

#define MEDICARE_SERVICE	"MCASERVICE"
struct _MessageMCAHost
{
char content[1024*4];
};
typedef struct _MessageMCAHost MessageMCAHost ;
typedef MessageMCAHost *pMessageMCAHost;

#define MAX_MBS_ITEMS 14

struct _MessageMCA
{
// TODO
char claimtype[2]; // 1:patient , 2:bulk bill
char cardentry[2]; // S:swipe ,   M:manual
char termid[30];
//CLAIM LEVEL
char transactionid[30];
char accountpaid[2];
char claimmcardno[30];
char claimirn[3];
char patientmcardno[30];
char patientirn[3];

char serviceprovider[10];
char benefitauth[2];
char cevrequest[2];
//optional
char accountref[10];
char payeeprovider[10];

//VOUCHER LEVEL 
char voucherid[20];
char servicetypecode[2];
char refprovidernum[10];
char refissuedate[10];
char refperiodtype[2];
char refoverridecode[2];
char refprovidername[41];

char reqprovidernum[10];
char reqissuedate[10];
char reqtypecode[2];
char reqoverridecode[2];
char reqprovidername[41];

//SERVICE LEVEL
int mbsitems;
char dateofservice[MAX_MBS_ITEMS][10];
char mbsnumber[MAX_MBS_ITEMS][7];
int chargeamount[MAX_MBS_ITEMS]; 
int patientcontribution[MAX_MBS_ITEMS];
int schedulefee[MAX_MBS_ITEMS];
int benefitamount[MAX_MBS_ITEMS];
char assessmentstatus[MAX_MBS_ITEMS][5];
char explainationcode[MAX_MBS_ITEMS][5];
char explainationtext[MAX_MBS_ITEMS][51];
//optional
char serviceid[MAX_MBS_ITEMS][10];
char itemoverridecode[MAX_MBS_ITEMS][3];
char lspn[MAX_MBS_ITEMS][7];
char equipmentid[MAX_MBS_ITEMS][6];
char selfdeem[MAX_MBS_ITEMS][3];
char scpid[MAX_MBS_ITEMS][5];
//char benefitassigned[MAX_MBS_ITEMS][8]; ->benefitamount
char restrictiveoverridecode[MAX_MBS_ITEMS][3];

//response
char claimassessmentstatus[5];
char claimfname[41];
char claimlname[41];
char patientfname[41];
char patientlname[41];
char providername[41];
char assessmentcode[5];
char assessmenttext[51 ];
char acceptancetype[5];
char concessionstatus[2];
char medicarestatus[2];

int totalbenefit;
int totalschfee;
int totalcharge;
int totalcontribution;
// for Accept/Decline/Cancel Message
char benefitassignmentauthorised[2];
char acceptindicator[2];

//patient claim - bank details
char bankcardno[5];
char receiptno[21];
}; 

typedef struct _MessageMCA MessageMCA;
typedef MessageMCA *pMessageMCA;

#endif /* ! __MC_MESSAGE_H__ */
