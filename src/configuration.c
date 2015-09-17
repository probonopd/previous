/*
  Hatari - configuration.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Configuration File

  The configuration file is now stored in an ASCII format to allow the user
  to edit the file manually.
*/
const char Configuration_fileid[] = "Hatari configuration.c : " __DATE__ " " __TIME__;

#include <SDL_keyboard.h>

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

/* Used to load/save configuration dialog options */
static const struct Config_Tag configs_ConfigDialog[] =
{
    { "bShowConfigDialogAtStartup", Bool_Tag, &ConfigureParams.ConfigDialog.bShowConfigDialogAtStartup },
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

/* Used to load/save mouse options */
static const struct Config_Tag configs_Mouse[] =
{
	{ "bEnableAutoGrab", Bool_Tag, &ConfigureParams.Mouse.bEnableAutoGrab },
    { "fLinSpeedNormal", Float_Tag, &ConfigureParams.Mouse.fLinSpeedNormal },
	{ "fLinSpeedLocked", Float_Tag, &ConfigureParams.Mouse.fLinSpeedLocked },
    { "fExpSpeedNormal", Float_Tag, &ConfigureParams.Mouse.fExpSpeedNormal },
	{ "fExpSpeedLocked", Float_Tag, &ConfigureParams.Mouse.fExpSpeedLocked },
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
	{ "keyDimension",  Int_Tag, &ConfigureParams.Shortcut.withModifier[SHORTCUT_DIMENSION] },
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
	{ "keyDimension",  Int_Tag, &ConfigureParams.Shortcut.withoutModifier[SHORTCUT_DIMENSION] },
	{ NULL , Error_Tag, NULL }
};


/* Used to load/save sound options */
static const struct Config_Tag configs_Sound[] =
{
    { "bEnableMicrophone", Bool_Tag, &ConfigureParams.Sound.bEnableMicrophone },
  	{ "bEnableSound", Bool_Tag, &ConfigureParams.Sound.bEnableSound },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save memory options */
static const struct Config_Tag configs_Memory[] =
{
	{ "nMemoryBankSize0", Int_Tag, &ConfigureParams.Memory.nMemoryBankSize[0] },
    { "nMemoryBankSize1", Int_Tag, &ConfigureParams.Memory.nMemoryBankSize[1] },
	{ "nMemoryBankSize2", Int_Tag, &ConfigureParams.Memory.nMemoryBankSize[2] },
	{ "nMemoryBankSize3", Int_Tag, &ConfigureParams.Memory.nMemoryBankSize[3] },
    { "nMemorySpeed", Int_Tag, &ConfigureParams.Memory.nMemorySpeed },
	{ "bAutoSave", Bool_Tag, &ConfigureParams.Memory.bAutoSave },
	{ "szMemoryCaptureFileName", String_Tag, ConfigureParams.Memory.szMemoryCaptureFileName },
	{ "szAutoSaveFileName", String_Tag, ConfigureParams.Memory.szAutoSaveFileName },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save boot options */
static const struct Config_Tag configs_Boot[] =
{
	{ "nBootDevice", Int_Tag, &ConfigureParams.Boot.nBootDevice },
	{ "bEnableDRAMTest", Bool_Tag, &ConfigureParams.Boot.bEnableDRAMTest },
	{ "bEnablePot", Bool_Tag, &ConfigureParams.Boot.bEnablePot },
	{ "bEnableSoundTest", Bool_Tag, &ConfigureParams.Boot.bEnableSoundTest },
	{ "bEnableSCSITest", Bool_Tag, &ConfigureParams.Boot.bEnableSCSITest },
    { "bLoopPot", Bool_Tag, &ConfigureParams.Boot.bLoopPot },
	{ "bVerbose", Bool_Tag, &ConfigureParams.Boot.bVerbose },
    { "bExtendedPot", Bool_Tag, &ConfigureParams.Boot.bExtendedPot },
	{ NULL , Error_Tag, NULL }
};

/* Used to load/save SCSI options */
static const struct Config_Tag configs_SCSI[] =
{
    { "szImageName0", String_Tag, ConfigureParams.SCSI.target[0].szImageName },
    { "nDeviceType0", Int_Tag, &ConfigureParams.SCSI.target[0].nDeviceType },
    { "bDiskInserted0", Bool_Tag, &ConfigureParams.SCSI.target[0].bDiskInserted },
    { "bWriteProtected0", Bool_Tag, &ConfigureParams.SCSI.target[0].bWriteProtected },
    
    { "szImageName1", String_Tag, ConfigureParams.SCSI.target[1].szImageName },
    { "nDeviceType1", Int_Tag, &ConfigureParams.SCSI.target[1].nDeviceType },
    { "bDiskInserted1", Bool_Tag, &ConfigureParams.SCSI.target[1].bDiskInserted },
    { "bWriteProtected1", Bool_Tag, &ConfigureParams.SCSI.target[1].bWriteProtected },

    { "szImageName2", String_Tag, ConfigureParams.SCSI.target[2].szImageName },
    { "nDeviceType2", Int_Tag, &ConfigureParams.SCSI.target[2].nDeviceType },
    { "bDiskInserted2", Bool_Tag, &ConfigureParams.SCSI.target[2].bDiskInserted },
    { "bWriteProtected2", Bool_Tag, &ConfigureParams.SCSI.target[2].bWriteProtected },

    { "szImageName3", String_Tag, ConfigureParams.SCSI.target[3].szImageName },
    { "nDeviceType3", Int_Tag, &ConfigureParams.SCSI.target[3].nDeviceType },
    { "bDiskInserted3", Bool_Tag, &ConfigureParams.SCSI.target[3].bDiskInserted },
    { "bWriteProtected3", Bool_Tag, &ConfigureParams.SCSI.target[3].bWriteProtected },

    { "szImageName4", String_Tag, ConfigureParams.SCSI.target[4].szImageName },
    { "nDeviceType4", Int_Tag, &ConfigureParams.SCSI.target[4].nDeviceType },
    { "bDiskInserted4", Bool_Tag, &ConfigureParams.SCSI.target[4].bDiskInserted },
    { "bWriteProtected4", Bool_Tag, &ConfigureParams.SCSI.target[4].bWriteProtected },

    { "szImageName5", String_Tag, ConfigureParams.SCSI.target[5].szImageName },
    { "nDeviceType5", Int_Tag, &ConfigureParams.SCSI.target[5].nDeviceType },
    { "bDiskInserted5", Bool_Tag, &ConfigureParams.SCSI.target[5].bDiskInserted },
    { "bWriteProtected5", Bool_Tag, &ConfigureParams.SCSI.target[5].bWriteProtected },

    { "szImageName6", String_Tag, ConfigureParams.SCSI.target[6].szImageName },
    { "nDeviceType6", Int_Tag, &ConfigureParams.SCSI.target[6].nDeviceType },
    { "bDiskInserted6", Bool_Tag, &ConfigureParams.SCSI.target[6].bDiskInserted },
    { "bWriteProtected6", Bool_Tag, &ConfigureParams.SCSI.target[6].bWriteProtected },

    { NULL , Error_Tag, NULL }
};

/* Used to load/save MO options */
static const struct Config_Tag configs_MO[] =
{
    { "szImageName0", String_Tag, ConfigureParams.MO.drive[0].szImageName },
    { "bDriveConnected0", Bool_Tag, &ConfigureParams.MO.drive[0].bDriveConnected },
    { "bDiskInserted0", Bool_Tag, &ConfigureParams.MO.drive[0].bDiskInserted },
    { "bWriteProtected0", Bool_Tag, &ConfigureParams.MO.drive[0].bWriteProtected },
    
    { "szImageName1", String_Tag, ConfigureParams.MO.drive[1].szImageName },
    { "bDriveConnected1", Bool_Tag, &ConfigureParams.MO.drive[1].bDriveConnected },
    { "bDiskInserted1", Bool_Tag, &ConfigureParams.MO.drive[1].bDiskInserted },
    { "bWriteProtected1", Bool_Tag, &ConfigureParams.MO.drive[1].bWriteProtected },

	{ NULL , Error_Tag, NULL }
};

/* Used to load/save floppy options */
static const struct Config_Tag configs_Floppy[] =
{
    { "szImageName0", String_Tag, ConfigureParams.Floppy.drive[0].szImageName },
    { "bDriveConnected0", Bool_Tag, &ConfigureParams.Floppy.drive[0].bDriveConnected },
    { "bDiskInserted0", Bool_Tag, &ConfigureParams.Floppy.drive[0].bDiskInserted },
    { "bWriteProtected0", Bool_Tag, &ConfigureParams.Floppy.drive[0].bWriteProtected },
    
    { "szImageName1", String_Tag, ConfigureParams.Floppy.drive[1].szImageName },
    { "bDriveConnected1", Bool_Tag, &ConfigureParams.Floppy.drive[1].bDriveConnected },
    { "bDiskInserted1", Bool_Tag, &ConfigureParams.Floppy.drive[1].bDiskInserted },
    { "bWriteProtected1", Bool_Tag, &ConfigureParams.Floppy.drive[1].bWriteProtected },
    
    { NULL , Error_Tag, NULL }
};

/* Used to load/save Ethernet options */
static const struct Config_Tag configs_Ethernet[] =
{
    { "bEthernetConnected", Bool_Tag, &ConfigureParams.Ethernet.bEthernetConnected },
    
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
	{ "bDSPMemoryExpansion", Bool_Tag, &ConfigureParams.System.bDSPMemoryExpansion },
	{ "bRealTimeClock", Bool_Tag, &ConfigureParams.System.bRealTimeClock },
	{ "bPatchTimerD", Bool_Tag, &ConfigureParams.System.bPatchTimerD },
	{ "bFastForward", Bool_Tag, &ConfigureParams.System.bFastForward },
    
    { "bAddressSpace24", Bool_Tag, &ConfigureParams.System.bAddressSpace24 },
    { "bCycleExactCpu", Bool_Tag, &ConfigureParams.System.bCycleExactCpu },
    { "n_FPUType", Int_Tag, &ConfigureParams.System.n_FPUType },
    { "bCompatibleFPU", Bool_Tag, &ConfigureParams.System.bCompatibleFPU },
    { "bMMU", Bool_Tag, &ConfigureParams.System.bMMU },
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
    
    /* Set defaults for config dialog */
	ConfigureParams.ConfigDialog.bShowConfigDialogAtStartup = true;

	/* Set defaults for debugger */
	ConfigureParams.Debugger.nNumberBase = 10;
	ConfigureParams.Debugger.nDisasmLines = 8;
	ConfigureParams.Debugger.nMemdumpLines = 8;

    /* Set defaults for Boot options */
    ConfigureParams.Boot.nBootDevice = BOOT_ROM;
    ConfigureParams.Boot.bEnableDRAMTest = false;
    ConfigureParams.Boot.bEnablePot = true;
    ConfigureParams.Boot.bEnableSoundTest = true;
    ConfigureParams.Boot.bEnableSCSITest = true;
    ConfigureParams.Boot.bLoopPot = false;
    ConfigureParams.Boot.bVerbose = true;
    ConfigureParams.Boot.bExtendedPot = false;
    
	/* Set defaults for SCSI disks */
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        strcpy(ConfigureParams.SCSI.target[i].szImageName, psWorkingDir);
        ConfigureParams.SCSI.target[i].nDeviceType = DEVTYPE_NONE;
        ConfigureParams.SCSI.target[i].bDiskInserted = false;
        ConfigureParams.SCSI.target[i].bWriteProtected = false;
    }
    
    /* Set defaults for MO drives */
    for (i = 0; i < MO_MAX_DRIVES; i++) {
        strcpy(ConfigureParams.MO.drive[i].szImageName, psWorkingDir);
        ConfigureParams.MO.drive[i].bDriveConnected = false;
        ConfigureParams.MO.drive[i].bDiskInserted = false;
        ConfigureParams.MO.drive[i].bWriteProtected = false;
    }
    
    /* Set defaults for floppy drives */
    for (i = 0; i < FLP_MAX_DRIVES; i++) {
        strcpy(ConfigureParams.Floppy.drive[i].szImageName, psWorkingDir);
        ConfigureParams.Floppy.drive[i].bDriveConnected = false;
        ConfigureParams.Floppy.drive[i].bDiskInserted = false;
        ConfigureParams.Floppy.drive[i].bWriteProtected = false;
    }
    
    /* Set defaults for Ethernet */
    ConfigureParams.Ethernet.bEthernetConnected = false;
    
	/* Set defaults for Keyboard */
	ConfigureParams.Keyboard.bDisableKeyRepeat = false;
	ConfigureParams.Keyboard.nKeymapType = KEYMAP_SCANCODE;
	strcpy(ConfigureParams.Keyboard.szMappingFileName, "");

    /* Set defaults for Mouse */
    ConfigureParams.Mouse.fLinSpeedNormal = 1.0;
    ConfigureParams.Mouse.fLinSpeedLocked = 1.0;
    ConfigureParams.Mouse.fExpSpeedNormal = 1.0;
    ConfigureParams.Mouse.fExpSpeedLocked = 1.0;
    ConfigureParams.Mouse.bEnableAutoGrab = true;

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
	ConfigureParams.Shortcut.withModifier[SHORTCUT_DIMENSION] = SDLK_n;

	/* Set defaults for Memory */
	memset(ConfigureParams.Memory.nMemoryBankSize, 16, 
           sizeof(ConfigureParams.Memory.nMemoryBankSize)); /* 64 MiB */
    ConfigureParams.Memory.nMemorySpeed = MEMORY_100NS;
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
	ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
	ConfigureParams.System.bDSPMemoryExpansion = false;
	ConfigureParams.System.bPatchTimerD = true;
	ConfigureParams.System.bRealTimeClock = true;
	ConfigureParams.System.bFastForward = false;
    
    ConfigureParams.System.bAddressSpace24 = false;
    ConfigureParams.System.bCycleExactCpu = false;
    ConfigureParams.System.n_FPUType = FPU_68882;
    ConfigureParams.System.bCompatibleFPU = true;
    ConfigureParams.System.bMMU = true;

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
 * Helper function for Configuration_Apply, check mouse speed settings
 * to be in the valid range between minimum and maximum value.
 */
void Configuration_CheckFloatMinMax(float *val, float min, float max)
{
    if (*val<min)
        *val=min;
    if (*val>max)
        *val=max;
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
    
    /* Mouse settings */
    Configuration_CheckFloatMinMax(&ConfigureParams.Mouse.fLinSpeedNormal,MOUSE_LIN_MIN,MOUSE_LIN_MAX);
    Configuration_CheckFloatMinMax(&ConfigureParams.Mouse.fLinSpeedLocked,MOUSE_LIN_MIN,MOUSE_LIN_MAX);
    Configuration_CheckFloatMinMax(&ConfigureParams.Mouse.fExpSpeedNormal,MOUSE_EXP_MIN,MOUSE_EXP_MAX);
    Configuration_CheckFloatMinMax(&ConfigureParams.Mouse.fExpSpeedLocked,MOUSE_EXP_MIN,MOUSE_EXP_MAX);
    
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
    
    /* Check memory size for each bank and change to supported values */
    Configuration_CheckMemory(ConfigureParams.Memory.nMemoryBankSize);
    
	/* Clean file and directory names */    
    File_MakeAbsoluteName(ConfigureParams.Rom.szRom030FileName);
    File_MakeAbsoluteName(ConfigureParams.Rom.szRom040FileName);
    File_MakeAbsoluteName(ConfigureParams.Rom.szRomTurboFileName);
    
    int i;
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        File_MakeAbsoluteName(ConfigureParams.SCSI.target[i].szImageName);
    }
    
    for (i = 0; i < MO_MAX_DRIVES; i++) {
        File_MakeAbsoluteName(ConfigureParams.MO.drive[i].szImageName);
    }
    
    for (i = 0; i < FLP_MAX_DRIVES; i++) {
        File_MakeAbsoluteName(ConfigureParams.Floppy.drive[i].szImageName);
    }
    
	File_MakeAbsoluteName(ConfigureParams.Memory.szMemoryCaptureFileName);
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
 * Set defaults depending on selected machine type.
 */
void Configuration_SetSystemDefaults(void) {
    switch (ConfigureParams.System.nMachineType) {
        case NEXT_CUBE030:
            ConfigureParams.System.bTurbo = false;
            ConfigureParams.System.bColor = false;
            ConfigureParams.System.nCpuLevel = 3;
            ConfigureParams.System.nCpuFreq = 25;
            ConfigureParams.System.n_FPUType = FPU_68882;
            ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
            ConfigureParams.System.bDSPMemoryExpansion = false;
            ConfigureParams.System.nSCSI = NCR53C90;
            ConfigureParams.System.nRTC = MC68HC68T1;
            ConfigureParams.System.bADB = false;
            break;
            
        case NEXT_CUBE040:
            ConfigureParams.System.bColor = false;
            ConfigureParams.System.nCpuLevel = 4;
            if (ConfigureParams.System.bTurbo) {
                ConfigureParams.System.nCpuFreq = 33;
                ConfigureParams.System.nRTC = MCCS1850;
            } else {
                ConfigureParams.System.nCpuFreq = 25;
                ConfigureParams.System.nRTC = MC68HC68T1;
            }
            ConfigureParams.System.n_FPUType = FPU_CPU;
            ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
            ConfigureParams.System.bDSPMemoryExpansion = true;
            ConfigureParams.System.nSCSI = NCR53C90A;
            ConfigureParams.System.bADB = false;
            break;
            
        case NEXT_STATION:
            ConfigureParams.System.nCpuLevel = 4;
            if (ConfigureParams.System.bTurbo) {
                ConfigureParams.System.nCpuFreq = 33;
                ConfigureParams.System.nRTC = MCCS1850;
            } else if (ConfigureParams.System.bColor) {
                ConfigureParams.System.nCpuFreq = 25;
                ConfigureParams.System.nRTC = MCCS1850;
            } else {
                ConfigureParams.System.nCpuFreq = 25;
                ConfigureParams.System.nRTC = MC68HC68T1;
            }
            ConfigureParams.System.n_FPUType = FPU_CPU;
            ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
            ConfigureParams.System.bDSPMemoryExpansion = true;
            ConfigureParams.System.nSCSI = NCR53C90A;
            ConfigureParams.System.bADB = false;
            break;
        default:
            break;
    }
}


/*-----------------------------------------------------------------------*/
/**
 * Check memory bank sizes for compatibility with the selected system.
 */
int Configuration_CheckMemory(int *banksize) {
    int i;
    
#define RESTRICTIVE_MEMCHECK 0
#if RESTRICTIVE_MEMCHECK
    /* To boot we need at least 4 MB in bank0 */
    if (banksize[0]<4) {
        banksize[0]=4;
    }
    
    /* On monochrome non-Turbo NeXTstations only the first
     * 2 banks are accessible via memory sockets */
    if (ConfigureParams.System.nMachineType == NEXT_STATION &&
        !ConfigureParams.System.bTurbo && !ConfigureParams.System.bColor) {
        banksize[2]=0;
        banksize[3]=0;
    }
#endif
    
    if (ConfigureParams.System.bTurbo) {
        for (i=0; i<4; i++) {
            if (banksize[i]<=0)
                banksize[i]=0;
            else if (banksize[i]<=2)
                banksize[i]=2;
            else if (banksize[i]<=8)
                banksize[i]=8;
            else if (banksize[i]<=32)
                banksize[i]=32;
            else
                banksize[i]=32;
        }
    } else if (ConfigureParams.System.bColor) {
        for (i=0; i<4; i++) {
            if (banksize[i]<=0)
                banksize[i]=0;
            else if (banksize[i]<=2)
                banksize[i]=2;
            else if (banksize[i]<=8)
                banksize[i]=8;
            else
                banksize[i]=8;
        }
    } else {
        for (i=0; i<4; i++) {
            if (banksize[i]<=0)
                banksize[i]=0;
            else if (banksize[i]<=1)
                banksize[i]=1;
            else if (banksize[i]<=4)
                banksize[i]=4;
            else if (banksize[i]<=16)
                banksize[i]=16;
            else
                banksize[i]=16;
        }
    }
    return (banksize[0]+banksize[1]+banksize[2]+banksize[3]);
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
    Configuration_LoadSection(psFileName, configs_ConfigDialog, "[ConfigDialog]");
	Configuration_LoadSection(psFileName, configs_Debugger, "[Debugger]");
	Configuration_LoadSection(psFileName, configs_Screen, "[Screen]");
	Configuration_LoadSection(psFileName, configs_Keyboard, "[Keyboard]");
	Configuration_LoadSection(psFileName, configs_ShortCutWithMod, "[ShortcutsWithModifiers]");
	Configuration_LoadSection(psFileName, configs_ShortCutWithoutMod, "[ShortcutsWithoutModifiers]");
    Configuration_LoadSection(psFileName, configs_Mouse, "[Mouse]");
	Configuration_LoadSection(psFileName, configs_Sound, "[Sound]");
	Configuration_LoadSection(psFileName, configs_Memory, "[Memory]");
    Configuration_LoadSection(psFileName, configs_Boot, "[Boot]");
	Configuration_LoadSection(psFileName, configs_SCSI, "[HardDisk]");
    Configuration_LoadSection(psFileName, configs_MO, "[MagnetoOptical]");
    Configuration_LoadSection(psFileName, configs_Floppy, "[Floppy]");
    Configuration_LoadSection(psFileName, configs_Ethernet, "[Ethernet]");
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
    Configuration_SaveSection(sConfigFileName, configs_ConfigDialog, "[ConfigDialog]");
	Configuration_SaveSection(sConfigFileName, configs_Debugger, "[Debugger]");
	Configuration_SaveSection(sConfigFileName, configs_Screen, "[Screen]");
	Configuration_SaveSection(sConfigFileName, configs_Keyboard, "[Keyboard]");
	Configuration_SaveSection(sConfigFileName, configs_ShortCutWithMod, "[ShortcutsWithModifiers]");
	Configuration_SaveSection(sConfigFileName, configs_ShortCutWithoutMod, "[ShortcutsWithoutModifiers]");
    Configuration_SaveSection(sConfigFileName, configs_Mouse, "[Mouse]");
	Configuration_SaveSection(sConfigFileName, configs_Sound, "[Sound]");
	Configuration_SaveSection(sConfigFileName, configs_Memory, "[Memory]");
    Configuration_SaveSection(sConfigFileName, configs_Boot, "[Boot]");
	Configuration_SaveSection(sConfigFileName, configs_SCSI, "[HardDisk]");
    Configuration_SaveSection(sConfigFileName, configs_MO, "[MagnetoOptical]");
    Configuration_SaveSection(sConfigFileName, configs_Floppy, "[Floppy]");
    Configuration_SaveSection(sConfigFileName, configs_Ethernet, "[Ethernet]");
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
    /* ROM files */
    MemorySnapShot_Store(ConfigureParams.Rom.szRom030FileName, sizeof(ConfigureParams.Rom.szRom030FileName));
    MemorySnapShot_Store(ConfigureParams.Rom.szRom040FileName, sizeof(ConfigureParams.Rom.szRom040FileName));
    MemorySnapShot_Store(ConfigureParams.Rom.szRomTurboFileName, sizeof(ConfigureParams.Rom.szRomTurboFileName));

    /* Memory options */
	MemorySnapShot_Store(ConfigureParams.Memory.nMemoryBankSize, sizeof(ConfigureParams.Memory.nMemoryBankSize));
    MemorySnapShot_Store(&ConfigureParams.Memory.nMemorySpeed, sizeof(ConfigureParams.Memory.nMemorySpeed));
    
    /* SCSI disks */
    int target;
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        MemorySnapShot_Store(ConfigureParams.SCSI.target[target].szImageName, sizeof(ConfigureParams.SCSI.target[target].szImageName));
        MemorySnapShot_Store(&ConfigureParams.SCSI.target[target].nDeviceType, sizeof(ConfigureParams.SCSI.target[target].nDeviceType));
        MemorySnapShot_Store(&ConfigureParams.SCSI.target[target].bDiskInserted, sizeof(ConfigureParams.SCSI.target[target].bDiskInserted));
        MemorySnapShot_Store(&ConfigureParams.SCSI.target[target].bWriteProtected, sizeof(ConfigureParams.SCSI.target[target].bWriteProtected));

    }
    
    /* MO drives */
    int drive;
    for (drive = 0; drive < MO_MAX_DRIVES; drive++) {
        MemorySnapShot_Store(ConfigureParams.MO.drive[drive].szImageName, sizeof(ConfigureParams.MO.drive[drive].szImageName));
        MemorySnapShot_Store(&ConfigureParams.MO.drive[drive].bDriveConnected, sizeof(ConfigureParams.MO.drive[drive].bDriveConnected));
        MemorySnapShot_Store(&ConfigureParams.MO.drive[drive].bDiskInserted, sizeof(ConfigureParams.MO.drive[drive].bDiskInserted));
        MemorySnapShot_Store(&ConfigureParams.MO.drive[drive].bWriteProtected, sizeof(ConfigureParams.MO.drive[drive].bWriteProtected));
    }
    
    /* Floppy drives */
    for (drive = 0; drive < FLP_MAX_DRIVES; drive++) {
        MemorySnapShot_Store(ConfigureParams.Floppy.drive[drive].szImageName, sizeof(ConfigureParams.Floppy.drive[drive].szImageName));
        MemorySnapShot_Store(&ConfigureParams.Floppy.drive[drive].bDriveConnected, sizeof(ConfigureParams.Floppy.drive[drive].bDriveConnected));
        MemorySnapShot_Store(&ConfigureParams.Floppy.drive[drive].bDiskInserted, sizeof(ConfigureParams.Floppy.drive[drive].bDiskInserted));
        MemorySnapShot_Store(&ConfigureParams.Floppy.drive[drive].bWriteProtected, sizeof(ConfigureParams.Floppy.drive[drive].bWriteProtected));
    }
    
    /* Ethernet options */
    MemorySnapShot_Store(&ConfigureParams.Ethernet.bEthernetConnected, sizeof(ConfigureParams.Ethernet.bEthernetConnected));

    /* Sound options */
    MemorySnapShot_Store(&ConfigureParams.Sound.bEnableSound, sizeof(ConfigureParams.Sound.bEnableSound));
    MemorySnapShot_Store(&ConfigureParams.Sound.bEnableMicrophone, sizeof(ConfigureParams.Sound.bEnableMicrophone));
    
    /* Monitor options */
	MemorySnapShot_Store(&ConfigureParams.Screen.nMonitorType, sizeof(ConfigureParams.Screen.nMonitorType));
	MemorySnapShot_Store(&ConfigureParams.Screen.bUseExtVdiResolutions, sizeof(ConfigureParams.Screen.bUseExtVdiResolutions));
	MemorySnapShot_Store(&ConfigureParams.Screen.nVdiWidth, sizeof(ConfigureParams.Screen.nVdiWidth));
	MemorySnapShot_Store(&ConfigureParams.Screen.nVdiHeight, sizeof(ConfigureParams.Screen.nVdiHeight));
	MemorySnapShot_Store(&ConfigureParams.Screen.nVdiColors, sizeof(ConfigureParams.Screen.nVdiColors));

    /* System options */
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
	MemorySnapShot_Store(&ConfigureParams.System.bDSPMemoryExpansion, sizeof(ConfigureParams.System.bDSPMemoryExpansion));
	MemorySnapShot_Store(&ConfigureParams.System.bRealTimeClock, sizeof(ConfigureParams.System.bRealTimeClock));
	MemorySnapShot_Store(&ConfigureParams.System.bPatchTimerD, sizeof(ConfigureParams.System.bPatchTimerD));
    
    MemorySnapShot_Store(&ConfigureParams.System.bAddressSpace24, sizeof(ConfigureParams.System.bAddressSpace24));
    MemorySnapShot_Store(&ConfigureParams.System.bCycleExactCpu, sizeof(ConfigureParams.System.bCycleExactCpu));
    MemorySnapShot_Store(&ConfigureParams.System.n_FPUType, sizeof(ConfigureParams.System.n_FPUType));
    MemorySnapShot_Store(&ConfigureParams.System.bCompatibleFPU, sizeof(ConfigureParams.System.bCompatibleFPU));
    MemorySnapShot_Store(&ConfigureParams.System.bMMU, sizeof(ConfigureParams.System.bMMU));

	if (!bSave)
		Configuration_Apply(true);
}

