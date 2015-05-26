/*
  Hatari - dialog.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_DIALOG_H
#define HATARI_DIALOG_H

#include "configuration.h"

/* prototypes for gui-sdl/dlg*.c functions: */
extern int Dialog_MainDlg(bool *bReset, bool *bLoadedSnapshot);
extern void Dialog_AboutDlg(void);
extern int DlgAlert_Notice(const char *text);
extern int DlgAlert_Query(const char *text);
extern void Dialog_DeviceDlg(void);
extern void DlgFloppy_Main(void);
extern void DlgSCSI_Main(void);
extern void DlgOptical_Main(void);
extern void DlgEthernet_Main(void);
extern void DlgSound_Main(void);
extern void Dialog_JoyDlg(void);
extern void Dialog_KeyboardDlg(void);
extern void Dialog_MouseDlg(void);
extern bool Dialog_MemDlg(void);
extern void Dialog_MemAdvancedDlg(int *membanks);
extern char* DlgNewDisk_Main(void);
extern void Dialog_MonitorDlg(void);
extern void Dialog_WindowDlg(void);
extern void Dialog_SoundDlg(void);
extern void Dialog_SystemDlg(void);
extern void Dialog_AdvancedDlg(void);
extern void DlgRom_Main(void);
extern void DlgBoot_Main(void);
extern void DlgMissing_Rom(void);
extern void DlgMissing_Disk(char type[], int num, char *name, bool *ins, bool *wp);
/* and dialog.c */
extern bool Dialog_DoProperty(void);
extern void Dialog_CheckFiles(void);

#endif
