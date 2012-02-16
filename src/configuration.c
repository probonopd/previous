/*
  Hatari - configuration.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Configuration File

  The configuration file is now stored in an ASCII format to allow the user
  to edit the file manually.
*/
const char Configuration_fileid[] = "Hatari configuration.c : " __DATE__ " " __TIME__;

#include <SDL_keysym.h>

#include "main.h"
#include "configuration.h"
#include "cfgopts.h"
#include "file.h"
#include "log.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "paths.h"
#include "screen.h"
#include "video.h"
#include "avi_record.h"
#include "clocks_timings.h"


CNF_PARAMS ConfigureParams;                 /* List of configuration for the emulator */
char sConfigFileName[FILENAME_MAX];         /* Stores the name of the configuration file */


/* Used to load/save logging options */
static const struct Config_Tag configs_Log[] =
{
	{ "sLogFileName", String_Tag, ConfigureParams.Log.sLogFileName },
	{ "sTraceFileName", String_Tag, ConfigureParams.Log.sTraceFileName },
	{ "nTextLogLevel", Int_Tag, &ConfigureParams.Log.nTextLogLevel },
	{ "nAlertDlgLogLevel", Int_Tag, &ConfigureParams.Log.nAlertDlgLogLevel },
	{ "bConfirmQuit", Bool_Tag, &ConfigureParams.Log.bConfirmQuit },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save debugger options */
static const struct Config_Tag configs_Debugger[] =
{
	{ "nNumberBase", Int_Tag, &ConfigureParams.Debugger.nNumberBase },
	{ "nDisasmLines", Int_Tag, &ConfigureParams.Debugger.nDisasmLines },
	{ "nMemdumpLines", Int_Tag, &ConfigureParams.Debugger.nMemdumpLines },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save screen options */
static const struct Config_Tag configs_Screen[] =
{
	{ "nMonitorType", Int_Tag, &ConfigureParams.Screen.nMonitorType },
//	{ "nFrameSkips", Int_Tag, &ConfigureParams.Screen.nFrameSkips },
	{ "bFullScreen", Bool_Tag, &ConfigureParams.Screen.bFullScreen },
    { "bKeepResolution", Bool_Tag, &ConfigureParams.Screen.bKeepResolution },
	{ "bAllowOverscan", Bool_Tag, &ConfigureParams.Screen.bAllowOverscan },
	{ "nSpec512Threshold", Int_Tag, &ConfigureParams.Screen.nSpec512Threshold },
	{ "nForceBpp", Int_Tag, &ConfigureParams.Screen.nForceBpp },
	{ "bAspectCorrect", Bool_Tag, &ConfigureParams.Screen.bAspectCorrect },
	{ "bUseExtVdiResolutions", Bool_Tag, &ConfigureParams.Screen.bUseExtVdiResolutions },
	{ "nVdiWidth", Int_Tag, &ConfigureParams.Screen.nVdiWidth },
	{ "nVdiHeight", Int_Tag, &ConfigureParams.Screen.nVdiHeight },
	{ "nVdiColors", Int_Tag, &ConfigureParams.Screen.nVdiColors },
	{ "bShowStatusbar", Bool_Tag, &ConfigureParams.Screen.bShowStatusbar },
	{ "bShowDriveLed", Bool_Tag, &ConfigureParams.Screen.bShowDriveLed },
	{ "bCrop", Bool_Tag, &ConfigureParams.Screen.bCrop },
	{ "nMaxWidth", Int_Tag, &ConfigureParams.Screen.nMaxWidth },
	{ "nMaxHeight", Int_Tag, &ConfigureParams.Screen.nMaxHeight },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save keyboard options */
static const struct Config_Tag configs_Keyboard[] =
{
	{ "bDisableKeyRepeat", Bool_Tag, &ConfigureParams.Keyboard.bDisableKeyRepeat },
	{ "nKeymapType", Int_Tag, &ConfigureParams.Keyboard.nKeymapType },
	{ "szMappingFileName", String_Tag, ConfigureParams.Keyboard.szMappingFileName },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save shortcut key bindings with modifiers options */
static const struct Config_Tag configs_ShortCutWithMod[] =
{
	{ "keyOptions",    Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_OPTIONS] },
	{ "keyFullScreen", Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_FULLSCREEN] },
	{ "keyMouseMode",  Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_MOUSEGRAB] },
	{ "keyColdReset",  Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_COLDRESET] },
	{ "keyWarmReset",  Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_WARMRESET] },
	{ "keyScreenShot", Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_SCREENSHOT] },
	{ "keyBossKey",    Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_BOSSKEY] },
	{ "keyCursorEmu",  Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_CURSOREMU] },
	{ "keyFastForward",Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_FASTFORWARD] },
	{ "keyRecAnim",    Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_RECANIM] },
	{ "keyRecSound",   Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_RECSOUND] },
	{ "keySound",      Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_SOUND] },
	{ "keyPause",      Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_PAUSE] },
	{ "keyDebugger",   Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_DEBUG] },
	{ "keyQuit",       Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_QUIT] },
	{ "keyLoadMem",    Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_LOADMEM] },
	{ "keySaveMem",    Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_SAVEMEM] },
	{ "keyInsertDiskA",Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_INSERTDISKA] },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save shortcut key bindings without modifiers options */
static const struct Config_Tag configs_ShortCutWithoutMod[] =
{
	{ "keyOptions",    Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_OPTIONS] },
	{ "keyFullScreen", Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_FULLSCREEN] },
	{ "keyMouseMode",  Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_MOUSEGRAB] },
	{ "keyColdReset",  Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_COLDRESET] },
	{ "keyWarmReset",  Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_WARMRESET] },
	{ "keyScreenShot", Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_SCREENSHOT] },
	{ "keyBossKey",    Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_BOSSKEY] },
	{ "keyCursorEmu",  Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_CURSOREMU] },
	{ "keyFastForward",Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_FASTFORWARD] },
	{ "keyRecAnim",    Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_RECANIM] },
	{ "keyRecSound",   Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_RECSOUND] },
	{ "keySound",      Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_SOUND] },
	{ "keyPause",      Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_PAUSE] },
	{ "keyDebugger",   Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_DEBUG] },
	{ "keyQuit",       Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_QUIT] },
	{ "keyLoadMem",    Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_LOADMEM] },
	{ "keySaveMem",    Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_SAVEMEM] },
	{ "keyInsertDiskA",Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_INSERTDISKA] },
	{ NULL , Error_Tag, NULL }
};


/* Used to load/save sound options */
static const struct Config_Tag configs_Sound[] =
{
    { "bEnableMicrophone", Bool_Tag, &ConfigureParams.Sound.bEnableMicrophone },
  	{ "bEnableSound", Bool_Tag, &ConfigureParams.Sound.bEnableSound },
  	{ "nPlaybackFreq", Int_Tag, &ConfigureParams.Sound.nPlaybackFreq },
  	{ "nSdlAudioBufferSize", Int_Tag, &ConfigureParams.Sound.SdlAudioBufferSize },
    { "szYMCaptureFileName", String_Tag, ConfigureParams.Sound.szYMCaptureFileName },
//    { "YmVolumeMixing", Int_Tag, &ConfigureParams.Sound.YmVolumeMixing },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save memory options */
static const struct Config_Tag configs_Memory[] =
{
	{ "nMemorySize", Int_Tag, &ConfigureParams.Memory.nMemorySize },
	{ "bAutoSave", Bool_Tag, &ConfigureParams.Memory.bAutoSave },
	{ "szMemoryCaptureFileName", String_Tag, ConfigureParams.Memory.szMemoryCaptureFileName },
	{ "szAutoSaveFileName", String_Tag, ConfigureParams.Memory.szAutoSaveFileName },
	{ NULL , Error_Tag, NULL }
};


/* Used to load/save floppy options */
static const struct Config_Tag configs_Floppy[] =
{
	{ "bAutoInsertDiskB", Bool_Tag, &ConfigureParams.DiskImage.bAutoInsertDiskB },
	{ "bSlowFloppy", Bool_Tag, &ConfigureParams.DiskImage.bSlowFloppy },
	{ "nWriteProtection", Int_Tag, &ConfigureParams.DiskImage.nWriteProtection },
	{ "szDiskAZipPath", String_Tag, ConfigureParams.DiskImage.szDiskZipPath[0] },
	{ "szDiskAFileName", String_Tag, ConfigureParams.DiskImage.szDiskFileName[0] },
	{ "szDiskBZipPath", String_Tag, ConfigureParams.DiskImage.szDiskZipPath[1] },
	{ "szDiskBFileName", String_Tag, ConfigureParams.DiskImage.szDiskFileName[1] },
	{ "szDiskImageDirectory", String_Tag, ConfigureParams.DiskImage.szDiskImageDirectory },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save HD options */
static const struct Config_Tag configs_HardDisk[] =
{
    { "szSCSIDiskImage0", String_Tag, ConfigureParams.HardDisk.szSCSIDiskImage0 },
    { "bSCSIImageAttached0", Bool_Tag, &ConfigureParams.HardDisk.bSCSIImageAttached0 },
    { "bCDROM0", Bool_Tag, &ConfigureParams.HardDisk.bCDROM0 },
    
    { "szSCSIDiskImage1", String_Tag, ConfigureParams.HardDisk.szSCSIDiskImage1 },
    { "bSCSIImageAttached1", Bool_Tag, &ConfigureParams.HardDisk.bSCSIImageAttached1 },
    { "bCDROM1", Bool_Tag, &ConfigureParams.HardDisk.bCDROM1 },
    
    { "szSCSIDiskImage2", String_Tag, ConfigureParams.HardDisk.szSCSIDiskImage2 },
    { "bSCSIImageAttached2", Bool_Tag, &ConfigureParams.HardDisk.bSCSIImageAttached2 },
    { "bCDROM2", Bool_Tag, &ConfigureParams.HardDisk.bCDROM2 },
    
    { "szSCSIDiskImage3", String_Tag, ConfigureParams.HardDisk.szSCSIDiskImage3 },
    { "bSCSIImageAttached3", Bool_Tag, &ConfigureParams.HardDisk.bSCSIImageAttached3 },
    { "bCDROM3", Bool_Tag, &ConfigureParams.HardDisk.bCDROM3 },
    
    { "szSCSIDiskImage4", String_Tag, ConfigureParams.HardDisk.szSCSIDiskImage4 },
    { "bSCSIImageAttached4", Bool_Tag, &ConfigureParams.HardDisk.bSCSIImageAttached4 },
    { "bCDROM4", Bool_Tag, &ConfigureParams.HardDisk.bCDROM4 },
    
    { "szSCSIDiskImage5", String_Tag, ConfigureParams.HardDisk.szSCSIDiskImage5 },
    { "bSCSIImageAttached5", Bool_Tag, &ConfigureParams.HardDisk.bSCSIImageAttached5 },
    { "bCDROM5", Bool_Tag, &ConfigureParams.HardDisk.bCDROM5 },
    
    { "szSCSIDiskImage6", String_Tag, ConfigureParams.HardDisk.szSCSIDiskImage6 },
    { "bSCSIImageAttached6", Bool_Tag, &ConfigureParams.HardDisk.bSCSIImageAttached6 },
    { "bCDROM6", Bool_Tag, &ConfigureParams.HardDisk.bCDROM6 },

	{ "bBootFromHardDisk", Bool_Tag, &ConfigureParams.HardDisk.bBootFromHardDisk },
	{ "nWriteProtection", Int_Tag, &ConfigureParams.HardDisk.nWriteProtection },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save ROM options */
static const struct Config_Tag configs_Rom[] =
{
    { "szRom030FileName", String_Tag, ConfigureParams.Rom.szRom030FileName },
    { "szRom040FileName", String_Tag, ConfigureParams.Rom.szRom040FileName },
    { "szRomTurboFileName", String_Tag, ConfigureParams.Rom.szRomTurboFileName },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save RS232 options */
static const struct Config_Tag configs_Rs232[] =
{
	{ "bEnableRS232", Bool_Tag, &ConfigureParams.RS232.bEnableRS232 },
	{ "szOutFileName", String_Tag, ConfigureParams.RS232.szOutFileName },
	{ "szInFileName", String_Tag, ConfigureParams.RS232.szInFileName },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save printer options */
static const struct Config_Tag configs_Printer[] =
{
	{ "bEnablePrinting", Bool_Tag, &ConfigureParams.Printer.bEnablePrinting },
	{ "bPrintToFile", Bool_Tag, &ConfigureParams.Printer.bPrintToFile },
	{ "szPrintToFileName", String_Tag, ConfigureParams.Printer.szPrintToFileName },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save MIDI options */
static const struct Config_Tag configs_Midi[] =
{
	{ "bEnableMidi", Bool_Tag, &ConfigureParams.Midi.bEnableMidi },
	{ "sMidiInFileName", String_Tag, ConfigureParams.Midi.sMidiInFileName },
	{ "sMidiOutFileName", String_Tag, ConfigureParams.Midi.sMidiOutFileName },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save system options */
static const struct Config_Tag configs_System[] =
{
    { "nMachineType", Int_Tag, &ConfigureParams.System.nMachineType },
    { "bColor", Bool_Tag, &ConfigureParams.System.bColor },
    { "bTurbo", Bool_Tag, &ConfigureParams.System.bTurbo },
    { "bADB", Bool_Tag, &ConfigureParams.System.bADB },
    { "nSCSI", Bool_Tag, &ConfigureParams.System.nSCSI },
    { "nRTC", Bool_Tag, &ConfigureParams.System.nRTC },
    
	{ "nCpuLevel", Int_Tag, &ConfigureParams.System.nCpuLevel },
	{ "nCpuFreq", Int_Tag, &ConfigureParams.System.nCpuFreq },
	{ "bCompatibleCpu", Bool_Tag, &ConfigureParams.System.bCompatibleCpu },
	{ "bBlitter", Bool_Tag, &ConfigureParams.System.bBlitter },
	{ "nDSPType", Int_Tag, &ConfigureParams.System.nDSPType },
	{ "bRealTimeClock", Bool_Tag, &ConfigureParams.System.bRealTimeClock },
	{ "bPatchTimerD", Bool_Tag, &ConfigureParams.System.bPatchTimerD },
	{ "bFastForward", Bool_Tag, &ConfigureParams.System.bFastForward },
    
#if ENABLE_WINUAE_CPU
    { "bAddressSpace24", Bool_Tag, &ConfigureParams.System.bAddressSpace24 },
    { "bCycleExactCpu", Bool_Tag, &ConfigureParams.System.bCycleExactCpu },
    { "n_FPUType", Int_Tag, &ConfigureParams.System.n_FPUType },
    { "bCompatibleFPU", Bool_Tag, &ConfigureParams.System.bCompatibleFPU },
    { "bMMU", Bool_Tag, &ConfigureParams.System.bMMU },
#endif
    { NULL , Error_Tag, NULL }
    };

/* Used to load/save video options */
static const struct Config_Tag configs_Video[] =
{
    { "AviRecordVcodec", Int_Tag, &ConfigureParams.Video.AviRecordVcodec },
    { "AviRecordFps", Int_Tag, &ConfigureParams.Video.AviRecordFps },
    { "AviRecordFile", String_Tag, ConfigureParams.Video.AviRecordFile },
	{ NULL , Error_Tag, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Set default configuration values.
 */
void Configuration_SetDefault(void)
{
	int i;
	const char *psHomeDir;
	const char *psWorkingDir;

	psHomeDir = Paths_GetHatariHome();
	psWorkingDir = Paths_GetWorkingDir();

	/* Clear parameters */
	memset(&ConfigureParams, 0, sizeof(CNF_PARAMS));

	/* Set defaults for logging and tracing */
	strcpy(ConfigureParams.Log.sLogFileName, "stderr");
	strcpy(ConfigureParams.Log.sTraceFileName, "stderr");
	ConfigureParams.Log.nTextLogLevel = LOG_TODO;
	ConfigureParams.Log.nAlertDlgLogLevel = LOG_ERROR;
	ConfigureParams.Log.bConfirmQuit = true;

	/* Set defaults for debugger */
	ConfigureParams.Debugger.nNumberBase = 10;
	ConfigureParams.Debugger.nDisasmLines = 8;
	ConfigureParams.Debugger.nMemdumpLines = 8;

	/* Set defaults for floppy disk images */
	ConfigureParams.DiskImage.bAutoInsertDiskB = true;
	ConfigureParams.DiskImage.bSlowFloppy = false;
	ConfigureParams.DiskImage.nWriteProtection = WRITEPROT_OFF;
	for (i = 0; i < 2; i++)
	{
		ConfigureParams.DiskImage.szDiskZipPath[i][0] = '\0';
		ConfigureParams.DiskImage.szDiskFileName[i][0] = '\0';
	}
	strcpy(ConfigureParams.DiskImage.szDiskImageDirectory, psWorkingDir);
	File_AddSlashToEndFileName(ConfigureParams.DiskImage.szDiskImageDirectory);

	/* Set defaults for hard disks */
    strcpy(ConfigureParams.HardDisk.szSCSIDiskImage0, psWorkingDir);
    ConfigureParams.HardDisk.bSCSIImageAttached0 = false;
    ConfigureParams.HardDisk.bCDROM0 = false;
    
    strcpy(ConfigureParams.HardDisk.szSCSIDiskImage1, psWorkingDir);
    ConfigureParams.HardDisk.bSCSIImageAttached1 = false;
    ConfigureParams.HardDisk.bCDROM1 = false;
    
    strcpy(ConfigureParams.HardDisk.szSCSIDiskImage2, psWorkingDir);
    ConfigureParams.HardDisk.bSCSIImageAttached2 = false;
    ConfigureParams.HardDisk.bCDROM2 = false;
    
    strcpy(ConfigureParams.HardDisk.szSCSIDiskImage3, psWorkingDir);
    ConfigureParams.HardDisk.bSCSIImageAttached3 = false;
    ConfigureParams.HardDisk.bCDROM3 = false;
    
    strcpy(ConfigureParams.HardDisk.szSCSIDiskImage4, psWorkingDir);
    ConfigureParams.HardDisk.bSCSIImageAttached4 = false;
    ConfigureParams.HardDisk.bCDROM4 = false;
    
    strcpy(ConfigureParams.HardDisk.szSCSIDiskImage5, psWorkingDir);
    ConfigureParams.HardDisk.bSCSIImageAttached5 = false;
    ConfigureParams.HardDisk.bCDROM5 = false;
    
    strcpy(ConfigureParams.HardDisk.szSCSIDiskImage6, psWorkingDir);
    ConfigureParams.HardDisk.bSCSIImageAttached6 = false;
    ConfigureParams.HardDisk.bCDROM6 = false;

    
	ConfigureParams.HardDisk.bBootFromHardDisk = false;
	ConfigureParams.HardDisk.nWriteProtection = WRITEPROT_OFF;

	/* Set defaults for Keyboard */
	ConfigureParams.Keyboard.bDisableKeyRepeat = false;
	ConfigureParams.Keyboard.nKeymapType = KEYMAP_SYMBOLIC;
	strcpy(ConfigureParams.Keyboard.szMappingFileName, "");
  
	/* Set defaults for Shortcuts */
	ConfigureParams.Shortcut.withoutModifier[SHORTCUT_OPTIONS] = SDLK_F12;
	ConfigureParams.Shortcut.withoutModifier[SHORTCUT_FULLSCREEN] = SDLK_F11;
    
	ConfigureParams.Shortcut.withModifier[SHORTCUT_PAUSE] = SDLK_p;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_DEBUG] = SDLK_d;
    
	ConfigureParams.Shortcut.withModifier[SHORTCUT_OPTIONS] = SDLK_o;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_FULLSCREEN] = SDLK_f;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_MOUSEGRAB] = SDLK_m;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_COLDRESET] = SDLK_c;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_WARMRESET] = SDLK_r;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_SCREENSHOT] = SDLK_g;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_BOSSKEY] = SDLK_i;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_CURSOREMU] = SDLK_j;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_FASTFORWARD] = SDLK_x;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_RECANIM] = SDLK_a;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_RECSOUND] = SDLK_y;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_SOUND] = SDLK_s;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_QUIT] = SDLK_q;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_LOADMEM] = SDLK_l;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_SAVEMEM] = SDLK_k;
	ConfigureParams.Shortcut.withModifier[SHORTCUT_INSERTDISKA] = SDLK_1;

	/* Set defaults for Memory */
	ConfigureParams.Memory.nMemorySize = 8;     /* 8 MiB */
	ConfigureParams.Memory.bAutoSave = false;
	sprintf(ConfigureParams.Memory.szMemoryCaptureFileName, "%s%chatari.sav",
	        psHomeDir, PATHSEP);
	sprintf(ConfigureParams.Memory.szAutoSaveFileName, "%s%cauto.sav",
	        psHomeDir, PATHSEP);

	/* Set defaults for Printer */
	ConfigureParams.Printer.bEnablePrinting = false;
	ConfigureParams.Printer.bPrintToFile = true;
	sprintf(ConfigureParams.Printer.szPrintToFileName, "%s%chatari.prn",
	        psHomeDir, PATHSEP);

	/* Set defaults for RS232 */
	ConfigureParams.RS232.bEnableRS232 = false;
	strcpy(ConfigureParams.RS232.szOutFileName, "/dev/modem");
	strcpy(ConfigureParams.RS232.szInFileName, "/dev/modem");

	/* Set defaults for MIDI */
	ConfigureParams.Midi.bEnableMidi = false;
	strcpy(ConfigureParams.Midi.sMidiInFileName, "/dev/snd/midiC1D0");
	strcpy(ConfigureParams.Midi.sMidiOutFileName, "/dev/snd/midiC1D0");

	/* Set defaults for Screen */
	ConfigureParams.Screen.bFullScreen = false;
    ConfigureParams.Screen.bKeepResolution = true;
//	ConfigureParams.Screen.nFrameSkips = AUTO_FRAMESKIP_LIMIT;
	ConfigureParams.Screen.bAllowOverscan = true;
	ConfigureParams.Screen.nSpec512Threshold = 16;
	ConfigureParams.Screen.nForceBpp = 0;
	ConfigureParams.Screen.bAspectCorrect = true;
	ConfigureParams.Screen.nMonitorType = MONITOR_TYPE_RGB;
	ConfigureParams.Screen.bUseExtVdiResolutions = false;
	ConfigureParams.Screen.bShowStatusbar = true;
	ConfigureParams.Screen.bShowDriveLed = true;
	ConfigureParams.Screen.bCrop = false;
	/* target 800x600 screen with statusbar out of screen */
	ConfigureParams.Screen.nMaxWidth = 0;
	ConfigureParams.Screen.nMaxHeight = 0;

	/* Set defaults for Sound */
    ConfigureParams.Sound.bEnableMicrophone = true;
	ConfigureParams.Sound.bEnableSound = true;
	ConfigureParams.Sound.nPlaybackFreq = 44100;
	sprintf(ConfigureParams.Sound.szYMCaptureFileName, "%s%chatari.wav",
	        psWorkingDir, PATHSEP);
	ConfigureParams.Sound.SdlAudioBufferSize = 0;
//  ConfigureParams.Sound.YmVolumeMixing = YM_TABLE_MIXING;

	/* Set defaults for Rom */
    sprintf(ConfigureParams.Rom.szRom030FileName, "%s%cRev_1.0_v41.BIN",
            Paths_GetWorkingDir(), PATHSEP);
    sprintf(ConfigureParams.Rom.szRom040FileName, "%s%cRev_2.5_v66.BIN",
            Paths_GetWorkingDir(), PATHSEP);
    sprintf(ConfigureParams.Rom.szRomTurboFileName, "%s%cRev_3.3_v74.BIN",
            Paths_GetWorkingDir(), PATHSEP);


	/* Set defaults for System */
    ConfigureParams.System.nMachineType = NEXT_CUBE030;
    ConfigureParams.System.bColor = false;
    ConfigureParams.System.bTurbo = false;
    ConfigureParams.System.bADB = false;
    ConfigureParams.System.nSCSI = NCR53C90;
    ConfigureParams.System.nRTC = MC68HC68T1;
    
	ConfigureParams.System.nCpuLevel = 3;
	ConfigureParams.System.nCpuFreq = 25;
	ConfigureParams.System.bCompatibleCpu = true;
	ConfigureParams.System.bBlitter = false;
	ConfigureParams.System.nDSPType = DSP_TYPE_NONE;
	ConfigureParams.System.bPatchTimerD = true;
	ConfigureParams.System.bRealTimeClock = true;
	ConfigureParams.System.bFastForward = false;
    
#if ENABLE_WINUAE_CPU
    ConfigureParams.System.bAddressSpace24 = false;
    ConfigureParams.System.bCycleExactCpu = false;
    ConfigureParams.System.n_FPUType = FPU_68882;
    ConfigureParams.System.bCompatibleFPU = true;
    ConfigureParams.System.bMMU = true;
#endif

    /* Set defaults for Video */
#if HAVE_LIBPNG
    ConfigureParams.Video.AviRecordVcodec = AVI_RECORD_VIDEO_CODEC_PNG;
#else
    ConfigureParams.Video.AviRecordVcodec = AVI_RECORD_VIDEO_CODEC_BMP;
#endif
    ConfigureParams.Video.AviRecordFps = 0;			/* automatic FPS */
    sprintf(ConfigureParams.Video.AviRecordFile, "%s%chatari.avi", psWorkingDir, PATHSEP);


	/* Initialize the configuration file name */
	if (strlen(psHomeDir) < sizeof(sConfigFileName)-13)
		sprintf(sConfigFileName, "%s%cprevious.cfg", psHomeDir, PATHSEP);
	else
		strcpy(sConfigFileName, "previous.cfg");

#if defined(__AMIGAOS4__)
	/* Fix default path names on Amiga OS */
	sprintf(ConfigureParams.Rom.szRom030FileName, "%sRev_1.0_v41.BIN", Paths_GetWorkingDir());
    sprintf(ConfigureParams.Rom.szRom040FileName, "%sRev_2.5_v66.BIN", Paths_GetWorkingDir());
    sprintf(ConfigureParams.Rom.szRom040FileName, "%sRev_3.3_v74.BIN", Paths_GetWorkingDir());
#endif
}


/*-----------------------------------------------------------------------*/
/**
 * Copy details from configuration structure into global variables for system,
 * clean file names, etc...  Called from main.c and dialog.c files.
 */
void Configuration_Apply(bool bReset)
{
	if (bReset)
	{
		/* Set resolution change */
	}
//	if (ConfigureParams.Screen.nFrameSkips < AUTO_FRAMESKIP_LIMIT)
//	{
//        nFrameSkips = ConfigureParams.Screen.nFrameSkips;
//	}

    /* Init clocks for this machine */
    ClocksTimings_InitMachine ( ConfigureParams.System.nMachineType );
    
    
	/* Sound settings */
	/* SDL sound buffer in ms */
//	SdlAudioBufferSize = ConfigureParams.Sound.SdlAudioBufferSize;
//	if ( SdlAudioBufferSize == 0 )			/* use default setting for SDL */
//		;
//	else if ( SdlAudioBufferSize < 10 )		/* min of 10 ms */
//		SdlAudioBufferSize = 10;
//	else if ( SdlAudioBufferSize > 100 )		/* max of 100 ms */
//		SdlAudioBufferSize = 100;

	/* Set playback frequency */
//	Audio_SetOutputAudioFreq(ConfigureParams.Sound.nPlaybackFreq);

	/* YM Mixing */
//    if ( ( ConfigureParams.Sound.YmVolumeMixing != YM_LINEAR_MIXING )
//            && ( ConfigureParams.Sound.YmVolumeMixing != YM_TABLE_MIXING ) )
//        ConfigureParams.Sound.YmVolumeMixing = YM_TABLE_MIXING;

//    YmVolumeMixing = ConfigureParams.Sound.YmVolumeMixing;
//    Sound_SetYmVolumeMixing();
    
    /* Check/constrain CPU settings and change corresponding
    * UAE cpu_level & cpu_compatible variables
    */
    M68000_CheckCpuSettings();
    
    /* Cut memory size to supported values */
    if (ConfigureParams.System.nMachineType == NEXT_CUBE030 && ConfigureParams.Memory.nMemorySize > 64)
        ConfigureParams.Memory.nMemorySize = 64;
    else if (ConfigureParams.Memory.nMemorySize > 128)
        ConfigureParams.Memory.nMemorySize = 128;
    else if (ConfigureParams.Memory.nMemorySize < 8)
        ConfigureParams.Memory.nMemorySize = 8;
    
	/* Clean file and directory names */    
    File_MakeAbsoluteName(ConfigureParams.Rom.szRom030FileName);
    File_MakeAbsoluteName(ConfigureParams.Rom.szRom040FileName);
    File_MakeAbsoluteName(ConfigureParams.Rom.szRomTurboFileName);
    
    File_MakeAbsoluteName(ConfigureParams.HardDisk.szSCSIDiskImage0);
    File_MakeAbsoluteName(ConfigureParams.HardDisk.szSCSIDiskImage1);
    File_MakeAbsoluteName(ConfigureParams.HardDisk.szSCSIDiskImage2);
    File_MakeAbsoluteName(ConfigureParams.HardDisk.szSCSIDiskImage3);
    File_MakeAbsoluteName(ConfigureParams.HardDisk.szSCSIDiskImage4);
    File_MakeAbsoluteName(ConfigureParams.HardDisk.szSCSIDiskImage5);
    File_MakeAbsoluteName(ConfigureParams.HardDisk.szSCSIDiskImage6);

    
	File_MakeAbsoluteName(ConfigureParams.Memory.szMemoryCaptureFileName);
	File_MakeAbsoluteName(ConfigureParams.Sound.szYMCaptureFileName);
	if (strlen(ConfigureParams.Keyboard.szMappingFileName) > 0)
		File_MakeAbsoluteName(ConfigureParams.Keyboard.szMappingFileName);
    File_MakeAbsoluteName(ConfigureParams.Video.AviRecordFile);
	
	/* make path names absolute, but handle special file names */
	File_MakeAbsoluteSpecialName(ConfigureParams.Log.sLogFileName);
	File_MakeAbsoluteSpecialName(ConfigureParams.Log.sTraceFileName);
	File_MakeAbsoluteSpecialName(ConfigureParams.RS232.szInFileName);
	File_MakeAbsoluteSpecialName(ConfigureParams.RS232.szOutFileName);
	File_MakeAbsoluteSpecialName(ConfigureParams.Midi.sMidiInFileName);
	File_MakeAbsoluteSpecialName(ConfigureParams.Midi.sMidiOutFileName);
	File_MakeAbsoluteSpecialName(ConfigureParams.Printer.szPrintToFileName);
}


/*-----------------------------------------------------------------------*/
/**
 * Load a settings section from the configuration file.
 */
static int Configuration_LoadSection(const char *pFilename, const struct Config_Tag configs[], const char *pSection)
{
	int ret;

	ret = input_config(pFilename, configs, pSection);

	if (ret < 0)
		fprintf(stderr, "Can not load configuration file %s (section %s).\n",
		        pFilename, pSection);

	return ret;
}


/*-----------------------------------------------------------------------*/
/**
 * Load program setting from configuration file. If psFileName is NULL, use
 * the configuration file given in configuration / last selected by user.
 */
void Configuration_Load(const char *psFileName)
{
	if (psFileName == NULL)
		psFileName = sConfigFileName;

	if (!File_Exists(psFileName))
	{
		Log_Printf(LOG_DEBUG, "Configuration file %s not found.\n", psFileName);
		return;
	}

	Configuration_LoadSection(psFileName, configs_Log, "[Log]");
	Configuration_LoadSection(psFileName, configs_Debugger, "[Debugger]");
	Configuration_LoadSection(psFileName, configs_Screen, "[Screen]");
	Configuration_LoadSection(psFileName, configs_Keyboard, "[Keyboard]");
	Configuration_LoadSection(psFileName, configs_ShortCutWithMod, "[ShortcutsWithModifiers]");
	Configuration_LoadSection(psFileName, configs_ShortCutWithoutMod, "[ShortcutsWithoutModifiers]");
	Configuration_LoadSection(psFileName, configs_Sound, "[Sound]");
	Configuration_LoadSection(psFileName, configs_Memory, "[Memory]");
	Configuration_LoadSection(psFileName, configs_Floppy, "[Floppy]");
	Configuration_LoadSection(psFileName, configs_HardDisk, "[HardDisk]");
	Configuration_LoadSection(psFileName, configs_Rom, "[ROM]");
	Configuration_LoadSection(psFileName, configs_Rs232, "[RS232]");
	Configuration_LoadSection(psFileName, configs_Printer, "[Printer]");
	Configuration_LoadSection(psFileName, configs_Midi, "[Midi]");
	Configuration_LoadSection(psFileName, configs_System, "[System]");
    Configuration_LoadSection(psFileName, configs_Video, "[Video]");
}


/*-----------------------------------------------------------------------*/
/**
 * Save a settings section to configuration file
 */
static int Configuration_SaveSection(const char *pFilename, const struct Config_Tag configs[], const char *pSection)
{
	int ret;

	ret = update_config(pFilename, configs, pSection);

	if (ret < 0)
		fprintf(stderr, "Error while updating section %s in %s\n", pSection, pFilename);

	return ret;
}


/*-----------------------------------------------------------------------*/
/**
 * Save program setting to configuration file
 */
void Configuration_Save(void)
{
	if (Configuration_SaveSection(sConfigFileName, configs_Log, "[Log]") < 0)
	{
		Log_AlertDlg(LOG_ERROR, "Error saving config file.");
		return;
	}
	Configuration_SaveSection(sConfigFileName, configs_Debugger, "[Debugger]");
	Configuration_SaveSection(sConfigFileName, configs_Screen, "[Screen]");
	Configuration_SaveSection(sConfigFileName, configs_Keyboard, "[Keyboard]");
	Configuration_SaveSection(sConfigFileName, configs_ShortCutWithMod, "[ShortcutsWithModifiers]");
	Configuration_SaveSection(sConfigFileName, configs_ShortCutWithoutMod, "[ShortcutsWithoutModifiers]");
	Configuration_SaveSection(sConfigFileName, configs_Sound, "[Sound]");
	Configuration_SaveSection(sConfigFileName, configs_Memory, "[Memory]");
	Configuration_SaveSection(sConfigFileName, configs_Floppy, "[Floppy]");
	Configuration_SaveSection(sConfigFileName, configs_HardDisk, "[HardDisk]");
	Configuration_SaveSection(sConfigFileName, configs_Rom, "[ROM]");
	Configuration_SaveSection(sConfigFileName, configs_Rs232, "[RS232]");
	Configuration_SaveSection(sConfigFileName, configs_Printer, "[Printer]");
	Configuration_SaveSection(sConfigFileName, configs_Midi, "[Midi]");
	Configuration_SaveSection(sConfigFileName, configs_System, "[System]");
    Configuration_SaveSection(sConfigFileName, configs_Video, "[Video]");
}


/*-----------------------------------------------------------------------*/
/**
 * Save/restore snapshot of configuration variables
 * ('MemorySnapShot_Store' handles type)
 */
void Configuration_MemorySnapShot_Capture(bool bSave)
{        
    MemorySnapShot_Store(ConfigureParams.Rom.szRom030FileName, sizeof(ConfigureParams.Rom.szRom030FileName));
    MemorySnapShot_Store(ConfigureParams.Rom.szRom040FileName, sizeof(ConfigureParams.Rom.szRom040FileName));
    MemorySnapShot_Store(ConfigureParams.Rom.szRomTurboFileName, sizeof(ConfigureParams.Rom.szRomTurboFileName));

	MemorySnapShot_Store(&ConfigureParams.Memory.nMemorySize, sizeof(ConfigureParams.Memory.nMemorySize));
    
    MemorySnapShot_Store(&ConfigureParams.HardDisk.bSCSIImageAttached0, sizeof(ConfigureParams.HardDisk.bSCSIImageAttached0));
    MemorySnapShot_Store(&ConfigureParams.HardDisk.bSCSIImageAttached1, sizeof(ConfigureParams.HardDisk.bSCSIImageAttached1));
    MemorySnapShot_Store(&ConfigureParams.HardDisk.bSCSIImageAttached2, sizeof(ConfigureParams.HardDisk.bSCSIImageAttached2));
    MemorySnapShot_Store(&ConfigureParams.HardDisk.bSCSIImageAttached3, sizeof(ConfigureParams.HardDisk.bSCSIImageAttached3));
    MemorySnapShot_Store(&ConfigureParams.HardDisk.bSCSIImageAttached4, sizeof(ConfigureParams.HardDisk.bSCSIImageAttached4));
    MemorySnapShot_Store(&ConfigureParams.HardDisk.bSCSIImageAttached5, sizeof(ConfigureParams.HardDisk.bSCSIImageAttached5));
    MemorySnapShot_Store(&ConfigureParams.HardDisk.bSCSIImageAttached6, sizeof(ConfigureParams.HardDisk.bSCSIImageAttached6));

    
    MemorySnapShot_Store(ConfigureParams.HardDisk.szSCSIDiskImage0, sizeof(ConfigureParams.HardDisk.szSCSIDiskImage0));
    MemorySnapShot_Store(ConfigureParams.HardDisk.szSCSIDiskImage1, sizeof(ConfigureParams.HardDisk.szSCSIDiskImage1));
    MemorySnapShot_Store(ConfigureParams.HardDisk.szSCSIDiskImage2, sizeof(ConfigureParams.HardDisk.szSCSIDiskImage2));
    MemorySnapShot_Store(ConfigureParams.HardDisk.szSCSIDiskImage3, sizeof(ConfigureParams.HardDisk.szSCSIDiskImage3));
    MemorySnapShot_Store(ConfigureParams.HardDisk.szSCSIDiskImage4, sizeof(ConfigureParams.HardDisk.szSCSIDiskImage4));
    MemorySnapShot_Store(ConfigureParams.HardDisk.szSCSIDiskImage5, sizeof(ConfigureParams.HardDisk.szSCSIDiskImage5));
    MemorySnapShot_Store(ConfigureParams.HardDisk.szSCSIDiskImage6, sizeof(ConfigureParams.HardDisk.szSCSIDiskImage6));

	MemorySnapShot_Store(&ConfigureParams.Screen.nMonitorType, sizeof(ConfigureParams.Screen.nMonitorType));
	MemorySnapShot_Store(&ConfigureParams.Screen.bUseExtVdiResolutions, sizeof(ConfigureParams.Screen.bUseExtVdiResolutions));
	MemorySnapShot_Store(&ConfigureParams.Screen.nVdiWidth, sizeof(ConfigureParams.Screen.nVdiWidth));
	MemorySnapShot_Store(&ConfigureParams.Screen.nVdiHeight, sizeof(ConfigureParams.Screen.nVdiHeight));
	MemorySnapShot_Store(&ConfigureParams.Screen.nVdiColors, sizeof(ConfigureParams.Screen.nVdiColors));

	MemorySnapShot_Store(&ConfigureParams.System.nCpuLevel, sizeof(ConfigureParams.System.nCpuLevel));
	MemorySnapShot_Store(&ConfigureParams.System.nCpuFreq, sizeof(ConfigureParams.System.nCpuFreq));
	MemorySnapShot_Store(&ConfigureParams.System.bCompatibleCpu, sizeof(ConfigureParams.System.bCompatibleCpu));
	
    
    MemorySnapShot_Store(&ConfigureParams.System.nMachineType, sizeof(ConfigureParams.System.nMachineType));
    MemorySnapShot_Store(&ConfigureParams.System.bColor, sizeof(ConfigureParams.System.bColor));
    MemorySnapShot_Store(&ConfigureParams.System.bTurbo, sizeof(ConfigureParams.System.bTurbo));
    MemorySnapShot_Store(&ConfigureParams.System.bADB, sizeof(ConfigureParams.System.bADB));
    MemorySnapShot_Store(&ConfigureParams.System.nSCSI, sizeof(ConfigureParams.System.nSCSI));
    MemorySnapShot_Store(&ConfigureParams.System.nRTC, sizeof(ConfigureParams.System.nRTC));
    
	MemorySnapShot_Store(&ConfigureParams.System.bBlitter, sizeof(ConfigureParams.System.bBlitter));
	MemorySnapShot_Store(&ConfigureParams.System.nDSPType, sizeof(ConfigureParams.System.nDSPType));
	MemorySnapShot_Store(&ConfigureParams.System.bRealTimeClock, sizeof(ConfigureParams.System.bRealTimeClock));
	MemorySnapShot_Store(&ConfigureParams.System.bPatchTimerD, sizeof(ConfigureParams.System.bPatchTimerD));
    
#if ENABLE_WINUAE_CPU
    MemorySnapShot_Store(&ConfigureParams.System.bAddressSpace24, sizeof(ConfigureParams.System.bAddressSpace24));
    MemorySnapShot_Store(&ConfigureParams.System.bCycleExactCpu, sizeof(ConfigureParams.System.bCycleExactCpu));
    MemorySnapShot_Store(&ConfigureParams.System.n_FPUType, sizeof(ConfigureParams.System.n_FPUType));
    MemorySnapShot_Store(&ConfigureParams.System.bCompatibleFPU, sizeof(ConfigureParams.System.bCompatibleFPU));
    MemorySnapShot_Store(&ConfigureParams.System.bMMU, sizeof(ConfigureParams.System.bMMU));
#endif
    
	MemorySnapShot_Store(&ConfigureParams.DiskImage.bSlowFloppy, sizeof(ConfigureParams.DiskImage.bSlowFloppy));

	if (!bSave)
		Configuration_Apply(true);
}

