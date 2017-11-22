#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef __BCC__
#include <stat.h>
#include <dir.h>
#else
#include <unistd.h>
#endif

#include "filefuncs.h"
#include "myMemory.h"

/* Note that errstr[] is externed in generic.h via filefuncs.h */
/* 01/05/2011	(drs) removed unused variable *p from MkDir()
 06/21/2013	(DLM)	memory leak in function getfiles(): variables dname and fname need to be free'd
 */

char **getfiles(const char *fspec, int *nfound);


/**
 * @brief Prints an error message and throws an error or warning. Works both for rSOILWAT2
 *  and SOILWAT2-standalone.
 *
 * @param code The error/warning code. If `code` is not 0, then it is passed to `exit`
 *  (SOILWAT2) / `error` (rSOILWAT2). If `code` is 0, then it is passed to
 *  `warning` (rSOILWAT2), respectively.
 * @param format The character string with formatting (as for `printf`).
 * @param ... Variables to be printed.
 */
void sw_error(int code, const char *format, ...)
{
  va_list(ap);
  va_start(ap, format);
#ifdef RSOILWAT
  REvprintf(format, ap);
#else
  vfprintf(stderr, format, ap);
#endif
  va_end(ap);

  if (code == 0) {
    #ifdef RSOILWAT
      warning("Warning: %d\n", code);
    #endif

  } else {
    #ifdef RSOILWAT
      error("exit %d\n", code);
    #else
      exit(code);
    #endif
  }
}



/**************************************************************/
Bool GetALine(FILE *f, char buf[]) {
	/* Read a line of possibly commented input from the file *f.
	 * Skip blank lines and comment lines.  Comments within the
	 * line are removed and trailing whitespace is removed.
	 */
	char *p;
	Bool not_eof = FALSE;
	while (!isnull( fgets(buf, 1024, f) )) {
		if (!isnull( p=strchr(buf, (int) '\n')))
			*p = '\0';

		UnComment(buf);
		if (*buf != '\0') {
			not_eof = TRUE;
			break;
		}
	}
	return (not_eof);
}

/**************************************************************/
char *DirName(const char *p) {
	/* copy path (if any) to a static buffer.
	 * Be sure to copy the return value to a more stable buffer
	 * before moving on.
	 */
	static char s[FILENAME_MAX];
	char *c;
	int l;
	char sep1 = '/', sep2 = '\\';

	*s = '\0';
	if (!(c = (char*) strrchr(p, (int) sep1)))
		c = (char*) strrchr(p, (int) sep2);

	if (c) {
		l = c - p + 1;
		strncpy(s, p, l);
		s[l] = '\0';
	}
	return s;
}

/**************************************************************/
const char *BaseName(const char *p) {
	/* return a pointer to the terminal element (file) of a path. */
	/* Doesn't modify the string, but you'll probably want to
	 * copy the result to a stable buffer. */
	char *c;
	char sep1 = '/', sep2 = '\\';

	if (!(c = (char*) strrchr(p, (int) sep1)))
		c = (char*) strrchr(p, (int) sep2);

	return ((c != NULL )? c+1 : p);
}

/**************************************************************/
FILE * OpenFile(const char *name, const char *mode) {
	FILE *fp;

	fp = fopen(name, mode);
	if (isnull(fp))
		LogError(logfp, LOGERROR | LOGEXIT, "Cannot open file %s: %s", name, strerror(errno));
	return (fp);
}

/**************************************************************/
void CloseFile(FILE **f) {
	/* This routine is a wrapper for the basic fclose() so
	 it might be possible to add more code like error checking
	 or other special features in the future.

	 Currently, the FILE pointer is set to NULL so it could be
	 used as a check for whether the file is opened or not.
	 */
	if (*f == NULL ) {
		fprintf(logfp, "WARNING: Tried to close file that exist/isn't open!!!\n");
		return;
	}
	fclose(*f);

	*f = NULL;
}

/**************************************************************/
Bool FileExists(const char *name) {
	/* return FALSE if name is not a regular file
	 * eg, directory, pipe, etc.
	 */
	struct stat statbuf;
	Bool result = FALSE;

	if (0 == stat(name, &statbuf))
		result = S_ISREG(statbuf.st_mode) ? TRUE : FALSE;

	return (result);
}

/**************************************************************/
Bool DirExists(const char *dname) {
	/* test for existance of a directory
	 * dname is name of directory, not including filename
	 */

	struct stat statbuf;
	Bool result = FALSE;

	if (0 == stat(dname, &statbuf))
		result = S_ISDIR(statbuf.st_mode) ? TRUE : FALSE;

	return (result);
}

/**************************************************************/
Bool ChDir(const char *dname) {
	/* wrapper in case portability problems come up */

	return (chdir(dname) == 0) ? TRUE : FALSE;

}

/**************************************************************/
/* Mapping mdir() function to OS specific version */

#ifdef _WIN32  /* 32- and 64-bit Windows OS: Windows XP, Vista, 7, 8 */
#define mkdir(d, m) mkdir(d)
#elif _WIN64
#define mkdir(d, m) mkdir(d)
#elif __linux__                        /* linux: Centos, Debian, Fedora, OpenSUSE, RedHat, Ubuntu */
#define mkdir(d, m) mkdir(d, m)
#elif __APPLE__ && __MACH__            /* (Mac) OS X, macOS, iOS, Darwin */
#define mkdir(d, m) mkdir(d, m)
#endif

Bool MkDir(const char *dname) {
	/* make a path with 'mkdir -p' -like behavior. provides an
	 * interface for portability problems.
	 * RELATIVE PATH ONLY solves problems like "C:\etc" and null
	 * first element in absolute path.
	 * if you need to make an absolute path, use ChDir() first.
	 * if you care about mode of new dir, use mkdir(), not MkDir()
	 * if MkDir returns FALSE, check errno.
	 *
	 * Notes:
	 * - portability issues seem to be quite problematic, at least
	 *   between platforms and GCC and Borland.  The only common
	 *   error code is EACCES, so if something else happens (and it
	 *   well might in unix), more tests have to be included, perhaps
	 *   with macros that test the compiler/platform.
	 * - we're borrowing errstr to build the path to facilitate the
	 *   -p behavior.
	 */

	int r, i, n;
	Bool result = TRUE;
	char *a[256] = { 0 }, /* points to each path element for mkdir -p behavior */
		*c; /* duplicate of dname so we don't change it */
	const char *delim = "\\/"; /* path separators */

	if (isnull(dname))
		return FALSE;

	if (NULL == (c = strdup(dname))) {
		sw_error(-1, "Out of memory making string in MkDir()");
	}

	n = 0;
	a[n++] = strtok(c, delim);
	while (NULL != (a[n++] = strtok(NULL, delim)))
		; /* parse path */
	n--;
	errstr[0] = '\0';
	for (i = 0; i < n; i++) {
		strcat(errstr, a[i]);
		if (!DirExists(errstr)) {
			if (0 != (r = mkdir(errstr, 0777))) {
				if (errno == EACCES) {
					result = FALSE;
					break;
				}
			}
		}
		strcat(errstr, "/");
	}

	return result;
}

#undef mkdir

/**************************************************************/
Bool RemoveFiles(const char *fspec) {
	/* delete files matching fspec. ASSUMES terminal element
	 *   describes files, ie, is not a directory.
	 * fspec may contain leading path (eg, here/and/now/files)
	 * fspec can't be too complicated because I'm not including the
	 *   full regexp library.  Specifically you can include a *
	 *   anywhere in the terminal path member,
	 *   eg: /here/now/fi*les, /here/now/files*
	 *   or /here/now/files
	 * Returns TRUE if all files removed, FALSE otherwise.
	 * Check errno if return is FALSE.
	 */

	char **flist, fname[1024];
	int i, nfiles, dlen, result = TRUE;

	if (fspec == NULL )
		return TRUE;

	if ((flist = getfiles(fspec, &nfiles))) {
		strcpy(fname, DirName(fspec));
		dlen = strlen(fname);
		for (i = 0; i < nfiles; i++) {
			strcpy(fname + dlen, flist[i]);
			if (0 != remove(fname)) {
				result = FALSE;
				break;
			}
		}
	}

	for (i = 0; i < nfiles; i++)
		Mem_Free(flist[i]);
	Mem_Free(flist);

	return (Bool) result;
}

/**************************************************************/
char **getfiles(const char *fspec, int *nfound) {
	/* return a list of files described by fspec
	 * fspec is as described in RemoveFiles(),
	 * **flist is a dynamic char array containing pointers to the
	 *   file names found that match fspec
	 * nfound is the number of files found, also, num elements in flist
	 */

	char **flist = NULL, *dname, *fname, *fn1, *fn2, *p2;

	int len1, len2;
	Bool match, alloc = FALSE;

	DIR *dir;
	struct dirent *ent;

	assert(fspec != NULL);

	dname = Str_Dup(DirName(fspec));
	fname = Str_Dup(BaseName(fspec));

	if (strchr(fname, '*')) {
		fn1 = strtok(fname, "*");
		fn2 = strtok(NULL, "*");
	} else {
		fn1 = fname;
		fn2 = NULL;
	}
	len1 = (fn1) ? strlen(fn1) : 0;
	len2 = (fn2) ? strlen(fn2) : 0;

	(*nfound) = 0;

	if ((dir = opendir(dname)) == NULL )
		return NULL ;

	while ((ent = readdir(dir)) != NULL ) {
		match = TRUE;
		if (fn1)
			match = (0 == strncmp(ent->d_name, fn1, len1)) ? TRUE : FALSE;
		if (match && fn2) {
			p2 = ent->d_name + (strlen(ent->d_name) - len2);
			match = (0 == strcmp(fn2, p2)) ? TRUE : FALSE;
		}

		if (match) {
			(*nfound)++;
			if (alloc) {
				flist = (char **) Mem_ReAlloc(flist, sizeof(char *) * (*nfound));
			} else {
				flist = (char **) Mem_Malloc(sizeof(char *) * (*nfound), "getfiles");
				alloc = TRUE;
			}
			flist[(*nfound) - 1] = Str_Dup(ent->d_name);
		}
	}

	closedir(dir);
	free(dname);
	free(fname);

	return flist;
}
