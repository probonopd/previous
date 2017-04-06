/*
  Hatari - options.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
#ifndef HATARI_OPTIONS_H
#define HATARI_OPTIONS_H

Uint32 Opt_GetNoParachuteFlag(void);
bool Opt_ParseParameters(int argc, const char * const argv[]);
char *Opt_MatchOption(const char *text, int state);

#endif /* HATARI_OPTIONS_H */
