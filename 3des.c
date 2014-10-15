#include <stdio.h>

#include <string.h>



#include "macro.h"



#define TDES



// Data used for DES Encryption/Decryption

const static INT8 pc1[64] = {

	 7, 15, 23, 55, 51, 43, 35,  -1,

	 6, 14, 22, 54, 50, 42, 34,  -1,

	 5, 13, 21, 53, 49, 41, 33,  -1,

	 4, 12, 20, 52, 48, 40, 32,  -1,

	 3, 11, 19, 27, 47, 39, 31,  -1,

	 2, 10, 18, 26, 46, 38, 30,  -1,

	 1,  9, 17, 25, 45, 37, 29,  -1,

	 0,  8, 16, 24, 44, 36, 28,  -1};



const BYTE rotation[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};

const static INT8 pc2[56] = {

	 4, 23,  6, 15,  5,  9, 19, 17,

	-1, 11,  2, 14, 22,  0,  8, 18,

	 1, -1,	13, 21, 10, -1, 12,  3,

	-1, 16, 20,	 7, 46, 30, 26, 47,

	34, 40, -1, 45, 27, -1, 38, 31,

	24, 43, -1, 36, 33,	42, 28, 35,

	37, 44, 32, 25, 41, -1, 29, 39};



const static BYTE ip[64] = {

	39,  7, 47, 15, 55, 23, 63, 31,

	38,  6, 46, 14, 54, 22, 62, 30,

	37,  5, 45, 13, 53, 21, 61, 29,

	36,  4, 44, 12, 52, 20, 60, 28,

	35,  3, 43, 11, 51, 19, 59, 27,

	34,  2, 42, 10, 50, 18, 58, 26,

	33,  1, 41,  9, 49, 17, 57, 25,

	32,  0, 40,  8, 48, 16, 56, 24};



const static BYTE eBit[32] = {

	47,  2,  3,  4,  5,

		 8,  9, 10, 11,

		14, 15, 16, 17,

		20, 21, 22, 23,

		26, 27, 28, 29,

		32, 33, 34, 35,

		38, 39, 40, 41,

		44, 45, 46};



const static BYTE sBox1[4][16] = { 

	{14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7}, 

	{0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8}, 

	{4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0}, 

	{15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13}}; 



const static BYTE sBox2[4][16] = { 

	{15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10}, 

	{3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5}, 

	{0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15}, 

	{13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9}};



const static BYTE sBox3[4][16] = { 

	{10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8}, 

	{13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1}, 

	{13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7}, 

	{1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12}}; 



const static BYTE sBox4[4][16] = { 

	{7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15}, 

	{13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9}, 

	{10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4}, 

	{3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14}};



const static BYTE sBox5[4][16] = { 

	{2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9},

	{14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6},

    {4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14},

	{11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3}};



const static BYTE sBox6[4][16] = { 

	{12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11},

	{10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8},

	{9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6},

	{4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13}};



const static BYTE sBox7[4][16] = { 

	{4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1},

	{13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6},

	{1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2},

	{6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12}};



const static BYTE sBox8[4][16] = { 

	{13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7},

	{1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2},

	{7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8},

	{2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}};



const static BYTE p[32] = {

		 8, 16, 22, 30,

		12, 27,  1, 17,

		23, 15, 29,  5,

		25, 19,  9,  0,

		 7, 13, 24,  2,

		 3, 28, 10, 18,

		31, 11, 21,  6,

		 4, 26, 14, 20};



const static BYTE invIP[64] = {

	 57, 49, 41, 33, 25, 17,  9,  1,

	 59, 51, 43, 35, 27, 19, 11,  3,

	 61, 53, 45, 37, 29, 21, 13,  5,

	 63, 55, 47, 39, 31, 23, 15,  7,

	 56, 48, 40, 32, 24, 16,  8,  0,

	 58, 50, 42, 34, 26, 18, 10,  2,

	 60, 52, 44, 36, 28, 20, 12,  4,

	 62, 54, 46, 38, 30, 22, 14,  6};



/*

**-----------------------------------------------------------------------------

** FUNCTION   :

**

** DESCRIPTION:

**

** PARAMETERS:

**

** RETURNS:

**

**-----------------------------------------------------------------------------

*/

static void Des(BPTR key, BPTR data, FLAG decrypt)

{

	BYTE i, j, r;

	BYTE bitmap;

	BYTE key56[7];
	BYTE key48[16][6];
	BYTE temp48[6];
	BYTE textBlock[8];


	// Create the 56-bit permutation key

	memset(key56, 0, sizeof(key56));

	for (i = 0; i < 8; i++)

	{

		for (bitmap = 0x80, j = 0; j < 7; j++, bitmap >>= 1)

		{

			if (bitmap & key[i])

			{

				INT8 newBit = pc1[i*8+j];

				key56[newBit >> 3] |= (0x80 >> (newBit & 0x07));

			}

		}

	}



	// Create 16 48-bit keys

	for (memset(key48, 0, sizeof(key48)), r = 0; r < 16; r++)

	{

		// Rotate the 56-bit key accordingly

		BYTE rot = rotation[r];

		BYTE invRot = 8 - rotation[r];

		BYTE save1 = (key56[4] >> invRot);

		BYTE save2 = (key56[0] >> invRot) << 4;



		key56[0] = (key56[0] << rot) + (key56[1] >> invRot);

		key56[1] = (key56[1] << rot) + (key56[2] >> invRot);

		key56[2] = (key56[2] << rot) + (key56[3] >> invRot);

		key56[4] = (key56[4] << rot) + (key56[5] >> invRot);

		key56[5] = (key56[5] << rot) + (key56[6] >> invRot);

		key56[6] = (key56[6] << rot) + ((key56[3] & 0x0F) >> (invRot - 4));

		key56[3] = ((key56[3] << rot) & 0x0F) + save1 +

					((key56[3] & 0xF0) << rot) + save2;



		// Apply permuted choice 2 to get one of the 48-bit sub-keys

		for (i = 0; i < 7; i++)

		{

			for (bitmap = 0x80, j = 0; j < 8; j++, bitmap >>= 1)

			{

				if (bitmap & key56[i])

				{

					INT8 newBit = pc2[i*8+j];

					if (newBit != -1)

						key48[r][newBit >> 3] |= (0x80 >> (newBit & 0x07));

				}

			}

		}

	}



	// Prepare the text block using an initial permutation

	memset(textBlock, 0, sizeof(textBlock));

	for (i = 0; i < sizeof(textBlock); i++)

	{

		for (bitmap = 0x80, j = 0; j < 8; j++, bitmap >>= 1)

		{

			if (bitmap & data[i])

			{

				BYTE newBit = ip[i*8+j];

				textBlock[newBit >> 3] |= (0x80 >> (newBit & 0x07));

			}

		}

	}



	for (r = 0; r < 16; r++)

	{

		BYTE sValue[4], fValue[4];



		// Calculate the E-BIT permutation to convert the right 32 bits into 48-bit entity = temp48

		memset(temp48, 0, sizeof(temp48));

		for (i = 4; i < 8; i++)

		{

			for (bitmap = 0x80, j = 0; j < 8; j++, bitmap >>= 1)

			{

				if (bitmap & textBlock[i])

				{

					BYTE bit = (i-4)*8+j;

					BYTE newBit = eBit[bit];

					temp48[newBit >> 3] |= (0x80 >> (newBit & 0x07));

					if ((bit & 0x03) == 0 || (bit & 0x03) == 3)

					{

						newBit = (newBit + 2) % 48;

						temp48[newBit >> 3] |= (0x80 >> (newBit & 0x07));

					}

				}

			}

		}



		// XOR Rn with Kn

		for (i = 0; i < 6; i++)

			temp48[i] ^= decrypt? key48[15-r][i]:key48[r][i];



		// Calculate the S-BOX value returning Rn to 32-bit entity = sValue

		{

			// Get sBox8 value

			i = ((temp48[5] & 0x20)? 2:0) + (temp48[5] & 0x01);

			j = (temp48[5] >> 1) & 0x0f;

			sValue[3] = sBox8[i][j];



			// Get sBox7 value

			i = ((temp48[4] & 0x08)? 2:0) + ((temp48[5] & 0x40)? 1:0);

			j = ((temp48[4] & 0x07) << 1) + (temp48[5] >> 7);

			sValue[3] |= (sBox7[i][j] << 4);



			// Get sBox6 value

			i = ((temp48[3] & 0x02)? 2:0) + ((temp48[4] & 0x10)? 1:0);

			j = ((temp48[3] & 0x01) << 3) + (temp48[4] >> 5);

			sValue[2] = sBox6[i][j];



			// Get sBox5 value

			i = ((temp48[3] & 0x80)? 2:0) + ((temp48[3] & 0x04)? 1:0);

			j = (temp48[3] >> 3) & 0x0f;

			sValue[2] |= (sBox5[i][j] << 4);



			// Get sBox4 value

			i = ((temp48[2] & 0x20)? 2:0) + (temp48[2] & 0x01);

			j = (temp48[2] >> 1) & 0x0f;

			sValue[1] = sBox4[i][j];



			// Get sBox3 value

			i = ((temp48[1] & 0x08)? 2:0) + ((temp48[2] & 0x40)? 1:0);

			j = ((temp48[1] & 0x07) << 1) + (temp48[2] >> 7);

			sValue[1] |= (sBox3[i][j] << 4);



			// Get sBox2 value

			i = ((temp48[0] & 0x02)? 2:0) + ((temp48[1] & 0x10)? 1:0);

			j = ((temp48[0] & 0x01) << 3) + (temp48[1] >> 5);

			sValue[0] = sBox2[i][j];



			// Get sBox1 value

			i = ((temp48[0] & 0x80)? 2:0) + ((temp48[0] & 0x04)? 1:0);

			j = (temp48[0] >> 3) & 0x0f;

			sValue[0] |= (sBox1[i][j] << 4);

		}



		// Permutate the S-BOX value into the fValue = f(Rn-1, Kn) = p(sBox(Kn ^ E(Rn-1)))

		for (memset(fValue, 0, sizeof(fValue)), i = 0; i < 4; i++)

		{

			for (bitmap = 0x80, j = 0; j < 8; j++, bitmap >>= 1)

			{

				if (bitmap & sValue[i])

				{

					BYTE newBit = p[i*8+j];

					fValue[newBit >> 3] |= (0x80 >> (newBit & 0x07));

				}

			}

		}



		// Now calculate the new Left and Right pair of the text block

		for (i = 0; i < 4; i++)

		{

			if (r != 15)

			{

				// XOR the fValue with the old Left value

				fValue[i] ^= textBlock[i];



				// Set the new left side (Ln) to the old right side (Rn-1)

				textBlock[i] = textBlock[i+4];



				// Set the new right side to the fValue

				textBlock[i+4] = fValue[i];

			}

			else

				textBlock[i] ^= fValue[i];

		}

	}



	// Reverse the initial permutation on the block. Data is now encrypted or decrypted.

	memset(data, 0, 8);

	for (i = 0; i < 8; i++)

	{

		for (bitmap = 0x80, j = 0; j < 8; j++, bitmap >>= 1)

		{

			if (bitmap & textBlock[i])

			{

				BYTE newBit = invIP[i*8+j];

				data[newBit >> 3] |= (0x80 >> (newBit & 0x07));

			}

		}

	}

}



/*

**-----------------------------------------------------------------------------

** FUNCTION   :

**

** DESCRIPTION:

**

** PARAMETERS:

**

** RETURNS:

**

**-----------------------------------------------------------------------------

*/

void DesEncrypt(BPTR key, BPTR data)

{

	Des(key, data, FALSE);

}



/*

**-----------------------------------------------------------------------------

** FUNCTION   :	Des3Encrypt

**

** DESCRIPTION:	Triple DES CBC encrypts the data

**

** PARAMETERS:	key		<=	Key to use during encryption

**				data	<=	Data to CBC encrypt

**				length	<=	Length of the data. Must be multiples of 8.

**

** RETURNS:		None

**

**-----------------------------------------------------------------------------

*/

void Des3Encrypt(BPTR key, BPTR data, WORD length)

{

	WORD i,j;

	BYTE iv[8];	// Initialisation vector. Initialise to ZEROs.



	for (memset(iv, 0, sizeof(iv)), i = 0; i < length; i+= 8)

	{

		// XOR encrypted block with next block

		for (j = 0; j < 8; j++)

			data[i+j] ^= iv[j];



		// Encrypt the 8-byte block

		Des(key, &data[i], FALSE);

#ifdef TDES

		Des(&key[8], &data[i], TRUE);

		Des(key, &data[i], FALSE);

#endif

		memcpy(iv, &data[i], sizeof(iv));

	}

}



/*

**-----------------------------------------------------------------------------

** FUNCTION   :	DesMac

**

** DESCRIPTION:	Calculate the MAC

**

** PARAMETERS:

**

** RETURNS:

**

**-----------------------------------------------------------------------------

*/

BPTR DesMac(BPTR key, BPTR data, WORD len, BYTE* out)

{

	WORD i,j;



	for (memset(out, 0, sizeof(out)), i = 0; i < len;)

	{

		for (j = 0; j < sizeof(out) && i < len; i++, j++)

			out[j] ^= data[i];

		DesEncrypt(key, out);

	}



	return out;

}



/*

**-----------------------------------------------------------------------------

** FUNCTION   :	Des3Mac

**

** DESCRIPTION:	Calculate the MAC

**

** PARAMETERS:

**

** RETURNS:

**

**-----------------------------------------------------------------------------

*/

BPTR Des3Mac(BPTR key, BPTR data, WORD len, BYTE* out)

{

	WORD i,j;



	for (memset(out, 0, sizeof(out)), i = 0; i < len;)

	{

		for (j = 0; j < sizeof(out) && i < len; i++, j++)

			out[j] ^= data[i];

		Des3Encrypt(key, out, sizeof(out));

	}



	return out;

}



/*
**-----------------------------------------------------------------------------
** FUNCTION   :	DesDecrypt

**

** DESCRIPTION:

**

** PARAMETERS:

**

** RETURNS:

**

**-----------------------------------------------------------------------------

*/

void DesDecrypt(BPTR key, BPTR data)

{

	Des(key, data, TRUE);

}



/*

**-----------------------------------------------------------------------------

** FUNCTION   :

**

** DESCRIPTION:

**

** PARAMETERS:

**

** RETURNS:

**

**-----------------------------------------------------------------------------

*/

void Des3Decrypt(BPTR key, BPTR data)

{

	Des(key, data, TRUE);

#ifdef TDES

	Des(&key[8], data, FALSE);

	Des(key, data, TRUE);

#endif

}

