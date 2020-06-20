/*	vecass - Unusual Binary Vector File Format Type 2 vector assembler
	
	Some of the "Unusual Binary Vector File Format Type 2" vector files are 
	made up of multiple layers of other vector files.
	
	e.g.
	89,93,97 are three layers of the same image, referenced from 100
	109 and 113 are two layers of the same image, referenced from 116
	
	This program will read the 'overall' file and assemble the image from the
	layer parts (which should already have been converted to SVG using ubvff2).

	Copyright 2020 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)
	
*/

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BLOCKSIZE 4096

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
    output |= (input&0x0000FF00) << 8;
    output |= (input&0x00FF0000) >> 8;
    output |= (input&0xFF000000) >> 24;
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

size_t bo_fread ( void * ptr, size_t size, size_t count, FILE * stream ) {
	// fread and perform byte order swapping if necessary
	size_t r = fread(ptr,size,count,stream);
	if(r!=count) {
		return r;
	}
	if(size==2 && systemIsLittleEndian()) {
		for(int i=0; i<count; i++) {
			((uint16_t*)ptr)[i] = reverseByteOrder2(((uint16_t*)ptr)[i]);
		}
	} else if(size==4 && systemIsLittleEndian()) {
		for(int i=0; i<count; i++) {
			((uint32_t*)ptr)[i] = reverseByteOrder4(((uint32_t*)ptr)[i]);
		}		
	}
	return r;
}

//----------------------------------------------------------------------------
//  GLOBAL VARIABLES
//----------------------------------------------------------------------------

FILE * fout = NULL;
int viewMinX=0, viewMinY=0, viewMaxX=1, viewMaxY=1;
char svgfilename[300] = "output.svg";
char prefix[256];
int depth=0;

//----------------------------------------------------------------------------
//  DUMPLIST: LIST OF INPUT FILES AND LAYERS
//----------------------------------------------------------------------------

#define MAX_DUMPLIST 100
struct DUMPLIST {
	uint16_t filenum;
	uint16_t layernum;
};
struct DUMPLIST dumpList[MAX_DUMPLIST];
int dumpListPos=0;

int addToDumpList(uint16_t filenum, uint16_t layernum) {
	dumpList[dumpListPos].filenum = filenum;
	dumpList[dumpListPos].layernum = layernum;
	dumpListPos++;
	if(dumpListPos >= MAX_DUMPLIST) {
		printf("error : dumpList overload!\n");
		return 1;
	}
	
	return 1;	
}

void sortDumpList() {			// basic bubble sort
	if(dumpListPos < 2) return;
	
	int dumpListRefs[MAX_DUMPLIST];
	for(int i=0; i<dumpListPos; i++) {
		dumpListRefs[i] = i;
	}
	
	int changed = 0;
	
	for(int reps=(dumpListPos-1); reps>0; reps--) {
		for(int i=0; i<reps; i++) {
			int a = dumpList[dumpListRefs[i]].layernum;
			int b = dumpList[dumpListRefs[i+1]].layernum;
			if(a>b) {
				int swap = dumpListRefs[i];
				dumpListRefs[i] = dumpListRefs[i+1];
				dumpListRefs[i+1] = swap;
				changed = 1;
			}
		}
	}
	
	if(changed) {				// only need to actually sort the data if it's out of order
		struct DUMPLIST outList[MAX_DUMPLIST];
		for(int i=0; i<dumpListPos; i++) { // copy the data into outList in the correct order			
			memcpy(&outList[i],&dumpList[dumpListRefs[i]],sizeof(outList[i]));			
		}
		
		for(int i=0; i<dumpListPos; i++) { // copy outList back to our actual list
			memcpy(&dumpList[i],&outList[i],sizeof(outList[i]));			
		}		
	}
}

//----------------------------------------------------------------------------
//  SETVIEWBOX: CHANGE THE VIEWBOX DIMENSIONS WITH HINDSIGHT
//----------------------------------------------------------------------------

int setViewbox(FILE * fout, int32_t minx, int32_t miny, int32_t maxx, int32_t maxy) {
	// <svg viewBox="VIEWBOX_PLACEHOLDER_1234" version...

	if(fflush(fout)!=0) {
		printf("SetViewbox: fflush failed!\n");
		return 1;
	}
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
	int r = sprintf(&buf[0],"\"%i %i %i %i\"", minx, miny, maxx, maxy);
	if(r<0) {
		printf("SetViewbox: sprintf failed\n");
		return 1;
	}
		
	// rpad with spaces
	int size1 = strlen(buf);
	if(size1<26) {
		for(int i=size1; i<26; i++) {
			buf[i]=' ';
		}
		buf[26]=0;
	}
	if(fwrite(&buf[0],1,26,fout) != 26) {
		printf("SetViewbox: fwrite failed\n");
		return 1;
	}
	if(fseek(fout,pos,SEEK_SET)!=0) {
		printf("SetViewbox: fseek failed\n");
		return 1;
	} 
	
	return 0;
}

//----------------------------------------------------------------------------
//  DUMPFROMLIST: DUMP THE REQUIRED SVG FILES WITH MODIFIED HEADER/FOOTER
//----------------------------------------------------------------------------

int dumpFromList() {
    	
	sortDumpList();

	for(int i=0; i<dumpListPos; i++) {
	
		// Open input file
		// generate filename from dumpList filenum
		char filename[300] = "";
		sprintf(filename,"%s%05u.svg",prefix,dumpList[i].filenum);
		
		FILE * fin = fopen(filename,"rb");
		if(fin==NULL) {
			printf("warning : dumpFromList: unable to open input file '%s'\n",filename);
			continue;
		}

		// Find file size
		fseek(fin,0,SEEK_END);
		uint32_t byteCount = ftell(fin);

		// What we want to do is skip the SVG header and footer
		// The header is 93+ bytes
		// The footer is 7 bytes
		byteCount -= 7; // cut off the footer

		fseek(fin,0,SEEK_SET);
		char headerbuf[150] = "";
		fread(headerbuf,1,150,fin);
		
		// before we chop off the header, we want to read in the viewbox
		int x1,y1,x2,y2;
		if(sscanf(&headerbuf[14],"%i %i %i %i",&x1,&y1,&x2,&y2) != 4) {
			printf("error : unable to read viewBox\n");
			return 1;
		}
		
		// printf("viewbox: %i %i %i %i\n",x1,y1,x2,y2);
		if(x1<viewMinX) viewMinX = x1;
		if(y1<viewMinY) viewMinY = y1;
		if(x2>viewMaxX) viewMaxX = x2;
		if(y2>viewMaxY) viewMaxY = y2;
		
		// Find the '\n' in the header
		char * nl = strchr(headerbuf,'\n');
		if(nl==NULL) { // couldn't find the newline
			printf("error : reading header\n");
			return 1;
		}
		
		int offset = nl - headerbuf + 1;
		byteCount -= offset;
		fseek(fin,offset,SEEK_SET);	
		
		// put in our lovely header instead lol
		fprintf(fout,"<g>\n");

		// Copy blocks
		char buf[BLOCKSIZE];
		for(uint32_t bytesLeft=byteCount; bytesLeft>0; ) {
			uint32_t count = BLOCKSIZE;
			if(bytesLeft<BLOCKSIZE) count = bytesLeft;

			if(fread(buf,1,count,fin) != count) {
				printf("error : dumpFromList: fread failed\n");
				return 1;
			}

			if(fwrite(buf,1,count,fout) != count) {
				printf("error : dumpFromList: fwrite failed\n");
				return 1;
			}
			bytesLeft -= count;
		}

		// Close input file
		fclose(fin);
		fprintf(fout,"</g>\n");
	}

	if(setViewbox(fout,viewMinX,viewMinY,viewMaxX,viewMaxY)) {
		printf("error : in setViewbox\n");
		return 1;
	}

    return 0;
}

//----------------------------------------------------------------------------
//  PROCESSFILE: RECURSIVE PROCESSING OF COMMANDS
//----------------------------------------------------------------------------

int processFile(char * baseFilename) {
	char spacer[80] = "";
	
	if(depth==10) {
		printf("warning : MAX DEPTH reached, not going deeper\n");
		return 0;
	}
	
	for(int i=0; i<depth; i++) {
		strcat(spacer,"    ");
	}
	
	// open command file
    FILE * fin1 = fopen(baseFilename, "rb");
    if(fin1==NULL) {
		printf("%serror : failed to open: %s\n", spacer, baseFilename);
		return 1;
    }
    fseek(fin1,0,SEEK_SET);

	// variables: states read from input file
	int32_t header[6];
	int16_t cmd;

	// Read the file header
	if(bo_fread(&header,4,3,fin1) != 3) {
		printf("%serror : read failed (header)\n",spacer);
		return 1;
	}

	// Check if file header makes sense of sorts
	if(header[0]==1) {	// This is a basic include file
		if(header[1]!=0) {
			printf("%serror : weird header\n",spacer);
			return 1;
		}
		if(depth==0) {
			printf("skip.shallow\n");
			return 0;
		}
		uint32_t data = header[2];
		uint16_t filenum = (data&0xFFFF0000)>>16;
		uint16_t layernum = (data&0xFFFF);
		printf("%sload layer %u from %05u.svg\n",spacer,layernum,filenum);	
		addToDumpList(filenum,layernum);
	} else if(header[0]<3 || header[0]>=MAX_DUMPLIST) { // generally number of commands
		printf("%sskip.type\n",spacer);
		return 1;
	} else if(header[0]==3 && depth==0) {
		printf("%sskip.three\n",spacer);
		return 1;
	} else if(header[1] == 0x48) {
		printf("%sskip.0x48\n",spacer);
		return 1;
	} else if(header[1] != 0 || header[2] != 0) { // header params check failed - probably a normal command file or points data
		printf("%sskip.not_group\n",spacer);
		return 1;
	} else {
		if(bo_fread(&header[3],4,3,fin1) != 3) {
			printf("%serror : read failed (header part 2)\n",spacer);
			return 1;
		}
		// should check  header[3] == 1

		if(depth==0) {
			fout = fopen(svgfilename, "w+b");
			printf("writing to %s\n",svgfilename);
			fprintf(fout,"%s","<svg viewBox=\"VIEWBOX_PLACEHOLDER_1234\" version=\"1.1\" baseProfile=\"full\" xmlns=\"http://www.w3.org/2000/svg\">\n");	
		}
			
		// Main input-file-reading loop
		while(!feof(fin1)) {
			if(bo_fread(&cmd,2,1,fin1) != 1) {
				break;
			}
			
			if(cmd==0) continue;
			else if(cmd==3 || cmd==4) {
				uint16_t incnum;
				if(bo_fread(&incnum,2,1,fin1) != 1) {
					printf("%serror : read failed (params)\n",spacer);
					break;
				}
				char nextFilename[300];
				sprintf(nextFilename,"%s%05u.bin",prefix,incnum);
				printf("%sinclude %s\n",spacer,nextFilename);			
				depth++;
				if(processFile(nextFilename)) {
					printf("error : failure\n");
					break;
				}
				depth--;
			}
		}
		printf("%send file\n",spacer);
		
		if(depth==0) {
			if(dumpFromList()) {
				printf("error : during dumpFromList\n");
				return 1;
			}
			fprintf(fout,"</svg>\n");
			fclose(fout);
			printf("done\n");
		}
	}		
		
	fclose(fin1);
	return 0;
}

//----------------------------------------------------------------------------
//  MAIN
//----------------------------------------------------------------------------

int main(int argc, char * argv[]) {    
    if(argc<3) {
		printf("%s","vecass: Unknown Binary Vector File Format Type 2, assemble from layers\n\n");
		printf("%s","usage: vecass cmdFile outputFile\n"
					"    cmdFile       File name of input file that contains vector assemble cmds.\n"
					"    outputFile    File name for SVG output. Can be auto.\n"
		);			
		return 0;
    }
    
	if(strlen(argv[1])>255) {
		printf("error : cmdFile name too long\n");
		return 1;
	}

	if(strlen(argv[2])>255) {
		printf("error : outputFile name too long\n");
		return 1;
	}
	
	char * filename = argv[1];
	
	// PREFIX is used for finding the *source* files of NNNNN.svg.
	strcpy(prefix, filename);
	const int l = strlen(filename);
	int done = 0;
	if(l>9) {
		int i;
		for(i=(l-9); i<(l-4); i++) {
			if(!isdigit(filename[i])) break;
		}
		if(i==(l-4) && memcmp(&filename[l-4],".bin",4)==0) {			
			// yes, we can use same prefix as the cmdFile
			prefix[l-9]=0;
			done=1;
		}
	}
	if(!done) {
		prefix[0]=0; // Just look in the current directory
	}

	// SVGFILENAME is used for creating the SVG file.
	if(memcmp(argv[2],"auto",5)==0) {
		if(l>5 && memcmp(&filename[l-4],".bin",4)==0) {
			// we'll just swap the extension
			memcpy(svgfilename,filename,l-4);
			strcat(svgfilename,".svg");
		} else {
			printf("error : unable to create auto name for outputFile\n");
			return 1;
		}
	}	
		
	return processFile(filename);
}



