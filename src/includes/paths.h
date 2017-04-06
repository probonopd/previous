/*
  Hatari

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_PATHS_H
#define HATARI_PATHS_H

void Paths_Init(const char *argv0);
const char *Paths_GetWorkingDir(void);
const char *Paths_GetDataDir(void);
const char *Paths_GetUserHome(void);
const char *Paths_GetHatariHome(void);

#endif
