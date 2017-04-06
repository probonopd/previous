/*
  Hatari - zip.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_ZIP_H
#define HATARI_ZIP_H


#include <dirent.h>

typedef struct
{
	char **names;
	int nfiles;
} zip_dir;

bool ZIP_FileNameIsZIP(const char *pszFileName);
struct dirent **ZIP_GetFilesDir(const zip_dir *files, const char *dir, int *entries);
void ZIP_FreeZipDir(zip_dir *zd);
zip_dir *ZIP_GetFiles(const char *pszFileName);
Uint8 *ZIP_ReadDisk(const char *pszFileName, const char *pszZipPath, long *pImageSize);
bool ZIP_WriteDisk(const char *pszFileName,unsigned char *pBuffer,int ImageSize);
Uint8 *ZIP_ReadFirstFile(const char *pszFileName, long *pImageSize, const char * const ppszExts[]);


#endif  /* HATARI_ZIP_H */
