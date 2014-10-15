
#include <stdio.h>
#include <string.h>
#include "medicare_message.h"

static int addJsonField( char *json, char *name, char *value)
{

	if(strlen(value))
		{
		sprintf( json, "%s,%s:%s", json, name, value );
		}
	return(0);
}

int	packjsonMCAService(char *json,pMessageMCA pmsg,int ind)
{
	char *query = json;
	char tmps[30];

	sprintf(query, "{TYPE:DATA");
	sprintf(tmps, "%s%d", MEDICARE_SERVICE,ind );
	addJsonField( query, "NAME", tmps );

	addJsonField( query, "GROUP", "iMEDICARE" );
	addJsonField( query, "STATUS", pmsg->assessmentstatus[ind] );
	addJsonField( query, "SERV_DATE", pmsg->dateofservice[ind] );
	addJsonField( query, "MBS_ITEM", pmsg->mbsnumber[ind] );
	sprintf(tmps, "%d", pmsg->chargeamount[ind]);
	addJsonField( query, "CHARGE_AMOUNT", tmps );
	sprintf(tmps, "%d", pmsg->schedulefee[ind]);
	addJsonField( query, "SCHED_FEE", tmps );
	sprintf(tmps, "%d", pmsg->benefitamount[ind]);
	addJsonField( query, "SERV_BENEFIT_AMT", tmps );
	sprintf(tmps, "%d", pmsg->patientcontribution[ind]);
	addJsonField( query, "CONTRIB_AMOUNT", tmps );

	addJsonField( query, "EXPL_CODE", pmsg->explainationcode[ind] );
	addJsonField( query, "EXPL_TEXT", pmsg->explainationtext[ind] );

	strcat(query,"}" );
	pmsg->totalbenefit += pmsg->benefitamount[ind];
	pmsg->totalschfee += pmsg->schedulefee[ind];
	pmsg->totalcharge += pmsg->chargeamount[ind];
	pmsg->totalcontribution += pmsg->patientcontribution[ind];

	return(0);
}

int	packjsonMCAClaim(char *json,pMessageMCA pmsg)
{

	char amt[30];
	char *query = json;

	sprintf(query, "{TYPE:DATA");
	addJsonField( query, "NAME", "iMEDICARE_RESP_SET" );
	addJsonField( query, "GROUP", "iMEDICARE" );

	addJsonField( query, "TRANSACTIONID", pmsg->transactionid);
	addJsonField( query, "ACC_PAID_IND", pmsg->accountpaid);

	addJsonField( query, "CONS_STATUS", pmsg->concessionstatus);
	addJsonField( query, "MEDIC_ELIG", pmsg->medicarestatus);

	addJsonField( query, "PAT_TRACK2", pmsg->patientmcardno);
	addJsonField( query, "PAT_INDV_NUM", pmsg->patientirn);
	addJsonField( query, "PAT_NAME", pmsg->patientfname);
	addJsonField( query, "PAT_SURNAME", pmsg->patientlname);

	addJsonField( query, "CLAIM_TRACK2", pmsg->claimmcardno);
	addJsonField( query, "CLAIM_INDV_NUM", pmsg->claimirn);
	addJsonField( query, "CLAIM_NAME", pmsg->claimfname);
	addJsonField( query, "CLAIM_SURNAME", pmsg->claimlname);

	addJsonField( query, "ACCEPT_TYPE", pmsg->acceptancetype);
	addJsonField( query, "SERV_PROV_NUM", pmsg->serviceprovider);
	addJsonField( query, "SERV_PROV_NAME", pmsg->providername);
	addJsonField( query, "CLAIM_ERROR_CODE", pmsg->claimassessmentstatus);
	addJsonField( query, "ASSES_STATUS", pmsg->assessmentcode);
	addJsonField( query, "ASSES_TEXT", pmsg->assessmenttext);

	addJsonField( query, "REF_PROV_NAME", pmsg->refprovidername);
	addJsonField( query, "REQ_PROV_NAME", pmsg->reqprovidername);
	sprintf(amt,"%d",pmsg->totalbenefit);
	addJsonField( query, "TOTALBENEFIT", amt);
	sprintf(amt,"%d",pmsg->totalcharge);
	addJsonField( query, "TOTALCHARGE", amt);
	sprintf(amt,"%d",pmsg->totalschfee);
	addJsonField( query, "TOTALSCHFEE", amt);
	sprintf(amt,"%d",pmsg->totalcontribution);
	addJsonField( query, "TOTALCONTRIBUTION", amt);
	
	strcat(query,"}" );

	return(0);
}

int	unpackjsonMCA(char *json,pMessageMCA pmsg)
{
	char msgname[30];		
	char amt[30];
	char tmps[30];

	getObjectField(json, 1, msgname, NULL, "NAME:");
	if (strncmp(msgname, MEDICARE_SERVICE,strlen(MEDICARE_SERVICE)) == 0) 
		{
		int ind = atoi(&msgname[0] + strlen(MEDICARE_SERVICE));

		pmsg->mbsitems = ind + 1;
		
		//service level
		getObjectField(json, 1, pmsg->dateofservice[ind], NULL, "SERV_DATE:");
		getObjectField(json, 1, pmsg->mbsnumber[ind], NULL, "MBS_ITEM:");
		getObjectField(json, 1, amt, NULL, "CHARGE_AMOUNT:");
		pmsg->chargeamount[ind] = atoi(amt);
		getObjectField(json, 1, amt, NULL, "CONTRIB_AMOUNT:");
		pmsg->patientcontribution[ind] = atoi(amt);

		getObjectField(json, 1, pmsg->serviceid[ind], NULL, "SERVICEID:");
		getObjectField(json, 1, pmsg->itemoverridecode[ind], NULL, "ITEM_ORIDE_CODE:");
		getObjectField(json, 1, pmsg->lspn[ind], NULL, "LSPN:");
		getObjectField(json, 1, pmsg->equipmentid[ind], NULL, "EQUIP:");
		getObjectField(json, 1, pmsg->selfdeem[ind], NULL, "SELF_DEEMED_CODE:");
		getObjectField(json, 1, pmsg->scpid[ind], NULL, "SCP_ID:");
		getObjectField(json, 1, pmsg->restrictiveoverridecode[ind], NULL, "RESTRICT_ORIDE_CODE:");
		
		}
	else		
		{// claim level
		getObjectField(json, 1, pmsg->claimtype, NULL, "CLAIM_TYPE:");
			pmsg->claimtype[0] = pmsg->claimtype[0] == 'P' ? '1' : '2' ;
		getObjectField(json, 1, pmsg->cardentry, NULL, "IS_CARD_SWIPED:");
			pmsg->cardentry[0] = pmsg->cardentry[0] == 'Y' ? 'S' : 'M' ;

		getObjectField(json, 1, pmsg->termid, NULL, "TERMID:");
		getObjectField(json, 1, pmsg->transactionid, NULL, "TRANSACTIONID:");
		getObjectField(json, 1, pmsg->accountpaid, NULL, "ACC_PAID_IND:");
		getObjectField(json, 1, pmsg->claimmcardno, NULL, "CLAIM_TRACK2:");
		getObjectField(json, 1, pmsg->claimirn, NULL, "CLAIM_INDV_NUM:");
		getObjectField(json, 1, pmsg->patientmcardno, NULL, "PAT_TRACK2:");
		getObjectField(json, 1, pmsg->patientirn, NULL, "PAT_INDV_NUM:");
		getObjectField(json, 1, pmsg->serviceprovider, NULL, "SERV_PROV_NUM:");
		getObjectField(json, 1, pmsg->cevrequest, NULL, "CEV:");
		getObjectField(json, 1, pmsg->accountref, NULL, "ACCT_REF_NUM:");
		getObjectField(json, 1, pmsg->payeeprovider, NULL, "PAYEE_NUM:");
		getObjectField(json, 1, pmsg->voucherid, NULL, "VOUCHERID:");
		getObjectField(json, 1, pmsg->servicetypecode, NULL, "SERV_TYPE_CODE:");
		getObjectField(json, 1, pmsg->refprovidernum, NULL, "REF_PROV_NUM:");
		getObjectField(json, 1, pmsg->refissuedate, NULL, "REF_ISS_DATE:");
		getObjectField(json, 1, pmsg->refoverridecode, NULL, "REF_ORIDE_CODE:");
		getObjectField(json, 1, pmsg->refperiodtype, NULL, "REF_PERIOD_CODE:");
		getObjectField(json, 1, pmsg->reqprovidernum, NULL, "REQ_PROV_NUM:");
		getObjectField(json, 1, pmsg->reqissuedate, NULL, "REQ_ISS_DATE:");
		getObjectField(json, 1, pmsg->reqtypecode, NULL, "REQ_PERIOD_CODE:");
		getObjectField(json, 1, pmsg->reqoverridecode, NULL, "REQ_ORIDE_CODE:");

		getObjectField(json, 1, pmsg->benefitassignmentauthorised, NULL, "BENEFITASSIGN:");
		getObjectField(json, 1, pmsg->acceptindicator, NULL, "ACCEPTIND:");

		getObjectField(json, 1, pmsg->bankcardno, NULL, "BANKCARD:");
		getObjectField(json, 1, pmsg->receiptno, NULL, "BANKRECEIPT:");
		}
	return(0);
}
