/*
  Prevous - dlgPrinter.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgPrinter_fileid[] = "Previous dlgPrinter.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


#define DLGPRINT_CONNECTED  3
#define DLGPRINT_A4         6
#define DLGPRINT_LETTER     7
#define DLGPRINT_B5         8
#define DLGPRINT_LEGAL      9

#define DLGPRINT_BROWSE     12
#define DLGPRINT_DIRECTORY  13

#define DLGPRINT_EXIT       14

char dlgprint_dirname[64];

/* The Printer options dialog: */
static SGOBJ printerdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 47,23, NULL },
    { SGTEXT, 0, 0, 16,1, 9,1, "Printer options" },

    { SGBOX, 0, 0, 1,4, 22,8, NULL },
    { SGCHECKBOX, 0, 0, 2,5, 19,1, "Printer connected" },
    
    { SGBOX, 0, 0, 24,4, 22,8, NULL },
	{ SGTEXT, 0, 0, 25,5, 30,1, "Paper size:" },
	{ SGRADIOBUT, 0, 0, 25,7, 4,1, "A4" },
    { SGRADIOBUT, 0, 0, 25,8, 8,1, "Letter" },
    { SGRADIOBUT, 0, 0, 25,9, 4,1, "B5" },
    { SGRADIOBUT, 0, 0, 25,10, 7,1, "Legal" },
    
    { SGBOX, 0, 0, 1,13, 45,5, NULL },
    { SGTEXT, 0, 0, 2,14, 19,1, "Directory to save printer output:" },
    { SGBUTTON, 0, 0, 37,14, 8,1, "Browse" },
    { SGTEXT, 0, 0, 2,16, 43,1, dlgprint_dirname },
    
	{ SGBUTTON, SG_DEFAULT, 0, 13,20, 21,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
 * Show and process the Printer options dialog.
 */
void DlgPrinter_Main(void)
{
	int but;

	SDLGui_CenterDlg(printerdlg);

    /* Set up the dialog from actual values */
    if (ConfigureParams.Printer.bPrinterConnected)
        printerdlg[DLGPRINT_CONNECTED].state |= SG_SELECTED;
    else
        printerdlg[DLGPRINT_CONNECTED].state &= ~SG_SELECTED;
    
    printerdlg[DLGPRINT_A4].state &= ~SG_SELECTED;
    printerdlg[DLGPRINT_LETTER].state &= ~SG_SELECTED;
    printerdlg[DLGPRINT_B5].state &= ~SG_SELECTED;
    printerdlg[DLGPRINT_LEGAL].state &= ~SG_SELECTED;

    switch (ConfigureParams.Printer.nPaperSize) {
        case PAPER_A4:
            printerdlg[DLGPRINT_A4].state |= SG_SELECTED;
            break;
        case PAPER_LETTER:
            printerdlg[DLGPRINT_LETTER].state |= SG_SELECTED;
            break;
        case PAPER_B5:
            printerdlg[DLGPRINT_B5].state |= SG_SELECTED;
            break;
        case PAPER_LEGAL:
            printerdlg[DLGPRINT_LEGAL].state |= SG_SELECTED;
            break;
            
        default:
            printerdlg[DLGPRINT_A4].state |= SG_SELECTED;
            break;
    }
    
    File_ShrinkName(dlgprint_dirname, ConfigureParams.Printer.szPrintToFileName,
                    printerdlg[DLGPRINT_DIRECTORY].w);
    

    /* Draw and process the dialog */
    
	do
	{
		but = SDLGui_DoDialog(printerdlg, NULL);
        
        if (but == DLGPRINT_BROWSE) {
            SDLGui_DirectorySelect(dlgprint_dirname,
                                   ConfigureParams.Printer.szPrintToFileName,
                                   printerdlg[DLGPRINT_DIRECTORY].w);
        }
	}
	while (but != DLGPRINT_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
    
    
    /* Read values from dialog */
    ConfigureParams.Printer.bPrinterConnected = printerdlg[DLGPRINT_CONNECTED].state & SG_SELECTED;
    
    if (printerdlg[DLGPRINT_A4].state & SG_SELECTED)
        ConfigureParams.Printer.nPaperSize = PAPER_A4;
    else if (printerdlg[DLGPRINT_LETTER].state & SG_SELECTED)
        ConfigureParams.Printer.nPaperSize = PAPER_LETTER;
    else if (printerdlg[DLGPRINT_B5].state & SG_SELECTED)
        ConfigureParams.Printer.nPaperSize = PAPER_B5;
    else if (printerdlg[DLGPRINT_LEGAL].state & SG_SELECTED)
        ConfigureParams.Printer.nPaperSize = PAPER_LEGAL;
    else
        ConfigureParams.Printer.nPaperSize = PAPER_A4;
}
