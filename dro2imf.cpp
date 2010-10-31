//
// DRO2IMF - Convert DOSBox OPL captures to id Software Music Format
//
// IMF songs are used in many Apogee games, from Commander Keen to Duke
// Nukem II.  There is also a converter available to convert IMF files into
// MIDI files.
//
// Written by malvineous@shikadi.net in June 2007, in a mishmash of rather
// messy C/C++.  Feel free to do what you want with this code, but if you
// make use of it please give credit where credit is due :-)
//
// http://www.shikadi.net/utils/
//

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <exception>

typedef unsigned char BYTE;
typedef unsigned short UINT16;
typedef unsigned long UINT32;

bool convert(FILE *hDRO, FILE *hIMF);
void addIMFTags(FILE *hIMF, char *cTitle, char *cComposer, char *cRemarks);
UINT16 readUINT16LE(FILE *hData);
UINT32 readUINT32LE(FILE *hData);
void writeUINT16LE(FILE *hData, UINT16 iVal);
void writeUINT32LE(FILE *hData, UINT32 iVal);

using namespace std;

int main(int iArgC, char *cArgV[])
{
	if ((iArgC != 3) && (iArgC != 6)) {
		cerr << "DOSBox OPL capture to id Software Music Format converter.\n"
		"Written by malvineous@shikadi.net in June 2007.\n"
		"\n"
		"Usage: dro2imf <drofile> <imffile> [<title> <composer> <remarks>]\n"
		"\n"
		" drofile is the DOSBox capture to convert\n"
		" imffile is the output IMF file that will be created\n"
		"\n"
		"IMF tags are optional, but if given all three tags must be specified\n"
		"(use \"\" to leave a field blank)\n"
		<< endl;
		return 1;
	}

	// Open the files
	FILE *hDRO = fopen(cArgV[1], "rb");
	if (hDRO == NULL) {
		cerr << "Unable to open " << cArgV[1] << endl;
		return 2;
	}

	FILE *hIMF = fopen(cArgV[2], "wb");
	if (hIMF == NULL) {
		cerr << "Unable to create " << cArgV[2] << endl;
		fclose(hDRO);
		return 3;
	}

	bool bSuccess = convert(hDRO, hIMF);
	if (iArgC == 6) {
		addIMFTags(hIMF, cArgV[3], cArgV[4], cArgV[5]);
	}
	
	fclose(hIMF);
	fclose(hDRO);
	
	if (!bSuccess) return 4;
	cout << "Wrote " << cArgV[2] << endl;
	return 0;
}

bool convert(FILE *hDRO, FILE *hIMF)
{
	BYTE cSig[8];
	fread(cSig, 8, 1, hDRO);
	if (strncmp((char *)cSig, "DBRAWOPL", 8) != 0) {
		cerr << "Input file is not in DOSBox .dro format!" << endl;
		return false;
	}
	
	UINT32 iVersion = readUINT32LE(hDRO);
	UINT32 iLengthMS = readUINT32LE(hDRO);
	UINT32 iLengthBytes = readUINT32LE(hDRO);
	UINT32 iHardwareType = readUINT32LE(hDRO);
//	BYTE iHardwareType;
//	fread(&iHardwareType, 1, 1, hDRO); // AdPlug ignores this, so we will too
	
	if (iVersion != 0x10000) {
		cerr << "Only version 1.0 files are supported - this is version " << (iVersion >> 16) << '.' << (iVersion & 0xFF) << endl;
		return false;
	}
	
	cout << "Data is " << iLengthBytes << " bytes long." << endl;
	
	// Make space for the file length, which we'll come back to write later
	writeUINT16LE(hIMF, 0);

	// Padding (optional?)
//	writeUINT32LE(hIMF, 0);

	// Need to start with NULL Adlib data, so when we write the first delay it won't stuff up the offsets
	writeUINT16LE(hIMF, 0);

	UINT16 iLastDelay = 0;
	for (UINT32 iLen = 0; iLen < iLengthBytes; iLen++) {
		BYTE iReg;
		fread(&iReg, 1, 1, hDRO);
		switch (iReg) {
			case 0x00: { // delay, BYTE param
				BYTE iDelayByte;
				fread(&iDelayByte, 1, 1, hDRO);
				iLastDelay += 1 + iDelayByte;
				iLen++;
				break;
			}
			case 0x01: // delay, UINT16LE param
				iLastDelay += 1 + readUINT16LE(hDRO);
				iLen+=2;
				break;
			case 0x02: // switch to chip #0
			case 0x03: // switch to chip #1
				cout << "Warning: This song uses multiple OPL chips, which the IMF format doesn't support!" << endl;
				// TODO: Make sure this doesn't print more than once per song conversion
				break;
			case 0x04: // escape, treat the next two bytes as data
				fread(&iReg, 1, 1, hDRO);
				iLen++;
				// Keep going into the normal data section
			default: {
				// Adlib data, transfer straight across
				
				// Write the previous note's delay first
				writeUINT16LE(hIMF, (UINT16)(iLastDelay * 560 / 1000));  // DRO runs at 1000Hz (delays are in milliseconds), IMF runs at 560Hz

				BYTE iData;
				fread(&iData, 1, 1, hDRO);
				iLen++;
				fwrite(&iReg, 1, 1, hIMF);
				fwrite(&iData, 1, 1, hIMF);
				iLastDelay = 0;
			}
		} // switch (iReg)
	} // for (loop through DRO data)
//	cout << endl;

	// Finish off with a zero-delay
	writeUINT16LE(hIMF, 0);
	
	UINT32 iEnd = ftell(hIMF) - 2; // we don't want to count the two bytes we're about to write at the file's start
	fseek(hIMF, 0, SEEK_SET);
	writeUINT16LE(hIMF, iEnd);

	return true;
}

void addIMFTags(FILE *hIMF, char *cTitle, char *cComposer, char *cRemarks)
{
	int iTitleLen = strlen(cTitle);
	if (iTitleLen >= 255) {
		cerr << "ERROR: Title field must be less than 255 characters, ignoring IMF tags." << endl;
		return;
	}
	int iComposerLen = strlen(cComposer);
	if (iComposerLen >= 255) {
		cerr << "ERROR: Composer field must be less than 255 characters, ignoring IMF tags." << endl;
		return;
	}
	int iRemarksLen = strlen(cRemarks);
	if (iRemarksLen >= 255) {
		cerr << "ERROR: Remarks field must be less than 255 characters, ignoring IMF tags." << endl;
		return;
	}
	
	// We want to append this at the end of the file
	fseek(hIMF, 0, SEEK_END);

	// Signature byte to indicate presence of this style of tags
	fwrite("\x1A", 1, 1, hIMF);

	fwrite(cTitle, iTitleLen + 1, 1, hIMF);
	cout << "Set title to '" << cTitle << "'" << endl;
	fwrite(cComposer, iComposerLen + 1, 1, hIMF);
	cout << "Set composer to '" << cComposer << "'" << endl;
	fwrite(cRemarks, iRemarksLen + 1, 1, hIMF);
	cout << "Set remarks to '" << cRemarks << "'" << endl;

	// Name of program that wrote the tags (not normally user-visible)
	fwrite("DRO2IMF\x00\x00", 9, 1, hIMF); // eight chars fixed + terminating NULL
	return;
}

// Read in two bytes and treat them as a little-endian unsigned 16-bit integer, should be host-endian-neutral
UINT16 readUINT16LE(FILE *hData)
{
	BYTE b[2];
	fread(&b, 1, 2, hData);
	return b[0] | (b[1] << 8);
}

// Read in four bytes and treat them as a little-endian unsigned 32-bit integer, should be host-endian-neutral
UINT32 readUINT32LE(FILE *hData)
{
	BYTE b[4];
	fread(&b, 1, 4, hData);
	return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

// Write out an int as a little-endian unsigned 16-bit integer, should be host-endian-neutral
void writeUINT16LE(FILE *hData, UINT16 iVal)
{
	BYTE b[2];
	b[0] = iVal & 0xFF;
	b[1] = iVal >> 8;
	fwrite(&b, 1, 2, hData);
	return;
}

// Write out a long as a little-endian unsigned 32-bit integer, should be host-endian-neutral
void writeUINT32LE(FILE *hData, UINT32 iVal)
{
	BYTE b[4];
	b[0] = iVal & 0xFF;
	b[1] = iVal >> 8;
	b[2] = iVal >> 16;
	b[3] = iVal >> 24;
	fwrite(&b, 1, 4, hData);
	return;
}
