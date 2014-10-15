
/*
 * Summary: reward message definination
 * Description: 
 * Copy: 
 * Author: 
 */

#ifndef __REWARD_MESSAGE_H__
#define __REWARD_MESSAGE_H__


typedef struct MessageReward MessageReward ;
struct MessageReward
{
//request
char transtype[10];
long long int gid;	//internal
char mid[20];
char tid[20];
char cardno[50];
char devicetime[26];
long amount;
long rewardamount;
long quantity; 		//not used
char vouchercode[20];

//response
char systemtime[26];
char transid[21];
int  msgcode;
char msgtext[250];
char msgtext1[22];
char msgtext2[22];
char expirydate[10];
};
typedef MessageReward *pMessageReward;

#endif /* ! __REWARD_MESSAGE_H__ */
