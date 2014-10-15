
/*
 * Summary: amex message definination
 * Description: 
 * Copy: 
 * Author: 
 */

#ifndef __AMEX_MESSAGE_H__
#define __AMEX_MESSAGE_H__


typedef struct MessageAmex MessageAmex ;
struct MessageAmex
{
//request
char mid[20];
char tid[10];
char transtype[20];
char invoiceno[10];
char devicetime[26];
char amount[15];
char cardnumber[20];
char cardtr1[80];
char cardtr2[40];
char expirydate[5];
char country[10];	//default:036
char currency[10];	//default:036

char pos_CardDataInpCpblCd[2];
char pos_CMAuthnCpblCd[2];
char pos_CardCptrCpblCd[2];
char pos_OprEnvirCd[2];
char pos_CMPresentCd[2];
char pos_CardPresentCd[2];
char pos_CardDataInpModeCd[2];
char pos_CMAuthnMthdCd[2];
char pos_CMAuthnEnttyCd[2];
char pos_CardDataOpCpblCd[2];
char pos_TrmnlOpCpblCd[2];
char pos_PINCptrCpblCd[2];

char cardcid[8];
char orig_traceno[10];
char orig_invoiceno[20];

//response
char sent[2];
char msgcode[5];
char msgtext[41];

char TransTs[26];
char MerSysTraceAudNbr[10];
char TransAprvCd[10];
char TransActCd[10];
char TransId[49];

//internal
char merno[11];
char systemtime[26];

};
typedef MessageAmex *pMessageAmex;

#endif /* ! __AMEX_MESSAGE_H__ */
