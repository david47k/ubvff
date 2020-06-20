/*	ubvff2.c - Analyser and SVG convertor for an Unusual Binary Vector File Format Type 2
	
	For Type 1 files, the vector data is contained in a single file. Use ubvff1.
	For Type 2 files, the vector data is dispersed over multiple files. Use this program.
	The file format is different between the two types.

	This file format "Type 2" has only been found in one program, A Bugs Li*e Print Studio. 
	Container file: Bugsai.mms

	The vector data is split into several files. One is a list of points. Another contains the 
	commands that use these points. Another contains a layer name. Yet another may contain
	references to multiple files to assemble multiple layers into a single image.
	
	Example command file for an image with a single layer: 00053.bin
	
	Example command file for an image with multiple layers:	00100.bin
	Use the program vecass to assemble multiple layers.

	Copyright 2020 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)
	
*/

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//----------------------------------------------------------------------------
//  BINARY FILE STRUCTURE
//----------------------------------------------------------------------------


struct BIN_HEADER_S {
	uint16_t z1;
	uint16_t cmdCount;
	uint16_t z2;
	uint16_t x1;
	uint16_t y1;
	uint16_t x2;
	uint16_t y2;
};

union BIN_HEADER {
	uint16_t words[7];
	struct BIN_HEADER_S params;
};

struct BIN_POINT {
	int32_t x;
	int32_t y;
};

struct BIN_CUBIC {
	struct BIN_POINT p[3];
};

struct BIN_COLOR {
	uint16_t cmd;
	uint16_t r;
	uint16_t g;
	uint16_t b;
	uint16_t z;
};

struct BIN_FOOTER {
	uint16_t cmd;
	uint16_t pfilenum;
	uint16_t z1;
	uint16_t z2;
	uint16_t z3;
};

union CMD_WORDS {
	uint16_t words[5];
};

struct CMD_TABLE {
	uint16_t cmd;
	char cmdStr[25];
};

struct CMD_TABLE cmdTable[] = {
	{ 0x01, "END_FILE" },				// 1,N,0,0,0 : n=points file # (this file# -1) 
	{ 0x02, "MOVE_TO" },				// 2,1,0,0,0 is always before a POINTSLIST 
	{ 0x03, "POINTS_LINES" },			// 3,X,0,0,0  probably a straight line, I've seen 1-4
	{ 0x04, "POINTS_CUBICS" },			// 4,X,0,0,0  probably cubics, X is number of points
	{ 0x05, "STROKE_COLOR" },			
	{ 0x06, "FILL_COLOR" },
	{ 0x07, "END_PATH" },				// [1] EndPathWithFill  ( [0] EndPathWithStroke )  [2] EndPath  ( [4] EndPathUnknown )
	{ 0x08, "STROKE_FLAG_A" },			// 0,1
	{ 0x09, "STROKE_FLAG_B" },			// 0,1,2
	{ 0x0A, "STROKE_WIDTH" }			// A,X,X,0,0 combo of two words into DWORD? 8000,1  B666,0   0,1 	
};

//----------------------------------------------------------------------------
//  SCALING AND FLOATING POINT DISPLAY
//----------------------------------------------------------------------------

const int32_t scaleFactor = 0x10000;				/* We use this everywhere so best it's a global */

void printFloat(int32_t x) {
	printf("% *.6f ",11,(double)x/(double)scaleFactor);
}

#define F_FLOAT_FORMAT "%.6f"

//----------------------------------------------------------------------------
//  SVG OUTPUT - GLOBAL VARIABLES AND FUNCTIONS
//----------------------------------------------------------------------------

int svgdump = 0;

enum SVGDUMP_STATE {
	DUMPSTATE_BEGIN=0,
	DUMPSTATE_AFTER_HEADER=1,
	DUMPSTATE_AFTER_START_PATH=2,
	DUMPSTATE_AFTER_LINE=3,
	DUMPSTATE_AFTER_CLOSE_PATH=4,
	DUMPSTATE_AFTER_END_PATH=5,
	DUMPSTATE_AFTER_FOOTER=6
} svgDumpState = 0;	


int dumpSVGHeader(FILE * fout, struct BIN_HEADER_S * header) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_BEGIN) {
		printf("\nInvalid state in dumpSVGHeader: %d\n",svgDumpState);
		return 1;
	}
	int r = fprintf(fout, "%s","<svg viewBox=\"VIEWBOX_PLACEHOLDER_1234\" version=\"1.1\" baseProfile=\"full\" xmlns=\"http://www.w3.org/2000/svg\">\n");

	if(r < 0) {
		printf("\nfprintf failed (dumpSVGHeader)\n");
		return 1;
	}
	svgDumpState = DUMPSTATE_AFTER_HEADER;
	return 0;
}

int dumpSVGStartPath(FILE * fout, struct BIN_POINT * p) {
	if(!svgdump) return 0;
	char base[20] = "";
	if(svgDumpState == DUMPSTATE_AFTER_CLOSE_PATH || svgDumpState == DUMPSTATE_AFTER_LINE) {
		sprintf(base,"%s","M ");
	}	
	else if(svgDumpState != DUMPSTATE_AFTER_HEADER && svgDumpState != DUMPSTATE_AFTER_END_PATH) {
		printf("\nInvalid state in dumpSVGStartPath: %d\n",svgDumpState);
		return 1;
	} else {
		sprintf(base,"%s","<path d=\"M ");
	}
		
	int r = fprintf(fout, "%s" F_FLOAT_FORMAT " " F_FLOAT_FORMAT " ",
		base,
		(double)p->x/(double)scaleFactor,
		(double)p->y/(double)scaleFactor);
	//
	if(r < 0) {
		printf("\nfprintf failed (dumpSVGStartPath)\n");
		return 1;
	}	
	svgDumpState = DUMPSTATE_AFTER_START_PATH;
	return 0;
}

int dumpSVGCubic(FILE * fout, struct BIN_CUBIC * c) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_START_PATH && svgDumpState != DUMPSTATE_AFTER_LINE) {
		printf("\nInvalid state in dumpSVGCubic: %d\n",svgDumpState);
		return 1;
	}
	int r = fprintf(fout, "C " F_FLOAT_FORMAT " " F_FLOAT_FORMAT ", " F_FLOAT_FORMAT " " F_FLOAT_FORMAT ", " F_FLOAT_FORMAT " " F_FLOAT_FORMAT " ", 
		(double)c->p[0].x/(double)scaleFactor, (double)c->p[0].y/(double)scaleFactor,
		(double)c->p[1].x/(double)scaleFactor, (double)c->p[1].y/(double)scaleFactor,
		(double)c->p[2].x/(double)scaleFactor, (double)c->p[2].y/(double)scaleFactor);
	//
	if(r < 0) {
		printf("\nfprintf failed (dumpSVGCubic)\n");
		return 1;
	}		
	svgDumpState = DUMPSTATE_AFTER_LINE;
	return 0;
}

int dumpSVGLine(FILE * fout, struct BIN_POINT * p) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_START_PATH && svgDumpState != DUMPSTATE_AFTER_LINE) {
		printf("\nInvalid state in dumpSVGLine: %d\n",svgDumpState);
		return 1;
	}
	int r = fprintf(fout, "L " F_FLOAT_FORMAT " " F_FLOAT_FORMAT " ", (double)p->x/(double)scaleFactor, (double)p->y/(double)scaleFactor);
	if(r < 0) {
		printf("\nfprintf failed (dumpSVGLine)\n");
		return 1;
	}			
	svgDumpState = DUMPSTATE_AFTER_LINE;
	return 0;
}

int dumpSVGClosePath(FILE * fout) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_LINE && svgDumpState != DUMPSTATE_AFTER_START_PATH) {
		printf("\nInvalid state in dumpSVGClosePath: %d\n",svgDumpState);
		return 1;
	}	
	int r = fprintf(fout,"%s","Z ");
	if(r < 0) {
		printf("\nfprintf failed (dumpSVGClosePath)\n");
		return 1;
	}				
	svgDumpState = DUMPSTATE_AFTER_CLOSE_PATH;
	return 0;
}

int dumpSVGEndPath(FILE * fout, int hasFill, struct BIN_COLOR * fillColor, int hasStroke, int32_t strokeWidth, struct BIN_COLOR * strokeColor) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_LINE && svgDumpState != DUMPSTATE_AFTER_CLOSE_PATH) {
		printf("\nInvalid state in dumpSVGEndPath: %d\n",svgDumpState);
		return 1;
	}	
	char strokeBuf[200]="stroke=\"none\" ";
	char fillBuf[100]="fill=\"none\" ";
	
	if(hasFill) {
		sprintf(fillBuf,"fill=\"rgb(%u,%u,%u)\" ",
			fillColor->r, fillColor->g, fillColor->b
		);
	}
	
	if(hasStroke) { 
		sprintf(strokeBuf,"stroke=\"rgb(%u,%u,%u)\" stroke-width=\"" F_FLOAT_FORMAT "\" stroke-linecap=\"butt\" stroke-linejoin=\"miter\" stroke-miterlimit=\"10\" ",
			strokeColor->r, strokeColor->g, strokeColor->b,
			(double)strokeWidth/(double)scaleFactor
		);
	}
	
	int r = fprintf(fout,"\" %s%s/>\n",fillBuf,strokeBuf);
	if(r < 0) {
		printf("\nfprintf failed (dumpSVGEndPath)\n");
		return 1;
	}					
	svgDumpState = DUMPSTATE_AFTER_END_PATH;
	return 0;
}

int dumpSVGFooter(FILE * fout) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_END_PATH) {
		printf("\nInvalid state in dumpSVGFooter: %d\n",svgDumpState);
		return 1;
	}	
	int r = fprintf(fout,"%s","</svg>\n");
	if(r < 0) {
		printf("\nfprintf failed (dumpSVGFooter)\n");
		return 1;
	}						
	svgDumpState = DUMPSTATE_AFTER_FOOTER;
	return 0;
}

int roundInt(int n, int d) {	// will round down if within 25% of divisor, otherwise round up
	int l = n / d;
	int r = n % d;
	if(r<(d/4)) return l;
	
	if(l>0) return l+1;
	return l-1;
}

int dumpSVGSetViewbox(FILE * fout, int32_t minx, int32_t miny, int32_t maxx, int32_t maxy) {
	if(!svgdump) return 0;

	// <svg viewBox="VIEWBOX_PLACEHOLDER_1234" version...

	long pos = ftell(fout);
	if(pos==-1L) {
		printf("SetViewbox: ftell failed!\n");
		return 1;
	}
	if(fseek(fout,13,SEEK_SET)!=0) {
		printf("SetViewbox: fseek failed\n");
		return 1;
	}
	char buf[30] = "";
	int r = sprintf(&buf[0],"\"%i %i %i %i\"", roundInt(minx,scaleFactor), roundInt(miny,scaleFactor), roundInt(maxx,scaleFactor), roundInt(maxy,scaleFactor));
	if(r<0) {
		printf("SetViewbox: sprintf failed\n");
		return 1;
	}
		
	int size1 = strlen(buf);
	if(size1<26) {
		// rpad with spaces
		for(int i=size1; i<26; i++) {
			buf[i]=' ';
		}
		buf[26]=0;
	}
	if(fwrite(&buf[0],1,26,fout) != 26) {
		printf("SetViewbox: fwrite failed\n");
		return 1;
	}
	// printf("viewbox dimensions: %s\n",buf);	
	
	if(fseek(fout,pos,SEEK_SET)!=0) {
		printf("SetViewbox: fseek failed\n");
		return 1;
	} 
	
	return 0;
}

//----------------------------------------------------------------------------
//  BYTE ORDER FUNCTIONS
//----------------------------------------------------------------------------
	
uint16_t reverseByteOrder2(uint16_t input) {
    uint16_t output = (input&0xFF)<<8;
    output |= (input&0xFF00)>>8;
    return output;
}

uint32_t reverseByteMixed(uint32_t input) {
    uint32_t output = 0;
    output |= (input&0x000000FF) << 8;
    output |= (input&0x0000FF00) >> 8;
    output |= (input&0x00FF0000) << 8;
    output |= (input&0xFF000000) >> 8;
    return output;
}	

uint32_t reverseByteMixedForBigEndian(uint32_t input) {
    uint32_t output = 0;
    output |= (input&0x0000FFFF) << 16;
    output |= (input&0xFFFF0000) >> 16;
    return output;
}	


int systemIsLittleEndian(){
    volatile uint32_t i=0x01234567;
    // return 0 for big endian, 1 for little endian.
    return (*((uint8_t*)(&i))) == 0x67;
}

int32_t viewMinX=0, viewMinY=0,viewMaxX=0x10000,viewMaxY=0x10000;	// viewport of the image

size_t bo_fread ( void * ptr, size_t size, size_t count, FILE * stream ) {
	/* fread and perform byte order swapping if necessary */
	size_t r = fread(ptr,size,count,stream);
	if(r!=count) {
		return r;
	}
	if(size==2 && systemIsLittleEndian()) {
		for(int i=0; i<count; i++) {
			((uint16_t*)ptr)[i] = reverseByteOrder2(((uint16_t*)ptr)[i]);
		}
	} else if(size==4) {
		if(systemIsLittleEndian()) {
			for(int i=0; i<count; i++) {
				((uint32_t*)ptr)[i] = reverseByteMixed(((uint32_t*)ptr)[i]);
			}		
		} else {
			for(int i=0; i<count; i++) {
				((uint32_t*)ptr)[i] = reverseByteMixedForBigEndian(((uint32_t*)ptr)[i]);
			}		
		}
		// The only items we read of size 4 are points
		// We can analyse the points to get image dimensions here
		for(int i=0; i<count; i++) {
			if(i%2==0) { // an X value
				if(((int32_t *)ptr)[i] > viewMaxX) viewMaxX = ((int32_t *)ptr)[i];
				else if(((int32_t *)ptr)[i] < viewMinX) viewMinX = ((int32_t *)ptr)[i];
			} else { // a Y value
				if(((int32_t *)ptr)[i] > viewMaxY) viewMaxY = ((int32_t *)ptr)[i];
				else if(((int32_t *)ptr)[i] < viewMinY) viewMinY = ((int32_t *)ptr)[i];			
			}
		}		

	}
	return r;
}

//----------------------------------------------------------------------------
//  OTHER FUNCTIONS
//----------------------------------------------------------------------------


void printParams(union CMD_WORDS * cmdw) {
	printf("0x%04X 0x%04X 0x%04X 0x%04X\n", cmdw->words[1], cmdw->words[2], cmdw->words[3], cmdw->words[4]);
}

void printError(char * str) {
	printf("  error : %s\n", str);
}

void printError2(char * str1, char * str2) {
	printf("  error : %s%s\n", str1, str2);
}

//----------------------------------------------------------------------------
//  MAIN
//----------------------------------------------------------------------------

int main(int argc, char * argv[]) {
    
    if(argc<3) {
		printf("%s","ubvff2: Unknown Binary Vector File Format Type 2, analyser and SVG converter\n\n");
		printf("%s","usage: ubvff2 cmdFile pointsFile [-svgdump outputFile] [-more] [-less]\n"
					"    cmdFile       File name of input file that contains vector commands.\n"
					"    pointsFile    File name of input file that contains point data.\n"
					"                  Can be \"auto\" to guess \"NNNNN.bin\" e.g. \"00123.bin\".\n"
					"    -svgdump      Create an svg file. File name can be \"auto\".\n"
					"    -more         Display more analysis information.\n"
					"    -less         Display less analysis information.\n"
		);			
		return 0;
    }
    
	char * filename1 = argv[1];
	char filename2[300];
	char svgfilename[300];
	int detail = 2;					// 1:little, 2:one line per command, 3:all 
	uint32_t offset = 4;
	
	if((strlen(argv[2])+1)>sizeof(filename2)) {
		printError("pointsFile name is too long");
		return 1;
	}

	if((strlen(argv[1])+1) > (sizeof(svgfilename)-10)) {
		printError("cmdFile name is too long");
		return 1;
	}	
	
	strcpy(filename2, argv[2]);
	
	for(int i=3; i<argc; i++) {
		if(strncmp(argv[i],"-svgdump",8)==0 && i<(argc-1)) {
			svgdump = 1;
			i++;
			if((strlen(argv[i])+1) > sizeof(svgfilename)) {
				printError("svg outputFile name is too long");
				return 1;
			}
			strcpy(svgfilename,argv[i]);
		} else if(strncmp(argv[i],"-more",5)==0) {
			detail++;
		} else if(strncmp(argv[i],"-less",5)==0) {
			detail--;
		}
	}

	// open command file
    FILE * fin1 = fopen(filename1, "rb");
    if(fin1==NULL) {
		printError2("failed to open command input file: ", filename1);
		return 1;
    }
    fseek(fin1,0,SEEK_SET);

	// come up with auto svg filename
	if(memcmp(svgfilename,"auto",5)==0) {
		strcpy(svgfilename, filename1);
		// remove common trailing extensions for common file names
		int s = strlen(svgfilename);
		if(s>5) {
			int dotPos=-1;
			for(int i=s-5; i<s; i++) {
				if(svgfilename[i]=='/' || svgfilename[i]=='\\') dotPos=-1;
				else if(svgfilename[i]=='.') dotPos=i;
			}
			if(dotPos != -1) {
				svgfilename[dotPos] = 0;
			}
		}
		s = strlen(svgfilename);
		if((s+5) < sizeof(svgfilename)) {
			memcpy(&svgfilename[s],".svg",5);
		} else {
			printError("auto filename is too long!");
			return 1;
		}
	}

	// variables: states read from input file
	struct BIN_COLOR fillColor;
	struct BIN_COLOR strokeColor;
	union BIN_HEADER header;
	struct BIN_FOOTER footer;
	uint32_t strokeWidth = 0x10000;
	uint16_t strokeFlagA = 0;
	uint16_t strokeFlagB = 0;
	union CMD_WORDS cmdw;
	int hasStroke;
	int hasFill;
	
	// Read the file header
	if(bo_fread(&header,2,7,fin1) != 7) {
		printError("read failed (header)");
		return 1;
	}
	
	// Check if file header makes sense of sorts
	if(header.words[1]<=0x0A) {					// at least this many commands in a typical file
		printError("not a valid command file (header check failed)");
		return 1;
	}
	
	// Read in the footer
	fseek(fin1,-10,SEEK_END);
	if(bo_fread(&footer,2,5,fin1) != 5) {
		printError("read failed (footer)");
		return 1;
	}
	
	// Check if file footer makes sense
	if(footer.cmd != 0x01 || footer.z1 != 0 || footer.z2 != 0 || footer.z3 != 0) {
		printError("not a valid command file (footer check failed)");
		return 1;
	}

	// Come up with pointsFile name if set to auto
	if(memcmp(filename2,"auto",5)==0) {
		// If the cmdFile name ends in NNNNN.bin, we can use the same prefix
		int l = strlen(filename1);
		int done = 0;
		if(l>9) {
			int i;
			for(i=(l-9); i<(l-4); i++) {
				if(!isdigit(filename1[i])) break;
			}
			if(i==(l-4) && memcmp(&filename1[l-4],".bin",4)==0) {
				// we can use same prefix
				memcpy(filename2,filename1,l-9);
				sprintf(&filename2[l-9],"%05u.bin",footer.pfilenum);
				done = 1;
			}
		}
		// otherwise we don't use prefix
		if(!done) {
			sprintf(filename2,"%05u.bin",footer.pfilenum);		
		}
	}
	
	// Open the pointsFile 
	FILE * fin2 = fopen(filename2, "rb");
    if(fin2==NULL) {
		printError2("failed to open points input file: ", filename2);
		return 1;
    }

	// check pointsFile header is valid
	fseek(fin2,0,SEEK_SET);
	uint16_t pFileHeader[2];
	if(bo_fread(pFileHeader,2,2,fin2) != 2) {
		printError("read failed (pointsFile)");
		return 1;
	}
	
	// offset points file to start of points data
	fseek(fin2,offset,SEEK_SET);

	// Display vital statistics
	printf("command file (%5u commands) : %s\n", header.params.cmdCount, filename1);
	printf("points file  (%5u points  ) : %s\n", pFileHeader[1], filename2);

	// open output file if we are dumping
	FILE * fout = NULL;	
	if(svgdump) {
		// Open output file
		fout = fopen(svgfilename,"w+b");
		if(fout==NULL) {
			printError2("unable to open output file: ", svgfilename);
			return 1;
		}
		printf("svg output file               : %s\n", svgfilename);
	}
	
	// SVG: output the header
	dumpSVGHeader(fout, &header.params);
	
	// Seek to the start of the command data
	fseek(fin1,14,SEEK_SET);
	
	uint16_t cmdCounter = 0;
	
	// Main input-file-reading loop
	for(cmdCounter = 1; cmdCounter < header.params.cmdCount; cmdCounter++) {
		if(bo_fread(&cmdw,2,5,fin1) != 5) {
			printError("read failed (command)");
			break;
		}
		
		uint16_t cmd = cmdw.words[0];
		
		char strBuf[257];
		
		// Find the index into cmdTable
		uint32_t idx = 0;
		uint32_t cmdTableSize = sizeof(cmdTable)/sizeof(struct CMD_TABLE);
		for(idx=0; idx<cmdTableSize; idx++) {
			if(cmdTable[idx].cmd == cmd) break;
		}
			
		if(idx == cmdTableSize) {
			sprintf(strBuf,"UNKNOWN 0x%04X",cmd);
			printf("%-24s",strBuf);
		} else {			
			if(detail >= 2) printf("%-24s",cmdTable[idx].cmdStr);
		}
			
		// Process parameters
		if(cmd==0x01) {						// END_FILE
			dumpSVGFooter(fout);
			dumpSVGSetViewbox(fout,viewMinX,viewMinY,viewMaxX,viewMaxY);
			cmdCounter++;
			if(detail >= 2) {
				printf("\n");
			}
			break;
		} else if(cmd==0x02) {				// MOVE_TO
			if(cmdw.words[1] != 1) {
				printParams(&cmdw);
				sprintf(strBuf,"MOVE_TO has parameter that isn't 1: %u", cmdw.words[1]);
				printError(strBuf);
				break;
			}
			struct BIN_POINT p;
			if(bo_fread(&p,4,2,fin2) != 2) {
				printError("read failed (MOVE_TO)");
				break;
			}
			if(detail >= 2) {
				printFloat(p.x);
				printFloat(p.y);
				printf("\n");
			}
			if(dumpSVGStartPath(fout,&p)) {
				break;
			}
		} else if(cmd==0x03) {				// POINTS_LINES
			uint16_t pTotal = cmdw.words[1];
			struct BIN_POINT * points;
			if(pTotal==0) {
				sprintf(strBuf,"unexpected pTotal (POINTS_LINES): %u", pTotal);
				printError(strBuf);
				break;
			}
			points = malloc(sizeof(struct BIN_POINT) * pTotal);
			if(points==NULL) {
				printParams(&cmdw);
				printError("out of memory (POINTS_LINES)");
				break;
			}
			if(bo_fread(points,4,pTotal*2,fin2) != pTotal*2) {
				printError("read failed (POINTS_LINES)");
				break;
			}
			if(detail >= 2) {
				printf("%u lines\n", pTotal);
			}
			for(int i=0; i<pTotal; i++) {
				if(dumpSVGLine(fout, &points[i])) {
					break;
				}
			}
			free(points);
		} else if(cmd==0x04) {				// POINTS_CUBICS
			uint16_t pTotal = cmdw.words[1];
			if(pTotal%3!=0 || pTotal==0) {
				printParams(&cmdw);
				sprintf(strBuf,"unexpected pTotal (POINTS_CUBICS): %u", pTotal);
				printError(strBuf);
				break;
			}
			struct BIN_CUBIC * cubics;
			cubics = malloc(sizeof(struct BIN_CUBIC) * (pTotal/3));
			if(cubics==NULL) {
				printParams(&cmdw);
				printError("out of memory (POINTS_CUBICS)");
				break;
			}
			if(bo_fread(cubics,4,pTotal*2,fin2) != pTotal*2) {
				printError("read failed (POINTS_CUBICS)");
				break;
			}
			if(detail >= 2) {
				printf("%u cubics\n", pTotal/3);
			}
			for(int i=0; i<(pTotal/3); i++) {
				if(dumpSVGCubic(fout, &cubics[i])) {
					break;
				}
			}
			free(cubics);
		} else if(cmd==0x05) {				// STROKE_COLOR
			memcpy(&strokeColor,&cmdw,sizeof(strokeColor));
			if(detail >= 2) {
				printf("rgb(%u,%u,%u)\n", strokeColor.r, strokeColor.g, strokeColor.b);
			}
		} else if(cmd==0x06) {				// FILL_COLOR
			memcpy(&fillColor,&cmdw,sizeof(fillColor));
			if(detail >= 2) {
				printf("rgb(%u,%u,%u)\n", fillColor.r, fillColor.g, fillColor.b);
			}
		} else if(cmd==0x07) {				// END_PATH
			// [1], [0], [2]  always appears in this order
			if(detail >= 2) {
				printf("%u\n",cmdw.words[1]);
			}
			if(cmdw.words[1] == 0x01) {		
				dumpSVGClosePath(fout);				// Close the path ('Z')
				hasStroke = 0;
				hasFill = 1;
			} else if(cmdw.words[1] == 0x00) {		// Has stroke
				hasStroke = 1;
			} else if(cmdw.words[1] == 0x02) {		// End the path.
				dumpSVGEndPath(fout,hasFill,&fillColor,hasStroke,strokeWidth,&strokeColor);
			} else if(cmdw.words[1] == 0x03) {		// Has NO stroke or fill.
				hasFill = 0;
			} else if(cmdw.words[1] == 0x04) {
				// Indicates start of No stroke, No fill area.
			} else if(cmdw.words[1] == 0x05) {
				// Indicates end of file with No stroke, No fill area(s).
			} else {
				sprintf(strBuf,"Unknown parameter to cmd 0x07: %u", cmdw.words[1]);
				printError(strBuf);
				break;
			}
		} else if(cmd==0x08) {				// STROKE_FLAG_A
			strokeFlagA = cmdw.words[1];
			if(detail >= 2) {
				printf("%u\n", strokeFlagA);
			}				
		} else if(cmd==0x09) {				// STROKE_FLAG_B
			strokeFlagB = cmdw.words[1];
			if(detail >= 2) {
				printf("%u\n", strokeFlagB);
			}
		} else if(cmd==0x0A) {				// STROKE_WIDTH
			strokeWidth = (cmdw.words[2] << 16) & cmdw.words[1];
			if(detail >= 2) {
				printFloat(strokeWidth);
				printf("\n");
			}			
		} else {
			printParams(&cmdw);				// UNKNOWN			
		}
	}

	int error = 0;

	if(svgdump) {
		if(svgDumpState==DUMPSTATE_AFTER_FOOTER) {
			uint8_t temp;
			bo_fread(&temp,1,1,fin1);

			if(!feof(fin1)) {
				printf("warning : additional data past END_FILE marker\n");
			}
			
			bo_fread(&temp,1,1,fin2);
			if(!feof(fin1)) {
				printf("warning : didn't reach end of points file\n");
			}

		} else {
			error = 1;
		}
	}

	if(cmdCounter != header.params.cmdCount) {
		printf("warning : cmdCounter got to %u of %u\n",cmdCounter,header.params.cmdCount);
		error = 1;
	}
	
	fclose(fin1);
	fclose(fin2);
	if(svgdump) {
		fclose(fout);
	}
	
	if(error) {
		printf("exiting due to error.\n");
	} else {
		printf("done.\n");
	}	
	
	return error;
}



