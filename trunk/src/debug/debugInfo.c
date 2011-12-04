/*
  Hatari - debuginfo.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  debuginfo.c - functions needed to show info about the atari HW & OS
   components and "lock" that info to be shown on entering the debugger.
*/
const char DebugInfo_fileid[] = "Hatari debuginfo.c : " __DATE__ " " __TIME__;

#include <stdio.h>
#include <assert.h>
#include "main.h"
#include "configuration.h"
#include "debugInfo.h"
#include "debugcpu.h"
#include "debugui.h"
#include "evaluate.h"
#include "file.h"
#include "ioMem.h"
#include "m68000.h"
#include "nextMemory.h"
#include "screen.h"
#include "video.h"


/* ------------------------------------------------------------------
 * TOS information
 */
#define OS_SYSBASE 0x4F2
#define OS_HEADER_SIZE 0x30

#define COOKIE_JAR 0x5A0

#define BASEPAGE_SIZE 0x100

#define GEM_MAGIC 0x87654321
#define GEM_MUPB_SIZE 0xC

#define RESET_MAGIC 0x31415926
#define RESET_VALID 0x426
#define RESET_VECTOR 0x42A

#define COUNTRY_SPAIN 4

/**
 * DebugInfo_GetSysbase: set osversion to given argument.
 * return sysbase address on success and zero on failure.
 */
static Uint32 DebugInfo_GetSysbase(Uint16 *osversion)
{
	return 00;
}

/**
 * DebugInfo_CurrentBasepage: get currently running TOS program basepage
 */
static Uint32 DebugInfo_CurrentBasepage(void)
{
	return 0;
}

/**
 * GetSegmentAddress: return segment address at given offset in
 * TOS process basepage or zero if that is missing/invalid.
 */
static Uint32 GetSegmentAddress(unsigned offset)
{
    Uint32 basepage = DebugInfo_CurrentBasepage();
    if (!basepage) {
        return 0;
    }
    if (!NEXTMemory_ValidArea(basepage, BASEPAGE_SIZE) ||
        NEXTMemory_ReadLong(basepage) != basepage) {
        fprintf(stderr, "Basepage address 0x%06x is invalid!\n", basepage);
        return 0;
    }
    return NEXTMemory_ReadLong(basepage+offset);
}

/**
 * DebugInfo_GetTEXT: return current program TEXT segment address
 * or zero if basepage missing/invalid.  For virtual debugger variable.
 */
Uint32 DebugInfo_GetTEXT(void)
{
    return GetSegmentAddress(0x08);
}
/**
 * DebugInfo_GetDATA: return current program DATA segment address
 * or zero if basepage missing/invalid.  For virtual debugger variable.
 */
Uint32 DebugInfo_GetDATA(void)
{
    return GetSegmentAddress(0x010);
}
/**
 * DebugInfo_GetBSS: return current program BSS segment address
 * or zero if basepage missing/invalid.  For virtual debugger variable.
 */
Uint32 DebugInfo_GetBSS(void)
{
    return GetSegmentAddress(0x18);
}

/**
 * DebugInfo_Basepage: show TOS process basepage information
 * at given address.
 */
static void DebugInfo_Basepage(Uint32 basepage)
{
}

/**
 * DebugInfo_OSHeader: display TOS OS Header
 */
static void DebugInfo_OSHeader(Uint32 dummy)
{
}


/* ------------------------------------------------------------------
 * Next HW information
 */

/**
 * DebugInfo_Rtc : display the Videl registers values.
 */
static void DebugInfo_Rtc(Uint32 dummy)
{
	Screen_Draw();
	fprintf(stdout,"%s",get_rtc_ram_info());
}

/**
 * DebugInfo_Crossbar : display the Crossbar registers values.
 */
static void DebugInfo_Crossbar(Uint32 dummy)
{
}


/* ------------------------------------------------------------------
 * CPU and DSP information wrappers
 */

/**
 * Helper to call debugcpu.c and debugdsp.c debugger commands
 */
static void DebugInfo_CallCommand(int (*func)(int, char* []), const char *command, Uint32 arg)
{
	char cmdbuffer[16], argbuffer[12];
	char *argv[] = { cmdbuffer, NULL };
	int argc = 1;

	assert(strlen(command) < sizeof(cmdbuffer));
	strcpy(cmdbuffer, command);
	if (arg) {
		sprintf(argbuffer, "$%x", arg);
		argv[argc++] = argbuffer;
	}
	func(argc, argv);
}

static void DebugInfo_CpuRegister(Uint32 arg)
{
	DebugInfo_CallCommand(DebugCpu_Register, "register", arg);
}
static void DebugInfo_CpuDisAsm(Uint32 arg)
{
	DebugInfo_CallCommand(DebugCpu_DisAsm, "disasm", arg);
}
static void DebugInfo_CpuMemDump(Uint32 arg)
{
	DebugInfo_CallCommand(DebugCpu_MemDump, "memdump", arg);
}

#if ENABLE_DSP_EMU

static void DebugInfo_DspRegister(Uint32 arg)
{
}
static void DebugInfo_DspDisAsm(Uint32 arg)
{
}

static void DebugInfo_DspMemDump(Uint32 arg)
{
}

/**
 * Convert arguments to Uint32 arg suitable for DSP memdump callback
 */
static Uint32 DebugInfo_DspMemArgs(int argc, char *argv[])
{
	Uint32 value;
	char space;
	if (argc != 2) {
		return 0;
	}
	space = toupper(argv[0][0]);
	if ((space != 'X' && space != 'Y' && space != 'P') || argv[0][1]) {
		fprintf(stderr, "ERROR: invalid DSP address space '%s'!\n", argv[0]);
		return 0;
	}
	if (!Eval_Number(argv[1], &value) || value > 0xffff) {
		fprintf(stderr, "ERROR: invalid DSP address '%s'!\n", argv[1]);
		return 0;
	}
	return ((Uint32)space<<16) | value;
}

#endif  /* ENABLE_DSP_EMU */


static void DebugInfo_RegAddr(Uint32 arg)
{
}

/**
 * Convert arguments to Uint32 arg suitable for RegAddr callback
 */
static Uint32 DebugInfo_RegAddrArgs(int argc, char *argv[])
{
	Uint32 value, *regaddr;
	if (argc != 2) {
		return 0;
	}

	if (strcmp(argv[0], "disasm") == 0) {
		value = 'D';
	} else if (strcmp(argv[0], "memdump") == 0) {
		value = 'M';
	} else {
		fprintf(stderr, "ERROR: regaddr operation can be only 'disasm' or 'memdump', not '%s'!\n", argv[0]);
		return 0;
	}

	if (strlen(argv[1]) != 2 ||
	    (!DebugCpu_GetRegisterAddress(argv[1], &regaddr) &&
	     (toupper(argv[1][0]) != 'R' || !isdigit(argv[1][1]) || argv[1][2]))) {
		/* not CPU register or Rx DSP register */
		fprintf(stderr, "ERROR: invalid address/data register '%s'!\n", argv[1]);
		return 0;
	}
	
	value |= argv[1][0] << 24;
	value |= argv[1][1] << 16;
	value &= 0xffff00ff;
	return value;
}


/* ------------------------------------------------------------------
 * wrappers for command to parse debugger input file
 */

/* file name to be given before calling the Parse function,
 * needs to be set separately as it's a host pointer which
 * can be 64-bit i.e. may not fit into Uint32.
 */
static char *parse_filename;

/**
 * Parse and exec commands in the previously given debugger input file
 */
static void DebugInfo_FileParse(Uint32 dummy)
{
    if (parse_filename) {
        DebugUI_ParseFile(parse_filename);
    } else {
        fputs("ERROR: debugger input file name to parse isn't set!\n", stderr);
    }
}

/**
 * Set which input file to parse.
 * Return true if file exists, false on error
 */
static Uint32 DebugInfo_FileArgs(int argc, char *argv[])
{
    if (argc != 1) {
        return false;
    }
    if (!File_Exists(argv[0])) {
        fprintf(stderr, "ERROR: given file '%s' doesn't exist!\n", argv[0]);
        return false;
    }
    if (parse_filename) {
        free(parse_filename);
    }
    parse_filename = strdup(argv[0]);
    return true;
}

/* ------------------------------------------------------------------
 * Debugger & readline TAB completion integration
 */

/**
 * Default information on entering the debugger
 */
static void DebugInfo_Default(Uint32 dummy)
{
	int hbl, fcycles, lcycles;
	Video_GetPosition(&fcycles, &hbl, &lcycles);
	fprintf(stderr, "\nCPU=$%x, VBL=%d, FrameCycles=%d, HBL=%d, LineCycles=%d, DSP=",
		M68000_GetPC(), 0, fcycles, hbl, lcycles);
		fprintf(stderr, "N/A\n");
}

static const struct {
	/* if overlaps with other functionality, list only for lock command */
	bool lock;
	const char *name;
	void (*func)(Uint32 arg);
	/* convert args in argv into single Uint32 for func */
	Uint32 (*args)(int argc, char *argv[]);
	const char *info;
} infotable[] = {
//    { false,"aes",       AES_Info,             NULL, "Show AES vector contents (with <value>, show opcodes)" },
	{ false,"basepage",  DebugInfo_Basepage,   NULL, "Show program basepage info at given <address>" },
//    { false,"cookiejar", DebugInfo_Cookiejar,  NULL, "Show TOS Cookiejar contents" },
	{ false,"crossbar",  DebugInfo_Crossbar,   NULL, "Show Falcon crossbar HW register values" },
	{ true, "default",   DebugInfo_Default,    NULL, "Show default debugger entry information" },
	{ true, "disasm",    DebugInfo_CpuDisAsm,  NULL, "Disasm CPU from PC or given <address>" },
#if ENABLE_DSP_EMU
	{ true, "dspdisasm", DebugInfo_DspDisAsm,  NULL, "Disasm DSP from given <address>" },
	{ true, "dspmemdump",DebugInfo_DspMemDump, DebugInfo_DspMemArgs, "Dump DSP memory from given <space> <address>" },
	{ true, "dspregs",   DebugInfo_DspRegister,NULL, "Show DSP registers values" },
#endif
    { true, "file",      DebugInfo_FileParse, DebugInfo_FileArgs, "Parse commands from given debugger input <file>" },
//    { false,"gemdos",    GemDOS_Info,          NULL, "Show GEMDOS HDD emu info (with <value>, show opcodes)" },
	{ true, "memdump",   DebugInfo_CpuMemDump, NULL, "Dump CPU memory from given <address>" },
	{ false,"osheader",  DebugInfo_OSHeader,   NULL, "Show TOS OS header information" },
	{ true, "regaddr",   DebugInfo_RegAddr, DebugInfo_RegAddrArgs, "Show <disasm|memdump> from CPU/DSP address pointed by <register>" },
	{ true, "registers", DebugInfo_CpuRegister,NULL, "Show CPU registers values" },
//    { false,"video",     DebugInfo_Video,      NULL, "Show Video related values" },
	{ false,"rtc",     DebugInfo_Rtc,      NULL, "Show Next's RTC registers" }
};

static int LockedFunction = 4; /* index for the "default" function */
static Uint32 LockedArgument;

/**
 * Show selected debugger session information
 * (when debugger is (again) entered)
 */
void DebugInfo_ShowSessionInfo(void)
{
	infotable[LockedFunction].func(LockedArgument);
}


/**
 * Readline match callback for info subcommand name completion.
 * STATE = 0 -> different text from previous one.
 * Return next match or NULL if no matches.
 */
static char *DebugInfo_Match(const char *text, int state, bool lock)
{
	static int i, len;
	const char *name;
	
	if (!state) {
		/* first match */
		len = strlen(text);
		i = 0;
	}
	/* next match */
	while (i++ < ARRAYSIZE(infotable)) {
		if (!lock && infotable[i-1].lock) {
			continue;
		}
		name = infotable[i-1].name;
		if (strncmp(name, text, len) == 0)
			return (strdup(name));
	}
	return NULL;
}
char *DebugInfo_MatchLock(const char *text, int state)
{
	return DebugInfo_Match(text, state, true);
}
char *DebugInfo_MatchInfo(const char *text, int state)
{
	return DebugInfo_Match(text, state, false);
}


/**
 * Show requested command information.
 */
int DebugInfo_Command(int nArgc, char *psArgs[])
{
	Uint32 value;
	const char *cmd;
	bool ok, lock;
	int i, sub;

	sub = -1;
	if (nArgc > 1) {
		cmd = psArgs[1];		
		/* which subcommand? */
		for (i = 0; i < ARRAYSIZE(infotable); i++) {
			if (strcmp(cmd, infotable[i].name) == 0) {
				sub = i;
				break;
			}
		}
	}

	if (infotable[sub].args) {
		/* value needs callback specific conversion */
		value = infotable[sub].args(nArgc-2, psArgs+2);
		ok = !!value;
	} else {
		if (nArgc > 2) {
			/* value is normal number */
			ok = Eval_Number(psArgs[2], &value);
		} else {
			value = 0;
			ok = true;
		}
	}

	lock = (strcmp(psArgs[0], "lock") == 0);
	
	if (sub < 0 || !ok) {
		/* no subcommand or something wrong with value, show info */
		fprintf(stderr, "%s subcommands are:\n", psArgs[0]);
		for (i = 0; i < ARRAYSIZE(infotable); i++) {
			if (!lock && infotable[i].lock) {
				continue;
			}
			fprintf(stderr, "- %s: %s\n",
				infotable[i].name, infotable[i].info);
		}
		return DEBUGGER_CMDDONE;
	}

	if (lock) {
		/* lock given subcommand and value */
		LockedFunction = sub;
		LockedArgument = value;
		fprintf(stderr, "Locked %s output.\n", psArgs[1]);
	} else {
		/* do actual work */
		infotable[sub].func(value);
	}
	return DEBUGGER_CMDDONE;
}
