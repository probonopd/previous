/*
  Hatari - configuration.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_CONFIGURATION_H
#define HATARI_CONFIGURATION_H

#include <stdio.h>

#define ENABLE_TESTING 0

/* Configuration Dialog */
typedef struct
{
  bool bShowConfigDialogAtStartup;
} CNF_CONFIGDLG;

/* Logging and tracing */
typedef struct
{
  char sLogFileName[FILENAME_MAX];
  char sTraceFileName[FILENAME_MAX];
  int nTextLogLevel;
  int nAlertDlgLogLevel;
  bool bConfirmQuit;
} CNF_LOG;


/* Debugger */
typedef struct
{
  int nNumberBase;
  int nDisasmLines;
  int nMemdumpLines;
} CNF_DEBUGGER;


/* ROM configuration */
typedef struct
{
    char szRom030FileName[FILENAME_MAX];
    char szRom040FileName[FILENAME_MAX];
    char szRomTurboFileName[FILENAME_MAX];
} CNF_ROM;


/* Sound configuration */
typedef struct
{
  bool bEnableMicrophone;
  bool bEnableSound;
} CNF_SOUND;

/* Dialog Keyboard */
typedef enum
{
  KEYMAP_SYMBOLIC,  /* Use keymapping with symbolic (ASCII) key codes */
  KEYMAP_SCANCODE,  /* Use keymapping with PC keyboard scancodes */
  KEYMAP_LOADED     /* Use keymapping with a map configuration file */
} KEYMAPTYPE;

typedef struct
{
  bool bDisableKeyRepeat;
  bool bSwapCmdAlt;
  KEYMAPTYPE nKeymapType;
  char szMappingFileName[FILENAME_MAX];
} CNF_KEYBOARD;


typedef enum {
  SHORTCUT_OPTIONS,
  SHORTCUT_FULLSCREEN,
  SHORTCUT_MOUSEGRAB,
  SHORTCUT_COLDRESET,
  SHORTCUT_WARMRESET,
  SHORTCUT_CURSOREMU,
  SHORTCUT_SOUND,
  SHORTCUT_DEBUG_M68K,
  SHORTCUT_DEBUG_I860,
  SHORTCUT_PAUSE,
  SHORTCUT_QUIT,
  SHORTCUT_DIMENSION,
  SHORTCUT_KEYS,  /* number of shortcuts */
  SHORTCUT_NONE
} SHORTCUTKEYIDX;

typedef struct
{
  int withModifier[SHORTCUT_KEYS];
  int withoutModifier[SHORTCUT_KEYS];
} CNF_SHORTCUT;


/* Dialog Mouse */
#define MOUSE_LIN_MIN   0.1
#define MOUSE_LIN_MAX   10.0
#define MOUSE_EXP_MIN   0.5
#define MOUSE_EXP_MAX   1.0
typedef struct
{
    bool bEnableAutoGrab;
    float fLinSpeedNormal;
    float fLinSpeedLocked;
    float fExpSpeedNormal;
    float fExpSpeedLocked;
} CNF_MOUSE;


/* Memory configuration */

typedef enum
{
  MEMORY_120NS,
  MEMORY_100NS,
  MEMORY_80NS,
  MEMORY_60NS
} MEMORY_SPEED;

typedef struct
{
  int nMemoryBankSize[4];
  MEMORY_SPEED nMemorySpeed;
} CNF_MEMORY;


/* Dialog Boot options */

typedef enum
{
    BOOT_ROM,
    BOOT_SCSI,
    BOOT_ETHERNET,
    BOOT_MO,
    BOOT_FLOPPY
} BOOT_DEVICE;

typedef struct
{
    BOOT_DEVICE nBootDevice;
    bool bEnableDRAMTest;
    bool bEnablePot;
    bool bExtendedPot;
    bool bEnableSoundTest;
    bool bEnableSCSITest;
    bool bLoopPot;
    bool bVerbose;
} CNF_BOOT;


/* Hard drives configuration */
#define ESP_MAX_DEVS 7
typedef enum {
    DEVTYPE_NONE,
    DEVTYPE_HARDDISK,
    DEVTYPE_CD,
    DEVTYPE_FLOPPY,
    NUM_DEVTYPES
} SCSI_DEVTYPE;

typedef struct {
    char szImageName[FILENAME_MAX];
    SCSI_DEVTYPE nDeviceType;
    bool bDiskInserted;
    bool bWriteProtected;
} SCSIDISK;

typedef enum
{
    WRITEPROT_OFF,
    WRITEPROT_ON,
    WRITEPROT_AUTO
} WRITEPROTECTION;

typedef struct {
    SCSIDISK target[ESP_MAX_DEVS];
    int nWriteProtection;
} CNF_SCSI;


/* Magneto-optical drives configuration */
#define MO_MAX_DRIVES   2
typedef struct {
    char szImageName[FILENAME_MAX];
    bool bDriveConnected;
    bool bDiskInserted;
    bool bWriteProtected;
} MODISK;

typedef struct {
    MODISK drive[MO_MAX_DRIVES];
} CNF_MO;


/* Floppy disk drives configuration */
#define FLP_MAX_DRIVES   2
typedef struct {
    char szImageName[FILENAME_MAX];
    bool bDriveConnected;
    bool bDiskInserted;
    bool bWriteProtected;
} FLPDISK;

typedef struct {
    FLPDISK drive[FLP_MAX_DRIVES];
} CNF_FLOPPY;


/* Ethernet configuration */
typedef struct {
    bool bEthernetConnected;
    bool bTwistedPair;
} CNF_ENET;

typedef enum
{
  MONITOR_TYPE_DUAL,
  MONITOR_TYPE_CPU,
  MONITOR_TYPE_DIMENSION,
} MONITORTYPE;

/* Screen configuration */
typedef struct
{
  MONITORTYPE nMonitorType;
  bool bFullScreen;
  bool bShowStatusbar;
  bool bShowDriveLed;
} CNF_SCREEN;


/* Printer configuration */
typedef enum
{
    PAPER_A4,
    PAPER_LETTER,
    PAPER_B5,
    PAPER_LEGAL
} PAPER_SIZE;

typedef struct
{
  bool bPrinterConnected;
  PAPER_SIZE nPaperSize;
  char szPrintToFileName[FILENAME_MAX];
} CNF_PRINTER;

/* Dialog System */
typedef enum
{
  NEXT_CUBE030,
  NEXT_CUBE040,
  NEXT_STATION
} MACHINETYPE;

typedef enum
{
  NCR53C90,
  NCR53C90A
} SCSICHIP;

typedef enum
{
  MC68HC68T1,
  MCCS1850
} RTCCHIP;

typedef enum
{
  DSP_TYPE_NONE,
  DSP_TYPE_DUMMY,
  DSP_TYPE_EMU
} DSPTYPE;

typedef enum
{
  FPU_NONE = 0,
  FPU_68881 = 68881,
  FPU_68882 = 68882,
  FPU_CPU = 68040
} FPUTYPE;

typedef struct
{
  bool bColor;
  bool bTurbo;
  bool bNBIC;
  SCSICHIP nSCSI;
  RTCCHIP nRTC;
  int nCpuLevel;
  int nCpuFreq;
  bool bCompatibleCpu;            /* Prefetch mode */
  MACHINETYPE nMachineType;
  bool bRealtime;                 /* TRUE if realtime sources shoud be used */
  DSPTYPE nDSPType;               /* how to "emulate" DSP */
  bool bDSPMemoryExpansion;
  bool bRealTimeClock;
  FPUTYPE n_FPUType;
  bool bCompatibleFPU;            /* More compatible FPU */
  bool bMMU;                      /* TRUE if MMU is enabled */
} CNF_SYSTEM;

typedef struct
{
    bool bEnabled;
    bool bI860Thread;
	bool bMainDisplay;
    int  nMemoryBankSize[4];
    char szRomFileName[FILENAME_MAX];
} CNF_ND;

/* State of system is stored in this structure */
/* On reset, variables are copied into system globals and used. */
typedef struct
{
  /* Configure */
  CNF_CONFIGDLG ConfigDialog;
  CNF_LOG       Log;
  CNF_DEBUGGER  Debugger;
  CNF_SCREEN    Screen;
  CNF_KEYBOARD  Keyboard;
  CNF_SHORTCUT  Shortcut;
  CNF_MOUSE     Mouse;
  CNF_SOUND     Sound;
  CNF_MEMORY    Memory;
  CNF_BOOT      Boot;
  CNF_SCSI      SCSI;
  CNF_MO        MO;
  CNF_FLOPPY    Floppy;
  CNF_ENET      Ethernet;
  CNF_ROM       Rom;
  CNF_PRINTER   Printer;
  CNF_SYSTEM    System;
  CNF_ND        Dimension;
} CNF_PARAMS;


extern CNF_PARAMS ConfigureParams;
extern char sConfigFileName[FILENAME_MAX];

void Configuration_SetDefault(void);
void Configuration_SetSystemDefaults(void);
void Configuration_Apply(bool bReset);
int Configuration_CheckMemory(int *banksize);
int  Configuration_CheckDimensionMemory(int *banksize);
void Configuration_CheckDimensionSettings(void);
void Configuration_CheckEthernetSettings();
void Configuration_Load(const char *psFileName);
void Configuration_Save(void);
void Configuration_MemorySnapShot_Capture(bool bSave);

#endif
