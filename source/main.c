#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEXT_BLACK		"\x1b[30m"
#define TEXT_RED			"\x1b[31m"
#define TEXT_GREEN		"\x1b[32m"
#define TEXT_YELLOW		"\x1b[33m"
#define TEXT_BLUE			"\x1b[34m"
#define TEXT_MAGENTA	"\x1b[35m"
#define TEXT_CYAN			"\x1b[36m"
#define TEXT_WHITE		"\x1b[37m"
#define TEXT_RESET		"\x1b[39m"

#define BACK_BLACK		"\x1b[40m"
#define BACK_RED			"\x1b[41m"
#define BACK_GREEN		"\x1b[42m"
#define BACK_YELLOW		"\x1b[43m"
#define BACK_BLUE			"\x1b[44m"
#define BACK_MAGENTA	"\x1b[45m"
#define BACK_CYAN			"\x1b[46m"
#define BACK_WHITE		"\x1b[47m"
#define BACK_RESET		"\x1b[49m"

static FS_archive nandArchive = {
	//.id = 0x567890AB, // NAND CTR FS
	.id = ARCH_NAND_RO,
	.lowPath = {
		.type = PATH_EMPTY,
		.size = 1,
		.data = (u8*)""
	}
};

static const wchar_t *nandPathString = L"/";

static FS_path nandPath = {
	.data = 0,
	.type = PATH_WCHAR,
	.size = 4
};

void dumpTicket(void){
	Result res = 0;
	Handle nandDir;
	u32 entryCount;
	FS_dirent dirent;
	
	printf("Opening NAND...\n");
	res = FSUSER_OpenArchive(NULL, &nandArchive);
	if(res != 0){
		printf("%sError opening NAND!\n%.8lX%s\n", TEXT_RED, res, TEXT_RESET);
		return;
	}
	
	printf("Opening /...\n");
	nandPath.data = (u8 *)nandPathString;
	res = FSUSER_OpenDirectory(NULL, &nandDir, nandArchive, nandPath);
	if(res != 0){
		printf("%sError opening /!\n%.8lX%s\n", TEXT_RED, res, TEXT_RESET);
		return;
	}
	
	printf("Reading...\n");
	for(;;){
		res = FSDIR_Read(nandDir, &entryCount, 1, &dirent);
		if(res != 0){
			printf("%sError reading dir!\n%.8lX%s\n", TEXT_RED, res, TEXT_RESET);
			return;
		}
		
		printf("%lu\n", entryCount);
		if(entryCount == 0){
			break;
		}
		
		printf("%lu%ls.%4s\n", &dirent, dirent.name, dirent.shortExt);
	}
	
	printf("Closing /...\n");
	res = FSDIR_Close(nandDir);
	if(res != 0){
		printf("%sError closing /!\n%.8lX%s\n", TEXT_RED, res, TEXT_RESET);
		return;
	}
	
	printf("Closing NAND...\n");
	res = FSUSER_CloseArchive(NULL, &nandArchive);
	if(res != 0){
		printf("%sError closing NAND!\n%.8lX%s\n", TEXT_RED, res, TEXT_RESET);
		return;
	}
}

int main(int argc, char **argv){
	// Initialize services
	gfxInitDefault();
	
	PrintConsole topScreen;
	PrintConsole bottomScreen;
	
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);

	//Move the cursor to the top left corner of the screen
	consoleSelect(&topScreen);
	printf("\x1b[0;0H");

	//Print a REALLY crappy poeam with colored text
	//\x1b[cm set a SGR (Select Graphic Rendition) parameter, where c is the parameter that you want to set
	//Please refer to http://en.wikipedia.org/wiki/ANSI_escape_code#CSI_codes to see all the possible SGR parameters
	//As of now ctrulib support only these parameters:
	//Reset (0), Half bright colors (2), Reverse (7), Text color (30-37) and Background color (40-47)
	printf("%sA:%s\tDump ticket.db\n", TEXT_GREEN, TEXT_RESET);
	
	consoleSelect(&bottomScreen);
	printf("\x1b[0;0H");
	
	// Main loop
	while (aptMainLoop()){
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		if (kDown & KEY_A){
			dumpTicket();
		}

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}

	// Exit services
	gfxExit();
	
	return 0;
}
