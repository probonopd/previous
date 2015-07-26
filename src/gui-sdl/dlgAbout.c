/*
  Previous - dlgAbout.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Show information about the program and its license.
*/
const char DlgAbout_fileid[] = "Previous dlgAbout.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "dialog.h"
#include "sdlgui.h"


/* The "About"-dialog: */
static SGOBJ aboutdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 56,22, NULL },
	{ SGTEXT, 0, 0, 21,1, 12,1, PROG_NAME },
	{ SGTEXT, 0, 0, 21,2, 12,1, "==============" },
	{ SGTEXT, 0, 0, 1,4, 38,1, "Previous emulates NeXT computer systems. It is derived" },
	{ SGTEXT, 0, 0, 1,5, 38,1, "from Hatari. This application is the work of many." },
	{ SGTEXT, 0, 0, 1,6, 38,1, "It was only possible with the help of the community at" },
	{ SGTEXT, 0, 0, 1,7, 38,1, "the NeXT International forums (www.nextcomputers.org)." },
	{ SGTEXT, 0, 0, 1,9, 38,1, "This program is free software; you can redistribute it" },
	{ SGTEXT, 0, 0, 1,10, 38,1, "and/or modify it under the terms of the GNU General" },
	{ SGTEXT, 0, 0, 1,11, 38,1, "Public License as published by the Free Software" },
	{ SGTEXT, 0, 0, 1,12, 38,1, "Foundation; either version 2 of the license, or (at" },
	{ SGTEXT, 0, 0, 1,13, 38,1, "your option) any later version." },
	{ SGTEXT, 0, 0, 1,15, 38,1, "This program is distributed in the hope that it will" },
	{ SGTEXT, 0, 0, 1,16, 38,1, "be useful, but WITHOUT ANY WARRANTY." },
	{ SGTEXT, 0, 0, 1,17, 38,1, "See the GNU General Public License for more details." },
	{ SGBUTTON, SG_DEFAULT, 0, 24,20, 8,1, "OK" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Show the "about" dialog:
 */
void Dialog_AboutDlg(void)
{
	/* Center PROG_NAME title string */
	aboutdlg[1].x = (aboutdlg[0].w - strlen(PROG_NAME)) / 2;

	SDLGui_CenterDlg(aboutdlg);
	SDLGui_DoDialog(aboutdlg, NULL);
}
