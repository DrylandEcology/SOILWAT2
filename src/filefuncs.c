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

#include "include/filefuncs.h"
#include "include/myMemory.h"
#ifdef RSOILWAT
  #include <R.h>    // for REvprintf(), error(), and warning()
#endif


/* 01/05/2011	(drs) removed unused variable *p from MkDir()
 06/21/2013	(DLM)	memory leak in function getfiles(): variables dname and fname need to be free'd
 */

#ifdef STEPWAT
void sw_error(int errorcode, const char *format, ...);
#endif

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static char **getfiles(const char *fspec, int *nfound, LOG_INFO* LogInfo) {
	/* return a list of files described by fspec
	 * fspec is as described in RemoveFiles(),
	 * **flist is a dynamic char array containing pointers to the
	 *   file names found that match fspec
	 * nfound is the number of files found, also, num elements in flist
	 */

	char **flist = NULL, *fname, *fn1, *fn2, *p2;
	char dname[FILENAME_MAX];

	int len1, len2;
	Bool match, alloc = swFALSE;

	DIR *dir;
	struct dirent *ent;

	assert(fspec != NULL);

	DirName(fspec, dname); // Copy `fspec` into `dname`
	fname = Str_Dup(BaseName(fspec), LogInfo);

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
		match = swTRUE;
		if (fn1)
			match = (0 == strncmp(ent->d_name, fn1, len1)) ? swTRUE : swFALSE;
		if (match && fn2) {
			p2 = ent->d_name + (strlen(ent->d_name) - len2);
			match = (0 == strcmp(fn2, p2)) ? swTRUE : swFALSE;
		}

		if (match) {
			(*nfound)++;
			if (alloc) {
				flist = (char **) Mem_ReAlloc(flist, sizeof(char *) * (*nfound), LogInfo);
			} else {
				flist = (char **) Mem_Malloc(sizeof(char *) * (*nfound), "getfiles", LogInfo);
				alloc = swTRUE;
			}
			flist[(*nfound) - 1] = Str_Dup(ent->d_name, LogInfo);
		}
	}

	closedir(dir);
	free(fname);

	return flist;
}



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

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
  va_list ap;
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
void LogError(LOG_INFO* LogInfo, const int mode, const char *fmt, ...) {
    /* uses global variable logged to indicate that a log message
     * was sent to output, which can be used to inform user, etc.
     *
     *  9-Dec-03 (cwb) Modified to accept argument list similar
     *           to fprintf() so sprintf(errstr...) doesn't need
     *           to be called each time replacement args occur.
     */

    char outfmt[MAX_LOG_SIZE] = {0}; /* to prepend err type str */
	char buf[MAX_LOG_SIZE];
	int nextWarn = LogInfo->numWarnings;
	va_list args;

	va_start(args, fmt);

    if (LOGWARN & mode)
        strcpy(outfmt, "WARNING: ");
    else if (LOGERROR & mode)
        strcpy(outfmt, "ERROR: ");

    strcat(outfmt, fmt);
    strcat(outfmt, "\n");

	vsnprintf(buf, MAX_LOG_SIZE, outfmt, args);

	if(LOGWARN & mode) {
		if(nextWarn < MAX_MSGS) {
			strcpy(LogInfo->warningMsgs[nextWarn], buf);
		}
		LogInfo->numWarnings++;
	} else if(LOGERROR & mode) {

		#ifdef STEPWAT
		sw_error(-1, outfmt);
		#else
		strcpy(LogInfo->errorMsg, buf);
		LogInfo->stopRun = swTRUE;
		#endif
	}

	LogInfo->logged = swTRUE;
	va_end(args);
}


/**************************************************************/
Bool GetALine(FILE *f, char buf[]) {
	/* Read a line of possibly commented input from the file *f.
	 * Skip blank lines and comment lines.  Comments within the
	 * line are removed and trailing whitespace is removed.
	 */
	char *p;
	Bool not_eof = swFALSE;
	while (!isnull( fgets(buf, 1024, f) )) {
		if (!isnull( p=strchr(buf, (int) '\n')))
			*p = '\0';

		UnComment(buf);
		if (*buf != '\0') {
			not_eof = swTRUE;
			break;
		}
	}
	return (not_eof);
}

/**************************************************************/
void DirName(const char *p, char *outString) {
	/* copy path (if any) to a static buffer.
	 * Be sure to copy the return value to a more stable buffer
	 * before moving on.
	 */
	char *c;
	int l;
	char sep1 = '/', sep2 = '\\';

	*outString = '\0';
	if (!(c = (char*) strrchr(p, (int) sep1)))
		c = (char*) strrchr(p, (int) sep2);

	if (c) {
		l = c - p + 1;
		strncpy(outString, p, l);
		outString[l] = '\0';
	}
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
FILE * OpenFile(const char *name, const char *mode, LOG_INFO* LogInfo) {
	FILE *fp;
	fp = fopen(name, mode);
	if (isnull(fp))
		LogError(LogInfo, LOGERROR, "Cannot open file %s: %s", name, strerror(errno));
	return (fp);
}

/**************************************************************/
void CloseFile(FILE **f, LOG_INFO* LogInfo) {
	/* This routine is a wrapper for the basic fclose() so
	 it might be possible to add more code like error checking
	 or other special features in the future.

	 Currently, the FILE pointer is set to NULL so it could be
	 used as a check for whether the file is opened or not.
	 */
	if (*f == NULL ) {
		LogError(LogInfo, LOGWARN,
			"Tried to close file that doesn't exist or isn't open!");
		return;
	}
	fclose(*f);

	*f = NULL;
}

/**************************************************************/
Bool FileExists(const char *name) {
	/* return swFALSE if name is not a regular file
	 * eg, directory, pipe, etc.
	 */
	struct stat statbuf;
	Bool result = swFALSE;

	if (0 == stat(name, &statbuf))
		result = S_ISREG(statbuf.st_mode) ? swTRUE : swFALSE;

	return (result);
}

/**************************************************************/
Bool DirExists(const char *dname) {
	/* test for existance of a directory
	 * dname is name of directory, not including filename
	 */

	struct stat statbuf;
	Bool result = swFALSE;

	if (0 == stat(dname, &statbuf))
		result = S_ISDIR(statbuf.st_mode) ? swTRUE : swFALSE;

	return (result);
}

/**************************************************************/
Bool ChDir(const char *dname) {
	/* wrapper in case portability problems come up */

	return (chdir(dname) == 0) ? swTRUE : swFALSE;

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

Bool MkDir(const char *dname, LOG_INFO* LogInfo) {
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
	Bool result = swTRUE;
	char *a[256] = { 0 }, /* points to each path element for mkdir -p behavior */
		*c; /* duplicate of dname so we don't change it */
	const char *delim = "\\/"; /* path separators */
	char errstr[MAX_ERROR];

	if (isnull(dname))
		return swFALSE;

	c = Str_Dup(dname, LogInfo);

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
					result = swFALSE;
					break;
				}
			}
		}
		strcat(errstr, "/");
	}

	free(c);
	return result;
}

#undef mkdir

/**************************************************************/
Bool RemoveFiles(const char *fspec, LOG_INFO* LogInfo) {
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

	char **flist, fname[FILENAME_MAX];
	int i, nfiles, dlen, result = swTRUE;

	if (fspec == NULL )
		return swTRUE;

	if ((flist = getfiles(fspec, &nfiles, LogInfo))) {
		DirName(fspec, fname); // Transfer `fspec` into `fname`
		dlen = strlen(fname);
		for (i = 0; i < nfiles; i++) {
			strcpy(fname + dlen, flist[i]);
			if (0 != remove(fname)) {
				result = swFALSE;
				break;
			}
		}
	}

	for (i = 0; i < nfiles; i++)
		Mem_Free(flist[i]);
	Mem_Free(flist);

	return (Bool) result;
}

