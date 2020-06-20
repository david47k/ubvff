/*	ubvff1.c - Analyser and SVG convertor for an Unusual Binary Vector File Format Type 1
	
	For Type 1 files, the vector data is contained in a single file. Use this program.
	For Type 2 files, the vector data is dispersed over multiple files. Use ubvff2.
	The file format is different between the two types.
	
	Example filenames:
	
	tscp001.BIN
	BWtscp001.BIN
	TZcp001.BIN
	TZcpBW001.BIN
	006pooh.BIN
	
	Copyright 2020 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)
	
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------
//  BINARY FILE STRUCTURE
//----------------------------------------------------------------------------

struct BIN_HEADER {
	int32_t x1;
	int32_t y1;
	int32_t x2;
	int32_t y2;
	int32_t unknown;
};

struct BIN_POINT {
	int32_t x;
	int32_t y;
};

struct BIN_CUBIC {
	struct BIN_POINT p[3];
};

struct BIN_COLOR {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t unused;
};

struct CMD_TABLE {
	uint32_t cmd;
	char cmdStr[25];
};

struct CMD_TABLE cmdTable[] = {
	{ 0x00, "CMD_00_LAYER_SEP" },		// comes after CMD_02 in between layers
	{ 0x01, "CMD_01_START_LAYER" },
	{ 0x02, "CMD_02_END_LAYER" },
	{ 0x03, "CMD_03_START_FILE" },
	{ 0x04, "CMD_04_STROKE_COLOR" },
	{ 0x05, "CMD_05_FILL_COLOR" },
	{ 0x06, "CMD_06_START_PATH" },		
	{ 0x07, "CMD_07_LINE" },			
	{ 0x08, "CMD_08_CUBIC" },
	{ 0x09, "CMD_09_END_PATH_SO" },		// stroke only
	{ 0x0A, "CMD_0A_END_PATH_FO" },		// fill only
	{ 0x0B, "CMD_0B_END_PATH_SF" },		// stroke and fill
	{ 0x0C, "CMD_0C_NOP" },
	{ 0x0D, "CMD_0D_CLOSE_PATH" },
	{ 0x0E, "CMD_0E_UNKNOWN_FLAG1" },	// usually only one, 0, in header, sometimes see a 0 or 1 elsewhere
	{ 0x0F, "CMD_0F_UNKNOWN_FLAG2" },	// unknown, 0 or 1
	{ 0x10, "CMD_10_STROKE_WIDTH" },
	{ 0x15, "CMD_15_END_FILE" }	
};

//----------------------------------------------------------------------------
//  SCALING AND FLOATING POINTS
//----------------------------------------------------------------------------

const int32_t scaleFactor = 0x8000;		// We use this everywhere so best it's a global

void printFloat(int32_t x) {
	printf("% *.6f ",11,(double)x/(double)scaleFactor);
}

#define F_FLOAT_FORMAT "%.6f"

int roundInt(int n, int d) {	// will round down if within 25% of divisor, otherwise round up
	int l = n / d;
	int r = n % d;
	if(r<(d/4)) return l;
	
	if(l>0) return l+1;
	return l-1;
}

//----------------------------------------------------------------------------
//  SVG OUTPUT - GLOBAL VARIABLES AND FUNCTIONS
//----------------------------------------------------------------------------

int svgdump = 0;

enum SVGDUMP_STATE {
	DUMPSTATE_BEGIN=0,
	DUMPSTATE_AFTER_HEADER=1,
	DUMPSTATE_AFTER_START_LAYER=2,
	DUMPSTATE_AFTER_START_PATH=3,
	DUMPSTATE_AFTER_LINE=4,
	DUMPSTATE_AFTER_CLOSE_PATH=5,
	DUMPSTATE_AFTER_END_PATH=6,
	DUMPSTATE_AFTER_END_LAYER=7,
	DUMPSTATE_AFTER_FOOTER=8
} svgDumpState = 0;	


int dumpSVGHeader(FILE * fout, struct BIN_HEADER * header) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_BEGIN) {
		printf("\nstate error : in dumpSVGHeader: %d\n", svgDumpState);
		return 1;
	}
	int r = fprintf(fout, "%s%d%s%d%s",
		"<svg viewBox=\"0 0 ",
		roundInt(header->x2,scaleFactor),
		" ",
		roundInt(header->y2,scaleFactor), 
		"\" version=\"1.1\" baseProfile=\"full\" xmlns=\"http://www.w3.org/2000/svg\">\n"
	);

	if(r < 0) {
		printf("\nerror : fprintf failed (dumpSVGHeader)\n");
		return 1;
	}
	svgDumpState = DUMPSTATE_AFTER_HEADER;
	return 0;
}

int dumpSVGStartLayer(FILE * fout) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_HEADER && svgDumpState != DUMPSTATE_AFTER_END_LAYER) {
		printf("\nstate error : in dumpSVGStartLayer: %d\n", svgDumpState);
		return 1;
	}
	int r = fprintf(fout, "<g>\n");
	if(r < 0) {
		printf("\nerror : fprintf failed (dumpSVGStartLayer)\n");
		return 1;
	}
	svgDumpState = DUMPSTATE_AFTER_START_LAYER;
	return 0;
}


int dumpSVGStartPath(FILE * fout, struct BIN_POINT * p) {
	if(!svgdump) return 0;
	char base[20] = "";
	if(svgDumpState == DUMPSTATE_AFTER_CLOSE_PATH || svgDumpState == DUMPSTATE_AFTER_LINE) {
		sprintf(base,"%s","M ");
	}	
	else if(svgDumpState != DUMPSTATE_AFTER_START_LAYER && svgDumpState != DUMPSTATE_AFTER_END_PATH) {
		printf("\nstate error : in dumpSVGStartPath: %d\n",svgDumpState);
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
		printf("\nerror : fprintf failed (dumpSVGStartPath)\n");
		return 1;
	}	
	svgDumpState = DUMPSTATE_AFTER_START_PATH;
	return 0;
}

int dumpSVGCubic(FILE * fout, struct BIN_CUBIC * c) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_START_PATH && svgDumpState != DUMPSTATE_AFTER_LINE) {
		printf("\nstate error : in dumpSVGCubic: %d\n", svgDumpState);
		return 1;
	}
	int r = fprintf(fout, "C " F_FLOAT_FORMAT " " F_FLOAT_FORMAT ", " F_FLOAT_FORMAT " " F_FLOAT_FORMAT ", " F_FLOAT_FORMAT " " F_FLOAT_FORMAT " ", 
		(double)c->p[0].x/(double)scaleFactor, (double)c->p[0].y/(double)scaleFactor,
		(double)c->p[1].x/(double)scaleFactor, (double)c->p[1].y/(double)scaleFactor,
		(double)c->p[2].x/(double)scaleFactor, (double)c->p[2].y/(double)scaleFactor);
	//
	if(r < 0) {
		printf("\nerror : fprintf failed (dumpSVGCubic)\n");
		return 1;
	}		
	svgDumpState = DUMPSTATE_AFTER_LINE;
	return 0;
}

int dumpSVGLine(FILE * fout, struct BIN_POINT * p) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_START_PATH && svgDumpState != DUMPSTATE_AFTER_LINE) {
		printf("\nstate error : in dumpSVGLine: %d\n",svgDumpState);
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
	if(svgDumpState != DUMPSTATE_AFTER_LINE) {
		printf("\nstate error : in dumpSVGClosePath: %d\n",svgDumpState);
		return 1;
	}	
	int r = fprintf(fout,"%s","Z ");
	if(r < 0) {
		printf("\nerror : fprintf failed (dumpSVGClosePath)\n");
		return 1;
	}				
	svgDumpState = DUMPSTATE_AFTER_CLOSE_PATH;
	return 0;
}

int dumpSVGEndPath(FILE * fout, int hasFill, struct BIN_COLOR * fillColor, int hasStroke, int32_t strokeWidth, struct BIN_COLOR * strokeColor) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_LINE && svgDumpState != DUMPSTATE_AFTER_CLOSE_PATH) {
		printf("\nstate error : in dumpSVGEndPath: %d\n",svgDumpState);
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
		printf("\nerror : fprintf failed (dumpSVGEndPath)\n");
		return 1;
	}					
	svgDumpState = DUMPSTATE_AFTER_END_PATH;
	return 0;
}

int dumpSVGEndLayer(FILE * fout) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_END_PATH && svgDumpState != DUMPSTATE_AFTER_START_LAYER) {
		printf("\nstate error : in dumpSVGEndLayer: %d\n",svgDumpState);
		return 1;
	}	
	int r = fprintf(fout,"%s","</g>\n");
	if(r < 0) {
		printf("\nerror : fprintf failed (dumpSVGFooter)\n");
		return 1;
	}						
	svgDumpState = DUMPSTATE_AFTER_END_LAYER;
	return 0;
}

int dumpSVGFooter(FILE * fout) {
	if(!svgdump) return 0;
	if(svgDumpState != DUMPSTATE_AFTER_END_LAYER) {
		printf("\nstate error : in dumpSVGFooter: %d\n",svgDumpState);
		return 1;
	}	
	int r = fprintf(fout,"%s","</svg>\n");
	if(r < 0) {
		printf("\nerror : fprintf failed (dumpSVGFooter)\n");
		return 1;
	}						
	svgDumpState = DUMPSTATE_AFTER_FOOTER;
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

uint32_t reverseByteOrder4(uint32_t input) {
    uint32_t output = 0;
    output |= (input&0x000000FF) << 24;
    output |= (input&0x0000FF00) <<  8;
    output |= (input&0x00FF0000) >> 8;
    output |= (input&0xFF000000) >> 24;
    return output;
}	
	
int systemIsLittleEndian(){
    volatile uint32_t i=0x01234567;
    // return 0 for big endian, 1 for little endian.
    return (*((uint8_t*)(&i))) == 0x67;
}

size_t bo_fread ( void * ptr, size_t size, size_t count, FILE * stream ) {
	// fread and perform byte order swapping if necessary
	size_t r = fread(ptr,size,count,stream);
	if(r!=count) {
		return r;
	}
	if(systemIsLittleEndian()) {
		if(size==2) {
			for(int i=0; i<count; i++) {
				((uint16_t*)ptr)[i] = reverseByteOrder2(((uint16_t*)ptr)[i]);
			}
		} else if(size==4) {
			for(int i=0; i<count; i++) {
				((uint32_t*)ptr)[i] = reverseByteOrder4(((uint32_t*)ptr)[i]);
			}		
		}
	}
	return r;
}

//----------------------------------------------------------------------------
// STRING ESCAPE
//----------------------------------------------------------------------------

int escapeStringA(char * dest, int destSize, char * source) {
	// Escape \'" and other non-asciis, output suitable for screen display
	// source must be null-terminated (or >= in size to dest...ish)
	
	char csbuf[6];
	int overflow = 0;
	int destPos = 0;
	
	if(destSize<1) return 1; 	// daft error

	for(int i=0; destPos<destSize; i++) {
		if(source[i]==0) {
			dest[destPos] = 0;
			break;
		} else if((source[i] < 32) || (source[i] > 126) || (source[i]=='\\') || (source[i]=='\'') || (source[i]=='"')) {
			if(destPos+4 >= destSize) {
				overflow = 1;
				break;
			}
			sprintf(csbuf,"\\x%02X",source[i]);
			memcpy(&dest[destPos],csbuf,4);
			destPos+=4;
		} else {
			dest[destPos] = source[i];
			destPos++;
		}
	}
	
	if(destPos==destSize) {
		overflow = 1;
		dest[destPos-1] = 0;
	} else {
		dest[destPos] = 0;
	}
	
	return overflow;
}

//----------------------------------------------------------------------------
//  MAIN
//----------------------------------------------------------------------------

int main(int argc, char * argv[]) {
    
    if(argc<2) {
		printf("%s","ubvff1: Unknown Binary Vector File Format Type 1, analyser and SVG converter.\n\n");
		printf("%s","usage: ubvff1 inputFile [-svgdump outputFile] [-more] [-less]\n"
					"    inputFile              File name of compatible input file.\n"
					"    -svgdump ouputFile     Create an svg file. Can be \"auto\".\n"
					"    -more                  Display more analysis information.\n"
					"    -less                  Display less analysis information.\n"
		);
		return 0;
    }
    
	char * filename = argv[1];
	char * svgfilename = "";
	int detail = 2;					// 1:little, 2:one line per command, 3:all
	char autoFilename[300];			// this needs to exist in this scope
	
	if(strlen(argv[1]) > (sizeof(autoFilename)-10)) {
		printf("error : input file name is too long\n");
		return 1;
	}	
	
	for(int i=2; i<argc; i++) {
		if(strncmp(argv[i],"-svgdump",8)==0 && i<(argc-1)) {
			svgdump = 1;
			i++;
			svgfilename = argv[i];
		} else if(strncmp(argv[i],"-more",5)==0) {
			detail++;
		} else if(strncmp(argv[i],"-less",5)==0) {
			detail--;
		}
	}

    FILE * f = fopen(filename, "rb");

    if(f==NULL) {
		printf("error : failed to open input file: %s\n", filename);
		return 1;
    }

    fseek(f,0,SEEK_SET);

	FILE * fout = NULL;
	
	// open output file if we are dumping
	if(svgdump) {
		// come up with auto svg filename
		if(memcmp(svgfilename,"auto",5)==0) {
			strcpy(autoFilename, filename);
			// remove common trailing extensions for common file names
			int s = strlen(autoFilename);
			if(s>5) {
				int dotPos=-1;
				for(int i=s-5; i<s; i++) {
					if(autoFilename[i]=='/' || autoFilename[i]=='\\') dotPos=-1;
					else if(autoFilename[i]=='.') dotPos=i;
				}
				if(dotPos != -1) {
					autoFilename[dotPos] = 0;
				}
			}
			s = strlen(autoFilename);
			if((s+5) < sizeof(autoFilename)) {
				memcpy(&autoFilename[s],".svg",5);
				svgfilename = autoFilename;
			} else {
				printf("error : auto filename is too long\n");
				return 1;
			}
		}

		// Open output file
		fout = fopen(svgfilename,"wb");
		if(fout==NULL) {
			printf("error : unable to open output file: %s\n", svgfilename);
			return 1;
		}
		printf("dumping SVG to : %s\n", svgfilename);
	}

	// States read from input file	
	char title[65]="";
	struct BIN_COLOR color;
	struct BIN_HEADER header;
	int32_t strokeWidth = 0x8000;
	struct BIN_COLOR strokeColor;
	uint32_t cmd;

	// Main input-file-reading loop
	while(!feof(f)) {
		if(bo_fread(&cmd,4,1,f) != 1) {
			break;
		}
		
		char strBuf[257];
		
		// Find the index into cmdTable
		uint32_t idx = 0;
		uint32_t cmdTableSize = sizeof(cmdTable)/sizeof(struct CMD_TABLE);
		for(idx=0; idx<cmdTableSize; idx++) {
			if(cmdTable[idx].cmd == cmd) break;
		}
			
		if(idx == cmdTableSize) {
			sprintf(strBuf,"UNKNOWN 0x%08X",cmd);
			printf("%-24s",strBuf);
		} else {			
			if(detail >= 2) printf("%-24s",cmdTable[idx].cmdStr);
		}
		
		// Process parameters
		if(cmd==0x00) {								// CMD_00_LAYER_SEP
			if(detail >= 2) printf("\n");
		} else if(cmd==0x01) {						// CMD_01_START_LAYER
			uint32_t strLength;
			if(bo_fread(&strLength,4,1,f) != 1) {
				printf("\nerror : nread failed (title size)\n");
				break;
			}
			if(strLength > (sizeof(title)-1)) {
				printf("\nerror : title string overflow\n");
				return 1;
			}
			
			for(int y=0; y<strLength; y++) { 		// string uses 32-bit padded characters!
				uint32_t dw;
				if(bo_fread(&dw,4,1,f) != 1) {
					printf("\nerror : read failed (layer name)\n");
					break;
				}
				title[y] = (char)dw;
			}
			title[strLength]=0;
			escapeStringA(strBuf,sizeof(strBuf),title);
			if(detail >= 2) printf("\"%s\"\n",strBuf);
			if(svgDumpState==DUMPSTATE_BEGIN) {
				if(dumpSVGHeader(fout, &header)) {	// at this point, we can start generating the SVG header
					break;
				}
			}
			if(dumpSVGStartLayer(fout)) {
				break;
			}
		} else if(cmd==0x02) {						// CMD_02_END_LAYER
			if(detail >= 2) printf("\n");
			if(svgDumpState == DUMPSTATE_AFTER_CLOSE_PATH) {
				printf("warning : missing END_PATH before END_LAYER\n");
				if(dumpSVGEndPath(fout,0,&color,0,strokeWidth,&strokeColor)) {
					break;
				}
			}
			if(dumpSVGEndLayer(fout)) {
				break;
			}
		} else if(cmd==0x03) {						// CMD_03_START_FILE
			if(bo_fread(&header,4,5,f) != 5) {
				printf("\nerror : fread failed (header)\n");
				break;
			}
			if(detail >= 2) {
				printFloat(header.x1);
				printFloat(header.y1);
				printFloat(header.x2);
				printFloat(header.y2);		
				printf("%d\n", header.unknown);
			}
		} else if(cmd==0x04) {						// CMD_04_STROKE_COLOR
			if(bo_fread(&strokeColor,4,1,f) != 1) {
				printf("\nerror : fread failed (stroke color)\n");
				break;
			}
			if(detail >= 2) printf("rgb(%u,%u,%u)\n",strokeColor.r,strokeColor.g,strokeColor.b);			
		} else if(cmd==0x05) {						// CMD_05_FILL_COLOR
			if(bo_fread(&color,4,1,f) != 1) {
				printf("\nerror : fread failed (color)\n");
				break;
			}
			if(detail >= 2) printf("rgb(%u,%u,%u)\n",color.r,color.g,color.b);
		} else if(cmd==0x06) {						// CMD_06_MOVE_TO
			struct BIN_POINT p;
			if(bo_fread(&p,4,2,f) != 2) {
				printf("\nerror : fread filed (startpath)\n");
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
		} else if(cmd==0x07) { 						// CMD_07_LINE
			struct BIN_POINT p;
			uint32_t pcount = 0;
			if(bo_fread(&pcount,4,1,f) != 1) {
				printf("\nerror : fread failed (line pcount)\n");
				break;
			}
			for(int y=0; y<pcount; y++) {
				if(bo_fread(&p,4,2,f) != 2) {
					printf("\nerror : fread failed (line point)\n");
					break;
				}
				if(y<3 || detail>2) {
					if(y>0 && y%3==0 && detail >= 2) {
						printf("\n                        "); // paddddddding
					}
					if(detail >= 2) {
						printFloat(p.x);
						printFloat(p.y);
					}
				} else if(y < 2 && detail >= 2) {
					printf("...");
				}
				if(dumpSVGLine(fout,&p)) {
					break;
				}
			}
			if(detail >= 2) printf("\n");
		} else if(cmd==0x08) { 						// CMD_08_CUBIC
			struct BIN_CUBIC c;
			uint32_t pcount = 0;
			if(bo_fread(&pcount,4,1,f) != 1) {
				printf("\nerror : fread failed (0x08 pcount)\n");
				break;
			}
			for(int y=0; y<(pcount/3); y++) { 		// cubics always consist of three points
				if(bo_fread(&c,4,6,f) != 6) {
					printf("\nerror : fread failed (0x08 cubic)\n");
					break;
				}
				if(y<1 || detail>2) {					
					if(y>0 && detail >= 2) {
						printf("\n                        "); // paddddddding
					}
					for(int j=0; j<3; j++) {
						if(detail >= 2) {
							printFloat(c.p[j].x);
							printFloat(c.p[j].y);
						}
					}
				} else if(y<2 && detail >= 2) {
					printf("...");
				}
				if(dumpSVGCubic(fout,&c)) {
					break;
				}
			}
			if(detail >= 2) printf("\n");
		} else if(cmd==0x09) {						// CMD_09_END_PATH_SO
			if(detail >= 2) printf("\n");
			if(dumpSVGEndPath(fout,0,&color,1,strokeWidth,&strokeColor)) {
				break; /* TODO: might need to fix fill color ???? */
			}
		} else if(cmd==0x0A || cmd==0x0B) { 		// CMD_0A_END_PATH_FO or CMD_OB_END_PATH_SF */
			if(dumpSVGEndPath(fout,1,&color,(cmd==0x0B),strokeWidth,&strokeColor)) {
				break;
			}
			if(detail >= 2) printf("\n");
		} else if(cmd==0x0C) { 						// CMD_0C_NOP
			if(detail >= 2) printf("\n");
		} else if(cmd==0x0D) { 						// CMD_0D_CLOSE_PATH
			if(dumpSVGClosePath(fout)) {
				break;
			}
			if(detail >= 2) printf("\n");
		} else if(cmd==0x0E) { 						// CMD_0E_UNKNOWN_FLAG1
			int32_t unknown;
			if(bo_fread(&unknown,4,1,f) != 1) {
				printf("\nerror : read failed (CMD_0E)\n");
				break;
			}
			if(detail >= 2) printf("0x%08X\n", unknown);
		} else if(cmd==0x0F) { 						// CMD_0F_UNKNOWN_FLAG2
			int32_t unknown;
			if(bo_fread(&unknown,4,1,f) != 1) {
				printf("\nerror : read failed (CMD_0F)\n");
				break;
			}
			if(detail >= 2) printf("0x%08X\n", unknown);
		} else if(cmd==0x10) { 						// CMD_10_STROKE_WIDTH
			if(bo_fread(&strokeWidth,4,1,f) != 1) {
				printf("\nerror : read failed (stroke width)\n");
				break;
			}
			if(detail >= 2) {
				printFloat(strokeWidth);
				printf("\n");
			}
		} else if(cmd==0x15) { 						// CMD_15_END_FILE
			if(detail >= 2) printf("\n");
			if(dumpSVGFooter(fout)) {
				break;
			}
			break; 		// we've finished
		} else { 		// unknown cmd 
			printf("\n");
		}
	}

	int error = 0;

	if(svgdump) {
		if(svgDumpState==DUMPSTATE_AFTER_FOOTER) {
			uint8_t temp;
			bo_fread(&temp,1,1,f);

			if(!feof(f)) {
				printf("warning : additional data past CMD_15_END_FILE marker\n");
			}
		} else {
			error = 1;
		}
	}
	
	fclose(f);
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



