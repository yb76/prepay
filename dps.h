/*
 * Summary: dps includes
 * Description: 
 * Copy: 
 * Author: 
 */

#ifndef __DPS_H__
#define __DPS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************* const definition ****************/
#define _USE_MYSQL 1

#ifndef DPS_MAXBUFLEN
#define DPS_MAXBUFLEN 10240
#endif

#ifndef DPS_TXN_TYPE
#define DPS_TXN_TYPE 1
#define TXNTYPE_PURCHASE 	"Purchase"
#define TXNTYPE_REFUND 		"Refund"
#define TXNTYPE_AUTH 		"Auth"
#define TXNTYPE_COMPLETE 	"Complete"
#define TXNTYPE_VOID 		"Void"
#define TXNTYPE_STATUS 		"Status"
#define TXNTYPE_SETTLE 		"Settle"
#define TXNTYPE_REVERSAL 	"Reversal"
#endif

#define PARAM_TXNDEF_ID 1001
#define PARAM_DPS_URL 1002
#define PARAM_DPS_DEBUG 1003
#define PARAM_SETTLEINTERVAL 1004
#define PARAM_COMP_AMT_LIMIT 1005

/********************* message definition ****************/
typedef struct MsgTerm2Dps MsgTerm2Dps ;
struct MsgTerm2Dps
{
//input
char mid[20];
char tid[9];
char invoiceno[10];
char devicetime[26];

char RisTxnRef[9];
char Amount[14];
char CardHolderName[65];
char CardNumber[21];
char Cvc2[5];
char DateExpiry[5];
char InputCurrency[5];
char TxnType[10];
char Track2[38];

//optional
char DateStart[5];
char IssueNumber[10];
char MerchantReference[65];
char EnableAddBillCard[2];
char DpsBillingId[17];
char BillingId[33];
char EnableAvsData[2];
char AvsAction[2];
char AvsPostCode[21];
char AvsStreetAddress[61];

//internal
char PostUsername[33];
char PostPassword[33];
char systemtime[20];
char DpsTxnRef[17];
};

typedef MsgTerm2Dps *pMsgTerm2Dps;

typedef struct MsgDps2Term MsgDps2Term ;
struct MsgDps2Term
{
//output
char msgcode[4]; //local error code , H00 : OK
char msgtext[41]; //local error message if msgcode is not zero
char sent[2]; //1: sent

char Authorized[2];
char ReCo[3];
char RxDate[15];
char RxDateLocal[15];
char LocalTimeZone[10];
char MerchantReference[65];
char CardName[20];
char Retry[2];
char StatusRequired[2];
char AuthCode[23];
char AmountBalance[13];
char Amount[13];
char CurrencyId[5];
char InputCurrencyId[5];
char InputCurrencyName[4];
char CurrencyRate[10];
char CurrencyName[10];
char CardHolderName[64];
char DateSettlement[9];
char TxnType[10];
char CardNumber[21];
char TxnMac[10];
char DateExpiry[5];
char ProductId[10];
char AcquirerDate[9];
char AcquirerTime[7];
char AcquirerId[10];
char Acquirer[20];
char AcquirerReCo[10];
char AcquirerResponseText[256];
char TestMode[2];
char CardId[2];
char CardHolderResponseText[256];
char CardHolderHelpText[256];
char CardHolderResponseDescription[256];
char MerchantResponseText[128];
char MerchantHelpText[256];
char MerchantResponseDescription[256];
char UrlFail[2];
char UrlSuccess[2];
char EnablePostResponse[2];
char PxPayName[10];
char PxPayLogoSrc[10];
char PxPayUserId[10];
char PxPayXsl[10];
char PxPayBgColor[10];
char PxPayOptions[10];
char AcquirerPort[30];
char AcquirerTxnRef[30];
char GroupAccount[10];
char DpsTxnRef[17];
char AllowRetry[2];
char DpsBillingId[2];
char BillingId[33];
char TransactionId[17];
char PxHostId[10];

char g_ReCo[3];
char g_ResponseText[65];
char g_HelpText[256];
char g_Success[2];
char g_DpsTxnRef[17];
char g_TxnRef[17];

char RisTxnRef[9];

};
typedef MsgDps2Term *pMsgDps2Term;

/********************* function declaration ****************/
#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif
