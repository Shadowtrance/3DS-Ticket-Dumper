#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#include <3ds/util/utf.h>

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

static const wchar_t *nandPathString = L"/../";
static u8 utf8[0x106];

static FS_path nandPath = {
	.data = 0,
	.type = PATH_WCHAR,
	.size = 0
};

int (*kernelCallback)(void) = NULL;
int backdoorReturn = 0;

u32 myPid = 0;

u32 curr_kproc_addr				= 0;
u32 kproc_start						= 0;
u32 kproc_size						= 0;
u32 kproc_num							= 0;
u32 kproc_codeset_offset	= 0;
u32 kproc_pid_offset			= 0;

static s32 __attribute__((naked)) kernelBackdoorWrapper(){
	__asm__ __volatile__ (
		"cpsid aif								\t\n"
		"push {r4, lr}						\t\n"
		"ldr r0, =kernelCallback	\t\n"
		"ldr r0, [r0]							\t\n"
		"blx r0										\t\n"
		"ldr r4, =backdoorReturn	\t\n"
		"str r0, [r4]							\t\n"
		"pop {r4, pc}							\t\n"
	);
}

int kernelBackdoor(int (*callback)(void)){
	kernelCallback = callback;
	svcBackdoor(kernelBackdoorWrapper);
	return backdoorReturn;
}

int patchPid(){
	*(u32*)(curr_kproc_addr + kproc_pid_offset) = 0;
	return 0;
}

int unpatchPid(){
	*(u32*)(curr_kproc_addr + kproc_pid_offset) = myPid;
	return 0;
}

void reinitSrv(){
	srvExit();
	srvInit();
}

Result patchServices(){
	svcGetProcessId(&myPid, 0xFFFF8001);
	kernelBackdoor(patchPid);
	reinitSrv();
	u32 currentPid;
	svcGetProcessId(&currentPid, 0xFFFF8001);
	kernelBackdoor(unpatchPid);
	
	return currentPid != 0;
}

void findKProcStart(){
	// Get the vtable* of the current application's KProcess.
	// The vtable* is the first u32 in the KProcess struct, and
	// it's constant between all KProcesses.
	u32 kproc_vtable_ptr = *(u32*)curr_kproc_addr;
	
	u32 itr_kproc_addr;
	
	for(itr_kproc_addr = curr_kproc_addr;; itr_kproc_addr -= kproc_size){
		u32 itr_kproc_vtable_ptr = *(u32*)itr_kproc_addr;
		if(itr_kproc_vtable_ptr != kproc_vtable_ptr){
			// If the current iteration's vtable* doesn't match, we
			// overran the KProcess list.
			kproc_start = itr_kproc_addr + kproc_size;
			return;
		}
	}
}

int scanKProcList(){
	curr_kproc_addr = *(u32*)0xFFFF9004;
	findKProcStart();
}

void saveConstants(){
	u32 kversion = *(vu32*)0x1FF80000; // KERNEL_VERSION register
	
	u8 is_n3ds = 0;
	APT_CheckNew3DS(NULL, &is_n3ds);
	
	if(kversion < 0x022C0600){
		kproc_size = 0x260;
		kproc_num  = 0x2C; // TODO: Verify
		kproc_codeset_offset = 0xA8;
		kproc_pid_offset     = 0xAC;
	}else if(!is_n3ds){
		kproc_size = 0x268;
		kproc_num  = 0x2C; // TODO: Verify
		kproc_codeset_offset = 0xB0;
		kproc_pid_offset     = 0xB4;
	}else{
		kproc_size = 0x270;
		kproc_num  = 0x2F; // TODO: Verify
		kproc_codeset_offset = 0xB8;
		kproc_pid_offset     = 0xBC;
	}
	kernelBackdoor(scanKProcList);
}

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
	nandPath.size = (wcslen(nandPathString) + 1) * 2;
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
		
		if(entryCount == 0){
			break;
		}
		
		utf16_to_utf8(utf8, dirent.name, 0x106);
		printf("/%s\n", utf8);
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
	Result res;
	
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
	
	printf("%sPatching service access...%s\n", TEXT_GREEN, TEXT_RESET);
	saveConstants();
	res = patchServices();
	if(res){
		printf("%sPatching failed!%s\n", TEXT_RED, TEXT_RESET);
		while(aptMainLoop()){if(hidKeysDown()){break;}}
	}
	
	// Main loop
	while (aptMainLoop() && res == 0){
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
