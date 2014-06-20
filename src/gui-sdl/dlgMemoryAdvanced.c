/*
  Previous - dlgMemoryAdvanced.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

*/
const char DlgMemoryAdvanced_fileid[] = "Hatari dlgMemoryAdvanced.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"


#define DLGMA_BANK0_2MB     4
#define DLGMA_BANK0_4MB     5
#define DLGMA_BANK0_8MB     6
#define DLGMA_BANK0_16MB    7
#define DLGMA_BANK0_32MB    8
#define DLGMA_BANK0_NONE    9

#define DLGMA_BANK1_2MB     12
#define DLGMA_BANK1_4MB     13
#define DLGMA_BANK1_8MB     14
#define DLGMA_BANK1_16MB    15
#define DLGMA_BANK1_32MB    16
#define DLGMA_BANK1_NONE    17

#define DLGMA_BANK2_2MB     20
#define DLGMA_BANK2_4MB     21
#define DLGMA_BANK2_8MB     22
#define DLGMA_BANK2_16MB    23
#define DLGMA_BANK2_32MB    24
#define DLGMA_BANK2_NONE    25

#define DLGMA_BANK3_2MB     28
#define DLGMA_BANK3_4MB     29
#define DLGMA_BANK3_8MB     30
#define DLGMA_BANK3_16MB    31
#define DLGMA_BANK3_32MB    32
#define DLGMA_BANK3_NONE    33

#define DLGMA_CHECK         37
#define DLGMA_EXIT          38


void Dialog_MemAdvancedDlgDraw(int *memorybank);
void Dialog_MemAdvancedDlgRead(int *memorybank);

static SGOBJ memoryadvanceddlg[] =
{
    { SGBOX, 0, 0, 0,0, 63,24, NULL },
    { SGTEXT, 0, 0, 22,1, 14,1, "Custom memory setup" },
    
    { SGBOX, 0, 0, 2,4, 14,10, NULL },
    { SGTEXT, 0, 0, 3,5, 14,1, "Bank0" },
    { SGRADIOBUT, 0, 0, 4,7, 6,1, "2 MB" },
    { SGRADIOBUT, 0, 0, 4,8, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 4,9, 6,1, "8 MB" },
    { SGRADIOBUT, 0, 0, 4,10, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 4,11, 7,1, "32 MB" },
    { SGRADIOBUT, 0, 0, 4,12, 6,1, "none" },
    
    { SGBOX, 0, 0, 17,4, 14,10, NULL },
    { SGTEXT, 0, 0, 18,5, 14,1, "Bank1" },
    { SGRADIOBUT, 0, 0, 19,7, 6,1, "2 MB" },
    { SGRADIOBUT, 0, 0, 19,8, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 19,9, 6,1, "8 MB" },
    { SGRADIOBUT, 0, 0, 19,10, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 19,11, 7,1, "32 MB" },
    { SGRADIOBUT, 0, 0, 19,12, 6,1, "none" },
    
    { SGBOX, 0, 0, 32,4, 14,10, NULL },
    { SGTEXT, 0, 0, 33,5, 14,1, "Bank2" },
    { SGRADIOBUT, 0, 0, 34,7, 6,1, "2 MB" },
    { SGRADIOBUT, 0, 0, 34,8, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 34,9, 6,1, "8 MB" },
    { SGRADIOBUT, 0, 0, 34,10, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 34,11, 7,1, "32 MB" },
    { SGRADIOBUT, 0, 0, 34,12, 6,1, "none" },
    
    { SGBOX, 0, 0, 47,4, 14,10, NULL },
    { SGTEXT, 0, 0, 48,5, 14,1, "Bank3" },
    { SGRADIOBUT, 0, 0, 49,7, 6,1, "2 MB" },
    { SGRADIOBUT, 0, 0, 49,8, 6,1, "4 MB" },
    { SGRADIOBUT, 0, 0, 49,9, 6,1, "8 MB" },
    { SGRADIOBUT, 0, 0, 49,10, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 49,11, 7,1, "32 MB" },
    { SGRADIOBUT, 0, 0, 49,12, 6,1, "none" },
    
    { SGTEXT, 0, 0, 3,15, 14,1, "Some values are only valid on certain machines. They will" },
    { SGTEXT, 0, 0, 3,16, 14,1, "be corrected automatically when leaving this dialog." },
    { SGTEXT, 0, 0, 3,18, 14,1, "For booting Bank0 must contain at least 4 MB of memory." },

    { SGBUTTON, 0, 0, 20,21, 9,1, "Check" },
    { SGBUTTON, SG_DEFAULT, 0, 35,21, 8,1, "OK" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
  * Show and process the "Memory Advanced" dialog.
  */
void Dialog_MemAdvancedDlg(int *membank) {

    int but;

 	SDLGui_CenterDlg(memoryadvanceddlg);
 
 	/* Set up dialog from actual values: */
    Dialog_MemAdvancedDlgDraw(membank);
 		
 	/* Draw and process the dialog: */
    do
	{
        but = SDLGui_DoDialog(memoryadvanceddlg, NULL);
        printf("Button: %i\n", but);
        switch (but) {
            case DLGMA_CHECK:
                Dialog_MemAdvancedDlgRead(membank);
                Configuration_CheckMemory(membank);
                Dialog_MemAdvancedDlgDraw(membank);
                break;
            default:
                break;
        }
    }
    while (but != DLGMA_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
 
 	/* Read values from dialog: */
    Dialog_MemAdvancedDlgRead(membank);
}


/* Function to set up the dialog from the actual values */
void Dialog_MemAdvancedDlgDraw(int *bank) {
    int i, j;
    for (i = 0; i<4; i++) {
        
        for (j = (DLGMA_BANK0_2MB+(8*i)); j <= (DLGMA_BANK0_NONE+(8*i)); j++)
        {
            memoryadvanceddlg[j].state &= ~SG_SELECTED;
        }
        
        switch (bank[i])
        {
            case 0:
                memoryadvanceddlg[DLGMA_BANK0_NONE+(i*8)].state |= SG_SELECTED;
                break;
            case 2:
                memoryadvanceddlg[DLGMA_BANK0_2MB+(i*8)].state |= SG_SELECTED;
                break;
            case 4:
                memoryadvanceddlg[DLGMA_BANK0_4MB+(i*8)].state |= SG_SELECTED;
                break;
            case 8:
                memoryadvanceddlg[DLGMA_BANK0_8MB+(i*8)].state |= SG_SELECTED;
                break;
            case 16:
                memoryadvanceddlg[DLGMA_BANK0_16MB+(i*8)].state |= SG_SELECTED;
                break;
            case 32:
                memoryadvanceddlg[DLGMA_BANK0_32MB+(i*8)].state |= SG_SELECTED;
                break;
                
            default:
                break;
        }
    }
}


/* Function to read the actual values from the dialog */
void Dialog_MemAdvancedDlgRead(int *bank) {
    int i;
    for (i = 0; i<4; i++) {
        if (memoryadvanceddlg[(DLGMA_BANK0_2MB)+(i*8)].state & SG_SELECTED)
            bank[i] = 2;
        else if (memoryadvanceddlg[(DLGMA_BANK0_4MB)+(i*8)].state & SG_SELECTED)
            bank[i] = 4;
        else if (memoryadvanceddlg[(DLGMA_BANK0_8MB)+(i*8)].state & SG_SELECTED)
            bank[i] = 8;
        else if (memoryadvanceddlg[(DLGMA_BANK0_16MB)+(i*8)].state & SG_SELECTED)
            bank[i] = 16;
        else if (memoryadvanceddlg[(DLGMA_BANK0_32MB)+(i*8)].state & SG_SELECTED)
            bank[i] = 32;
        else if (memoryadvanceddlg[(DLGMA_BANK0_NONE)+(i*8)].state & SG_SELECTED)
            bank[i] = 0;
    }
}