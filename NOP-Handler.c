#include <proto/exec.h>

#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#include "OS4Compatibility.h"
#include "Debug.h"

struct NOPFile {
	bool open;
	bool unlimited;
	uint64_t bytesLeft;
};

struct Context {
	struct NOPFile nopFiles[16];
};

struct BCPLString {
	UBYTE length;
	UBYTE chars[];
};

struct SizedString {
	unsigned length;
	const char *chars;
};


struct ParseSizeResult {
	uint64_t value;
	unsigned length;
};

static struct DosPacket *GetPacket(struct ExecBase *SysBase, struct MsgPort *port);
static void ReplyPacket(struct ExecBase *SysBase, struct DosPacket *packet, struct MsgPort *nextReplyPort, ULONG res1, ULONG res2);
static void InitContext(struct Context *ctx);
static struct NOPFile *OpenNOPFile(struct Context *ctx, bool unlimited, uint64_t size);
static void CloseNOPFile(struct NOPFile *nopFile);
static unsigned GetNOPFilesOpen(struct Context *ctx);
static struct NOPFile *GetNOPFile(struct Context *ctx, unsigned index);
static unsigned GetNOPFileIndex(struct Context *ctx, struct NOPFile *nopFile);
static LONG ReadNOPFile(struct NOPFile *nopFile, LONG bytesToRead);
static struct SizedString AfterDevice(struct SizedString path);
static struct ParseSizeResult ParseSize(struct SizedString size);

const char Version[] = "$VER: NOP-Handler 0.3 (12.11.2025) by Patrik Axelsson";

void NOPHandler(struct ExecBase *SysBase) {
	struct Process *process = (struct Process *) FindTask(NULL);
	struct MsgPort *handlerPort = &process->pr_MsgPort;

	kprintf("Waiting for startup packet!\n");
	WaitPort(handlerPort);
	struct DosPacket *startupPacket = GetPacket(SysBase, handlerPort);
	struct DosList *dosList = (void *) BADDR(startupPacket->dp_Arg3);
	kprintf("Received startup packet!\n");
	// Single-instance handler
	dosList->dol_Task = handlerPort;
	ReplyPacket(SysBase, startupPacket, handlerPort, DOSTRUE, 0);
	kprintf("Started!\n");

	struct Context ctx;
	InitContext(&ctx);

	int running = 1;
	while (running) {
		WaitPort(handlerPort);
		struct DosPacket *packet = GetPacket(SysBase, handlerPort);
		kprintf("%s(%ld), dp_Type: %ld, dp_Arg1: 0x%lx, dp_Arg2: 0x%lx, dp_Arg3: 0x%lx\n", ActionToName(packet->dp_Type), packet->dp_Type, packet->dp_Arg1, packet->dp_Arg2, packet->dp_Arg3);
		switch (packet->dp_Type) {
			// Implementing this stops workbench from asking ACTION_LOCATE_OBJECT,
			// however it still keeps re-asking this repeatedly
			case ACTION_IS_FILESYSTEM:
				ReplyPacket(SysBase, packet, handlerPort, DOSFALSE, 0);
				break;
			case ACTION_FINDINPUT:
			case ACTION_FINDOUTPUT:
			case ACTION_FINDUPDATE: {
					kprintf("Opening: '%b'\n", packet->dp_Arg3);
					#ifdef AROS_FAST_BSTR
					const char *cPath = BADDR(packet->dp_Arg3);
					struct SizedString path = {strlen(cPath), cPath};
					#else
					struct BCPLString *bPath = BADDR(packet->dp_Arg3);
					struct SizedString path = {bPath->length, bPath->chars};
					#endif
					struct SizedString afterDevice = AfterDevice(path);
					struct ParseSizeResult parseResult = ParseSize(afterDevice);
					if (parseResult.length != afterDevice.length) {
						ReplyPacket(SysBase, packet, handlerPort, DOSFALSE, ERROR_BAD_NUMBER);
						break;
					}

					struct NOPFile *nopFile = OpenNOPFile(&ctx, 0 == parseResult.length, parseResult.value);
					if (NULL == nopFile) {
						ReplyPacket(SysBase, packet, handlerPort, DOSFALSE, ERROR_NO_FREE_STORE);
						break;
					}

					struct FileHandle *fileHandle = BADDR(packet->dp_Arg1);
					fileHandle->fh_Port = 0; // Not interactive
					fileHandle->fh_Arg1 = GetNOPFileIndex(&ctx, nopFile);
					ReplyPacket(SysBase, packet, handlerPort, DOSTRUE, 0);
					break;
				}
				break;
			case ACTION_END: {
					kprintf("Closing %lu\n", packet->dp_Arg1);
					struct NOPFile *nopFile = GetNOPFile(&ctx, packet->dp_Arg1);
					CloseNOPFile(nopFile);
					ReplyPacket(SysBase, packet, handlerPort, DOSTRUE, 0);
				}
				break;
			case ACTION_READ: {
					struct NOPFile *nopFile = GetNOPFile(&ctx, packet->dp_Arg1);
					LONG bytesToRead = packet->dp_Arg3;
					LONG bytesRead = ReadNOPFile(nopFile, bytesToRead);
					ReplyPacket(SysBase, packet, handlerPort, bytesRead, 0);
				}
				break;
			case ACTION_WRITE: {
					LONG bytesToWrite = packet->dp_Arg3;
					ReplyPacket(SysBase, packet, handlerPort, bytesToWrite, 0);
				}
				break;
			case ACTION_DIE: {
					if (0 != GetNOPFilesOpen(&ctx)) {
						ReplyPacket(SysBase, packet, NULL, DOSFALSE, ERROR_OBJECT_IN_USE);
						break;
					}
					Forbid();
					dosList->dol_Task = NULL;
					struct DosPacket *unansweredPacket;
					while ((unansweredPacket = GetPacket(SysBase, handlerPort)) != NULL) {
						ReplyPacket(SysBase, unansweredPacket, NULL, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
					}
					running = 0;
					ReplyPacket(SysBase, packet, NULL, DOSTRUE, 0);
				}
				break;
			default:
				ReplyPacket(SysBase, packet, handlerPort, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
				break;
		}
	}
	kprintf("Bye!\n");
}

static void InitContext(struct Context *ctx) {
	for (unsigned i = 0; i < ARRAY_SIZE(ctx->nopFiles); i++) {
		struct NOPFile *nopFile = &ctx->nopFiles[i];
		nopFile->open = false;
	}
}

static struct DosPacket *GetPacket(struct ExecBase *SysBase, struct MsgPort *port) {
	struct Message *message = GetMsg(port);
	return NULL != message ? (void *) message->mn_Node.ln_Name : NULL;
}

static void ReplyPacket(struct ExecBase *SysBase, struct DosPacket *packet, struct MsgPort *nextReplyPort, ULONG res1, ULONG res2) {
	struct MsgPort *toPort = packet->dp_Port;
	packet->dp_Port = nextReplyPort;
	packet->dp_Res1 = res1;
	packet->dp_Res2 = res2;
	struct Message *linkedMessage = packet->dp_Link;
	PutMsg(toPort, linkedMessage);
}

static struct NOPFile *OpenNOPFile(struct Context *ctx, bool unlimited, uint64_t size) {
	for (unsigned i = 0; i < ARRAY_SIZE(ctx->nopFiles); i++) {
		struct NOPFile *nopFile = &ctx->nopFiles[i];
		if (!nopFile->open) {
			nopFile->open = true;
			nopFile->unlimited = unlimited;
			nopFile->bytesLeft = size;
			return nopFile;
		}
	} 
	return NULL;
}

static unsigned GetNOPFilesOpen(struct Context *ctx) {
	unsigned opens = 0;
	for (unsigned i = 0; i < ARRAY_SIZE(ctx->nopFiles); i++) {
		struct NOPFile *nopFile = &ctx->nopFiles[i];
		if (nopFile->open) {
			opens++;
		}
	}
	return opens;
}

static LONG ReadNOPFile(struct NOPFile *nopFile, LONG bytesToRead) {
	if (nopFile->unlimited) {
		return bytesToRead;
	}
	if (bytesToRead > nopFile->bytesLeft) {
		bytesToRead = nopFile->bytesLeft;
	}
	nopFile->bytesLeft -= bytesToRead;
	return bytesToRead;
}

static void CloseNOPFile(struct NOPFile *nopFile) {
	if (NULL != nopFile) {
		nopFile->open = false;
	}
}

static struct NOPFile *GetNOPFile(struct Context *ctx, unsigned index) {
	return &ctx->nopFiles[index];
}

static unsigned GetNOPFileIndex(struct Context *ctx, struct NOPFile *nopFile) {
	return nopFile - ctx->nopFiles;
}

static struct SizedString AfterDevice(struct SizedString path) {
	struct SizedString start = path;
	char c;
	while (path.length--) {
		if (*path.chars++ == ':') {
			return path;
		}
	}
	return start;
}

/* Parses a string with a size consisting of a number followed by an optional
 * multyplying prefix 'K' (1024), 'M' (1048576) or G (1073741824).
 */
static struct ParseSizeResult ParseSize(struct SizedString size) {
	uint32_t number = 0;
	unsigned sizeLength;
	unsigned shiftBits = 0;
	for (sizeLength = 0; sizeLength < size.length; sizeLength++) {
		char c = size.chars[sizeLength];
		int digit = c - '0';
		if (digit < 0 || digit > 9) {
			switch (c) {
				case 'G':
					shiftBits += 10;
				case 'M':
					shiftBits += 10;
				case 'K':
					shiftBits += 10;
					sizeLength++;
			}
			break;
		}
		number = number * 10 + digit;
	}
	return (struct ParseSizeResult) {
		.value = (uint64_t) number << shiftBits,
		.length = sizeLength
	};
}
