/*
  Hatari - file.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_FILE_H
#define HATARI_FILE_H

void File_CleanFileName(char *pszFileName);
void File_AddSlashToEndFileName(char *pszFileName);
bool File_DoesFileExtensionMatch(const char *pszFileName, const char *pszExtension);
const char *File_RemoveFileNameDrive(const char *pszFileName);
bool File_DoesFileNameEndWithSlash(char *pszFileName);
Uint8 *File_Read(const char *pszFileName, long *pFileSize, const char * const ppszExts[]);
bool File_Save(const char *pszFileName, const Uint8 *pAddress, size_t Size, bool bQueryOverwrite);
off_t File_Length(const char *pszFileName);
bool File_Exists(const char *pszFileName);
bool File_DirExists(const char *psDirName);
bool File_QueryOverwrite(const char *pszFileName);
char* File_FindPossibleExtFileName(const char *pszFileName,const char * const ppszExts[]);
void File_SplitPath(const char *pSrcFileName, char *pDir, char *pName, char *Ext);
char* File_MakePath(const char *pDir, const char *pName, const char *pExt);
void File_ShrinkName(char *pDestFileName, const char *pSrcFileName, int maxlen);
FILE *File_Open(const char *path, const char *mode);
FILE *File_Close(FILE *fp);
bool File_InputAvailable(FILE *fp);
void File_MakeAbsoluteSpecialName(char *pszFileName);
void File_MakeAbsoluteName(char *pszFileName);
void File_MakeValidPathName(char *pPathName);
void File_PathShorten(char *path, int dirs);
void File_HandleDotDirs(char *path);

#endif /* HATARI_FILE_H */
