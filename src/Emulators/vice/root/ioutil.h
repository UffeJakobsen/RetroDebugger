/*
 * ioutil.h - Miscellaneous IO utility functions.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef VICE_IOUTIL_H
#define VICE_IOUTIL_H

#define IOUTIL_ACCESS_R_OK 4
#define IOUTIL_ACCESS_W_OK 2
#define IOUTIL_ACCESS_X_OK 1
#define IOUTIL_ACCESS_F_OK 0

#define IOUTIL_MKDIR_RWXU   0700
#define IOUTIL_MKDIR_RWXUG  0770
#define IOUTIL_MKDIR_RWXUGO 0777

#define IOUTIL_ERRNO_EPERM  0
#define IOUTIL_ERRNO_EEXIST 1
#define IOUTIL_ERRNO_EACCES 2
#define IOUTIL_ERRNO_ENOENT 3
#define IOUTIL_ERRNO_ERANGE 4

int ioutil_access(const char *pathname, int mode);
int ioutil_chdir(const char *path);
int ioutil_errno(unsigned int check);
char *ioutil_getcwd(char *buf, int size);
int ioutil_isatty(int desc);
unsigned int ioutil_maxpathlen(void);
int ioutil_mkdir(const char *pathname, int mode);
int ioutil_remove(const char *name);
int ioutil_rename(const char *oldpath, const char *newpath);
int ioutil_stat(const char *file_name, unsigned int *len, unsigned int *isdir);

char *ioutil_current_dir(void);

struct ioutil_name_table_s {
    char *name;
};
typedef struct ioutil_name_table_s ioutil_name_table_t;

struct ioutil_dir_s {
    ioutil_name_table_t *dirs;
    ioutil_name_table_t *files;
    int dir_amount;
    int file_amount;
    int counter;
};
typedef struct ioutil_dir_s ioutil_dir_t;

ioutil_dir_t *ioutil_opendir(const char *path);
char *ioutil_readdir(ioutil_dir_t *ioutil_dir);
void ioutil_closedir(ioutil_dir_t *ioutil_dir);

/* VICE 3.10 forward-compat: archdep_* aliases for ioutil_* functions.
   VICE 3.10 removed ioutil and uses archdep_* directly.
   These aliases allow new VICE 3.10 code to compile with our ioutil backend.
   Note: archdep_mkdir/stat/rename already exist as real functions in archapi.h
   (ioutil.c already delegates to them), so no macros needed for those. */
#define archdep_access      ioutil_access
#define archdep_chdir       ioutil_chdir
#define archdep_remove      ioutil_remove
#define archdep_current_dir ioutil_current_dir

#define ARCHDEP_ACCESS_R_OK  IOUTIL_ACCESS_R_OK
#define ARCHDEP_ACCESS_W_OK  IOUTIL_ACCESS_W_OK
#define ARCHDEP_ACCESS_X_OK  IOUTIL_ACCESS_X_OK
#define ARCHDEP_ACCESS_F_OK  IOUTIL_ACCESS_F_OK

#define ARCHDEP_MKDIR_RWXU   IOUTIL_MKDIR_RWXU
#define ARCHDEP_MKDIR_RWXUG  IOUTIL_MKDIR_RWXUG
#define ARCHDEP_MKDIR_RWXUGO IOUTIL_MKDIR_RWXUGO

/* VICE 3.10 uses archdep_dir_t instead of ioutil_dir_t */
typedef struct ioutil_dir_s archdep_dir_t;
/* archdep_opendir takes (path, mode) in 3.10; ioutil_opendir only takes (path) */
static inline archdep_dir_t *archdep_opendir(const char *path, int mode) {
    (void)mode;
    return ioutil_opendir(path);
}
#define archdep_readdir   ioutil_readdir
#define archdep_closedir  ioutil_closedir
/* VICE 3.10 directory position functions — stub implementations.
   Our ioutil_dir_s already has a counter field used by ioutil_readdir. */
static inline void archdep_rewinddir(archdep_dir_t *dir) {
    if (dir) dir->counter = 0;
}
static inline int archdep_telldir(archdep_dir_t *dir) {
    return dir ? dir->counter : 0;
}
static inline void archdep_seekdir(archdep_dir_t *dir, int pos) {
    if (dir) dir->counter = pos;
}

/* VICE 3.10 archdep_dir.h defines */
#define ARCHDEP_OPENDIR_ALL_FILES  0

/* VICE 3.10 ARCHDEP_FSDEVICE_DEFAULT_DIR (was FSDEVICE_DEFAULT_DIR) */
#ifndef ARCHDEP_FSDEVICE_DEFAULT_DIR
#define ARCHDEP_FSDEVICE_DEFAULT_DIR "."
#endif

/* VICE 3.10 ARCHDEP_PATH_MAX — also defined in archdep_defs.h, but
   duplicated here to work around Xcode explicit-module caching */
#ifndef ARCHDEP_PATH_MAX
# include <limits.h>
# ifdef PATH_MAX
#  define ARCHDEP_PATH_MAX PATH_MAX
# else
#  define ARCHDEP_PATH_MAX 4096
# endif
#endif

#endif
