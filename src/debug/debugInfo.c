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
#include <ctype.h>
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
 * Next HW information
 */

char* get_rtc_ram_info(void);

/**
 * DebugInfo_Rtc : display the Videl registers values.
 */
static void DebugInfo_Rtc(Uint32 dummy) {
    Update_StatusBar();
	fprintf(stdout,"%s",get_rtc_ram_info());
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
       // fputs("ERROR: debugger input file name to parse isn't set!\n", stderr);
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
	fprintf(stderr, "\nCPU=$%x, DSP=",
		M68000_GetPC());
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
	{ true, "default",   DebugInfo_Default,    NULL, "Show default debugger entry information" },
	{ true, "disasm",    DebugInfo_CpuDisAsm,  NULL, "Disasm CPU from PC or given <address>" },
#if ENABLE_DSP_EMU
	{ true, "dspdisasm", DebugInfo_DspDisAsm,  NULL, "Disasm DSP from given <address>" },
	{ true, "dspmemdump",DebugInfo_DspMemDump, DebugInfo_DspMemArgs, "Dump DSP memory from given <space> <address>" },
	{ true, "dspregs",   DebugInfo_DspRegister,NULL, "Show DSP registers values" },
#endif
    { true, "file",      DebugInfo_FileParse, DebugInfo_FileArgs, "Parse commands from given debugger input <file>" },
	{ true, "memdump",   DebugInfo_CpuMemDump, NULL, "Dump CPU memory from given <address>" },
	{ true, "regaddr",   DebugInfo_RegAddr, DebugInfo_RegAddrArgs, "Show <disasm|memdump> from CPU/DSP address pointed by <register>" },
	{ true, "registers", DebugInfo_CpuRegister,NULL, "Show CPU registers values" },
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
