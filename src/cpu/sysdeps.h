#ifndef UAE_SYSDEPS_H
#define UAE_SYSDEPS_H

/*
  * UAE - The Un*x Amiga Emulator
  *
  * Try to include the right system headers and get other system-specific
  * stuff right & other collected kludges.
  *
  * If you think about modifying this, think twice. Some systems rely on
  * the exact order of the #include statements. That's also the reason
  * why everything gets included unconditionally regardless of whether
  * it's actually needed by the .c file.
  *
  * Copyright 1996, 1997 Bernd Schmidt
  */
//#include <string>
//using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
//#include <tchar.h>
#include "compat.h"

#ifndef __STDC__
#ifndef _MSC_VER
#error "Your compiler is not ANSI. Get a real one."
#endif
#endif

#include <stdarg.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_VALUES_H
#include <values.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef HAVE_SYS_UTIME_H
# include <sys/utime.h>
#endif

#include <errno.h>
#include <assert.h>

#if EEXIST == ENOTEMPTY
#define BROKEN_OS_PROBABLY_AIX
#endif

#ifdef __NeXT__
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#define S_IXUSR S_IEXEC
#define S_ISDIR(val) (S_IFDIR & val)
struct utimbuf
{
    time_t actime;
    time_t modtime;
};
#endif

#if defined(__GNUC__) && defined(AMIGA)
/* gcc on the amiga need that __attribute((regparm)) must */
/* be defined in function prototypes as well as in        */
/* function definitions !                                 */
#define REGPARAM2 REGPARAM
#else /* not(GCC & AMIGA) */
#define REGPARAM2
#endif

/* sam: some definitions so that SAS/C can compile UAE */
#if defined(__SASC) && defined(AMIGA)
#define REGPARAM2
#define REGPARAM
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#define S_IXUSR S_IEXECUTE
#define S_ISDIR(val) (S_IFDIR & val)
#define mkdir(x,y) mkdir(x)
#define truncate(x,y) 0
#define creat(x,y) open("T:creat",O_CREAT|O_TEMP|O_RDWR) /* sam: for zfile.c */
#define strcasecmp stricmp
#define utime(file,time) 0
struct utimbuf
{
    time_t actime;
    time_t modtime;
};
#endif

#if defined(WARPUP)
#include "devices/timer.h"
#include "osdep/posixemu.h"
#define REGPARAM
#define REGPARAM2
#define RETSIGTYPE
#define USE_ZFILE
#define strcasecmp stricmp
#define memcpy q_memcpy
#define memset q_memset
#define strdup my_strdup
#define random rand
#define creat(x,y) open("T:creat",O_CREAT|O_RDWR|O_TRUNC,777)
void* q_memset(void*,int,size_t);
void* q_memcpy(void*,const void*,size_t);
#endif

#ifdef __DOS__
#include <pc.h>
#include <io.h>
#endif

/* Acorn specific stuff */
#ifdef ACORN

#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#define S_IXUSR S_IEXEC

#define strcasecmp stricmp

#endif

#ifndef L_tmpnam
#define L_tmpnam 128 /* ought to be safe */
#endif

/* If char has more then 8 bits, good night. */
typedef unsigned char uae_u8;
typedef signed char uae_s8;
typedef char uae_char;

typedef struct { uae_u8 RGB[3]; } RGB;

#if SIZEOF_SHORT == 2
typedef unsigned short uae_u16;
typedef short uae_s16;
#elif SIZEOF_INT == 2
typedef unsigned int uae_u16;
typedef int uae_s16;
#else
#error No 2 byte type, you lose.
#endif

#if SIZEOF_INT == 4
typedef unsigned int uae_u32;
typedef int uae_s32;
#elif SIZEOF_LONG == 4
typedef unsigned long uae_u32;
typedef long uae_s32;
#else
#error No 4 byte type, you lose.
#endif

typedef uae_u32 uaecptr;

#undef uae_s64
#undef uae_u64

#if SIZEOF_LONG_LONG == 8
#define uae_s64 long long
#define uae_u64 unsigned long long
#define VAL64(a) (a ## LL)
#define UVAL64(a) (a ## uLL)
#elif SIZEOF___INT64 == 8
#define uae_s64 __int64
#define uae_u64 unsigned __int64
#define VAL64(a) (a)
#define UVAL64(a) (a)
#elif SIZEOF_LONG == 8
#define uae_s64 long;
#define uae_u64 unsigned long;
#define VAL64(a) (a ## l)
#define UVAL64(a) (a ## ul)
#endif

#ifdef HAVE_STRDUP
#define my_strdup _tcsdup
#else
TCHAR *my_strdup (const TCHAR*s);
#endif
TCHAR *my_strdup_ansi (const char*);
TCHAR *au (const char*);
char *ua (const TCHAR*);
TCHAR *aucp (const char *s, unsigned int cp);
char *uacp (const TCHAR *s, unsigned int cp);
TCHAR *au_fs (const char*);
char *ua_fs (const TCHAR*, int);
char *ua_copy (char *dst, int maxlen, const TCHAR *src);
TCHAR *au_copy (TCHAR *dst, int maxlen, const char *src);
char *ua_fs_copy (char *dst, int maxlen, const TCHAR *src, int defchar);
TCHAR *au_fs_copy (TCHAR *dst, int maxlen, const char *src);
char *uutf8 (const TCHAR *s);
TCHAR *utf8u (const char *s);
void unicode_init (void);
void to_lower (TCHAR *s, int len);
void to_upper (TCHAR *s, int len);
/* We can only rely on GNU C getting enums right. Mickeysoft VSC++ is known
 * to have problems, and it's likely that other compilers choke too. */
#ifdef __GNUC__
#define ENUMDECL typedef enum
#define ENUMNAME(name) name

/* While we're here, make abort more useful.  */
/*#define abort() \
  do { \
    write_log ("Internal error; file %s, line %d\n", __FILE__, __LINE__); \
    (abort) (); \
} while (0)
*/#else
#define ENUMDECL enum
#define ENUMNAME(name) ; typedef int name
#endif

/*
 * Porters to weird systems, look! This is the preferred way to get
 * filesys.c (and other stuff) running on your system. Define the
 * appropriate macros and implement wrappers in a machine-specific file.
 *
 * I guess the Mac port could use this (Ernesto?)
 */

#undef DONT_HAVE_POSIX
#undef DONT_HAVE_REAL_POSIX /* define if open+delete doesn't do what it should */
#undef DONT_HAVE_STDIO
#undef DONT_HAVE_MALLOC

#if defined(WARPUP)
#define DONT_HAVE_POSIX
#endif

#if defined _WIN32

#if defined __WATCOMC__

#define O_NDELAY 0
#include <direct.h>
#define dirent direct
#define mkdir(a,b) mkdir(a)
#define strcasecmp stricmp

#elif defined __MINGW32__

#define O_NDELAY 0
#define mkdir(a,b) mkdir(a)

#elif defined _MSC_VER

#ifdef HAVE_GETTIMEOFDAY
#include <winsock.h> // for 'struct timeval' definition
void gettimeofday( struct timeval *tv, void *blah );
#endif

#define O_NDELAY 0

#define FILEFLAG_DIR     0x1
#define FILEFLAG_ARCHIVE 0x2
#define FILEFLAG_WRITE   0x4
#define FILEFLAG_READ    0x8
#define FILEFLAG_EXECUTE 0x10
#define FILEFLAG_SCRIPT  0x20
#define FILEFLAG_PURE    0x40

#ifdef REGPARAM2
#undef REGPARAM2
#endif
#define REGPARAM2 __fastcall
#define REGPARAM3 __fastcall
#define REGPARAM

#include <io.h>
#define O_BINARY _O_BINARY
#define O_WRONLY _O_WRONLY
#define O_RDONLY _O_RDONLY
#define O_RDWR   _O_RDWR
#define O_CREAT  _O_CREAT
#define O_TRUNC  _O_TRUNC
#define strcasecmp _tcsicmp 
#define strncasecmp _tcsncicmp 
#define W_OK 0x2
#define R_OK 0x4
#define STAT struct stat
#define DIR struct DIR
struct direct
{
    TCHAR d_name[1];
};
#include <sys/utime.h>
#define utimbuf __utimbuf64
#define USE_ZFILE

#undef S_ISDIR
#undef S_IWUSR
#undef S_IRUSR
#undef S_IXUSR
#define S_ISDIR(a) (a&FILEFLAG_DIR)
#define S_ISARC(a) (a&FILEFLAG_ARCHIVE)
#define S_IWUSR FILEFLAG_WRITE
#define S_IRUSR FILEFLAG_READ
#define S_IXUSR FILEFLAG_EXECUTE

/* These are prototypes for functions from the Win32 posixemu file */
void get_time (time_t t, long* days, long* mins, long* ticks);
time_t put_time (long days, long mins, long ticks);

/* #define DONT_HAVE_POSIX - don't need all of Mathias' posixemu_functions, just a subset (below) */
#define chmod(a,b) posixemu_chmod ((a), (b))
int posixemu_chmod (const TCHAR *, int);
#define stat(a,b) posixemu_stat ((a), (b))
int posixemu_stat (const TCHAR *, struct _stat64 *);
#define mkdir(x,y) mkdir(x)
#define truncate posixemu_truncate
int posixemu_truncate (const TCHAR *, long int);
#define utime posixemu_utime
int posixemu_utime (const TCHAR *, struct utimbuf *);
#define opendir posixemu_opendir
DIR * posixemu_opendir (const TCHAR *);
#define readdir posixemu_readdir
struct dirent* posixemu_readdir (DIR *);
#define closedir posixemu_closedir
void posixemu_closedir (DIR *);

#endif

#endif /* _WIN32 */

#ifdef DONT_HAVE_POSIX

#define access posixemu_access
int posixemu_access (const TCHAR *, int);
#define open posixemu_open
int posixemu_open (const TCHAR *, int, int);
#define close posixemu_close
void posixemu_close (int);
#define read posixemu_read
int posixemu_read (int, TCHAR *, int);
#define write posixemu_write
int posixemu_write (int, const TCHAR *, int);
#undef lseek
#define lseek posixemu_seek
int posixemu_seek (int, int, int);
#define stat(a,b) posixemu_stat ((a), (b))
int posixemu_stat (const TCHAR *, STAT *);
#define mkdir posixemu_mkdir
int mkdir (const TCHAR *, int);
#define rmdir posixemu_rmdir
int posixemu_rmdir (const TCHAR *);
#define unlink posixemu_unlink
int posixemu_unlink (const TCHAR *);
#define truncate posixemu_truncate
int posixemu_truncate (const TCHAR *, long int);
#define rename posixemu_rename
int posixemu_rename (const TCHAR *, const TCHAR *);
#define chmod posixemu_chmod
int posixemu_chmod (const TCHAR *, int);
#define tmpnam posixemu_tmpnam
void posixemu_tmpnam (TCHAR *);
#define utime posixemu_utime
int posixemu_utime (const TCHAR *, struct utimbuf *);
#define opendir posixemu_opendir
DIR * posixemu_opendir (const TCHAR *);
#define readdir posixemu_readdir
struct dirent* readdir (DIR *);
#define closedir posixemu_closedir
void closedir (DIR *);

/* This isn't the best place for this, but it fits reasonably well. The logic
 * is that you probably don't have POSIX errnos if you don't have the above
 * functions. */
long dos_errno (void);

#endif

#ifdef DONT_HAVE_STDIO

FILE *stdioemu_fopen (const TCHAR *, const TCHAR *);
#define fopen(a,b) stdioemu_fopen(a, b)
int stdioemu_fseek (FILE *, int, int);
#define fseek(a,b,c) stdioemu_fseek(a, b, c)
int stdioemu_fread (TCHAR *, int, int, FILE *);
#define fread(a,b,c,d) stdioemu_fread(a, b, c, d)
int stdioemu_fwrite (const TCHAR *, int, int, FILE *);
#define fwrite(a,b,c,d) stdioemu_fwrite(a, b, c, d)
int stdioemu_ftell (FILE *);
#define ftell(a) stdioemu_ftell(a)
int stdioemu_fclose (FILE *);
#define fclose(a) stdioemu_fclose(a)

#endif

#ifdef DONT_HAVE_MALLOC

#define malloc(a) mallocemu_malloc(a)
void *mallocemu_malloc (int size);
#define free(a) mallocemu_free(a)
void mallocemu_free (void *ptr);

#endif

#ifdef X86_ASSEMBLY
#define ASM_SYM_FOR_FUNC(a) __asm__(a)
#else
#define ASM_SYM_FOR_FUNC(a)
#endif

//#include "target.h"

#ifdef UAE_CONSOLE
#undef write_log
#define write_log write_log_standard
#endif

#if __GNUC__ - 1 > 1 || __GNUC_MINOR__ - 1 > 6
void write_log (const TCHAR *, ...) __attribute__ ((format (printf, 1, 2)));
#else
void write_log (const TCHAR *, ...);
#endif
void write_dlog (const TCHAR *, ...);

void flush_log (void);
void close_console (void);
void reopen_console (void);
void console_out (const TCHAR *);
void console_flush (void);
int console_get (TCHAR *, int);
TCHAR console_getch (void);
void gui_message (const TCHAR *,...);
int gui_message_multibutton (int flags, const TCHAR *format,...);
#define write_log_err write_log
void logging_init (void);
FILE *log_open (const TCHAR *name, int append, int bootlog);
void log_close (FILE *f);


#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef STATIC_INLINE
#if __GNUC__ - 1 > 1 && __GNUC_MINOR__ - 1 >= 0
#define STATIC_INLINE static __inline__ __attribute__ ((always_inline))
#define NOINLINE __attribute__ ((noinline))
#define NORETURN __attribute__ ((noreturn))
#elif _MSC_VER
#define STATIC_INLINE static __forceinline
#define NOINLINE __declspec(noinline)
#define NORETURN __declspec(noreturn)
#else
#define STATIC_INLINE static __inline__
#define NOINLINE
#define NORETURN
#endif
#endif

/*
 * You can specify numbers from 0 to 5 here. It is possible that higher
 * numbers will make the CPU emulation slightly faster, but if the setting
 * is too high, you will run out of memory while compiling.
 * Best to leave this as it is.
 */
#define CPU_EMU_SIZE 0

/*
 * Byte-swapping functions
 */

/* Try to use system bswap_16/bswap_32 functions. */
#if defined HAVE_BSWAP_16 && defined HAVE_BSWAP_32
# include <byteswap.h>
#  ifdef HAVE_BYTESWAP_H
#  include <byteswap.h>
# endif
#else
/* Else, if using SDL, try SDL's endian functions. */
# ifdef USE_SDL
#  include <SDL_endian.h>
#  define bswap_16(x) SDL_Swap16(x)
#  define bswap_32(x) SDL_Swap32(x)
# else
/* Otherwise, we'll roll our own. */
#  define bswap_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))
#  define bswap_32(x) (((x) << 24) | (((x) << 8) & 0x00FF0000) | (((x) >> 8) & 0x0000FF00) | ((x) >> 24))
# endif
#endif

#endif

#ifndef __cplusplus

#define xmalloc(T, N) malloc(sizeof (T) * (N))
#define xcalloc(T, N) calloc(sizeof (T), N)
#define xfree(T) free(T)
#define xrealloc(T, TP, N) realloc(TP, sizeof (T) * (N))

#else

#define xmalloc(T, N) static_cast<T*>(malloc (sizeof (T) * (N)))
#define xcalloc(T, N) static_cast<T*>(calloc (sizeof (T), N))
#define xrealloc(T, TP, N) static_cast<T*>(realloc (TP, sizeof (T) * (N)))
#define xfree(T) free(T)

#endif
