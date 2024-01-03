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


/* 01/05/2011	(drs) removed unused variable *p from MkDir()
 06/21/2013	(DLM)	memory leak in function getfiles(): variables dname and fname need to be free'd
 */

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void freeGetfilesEarly(char **flist, int nfound, DIR *dir, char *fname) {
    int index;

    if(!isnull(flist)) {
        for(index = 0; index < nfound; index++) {
            if(!isnull(flist[index])) {
                free(flist[index]);
            }
        }

        free(flist);
        flist = NULL;
    }

    closedir(dir);
    free(fname);
    fname = NULL;
}

static char **getfiles(const char *fspec, int *nfound, LOG_INFO* LogInfo) {
	/* return a list of files described by fspec
	 * fspec is as described in RemoveFiles(),
	 * **flist is a dynamic char array containing pointers to the
	 *   file names found that match fspec
	 * nfound is the number of files found, also, num elements in flist
	 */

	char **flist = NULL, *fname, *fn1, *fn2, *p2;
	char dname[FILENAME_MAX];

    int startIndex = 0, strLen = 0; // For `sw_strtok()`

	int len1, len2;
	Bool match, alloc = swFALSE;

	DIR *dir;
	struct dirent *ent;

	assert(fspec != NULL);

	DirName(fspec, dname); // Copy `fspec` into `dname`
	fname = Str_Dup(BaseName(fspec), LogInfo);
    if(LogInfo->stopRun) {
        return NULL; // Exit function prematurely due to error
    }

	if (strchr(fname, '*')) {
		fn1 = sw_strtok(fname, &startIndex, &strLen, "*");
		fn2 = sw_strtok(fname, &startIndex, &strLen, "*");
	} else {
		fn1 = fname;
		fn2 = NULL;
	}
	len1 = (fn1) ? strlen(fn1) : 0;
	len2 = (fn2) ? strlen(fn2) : 0;

	(*nfound) = 0;

	if ((dir = opendir(dname)) == NULL ) {
        free(fname);
		return NULL ;
    }

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

                if(LogInfo->stopRun) {
                    freeGetfilesEarly(flist, 0, dir, fname);
                    return NULL; // Exit function prematurely due to error
                }
			} else {
				flist = (char **) Mem_Malloc(sizeof(char *) * (*nfound), "getfiles", LogInfo);

                if(LogInfo->stopRun) {
                    freeGetfilesEarly(flist, 0, dir, fname);
                    return NULL; // Exit function prematurely due to error
                }
				alloc = swTRUE;
			}
			flist[(*nfound) - 1] = Str_Dup(ent->d_name, LogInfo);
            if(LogInfo->stopRun) {
                freeGetfilesEarly(flist, *nfound, dir, fname);
                return NULL; // Exit function prematurely due to error
            }
		}
	}

	closedir(dir);
	free(fname);

	return flist;
}



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**************************************************************/

/**
  @brief Compose, store and count warning and error messages

  @param[in,out] LogInfo Holds information on warnings and errors
  @param[in] mode Indicator whether message is a warning or error
  @param[in] fmt Message string with optional format specifications for ... arguments
  @param[in] ... Additional values that are injected into fmt
*/
void LogError(LOG_INFO* LogInfo, const int mode, const char *fmt, ...) {
    /* 9-Dec-03 (cwb) Modified to accept argument list similar
     *           to fprintf() so sprintf(errstr...) doesn't need
     *           to be called each time replacement args occur.
     */

    char outfmt[MAX_LOG_SIZE] = {0}; /* to prepend err type str */
	char buf[MAX_LOG_SIZE];
    char msgType[MAX_LOG_SIZE];
	int nextWarn = LogInfo->numWarnings;
	va_list args;
    int expectedWriteSize;

	va_start(args, fmt);

    if (LOGWARN & mode)
        strcpy(msgType, "WARNING: ");
    else if (LOGERROR & mode)
        strcpy(msgType, "ERROR: ");

    expectedWriteSize = snprintf(outfmt, MAX_LOG_SIZE, "%s%s\n", msgType, fmt);
    if(expectedWriteSize > MAX_LOG_SIZE) {
        // Silence gcc (>= 7.1) compiler flag `-Wformat-truncation=`, i.e., handle output truncation
        fprintf(stderr, "Programmer: message exceeds the maximum size.\n");
        #ifdef SWDEBUG
        exit(EXIT_FAILURE);
        #endif
    }

    expectedWriteSize = vsnprintf(buf, MAX_LOG_SIZE, outfmt, args);
    #ifdef SWDEBUG
    if(expectedWriteSize > MAX_LOG_SIZE) {
        fprintf(stderr, "Programmer: Injecting arguments to final message buffer "
                        "makes it exceed the maximum size.\n");
        exit(EXIT_FAILURE);
    }
    #endif

	if(LOGWARN & mode) {
		if(nextWarn < MAX_MSGS) {
			strcpy(LogInfo->warningMsgs[nextWarn], buf);
		}
		LogInfo->numWarnings++;
	} else if(LOGERROR & mode) {

		#ifdef STEPWAT
		fprintf(stderr, "%s", buf);

		// Consider updating STEPWAT2: instead of exiting/crashing, do catch
		// errors, recoil, and use `sw_write_warnings(); sw_fail_on_error()`
		// as SOILWAT2 >= 7.2.0
		exit(EXIT_FAILURE);
		#else
		strcpy(LogInfo->errorMsg, buf);
		LogInfo->stopRun = swTRUE;
		#endif
	}

	va_end(args);
}


/**************************************************************/
Bool GetALine(FILE *f, char buf[], int numChars) {
	/* Read a line of possibly commented input from the file *f.
	 * Skip blank lines and comment lines.  Comments within the
	 * line are removed and trailing whitespace is removed.
	 */
	char *p;
	Bool not_eof = swFALSE;
	while (!isnull( fgets(buf, numChars, f) )) {
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
	if (isnull(fp)) {
		LogError(LogInfo, LOGERROR, "Cannot open file %s: %s", name, strerror(errno));
        return NULL; // Exit function prematurely due to error
    }
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

    int startIndex = 0, strLen = 0; // For `sw_strtok()`

	if (isnull(dname))
		return swFALSE;

	c = Str_Dup(dname, LogInfo);
    if(LogInfo->stopRun) {
        return swFALSE; // Exit function prematurely due to error
    }

	n = 0;
	while (NULL != (a[n++] = sw_strtok(c, &startIndex, &strLen, delim)))
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
    if(LogInfo->stopRun) {
        return swFALSE;
    }

	for (i = 0; i < nfiles; i++) {
		free(flist[i]);
	}
	free(flist);

	return (Bool) result;
}


/** Copy a file

@param[in] from The file path of the source (original) file.
@param[in] to The file path to the file copy (destination).
@param[out] LogInfo Holds information on warnings and errors

@return swTRUE on success and swFALSE on failure.
*/
Bool CopyFile(const char *from, const char *to, LOG_INFO* LogInfo) {
    char buffer[4096]; // or any other constant that is a multiple of 512
    size_t n;
    FILE *ffrom, *fto;

    ffrom = fopen(from, "r");
    if (ffrom == NULL) {
      LogError(LogInfo, LOGERROR, "CopyFile: error opening source file %s.\n", *from);
      return swFALSE;  // Exit function prematurely due to error
    }

    fto = fopen(to, "w");
    if (fto == NULL) {
      fclose(ffrom);
      LogError(LogInfo, LOGERROR, "CopyFile: error opening destination file %s.\n", to);
      return swFALSE;  // Exit function prematurely due to error
    }

    while ((n = fread(buffer, 1, sizeof buffer, ffrom)) > 0) {
        if (fwrite(buffer, 1, n, fto) != n) {
            LogError(LogInfo, LOGERROR, "CopyFile: error while copying to %s.\n", to);
            fclose(ffrom);
            fclose(fto);
            return swFALSE;  // Exit function prematurely due to error
        }
    }

    if (ferror(ffrom)) {
      LogError(LogInfo, LOGERROR, "CopyFile: error reading source file %s.\n", from);
      fclose(ffrom);
      fclose(fto);
      return swFALSE;  // Exit function prematurely due to error
    }

    // clean up
    if (fclose(ffrom) == EOF) {
      LogError(LogInfo, LOGERROR, "CopyFile: error closing source file %s.\n", from);
      return swFALSE;  // Exit function prematurely due to error
    }

    if (fclose(fto) == EOF) {
      LogError(LogInfo, LOGERROR, "CopyFile: error closing destination file %s.\n", to);
      return swFALSE;  // Exit function prematurely due to error
    }

    return swTRUE;
}


/**
 * @brief Convert a key read-in from an input file to an index
 *  the caller can understand
 *
 * @param[in] key Key found within the file to test for
 * @param[in] possibleKeys A list of possible keys that can be found
 * @param[in] numPossKeys Number of keys within `possibleKeys`
*/
int key_to_id(const char* key, const char **possibleKeys,
              int numPossKeys) {
    int id;

    for(id = 0; id < numPossKeys; id++) {
        if(strcmp(key, possibleKeys[id]) == 0) {
            return id;
        }
    }

    return KEY_NOT_FOUND;
}

/**
  @brief Mark a found key as found and warns if duplicate

  @param[in] keyID Index of key (as returned by key_to_id()).
  @param[in] possibleKeys A list of possible keys that can be found
      (used for warning messages).
  @param[in,out] hasKeys Array that is updated if keyID is found.
  @param[out] LogInfo Holds information on warnings and errors
*/
void set_hasKey(int keyID, const char **possibleKeys, Bool *hasKeys, LOG_INFO* LogInfo) {
    if (keyID != KEY_NOT_FOUND) {
        if (hasKeys[keyID]) {
            LogError(
                LogInfo,
                LOGWARN,
                "Duplicate input key '%s' found.",
                possibleKeys[keyID]
            );
        }

        hasKeys[keyID] = swTRUE;
    }
}

/**
  @brief Throw error if a required key was not found

  @param[in] hasKeys Array with found keys.
  @param[in] requiredKeys Array with required keys.
  @param[in] possibleKeys A list of possible keys
      (used for error message).
  @param[in] numKeys Number of keys.
  @param[out] LogInfo Holds information on warnings and errors
*/
void check_requiredKeys(Bool *hasKeys, const Bool *requiredKeys, const char **possibleKeys, int numKeys, LOG_INFO* LogInfo) {
    int keyID;

    for (keyID = 0; keyID < numKeys; keyID++) {
        if (!hasKeys[keyID] && requiredKeys[keyID]) {
            LogError(
                LogInfo,
                LOGERROR,
                "Required input key '%s' not found.\n",
                possibleKeys[keyID]
            );
            return; // Exit function prematurely due to error
        }
    }
}
