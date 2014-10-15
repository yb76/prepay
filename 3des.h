#ifndef __3DES_H_
#define __3DES_H_


/*
**-----------------------------------------------------------------------------
** This file contains proprietary information of Fox Technology
** Limited. Copying or reproduction without prior written
** approval is prohibited.
**
** Copyright (c) 2002
** Dansoft Australia Limited, NSW AUSTRALIA
**-----------------------------------------------------------------------------
*/

/*
**-----------------------------------------------------------------------------
** PROJECT:			Generic - testing
**
** FILE NAME:		3des.h
**
** DATE CREATED:	04 Sep 2003
**
** AUTHOR:			Tareq Hafez
**
** DESCRIPTION:		Triple DES Encryption functions
**-----------------------------------------------------------------------------
*/


/*
**-----------------------------------------------------------------------------
**
** Include Files
**
**-----------------------------------------------------------------------------
*/


/*
** Standard include files
*/


/*
** Local include files
*/


/*
**-----------------------------------------------------------------------------
** Module variable definitions and initialisations.
**-----------------------------------------------------------------------------
*/


/*
**-----------------------------------------------------------------------------
** Function Declarations
**-----------------------------------------------------------------------------
*/

void DesEncrypt(BPTR key, BPTR data);
void Des3Encrypt(BPTR key, BPTR data, WORD length);

BPTR DesMac(BPTR key, BPTR data, WORD len, BYTE* out);
BPTR Des3Mac(BPTR key, BPTR data, WORD len, BYTE* out);

void DesDecrypt(BPTR key, BPTR data);
void Des3Decrypt(BPTR key, BPTR data);

#endif /* __3DES_H_ */

