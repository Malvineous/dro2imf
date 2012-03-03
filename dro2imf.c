/*
DRO2IMF v1.1 - Convert DOSBox OPL captures to id Software Music Format

IMF songs are used in many Apogee games, from Commander Keen to Duke
Nukem II.  There is also a converter available to convert IMF files into
MIDI files.

Written by malvineous@shikadi.net in June 2007, modified by NY00123
in February 2012.  Feel free to do what you want with this code, but if you
make use of it please give credit where credit is due :-)

http://www.shikadi.net/utils/
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DRO2IMF_MAX_CODEMAP_LEN 128

int convert(FILE *hDRO, FILE *hIMF, int iIMFRate, int iIMFTypeNum);
void addIMFTags(FILE *hIMF, char *cTitle, char *cComposer, char *cRemarks);
uint16_t readUINT16LE(FILE *hData);
uint32_t readUINT32LE(FILE *hData);
void writeUINT16LE(FILE *hData, uint16_t iVal);
void writeUINT32LE(FILE *hData, uint32_t iVal);

int printHelpAndReturn()
{
	fprintf(stderr,
	        "DOSBox OPL capture to id Software Music Format converter.\n"
	        "Written by malvineous@shikadi.net in June 2007.\n"
	        "\n"
	        "Usage: dro2imf -in <drofile> -out <imffile> [optional arguments]\n"
	        "\n"
	        " drofile is the DOSBox capture to convert.\n"
	        " imffile is the output IMF file that will be created.\n"
	        "\n"
	        "Optional arguments:\n"
	        "-rate <rate>: IMF rate in Hz. By default, 560Hz is set (for Keen).\n"
	        "-type <type>: Output a Type-0 IMF file (Keen) or a Type-1 file (Wolf3D),\n"
	        "passing 0 or 1 after \"-type\". By default, Type-0 is chosen.\n"
	        "-tags <title> <composer> <remarks>: IMF tags. Use with Type-1 files.\n"
	        "\n"
	        "* IMF rates to use: 560Hz for Commander Keen,\n"
	        "700Hz for Wolfenstein 3D, 280Hz for Duke Nukem II.\n"
	        "* IMF tags are optional, but if given all three tags must be specified\n"
	        "(use \"\" to leave a field blank).\n"
	        "\n"
	        "Examples:\n"
	        "dro2imf -in mycapt.dro -out convcapt.imf\n"
	        "dro2imf -in rmus.dro -out omus.wlf -rate 700 -type 1 -tags \"My song\" Squirb \"\"\n"
	        "\n");
	return 1;
}

int main(int iArgC, char *cArgV[])
{
	int iIMFRate = 560, iIMFTypeNum = 0, iArgCurr = 1;
	char *cInFileName, *cOutFileName, *cTitle, *cComposer, *cRemarks;
	cInFileName = cOutFileName = cTitle = cComposer = cRemarks = NULL;
	while (iArgCurr < iArgC) {
		if (!strcmp(cArgV[iArgCurr], "-in")) {
			if (++iArgCurr >= iArgC)
				return printHelpAndReturn();
			cInFileName = cArgV[iArgCurr++];
		}
		else if (!strcmp(cArgV[iArgCurr], "-out")) {
			if (++iArgCurr >= iArgC)
				return printHelpAndReturn();
			cOutFileName = cArgV[iArgCurr++];
		}
		else if (!strcmp(cArgV[iArgCurr], "-rate")) {
			if (++iArgCurr >= iArgC)
				return printHelpAndReturn();
			iIMFRate = atoi(cArgV[iArgCurr++]);
		}
		else if (!strcmp(cArgV[iArgCurr], "-tags")) {
			if ((++iArgCurr) + 2 >= iArgC)
				return printHelpAndReturn();
			cTitle = cArgV[iArgCurr++];
			cComposer = cArgV[iArgCurr++];
			cRemarks = cArgV[iArgCurr++];
		}
		else if (!strcmp(cArgV[iArgCurr], "-type")) {
			if (++iArgCurr >= iArgC)
				return printHelpAndReturn();
			iIMFTypeNum = atoi(cArgV[iArgCurr++]);
		}
		else
			return printHelpAndReturn();
	}
	if (!cInFileName || !cOutFileName)
		return printHelpAndReturn();
	if (iIMFRate <= 0) {
		fprintf(stderr, "IMF rate must be a proper, not too large, positive value. Use 560Hz\n");
		fprintf(stderr, "for Commander Keen, 700Hz for Wolfenstein 3D, 280Hz for Duke Nukem II.\n");
		return 1;
	}
	if ((iIMFTypeNum != 0) && (iIMFTypeNum != 1)) {
		fprintf(stderr, "IMF type can only be 0 (e.g. for Commander Keen) or 1 (Wolfenstein 3D).\n");
		return 1;
	}

	/* Open the files */
	FILE *hDRO = fopen(cInFileName, "rb");
	if (hDRO == NULL) {
		fprintf(stderr, "Unable to open %s\n", cInFileName);
		return 2;
	}

	FILE *hIMF = fopen(cOutFileName, "wb");
	if (hIMF == NULL) {
		fprintf(stderr, "Unable to create %s\n", cOutFileName);
		fclose(hDRO);
		return 3;
	}

	int iConvResult = convert(hDRO, hIMF, iIMFRate, iIMFTypeNum);
	if (cTitle) {
		if (iIMFTypeNum == 1)
			addIMFTags(hIMF, cTitle, cComposer, cRemarks);
		else
			printf("Warning: The selected IMF file format is Type-0. Thus, IMF tags are ignored.\n");
	}
	
	fclose(hIMF);
	fclose(hDRO);
	
	if (iConvResult) return 4;
	printf("IMF Rate: %dHz\n", iIMFRate);
	printf("Type of IMF file: Type-%d\n", iIMFTypeNum);
	printf("Wrote %s\n", cOutFileName);
	return 0;
}

int convert(FILE *hDRO, FILE *hIMF, int iIMFRate, int iIMFTypeNum)
{
	uint8_t cSig[8];
	fread(cSig, 8, 1, hDRO);
	if (strncmp((char *)cSig, "DBRAWOPL", 8) != 0) {
		fprintf(stderr, "Input file is not in DOSBox .dro format!\n");
		return 1;
	}
	
	uint32_t iVersion = readUINT32LE(hDRO);
	if ((iVersion != 0x10000) && (iVersion != 0x2)) {
		fprintf(stderr, "Only version 0.1 (1.0) and 2.0 files are supported - this is version %d.%d\n",
		        (iVersion & 0xFFFF), (iVersion >> 16));
		return 1;
	}

	if (iVersion == 0x2) { /* Version 2.0 DRO file detected */
		uint32_t iLengthPairs = readUINT32LE(hDRO);
		/* uint32_t iLengthMS = readUINT32LE(hDRO);*/
		readUINT32LE(hDRO); /* Length in milli-seconds */
		uint8_t iTempByte, iShortDelayCode, iLongDelayCode, iCodemapLength, iCodeMap[DRO2IMF_MAX_CODEMAP_LEN];
		/* Hardware Type; AdPlug ignores this, so we will too. */
		fread(&iTempByte, 1, 1, hDRO);
		/*
		Format - Data arrangement. Currently a value of 0 is known
		and supported (commands and data are interleaved).
		Other values haven't yet been spotted.
		*/
		fread(&iTempByte, 1, 1, hDRO);
		if (iTempByte) {
			fprintf(stderr, "Unknown data arrangement detected; File format unsupported.\n");
			return 1;
		}
		/* Compression; 0 is currently supported (no compression). */
		fread(&iTempByte, 1, 1, hDRO);
		if (iTempByte) {
			fprintf(stderr, "Compression has been detected. Thus, file is unsupported.\n");
			return 1;
		}
		fread(&iShortDelayCode, 1, 1, hDRO);
		fread(&iLongDelayCode, 1, 1, hDRO);
		fread(&iCodemapLength, 1, 1, hDRO);
		if (iCodemapLength > DRO2IMF_MAX_CODEMAP_LEN) {
			fprintf(stderr, "Too long codemap size detected; File format unrecognized.\n");
			return 1;
		}
		fread(iCodeMap, iCodemapLength, 1, hDRO);

		printf("Data is %d bytes long.\n", iLengthPairs << 1);
	
		if (iIMFTypeNum == 1) /* Type-1 */
			/* Make space for the file length,
			which we'll come back to write later. */
			writeUINT16LE(hIMF, 0);
		else /* Type-0 */
			writeUINT32LE(hIMF, 0); /* Write 4 padding zeros */

		/* Need to start with NULL Adlib data, so when we write the first delay it won't stuff up the offsets */
		writeUINT16LE(hIMF, 0);

		uint16_t iLastDelay = 0;
		uint8_t iRegIndex, iDelayByte, iData;
		int iWarnOfDualOPL = 1;
		for (uint32_t iLen = 0; iLen < iLengthPairs; iLen++) {
			fread(&iRegIndex, 1, 1, hDRO);
			if (iRegIndex == iShortDelayCode) { /* Short delay code */
				fread(&iDelayByte, 1, 1, hDRO);
				iLastDelay += 1 + iDelayByte;
			}
			else if (iRegIndex == iLongDelayCode) { /* Long delay code */
				fread(&iDelayByte, 1, 1, hDRO);
				iLastDelay += ((uint16_t)(1 + iDelayByte) << 8);
			}
			else { /* Adlib data, transfer straight across */
				if (iRegIndex & 0x80) {
					uint8_t iOPLReg = iCodeMap[iRegIndex & 0x80];
					/* Skip data value! */
					fread(&iData, 1, 1, hDRO);
					if (iWarnOfDualOPL) {
						/*
						Only warn if a note is played on the second register set, to
						avoid the warning message when all we have is the initial
						state of all registers DOSBox includes at the start of a
						capture.
						*/
						if ((iOPLReg >= 0xB0) && (iOPLReg <= 0xB8) && (iData & 0x20)) {
							printf("Warning: This song uses multiple OPL chips, which the IMF format doesn't support!\n");
							iWarnOfDualOPL = 0;
						}
					}
					continue;
				}

				/*
				Write the previous note's delay first. Note that
				DRO runs at 1000Hz (delays are in milliseconds),
				while the IMF rate can be 560Hz, 700Hz, etc.
				*/
				writeUINT16LE(hIMF, (uint16_t)(iLastDelay * iIMFRate / 1000));

				fread(&iData, 1, 1, hDRO);
				fwrite(iCodeMap + iRegIndex, 1, 1, hIMF);
				fwrite(&iData, 1, 1, hIMF);
				iLastDelay = 0;
			}
		} /* for (loop through DRO data) */
	}
	else { /* Version 0.1 (1.0) DRO file detected */
		uint32_t iLengthMS = readUINT32LE(hDRO);
		uint32_t iLengthBytes = readUINT32LE(hDRO);

		uint8_t iHardwareType;
		/* AdPlug ignores this, so we will too. */
		fread(&iHardwareType, 1, 1, hDRO);
		/*
		We should better check the value's length in bytes, though.
		It can be 1 or 4 bytes long for the same version of 0.1.
		If there is a series of 4 bytes, the last 3 of them all being 0,
		we treat it as a 4 bytes long value. Otherwise, we treat it
		as a 1 byte long value and thus need to skip a few bytes back.
		*/
		if (!((fread(&iHardwareType, 1, 1, hDRO) == 1) && !iHardwareType
		      && (fread(&iHardwareType, 1, 1, hDRO) == 1) && !iHardwareType
		      && (fread(&iHardwareType, 1, 1, hDRO) == 1) && !iHardwareType))
			fseek(hDRO, sizeof(cSig)+sizeof(iVersion)+sizeof(iLengthMS)+sizeof(iLengthBytes)+1, SEEK_SET);

		printf("Data is %d bytes long.\n", iLengthBytes);
	
		if (iIMFTypeNum == 1) /* Type-1 */
			/* Make space for the file length,
			which we'll come back to write later. */
			writeUINT16LE(hIMF, 0);
		else /* Type-0 */
			writeUINT32LE(hIMF, 0); /* Write 4 padding zeros */

		/* Need to start with NULL Adlib data, so when we write the first delay it won't stuff up the offsets */
		writeUINT16LE(hIMF, 0);

		uint16_t iLastDelay = 0;
		uint8_t iReg, iDelayByte, iData;
		int iWarnOfDualOPL = 1;
		for (uint32_t iLen = 0; iLen < iLengthBytes; iLen++) {
			fread(&iReg, 1, 1, hDRO);
			switch (iReg) {
				case 0x00: { /* delay, BYTE param */
					fread(&iDelayByte, 1, 1, hDRO);
					iLastDelay += 1 + iDelayByte;
					iLen++;
					break;
				}
				case 0x01: /* delay, UINT16LE param */
					iLastDelay += 1 + readUINT16LE(hDRO);
					iLen+=2;
					break;
				case 0x02: /* switch to chip #0 */
				case 0x03: /* switch to chip #1 */
					if (iWarnOfDualOPL) {
						printf("Warning: This song uses multiple OPL chips, which the IMF format doesn't support!\n");
						iWarnOfDualOPL = 0;
					}
					break;
				case 0x04: /* escape, treat the next two bytes as data */
					fread(&iReg, 1, 1, hDRO);
					iLen++;
					/* Keep going into the normal data section */
				default: {
					/* Adlib data, transfer straight across */

					/*
					Write the previous note's delay first. Note that
					DRO runs at 1000Hz (delays are in milliseconds),
					while the IMF rate can be 560Hz, 700Hz, etc.
					*/
					writeUINT16LE(hIMF, (uint16_t)(iLastDelay * iIMFRate / 1000));

					fread(&iData, 1, 1, hDRO);
					iLen++;
					fwrite(&iReg, 1, 1, hIMF);
					fwrite(&iData, 1, 1, hIMF);
					iLastDelay = 0;
				}
			} /* switch (iReg) */
		} /* for (loop through DRO data) */
	}

	/* Finish off with a zero-delay (applies to DRO versions 0.1 and 2.0) */
	writeUINT16LE(hIMF, 0);

	if (iIMFTypeNum == 1) { /* IMF Type-1: Write IMF data size right at the beginning */
		uint32_t iEnd = ftell(hIMF) - 2; /* We don't want to count the two bytes we're about to write at the file's start */
		fseek(hIMF, 0, SEEK_SET);
		writeUINT16LE(hIMF, iEnd);
	}
	return 0;
}

void addIMFTags(FILE *hIMF, char *cTitle, char *cComposer, char *cRemarks)
{
	int iTitleLen = strlen(cTitle);
	if (iTitleLen >= 255) {
		fprintf(stderr, "ERROR: Title field must be less than 255 characters, ignoring IMF tags.\n");
		return;
	}
	int iComposerLen = strlen(cComposer);
	if (iComposerLen >= 255) {
		fprintf(stderr, "ERROR: Composer field must be less than 255 characters, ignoring IMF tags.\n");
		return;
	}
	int iRemarksLen = strlen(cRemarks);
	if (iRemarksLen >= 255) {
		fprintf(stderr, "ERROR: Remarks field must be less than 255 characters, ignoring IMF tags.\n");
		return;
	}
	
	/* We want to append this at the end of the file */
	fseek(hIMF, 0, SEEK_END);

	/* Signature byte to indicate presence of this style of tags */
	fwrite("\x1A", 1, 1, hIMF);

	fwrite(cTitle, iTitleLen + 1, 1, hIMF);
	printf("Set title to '%s'\n", cTitle);
	fwrite(cComposer, iComposerLen + 1, 1, hIMF);
	printf("Set composer to '%s'\n", cComposer);
	fwrite(cRemarks, iRemarksLen + 1, 1, hIMF);
	printf("Set remarks to '%s'\n", cRemarks);

	/* Name of program that wrote the tags (not normally user-visible) */
	fwrite("DRO2IMF\x00\x00", 9, 1, hIMF); /* Eight chars fixed + terminating NULL */
	return;
}

/* Read in two bytes and treat them as a little-endian unsigned 16-bit integer, should be host-endian-neutral */
uint16_t readUINT16LE(FILE *hData)
{
	uint8_t b[2];
	fread(&b, 1, 2, hData);
	return b[0] | (b[1] << 8);
}

/* Read in four bytes and treat them as a little-endian unsigned 32-bit integer, should be host-endian-neutral */
uint32_t readUINT32LE(FILE *hData)
{
	uint8_t b[4];
	fread(&b, 1, 4, hData);
	return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

/* Write out an int as a little-endian unsigned 16-bit integer, should be host-endian-neutral */
void writeUINT16LE(FILE *hData, uint16_t iVal)
{
	uint8_t b[2];
	b[0] = iVal & 0xFF;
	b[1] = iVal >> 8;
	fwrite(&b, 1, 2, hData);
	return;
}

/* Write out a long as a little-endian unsigned 32-bit integer, should be host-endian-neutral */
void writeUINT32LE(FILE *hData, uint32_t iVal)
{
	uint8_t b[4];
	b[0] = iVal & 0xFF;
	b[1] = iVal >> 8;
	b[2] = iVal >> 16;
	b[3] = iVal >> 24;
	fwrite(&b, 1, 4, hData);
	return;
}
