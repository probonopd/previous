/*
  Hatari - options.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Functions for showing and parsing all of Hatari's command line options.
  
  To add a new option:
  - Add option ID to the enum
  - Add the option information to HatariOptions[]
  - Add required actions for that ID to switch in Opt_ParseParameters()
*/
const char Options_fileid[] = "Hatari options.c : " __DATE__ " " __TIME__;

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <SDL.h>

#include "main.h"
#include "options.h"
#include "configuration.h"
#include "control.h"
#include "debugui.h"
#include "file.h"
#include "screen.h"
#include "video.h"
#include "log.h"
#include "paths.h"

#include "hatari-glue.h"

static bool bNoSDLParachute;

/*  List of supported options. */
enum {
	OPT_HEADER,	/* options section header */
	OPT_HELP,		/* general options */
	OPT_VERSION,
	OPT_CONFIRMQUIT,
	OPT_CONFIGFILE,
	OPT_KEYMAPFILE,
	OPT_MONITOR,
	OPT_FULLSCREEN,
	OPT_WINDOW,
	OPT_GRAB,
	OPT_STATUSBAR,
	OPT_DRIVE_LED,
	OPT_PRINTER,
	OPT_WRITEPROT_HD,
	OPT_MEMSIZE,		/* memory options */
	OPT_MEMSTATE,
	OPT_CPULEVEL,		/* CPU options */
	OPT_CPUCLOCK,
	OPT_COMPATIBLE,

	OPT_CPU_CYCLE_EXACT,	/* WinUAE CPU/FPU/bus options */
	OPT_CPU_ADDR24,
	OPT_FPU_TYPE,
	OPT_FPU_COMPATIBLE,
	OPT_MMU,

	OPT_MACHINE,		/* system options */
	OPT_REALTIME,
	OPT_DSP,
	OPT_MICROPHONE,
	OPT_SOUND,
	OPT_SOUNDBUFFERSIZE,
	OPT_RTC,
	OPT_DEBUG,		/* debug options */
	OPT_TRACE,
	OPT_TRACEFILE,
	OPT_PARSE,
	OPT_SAVECONFIG,
	OPT_PARACHUTE,
	OPT_CONTROLSOCKET,
	OPT_LOGFILE,
	OPT_LOGLEVEL,
	OPT_ALERTLEVEL,
	OPT_ERROR,
	OPT_CONTINUE
};

typedef struct {
	unsigned int id;	/* option ID */
	const char *chr;	/* short option */
	const char *str;	/* long option */
	const char *arg;	/* type name for argument, if any */
	const char *desc;	/* option description */
} opt_t;

/* it's easier to edit these if they are kept in the same order as the enums */
static const opt_t HatariOptions[] = {
	
	{ OPT_HEADER, NULL, NULL, NULL, "General" },
	{ OPT_HELP,      "-h", "--help",
	  NULL, "Print this help text and exit" },
	{ OPT_VERSION,   "-v", "--version",
	  NULL, "Print version number and exit" },
	{ OPT_CONFIRMQUIT, NULL, "--confirm-quit",
	  "<bool>", "Whether Hatari confirms quit" },
	{ OPT_CONFIGFILE, "-c", "--configfile",
	  "<file>", "Use <file> instead of the default hatari config file" },
	{ OPT_KEYMAPFILE, "-k", "--keymap",
	  "<file>", "Read (additional) keyboard mappings from <file>" },
    
	{ OPT_HEADER, NULL, NULL, NULL, "Common display" },
	{ OPT_MONITOR,      NULL, "--monitor",
	  "<x>", "Select monitor type (x = mono/rgb/vga/tv)" },
	{ OPT_FULLSCREEN,"-f", "--fullscreen",
	  NULL, "Start emulator in fullscreen mode" },
	{ OPT_WINDOW,    "-w", "--window",
	  NULL, "Start emulator in window mode" },
	{ OPT_GRAB, NULL, "--grab",
	  NULL, "Grab mouse (also) in window mode" },
	{ OPT_STATUSBAR, NULL, "--statusbar",
	  "<bool>", "Show statusbar (floppy leds etc)" },
	{ OPT_DRIVE_LED,   NULL, "--drive-led",
	  "<bool>", "Show overlay drive led when statusbar isn't shown" },

	{ OPT_HEADER, NULL, NULL, NULL, "Devices" },
	{ OPT_PRINTER,   NULL, "--printer",
	  "<file>", "Enable printer support and write data to <file>" },
	
	{ OPT_HEADER, NULL, NULL, NULL, "Disk" },
	{ OPT_WRITEPROT_HD, NULL, "--protect-hd",
	  "<x>", "Write protect harddrive <dir> contents (on/off/auto)" },
	
	{ OPT_HEADER, NULL, NULL, NULL, "Memory" },
	{ OPT_MEMSIZE,   "-s", "--memsize",
	  "<x>", "ST RAM size (x = size in MiB from 0 to 14, 0 = 512KiB)" },
	{ OPT_MEMSTATE,   NULL, "--memstate",
	  "<file>", "Load memory snap-shot <file>" },
	
	{ OPT_HEADER, NULL, NULL, NULL, "CPU" },
	{ OPT_CPULEVEL,  NULL, "--cpulevel",
	  "<x>", "Set the CPU type (x => 680x0) (EmuTOS/TOS 2.06 only!)" },
	{ OPT_CPUCLOCK,  NULL, "--cpuclock",
	  "<x>", "Set the CPU clock (x = 8/16/32)" },
	{ OPT_COMPATIBLE, NULL, "--compatible",
	  "<bool>", "Use a more compatible (but slower) 68000 CPU mode" },

	{ OPT_HEADER, NULL, NULL, NULL, "WinUAE CPU/FPU/bus" },
	{ OPT_CPU_CYCLE_EXACT, NULL, "--cpu-exact",
	  "<bool>", "Use cycle exact CPU emulation" },
	{ OPT_CPU_ADDR24, NULL, "--addr24",
	  "<bool>", "Use 24-bit instead of 32-bit addressing mode" },
	{ OPT_FPU_TYPE, NULL, "--fpu-type",
	  "<x>", "FPU type (x=none/68881/68882/internal)" },
	{ OPT_FPU_COMPATIBLE, NULL, "--fpu-compatible",
	  "<bool>", "Use more compatible, but slower FPU emulation" },
	{ OPT_MMU, NULL, "--mmu",
	  "<bool>", "Use MMU emulation" },

	{ OPT_HEADER, NULL, NULL, NULL, "Misc system" },
	{ OPT_REALTIME,   NULL, "--realtime",
	  "<bool>", "Use host realtime sources" },
	{ OPT_DSP,       NULL, "--dsp",
	  "<x>", "DSP emulation (x = none/dummy/emu)" },
	{ OPT_MICROPHONE,   NULL, "--mic",
	  "<bool>", "Enable/disable microphone" },
	{ OPT_SOUND,   NULL, "--sound",
	  "<x>", "Sound frequency (x=off/6000-50066, off=fastest)" },
	{ OPT_SOUNDBUFFERSIZE,   NULL, "--sound-buffer-size",
	  "<x>", "Sound buffer size for SDL in ms (x=0/10-100, 0=default)" },
	{ OPT_RTC,    NULL, "--rtc",
	  "<bool>", "Enable real-time clock" },

	{ OPT_HEADER, NULL, NULL, NULL, "Debug" },
	{ OPT_DEBUG,     "-D", "--debug",
	  NULL, "Toggle whether CPU exceptions invoke debugger" },
	{ OPT_TRACE,   NULL, "--trace",
	  "<trace1,...>", "Activate emulation tracing, see '--trace help'" },
	{ OPT_TRACEFILE, NULL, "--trace-file",
	  "<file>", "Save trace output to <file> (default=stderr)" },
	{ OPT_PARSE, NULL, "--parse",
	  "<file>", "Parse/execute debugger commands from <file>" },
	{ OPT_SAVECONFIG, NULL, "--saveconfig",
	  NULL, "Save current Hatari configuration and exit" },
	{ OPT_PARACHUTE, NULL, "--no-parachute",
	  NULL, "Disable SDL parachute to get Hatari core dumps" },
#if HAVE_UNIX_DOMAIN_SOCKETS
	{ OPT_CONTROLSOCKET, NULL, "--control-socket",
	  "<file>", "Hatari reads options from given socket at run-time" },
#endif
	{ OPT_LOGFILE, NULL, "--log-file",
	  "<file>", "Save log output to <file> (default=stderr)" },
	{ OPT_LOGLEVEL, NULL, "--log-level",
	  "<x>", "Log output level (x=debug/todo/info/warn/error/fatal)" },
	{ OPT_ALERTLEVEL, NULL, "--alert-level",
	  "<x>", "Show dialog for log messages above given level" },

	{ OPT_ERROR, NULL, NULL, NULL, NULL }
};


/**
 * Show version string and license.
 */
static void Opt_ShowVersion(void)
{
	printf("\n" PROG_NAME
	       " - the NeXT emulator.\n\n");
	printf(PROG_NAME " is free software licensed under the GNU General"
	       " Public License.\n\n");
}


/**
 * Calculate option + value len
 */
static unsigned int Opt_OptionLen(const opt_t *opt)
{
	unsigned int len;
	len = strlen(opt->str);
	if (opt->arg)
	{
		len += strlen(opt->arg);
		len += 1;
		/* with arg, short options go to another line */
	}
	else
	{
		if (opt->chr)
		{
			/* ' or -c' */
			len += 6;
		}
	}
	return len;
}


/**
 * Show single option
 */
static void Opt_ShowOption(const opt_t *opt, unsigned int maxlen)
{
	char buf[64];
	if (!maxlen)
	{
		maxlen = Opt_OptionLen(opt);
	}
	assert(maxlen < sizeof(buf));
	if (opt->arg)
	{
		sprintf(buf, "%s %s", opt->str, opt->arg);
		printf("  %-*s %s\n", maxlen, buf, opt->desc);
		if (opt->chr)
		{
			printf("    or %s %s\n", opt->chr, opt->arg);
		}
	}
	else
	{
		if (opt->chr)
		{
			sprintf(buf, "%s or %s", opt->str, opt->chr);
			printf("  %-*s %s\n", maxlen, buf, opt->desc);
		}
		else
		{
			printf("  %-*s %s\n", maxlen, opt->str, opt->desc);
		}
	}
}

/**
 * Show options for section starting from 'start_opt',
 * return next option after this section.
 */
static const opt_t *Opt_ShowHelpSection(const opt_t *start_opt)
{
	const opt_t *opt, *last;
	unsigned int len, maxlen = 0;
	const char *previous = NULL;

	/* find longest option name and check option IDs */
	for (opt = start_opt; opt->id != OPT_HEADER && opt->id != OPT_ERROR; opt++)
	{
		len = Opt_OptionLen(opt);
		if (len > maxlen)
		{
			maxlen = len;
		}
	}
	last = opt;
	
	/* output all options */
	for (opt = start_opt; opt != last; opt++)
	{
		if (previous != opt->str)
		{
			Opt_ShowOption(opt, maxlen);
		}
		previous = opt->str;
	}
	return last;
}


/**
 * Show help text.
 */
static void Opt_ShowHelp(void)
{
	const opt_t *opt = HatariOptions;

	Opt_ShowVersion();
	printf("Usage:\n previous [options] [directory|disk image|Atari program]\n");

	while(opt->id != OPT_ERROR)
	{
		if (opt->id == OPT_HEADER)
		{
			assert(opt->desc);
			printf("\n%s options:\n", opt->desc);
			opt++;
		}
		opt = Opt_ShowHelpSection(opt);
	}
	printf("\nSpecial option values:\n");
	printf("<bool>\tDisable by using 'n', 'no', 'off', 'false', or '0'\n");
	printf("\tEnable by using 'y', 'yes', 'on', 'true' or '1'\n");
	printf("<file>\tDevices accept also special 'stdout' and 'stderr' file names\n");
	printf("\t(if you use stdout for midi or printer, set log to stderr).\n");
	printf("\tSetting the file to 'none', disables given device or disk\n");
}


/**
 * Show Hatari version and usage.
 * If 'error' given, show that error message.
 * If 'optid' != OPT_ERROR, tells for which option the error is,
 * otherwise 'value' is show as the option user gave.
 * Return false if error string was given, otherwise true
 */
static bool Opt_ShowError(unsigned int optid, const char *value, const char *error)
{
	const opt_t *opt;

	Opt_ShowVersion();
	printf("Usage:\n hatari [options] [disk image name]\n\n"
	       "Try option \"-h\" or \"--help\" to display more information.\n");

	if (error)
	{
		if (optid == OPT_ERROR)
		{
			fprintf(stderr, "\nError: %s (%s)\n", error, value);
		}
		else
		{
			for (opt = HatariOptions; opt->id != OPT_ERROR; opt++)
			{
				if (optid == opt->id)
					break;
			}
			if (value != NULL)
			{
				fprintf(stderr,
					"\nError while parsing argument \"%s\" for option \"%s\":\n"
					"  %s\n", value, opt->str, error);
			}
			else
			{
				fprintf(stderr, "\nError (%s): %s\n", opt->str, error);
			}
			fprintf(stderr, "\nOption usage:\n");
			Opt_ShowOption(opt, 0);
		}
		return false;
	}
	return true;
}


/**
 * If 'conf' given, set it:
 * - true if given option 'arg' is y/yes/on/true/1
 * - false if given option 'arg' is n/no/off/false/0
 * Return false for any other value, otherwise true
 */
static bool Opt_Bool(const char *arg, int optid, bool *conf)
{
	const char *enablers[] = { "y", "yes", "on", "true", "1", NULL };
	const char *disablers[] = { "n", "no", "off", "false", "0", NULL };
	const char **bool_str, *orig = arg;
	char *input, *str;

	input = strdup(arg);
	str = input;
	while (*str)
	{
		*str++ = tolower(*arg++);
	}
	for (bool_str = enablers; *bool_str; bool_str++)
	{
		if (strcmp(input, *bool_str) == 0)
		{
			free(input);
			if (conf)
			{
				*conf = true;
			}
			return true;
		}
	}
	for (bool_str = disablers; *bool_str; bool_str++)
	{
		if (strcmp(input, *bool_str) == 0)
		{
			free(input);
			if (conf)
			{
				*conf = false;
			}
			return true;
		}
	}
	free(input);
	return Opt_ShowError(optid, orig, "Not a <bool> value");
}


/**
 * checks str argument agaist options of type "--option<digit>".
 * If match is found, returns ID for that, otherwise OPT_CONTINUE
 * and OPT_ERROR for errors.
 */
static int Opt_CheckBracketValue(const opt_t *opt, const char *str)
{
	const char *bracket, *optstr;
	size_t offset;
	int digit, i;

	if (!opt->str)
	{
		return OPT_CONTINUE;
	}
	bracket = strchr(opt->str, '<');
	if (!bracket)
	{
		return OPT_CONTINUE;
	}
	offset = bracket - opt->str;
	if (strncmp(opt->str, str, offset) != 0)
	{
		return OPT_CONTINUE;
	}
	digit = str[offset] - '0';
	if (digit < 0 || digit > 9)
	{
		return OPT_CONTINUE;
	}
	optstr = opt->str;
	for (i = 0; opt->str == optstr; opt++, i++)
	{
		if (i == digit)
		{
			return opt->id;
		}
	}
	/* fprintf(stderr, "opt: %s (%d), str: %s (%d), digit: %d\n",
		opt->str, offset+1, str, strlen(str), digit);
	 */
	return OPT_ERROR;
}


/**
 * matches string under given index in the argv against all Hatari
 * short and long options. If match is found, returns ID for that,
 * otherwise shows help and returns OPT_ERROR.
 * 
 * Checks also that if option is supposed to have argument,
 * whether there's one.
 */
static int Opt_WhichOption(int argc, const char * const argv[], int idx)
{
	const opt_t *opt;
	const char *str = argv[idx];
	int id;

	for (opt = HatariOptions; opt->id != OPT_ERROR; opt++)
	{	
		/* exact option name matches? */
		if (!((opt->str && !strcmp(str, opt->str)) ||
		      (opt->chr && !strcmp(str, opt->chr))))
		{
			/* no, maybe name<digit> matches? */
			id = Opt_CheckBracketValue(opt, str);
			if (id == OPT_CONTINUE)
			{
				continue;
			}
			if (id == OPT_ERROR)
			{
				break;
			}
		}
		/* matched, check args */
		if (opt->arg)
		{
			if (idx+1 >= argc)
			{
				Opt_ShowError(opt->id, NULL, "Missing argument");
				return OPT_ERROR;
			}
			/* early check for bools */
			if (strcmp(opt->arg, "<bool>") == 0)
			{
				if (!Opt_Bool(argv[idx+1], opt->id, NULL))
				{
					return OPT_ERROR;
				}
			}
		}
		return opt->id;
	}
	Opt_ShowError(OPT_ERROR, argv[idx], "Unrecognized option");
	return OPT_ERROR;
}


/**
 * If 'checkexits' is true, assume 'src' is a file and check whether it
 * exists before copying 'src' to 'dst'. Otherwise just copy option src
 * string to dst.
 * If a pointer to (bool) 'option' is given, set that option to true.
 * - However, if src is "none", leave dst unmodified & set option to false.
 *   ("none" is used to disable options related to file arguments)
 * Return false if there were errors, otherwise true
 */
static bool Opt_StrCpy(int optid, bool checkexist, char *dst, const char *src, size_t dstlen, bool *option)
{
	if (option)
	{
		*option = false;
		if(strcasecmp(src, "none") == 0)
		{
			return true;
		}
	}
	if (strlen(src) >= dstlen)
	{
		return Opt_ShowError(optid, src, "File name too long!");
	}
	if (checkexist && !File_Exists(src))
	{
		return Opt_ShowError(optid, src, "Given file doesn't exist (or has wrong file permissions)!");
	}
	if (option)
	{
		*option = true;
	}
	strcpy(dst, src);
	return true;
}


/**
 * Return SDL_INIT_NOPARACHUTE flag if user requested SDL parachute
 * to be disabled to get proper Hatari core dumps.  By default returns
 * zero so that SDL parachute will be used to restore video mode on
 * unclean Hatari termination.
 */
Uint32 Opt_GetNoParachuteFlag(void)
{
	if (bNoSDLParachute) {
		return SDL_INIT_NOPARACHUTE;
	}
	return 0;
}


/**
 * Handle last (non-option) argument.  It can be a path or filename.
 * Filename can be a disk image or Atari program.
 * Return false if it's none of these.
 */
static bool Opt_HandleArgument(const char *path)
{
	char *dir = NULL;
	Uint8 test[2];
	FILE *fp;

	/* Atari program? */
	if (File_Exists(path) && (fp = fopen(path, "rb"))) {

		/* file starts with GEMDOS magic? */
		if (fread(test, 1, 2, fp) == 2 &&
		    test[0] == 0x60 && test[1] == 0x1A) {

			const char *prgname = strrchr(path, PATHSEP);
			if (prgname) {
				dir = strdup(path);
				dir[prgname-path] = '\0';
				prgname++;
			} else {
				dir = strdup(Paths_GetWorkingDir());
				prgname = path;
			}
			/* after above, dir should point to valid dir,
			 * then make sure that given program from that
			 * dir will be started.
			 */
//			TOS_AutoStart(prgname);
		}
		fclose(fp);
	}
	if (dir) {
		path = dir;
	}

	/* GEMDOS HDD directory (as argument, or for the Atari program)? */
	if (File_DirExists(path)) {
//		if (Opt_StrCpy(OPT_HARDDRIVE, false, ConfigureParams.HardDisk.szHardDiskDirectories[0],
//			       path, sizeof(ConfigureParams.HardDisk.szHardDiskDirectories[0]),
//			       &ConfigureParams.HardDisk.bUseHardDiskDirectories)
//		    && ConfigureParams.HardDisk.bUseHardDiskDirectories)
		{
//			ConfigureParams.SCSI.bBootFromHardDisk = true;
		}
		if (dir) {
			free(dir);
		}
		return true;
	} else {
		if (dir) {
			/* if dir is set, it should be valid... */
			Log_Printf(LOG_ERROR, "Given atari program path '%s' doesn't exist (anymore?)!\n", dir);
			free(dir);
			exit(1);
		}
	}

	return Opt_ShowError(OPT_ERROR, path, "Not a disk image, Atari program or directory");
}

/**
 * parse all Hatari command line options and set Hatari state accordingly.
 * Returns true if everything was OK, false otherwise.
 */
bool Opt_ParseParameters(int argc, const char * const argv[])
{
	int ncpu, cpuclock, memsize, freq, temp;
	const char *errstr;
	int i, ok = true;

	for(i = 1; i < argc; i++)
	{
		/* last argument can be a non-option */
		if (argv[i][0] != '-' && i+1 == argc)
			return Opt_HandleArgument(argv[i]);
    
		/* WhichOption() checks also that there is an argument,
		 * so we don't need to check that below
		 */
		switch(Opt_WhichOption(argc, argv, i))
		{
		
			/* general options */
		case OPT_HELP:
			Opt_ShowHelp();
			return false;
			
		case OPT_VERSION:
			Opt_ShowVersion();
			return false;

		case OPT_CONFIRMQUIT:
			ok = Opt_Bool(argv[++i], OPT_CONFIRMQUIT, &ConfigureParams.Log.bConfirmQuit);
			break;
			
		case OPT_CONFIGFILE:
			i += 1;
			/* true -> file needs to exist */
			ok = Opt_StrCpy(OPT_CONFIGFILE, true, sConfigFileName,
					argv[i], sizeof(sConfigFileName), NULL);
			if (ok)
			{
				Configuration_Load(NULL);
			}
			break;
		
			/* common display options */

		case OPT_MONITOR:
			i += 1;
			if (strcasecmp(argv[i], "mono") == 0)
			{
//				ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_MONO;
			}
			else if (strcasecmp(argv[i], "rgb") == 0)
			{
//				ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_RGB;
			}
			else if (strcasecmp(argv[i], "vga") == 0)
			{
//				ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_VGA;
			}
			else if (strcasecmp(argv[i], "tv") == 0)
			{
//				ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_TV;
			}
			else
			{
				return Opt_ShowError(OPT_MONITOR, argv[i], "Unknown monitor type");
			}
			break;
			
		case OPT_FULLSCREEN:
			ConfigureParams.Screen.bFullScreen = true;
			break;
			
		case OPT_WINDOW:
			ConfigureParams.Screen.bFullScreen = false;
			break;

		case OPT_GRAB:
			bGrabMouse = true;
			break;
						
		case OPT_STATUSBAR:
			ok = Opt_Bool(argv[++i], OPT_STATUSBAR, &ConfigureParams.Screen.bShowStatusbar);
			break;
			
		case OPT_DRIVE_LED:
			ok = Opt_Bool(argv[++i], OPT_DRIVE_LED, &ConfigureParams.Screen.bShowDriveLed);
			break;
									
		case OPT_PRINTER:
			i += 1;
			/* "none" can be used to disable printer */
			ok = Opt_StrCpy(OPT_PRINTER, false, ConfigureParams.Printer.szPrintToFileName,
					argv[i], sizeof(ConfigureParams.Printer.szPrintToFileName),
					&ConfigureParams.Printer.bPrinterConnected);
			break;
			
			/* disk options */

		case OPT_WRITEPROT_HD:
			i += 1;
			if (strcasecmp(argv[i], "off") == 0)
				ConfigureParams.SCSI.nWriteProtection = WRITEPROT_OFF;
			else if (strcasecmp(argv[i], "on") == 0)
				ConfigureParams.SCSI.nWriteProtection = WRITEPROT_ON;
			else if (strcasecmp(argv[i], "auto") == 0)
				ConfigureParams.SCSI.nWriteProtection = WRITEPROT_AUTO;
			else
				return Opt_ShowError(OPT_WRITEPROT_HD, argv[i], "Unknown option value");
			break;
			
			/* Memory options */
		case OPT_MEMSIZE:
			memsize = atoi(argv[++i]);
			if (memsize < 0 || memsize > 14)
			{
				return Opt_ShowError(OPT_MEMSIZE, argv[i], "Invalid memory size");
			}
//			ConfigureParams.Memory.nMemorySize = memsize;
			break;
      			
			/* CPU options */
		case OPT_CPULEVEL:
			/* UAE core uses cpu_level variable */
			ncpu = atoi(argv[++i]);
			if(ncpu < 0 || ncpu > 4)
			{
				return Opt_ShowError(OPT_CPULEVEL, argv[i], "Invalid CPU level");
			}
			ConfigureParams.System.nCpuLevel = ncpu;
			break;
			
		case OPT_CPUCLOCK:
			cpuclock = atoi(argv[++i]);
			if(cpuclock != 8 && cpuclock != 16 && cpuclock != 32)
			{
				return Opt_ShowError(OPT_CPUCLOCK, argv[i], "Invalid CPU clock");
			}
			ConfigureParams.System.nCpuFreq = cpuclock;
			break;
			
		case OPT_COMPATIBLE:
			ok = Opt_Bool(argv[++i], OPT_COMPATIBLE, &ConfigureParams.System.bCompatibleCpu);
			break;

			/* system options */
		case OPT_MACHINE:
			i += 1;
			if (strcasecmp(argv[i], "st") == 0)
			{
				ConfigureParams.System.nCpuLevel = 0;
				ConfigureParams.System.nCpuFreq = 8;
			}
			else if (strcasecmp(argv[i], "ste") == 0)
			{
				ConfigureParams.System.nCpuLevel = 0;
				ConfigureParams.System.nCpuFreq = 8;
			}
			else if (strcasecmp(argv[i], "tt") == 0)
			{
				ConfigureParams.System.nCpuLevel = 3;
				ConfigureParams.System.nCpuFreq = 32;
			}
			else if (strcasecmp(argv[i], "falcon") == 0)
			{
#if ENABLE_DSP_EMU
		ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
#endif
				ConfigureParams.System.nCpuLevel = 3;
				ConfigureParams.System.nCpuFreq = 16;
			}
			else
			{
				return Opt_ShowError(OPT_MACHINE, argv[i], "Unknown machine type");
			}
			break;
			
		case OPT_REALTIME:
			ok = Opt_Bool(argv[++i], OPT_REALTIME, &ConfigureParams.System.bRealtime);
			break;
						
		case OPT_RTC:
			ok = Opt_Bool(argv[++i], OPT_RTC, &ConfigureParams.System.bRealTimeClock);
			break;			

		case OPT_DSP:
			i += 1;
			if (strcasecmp(argv[i], "none") == 0)
			{
				ConfigureParams.System.nDSPType = DSP_TYPE_NONE;
			}
			else if (strcasecmp(argv[i], "dummy") == 0)
			{
				ConfigureParams.System.nDSPType = DSP_TYPE_DUMMY;
			}
			else if (strcasecmp(argv[i], "emu") == 0)
			{
#if ENABLE_DSP_EMU
				ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
#else
				return Opt_ShowError(OPT_DSP, argv[i], "DSP type 'emu' support not compiled in");
#endif
			}
			else
			{
				return Opt_ShowError(OPT_DSP, argv[i], "Unknown DSP type");
			}
			break;

		case OPT_FPU_TYPE:
			i += 1;
			if (strcasecmp(argv[i], "none") == 0)
			{
				ConfigureParams.System.n_FPUType = FPU_NONE;
			}
			else if (strcasecmp(argv[i], "68881") == 0)
			{
				ConfigureParams.System.n_FPUType = FPU_68881;
			}
			else if (strcasecmp(argv[i], "68882") == 0)
			{
				ConfigureParams.System.n_FPUType = FPU_68882;
			}
			else if (strcasecmp(argv[i], "internal") == 0)
			{
				ConfigureParams.System.n_FPUType = FPU_CPU;
			}
			else
			{
				return Opt_ShowError(OPT_FPU_TYPE, argv[i], "Unknown FPU type");
			}
			break;

		case OPT_FPU_COMPATIBLE:
			ok = Opt_Bool(argv[++i], OPT_FPU_COMPATIBLE, &ConfigureParams.System.bCompatibleFPU);
			break;			

		case OPT_MMU:
			ok = Opt_Bool(argv[++i], OPT_MMU, &ConfigureParams.System.bMMU);
			break;

		case OPT_SOUND:
			i += 1;
			if (strcasecmp(argv[i], "off") == 0)
			{
				ConfigureParams.Sound.bEnableSound = false;
			}
			else
			{
				freq = atoi(argv[i]);
				if (freq != 44100)
				{
					return Opt_ShowError(OPT_SOUND, argv[i], "Unsupported sound frequency, only 44100 supported");
				}
				ConfigureParams.Sound.bEnableSound = true;
			}
			break;

		case OPT_SOUNDBUFFERSIZE:
			i += 1;
			temp = atoi(argv[i]);
			if ( temp == 0 )			/* use default setting for SDL */
				;
			else if (temp < 10 || temp > 100)
				{
					return Opt_ShowError(OPT_SOUNDBUFFERSIZE, argv[i], "Unsupported sound buffer size");
				}
//			ConfigureParams.Sound.SdlAudioBufferSize = temp;
			break;
			
		case OPT_MICROPHONE:
			ok = Opt_Bool(argv[++i], OPT_MICROPHONE, &ConfigureParams.Sound.bEnableMicrophone);
			break;			

		case OPT_KEYMAPFILE:
			i += 1;
			ok = Opt_StrCpy(OPT_KEYMAPFILE, true, ConfigureParams.Keyboard.szMappingFileName,
					argv[i], sizeof(ConfigureParams.Keyboard.szMappingFileName),
					NULL);
			if (ok)
			{
				ConfigureParams.Keyboard.nKeymapType = KEYMAP_LOADED;
			}
			break;
			
			/* debug options */
		case OPT_DEBUG:
			if (bExceptionDebugging)
			{
				fprintf(stderr, "Exception debugging disabled.\n");
				bExceptionDebugging = false;
			}
			else
			{
				fprintf(stderr, "Exception debugging enabled.\n");
				bExceptionDebugging = true;
			}
			break;

		case OPT_PARACHUTE:
			bNoSDLParachute = true;
			break;

			
		case OPT_TRACE:
			i += 1;
			errstr = Log_SetTraceOptions(argv[i]);
			if (errstr)
			{
				if (!errstr[0]) {
					/* silent parsing termination */
					return false;
				}
				return Opt_ShowError(OPT_TRACE, argv[i], errstr);
			}
			break;

		case OPT_TRACEFILE:
			i += 1;
			ok = Opt_StrCpy(OPT_TRACEFILE, false, ConfigureParams.Log.sTraceFileName,
					argv[i], sizeof(ConfigureParams.Log.sTraceFileName),
					NULL);
			break;

		case OPT_CONTROLSOCKET:
			i += 1;
			errstr = Control_SetSocket(argv[i]);
			if (errstr)
			{
				return Opt_ShowError(OPT_CONTROLSOCKET, argv[i], errstr);
			}
			break;

		case OPT_LOGFILE:
			i += 1;
			ok = Opt_StrCpy(OPT_LOGFILE, false, ConfigureParams.Log.sLogFileName,
					argv[i], sizeof(ConfigureParams.Log.sLogFileName),
					NULL);
			break;

		case OPT_PARSE:
			i += 1;
			ok = DebugUI_SetParseFile(argv[i]);
			break;

		case OPT_SAVECONFIG:
			/* Hatari-UI needs Hatari config to start */
			Configuration_Save();
			exit(0);
			break;

		case OPT_LOGLEVEL:
			i += 1;
			ConfigureParams.Log.nTextLogLevel = Log_ParseOptions(argv[i]);
			if (ConfigureParams.Log.nTextLogLevel == LOG_NONE)
			{
				return Opt_ShowError(OPT_LOGLEVEL, argv[i], "Unknown log level!");
			}
			break;

		case OPT_ALERTLEVEL:
			i += 1;
			ConfigureParams.Log.nAlertDlgLogLevel = Log_ParseOptions(argv[i]);
			if (ConfigureParams.Log.nAlertDlgLogLevel == LOG_NONE)
			{
				return Opt_ShowError(OPT_ALERTLEVEL, argv[i], "Unknown alert level!");
			}
			break;
		       
		case OPT_ERROR:
			/* unknown option or missing option parameter */
			return false;

		default:
			return Opt_ShowError(OPT_ERROR, argv[i], "Internal Hatari error, unhandled option");
		}
		if (!ok)
		{
			/* Opt_Bool() or Opt_StrCpy() failed */
			return false;
		}
	}

	return true;
}

/**
 * Readline match callback for option name completion.
 * STATE = 0 -> different text from previous one.
 * Return next match or NULL if no matches.
 */
char *Opt_MatchOption(const char *text, int state)
{
	static int i, len;
	const char *name;
	
	if (!state) {
		/* first match */
		len = strlen(text);
		i = 0;
	}
	/* next match */
	while (i < ARRAYSIZE(HatariOptions)) {
		name = HatariOptions[i++].str;
		if (name && strncasecmp(name, text, len) == 0)
			return (strdup(name));
	}
	return NULL;
}
