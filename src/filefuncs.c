

/* 01/05/2011	(drs) removed unused variable *p from MkDir()
 06/21/2013	(DLM)	memory leak in function getfiles(): variables dname and
 fname need to be free'd
 */

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/filefuncs.h"      // for BaseName, ChDir, CloseFile, Copy...
#include "include/generic.h"        // for swFALSE, Bool, swTRUE, LOGERROR
#include "include/myMemory.h"       // for Str_Dup, Mem_Malloc, Mem_ReAlloc
#include "include/SW_datastructs.h" // for LOG_INFO
#include "include/SW_Defines.h"     // for MAX_LOG_SIZE, KEY_NOT_FOUND, MAX...
#include "include/Times.h"          // for timeStringISO8601
#include <dirent.h>                 // for dirent, closedir, DIR, opendir, re...
#include <errno.h>                  // for errno, ERANGE
#include <limits.h>                 // for INT_MIN, LONG_MIN, ULONG_MAX
#include <math.h>                   // for HUGE_VAL
#include <stdarg.h>                 // for va_end, va_start
#include <stdio.h>                  // for NULL, fclose, FILE, fopen, EOF
#include <stdlib.h>                 // for free, strtod, strtof, strtol
#include <string.h>                 // for strlen, strrchr, memccpy, strchr
#include <sys/stat.h>               // for stat, mkdir, S_ISDIR, S_ISREG
#include <unistd.h>                 // for chdir

#ifdef RSOILWAT
#include <R.h> // for Rf_error() from <R_ext/Error.h>
#endif

/* Note
Some of these headers are not part of the C Standard Library header files;
however, they are part of the C POSIX library:
    * <dirent.h>
    * <sys/stat.h>
    * <unistd.h>
*/


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
// NOLINTBEGIN(misc-no-recursion)
static Bool RemoveFilesHelper(
    const char *fspec,
    size_t len1,
    size_t len2,
    Bool clearDir,
    char *fn1,
    char *fn2,
    int depth,
    int maxDepth,
    LOG_INFO *LogInfo
) {
    /* Recursively iterate through a specified starting directory
       and remove matching files;
       Explores subdirectories and deletes contents within before
       deleting the directory itself
     */

    struct stat statbuf;

    char fname[FILENAME_MAX] = "\0";
    char temp[FILENAME_MAX] = "\0";
    char *endFnamePtr = fname + sizeof fname - 1;
    char *fNamePlusDLen;
    char *p2;
    char dname[FILENAME_MAX];
    size_t dlen;
    size_t writeSize;
    Bool bufferFull = swFALSE;
    Bool passed = swTRUE;
    Bool isDir;
    int resSNP;

    Bool match;

    DIR *dir;
    struct dirent *ent;

    DirName(fspec, dname); // Copy `fspec` into `dname`
    dlen = strlen(dname);
    dir = opendir(dname);
    if (depth == maxDepth || isnull(dir)) {
        if (depth == maxDepth) {
            LogError(
                LogInfo,
                LOGWARN,
                "Max depth reached and could not explore %s.\n",
                fspec
            );
        }

        return swTRUE; // Exit function prematurely due to error
    }

    strncpy(fname, dname, sizeof fname);

    while ((ent = readdir(dir)) != NULL) {
        match = swTRUE;

        // When clearing the directory, make sure we don't include
        // previous directories ('.' and '..')
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        if (!clearDir) {
            if (fn1) {
                match = (Bool) (0 == strncmp(ent->d_name, fn1, len1));
            }
            if (match && fn2) {
                p2 = ent->d_name + (strlen(ent->d_name) - len2);
                match = (Bool) (0 == strcmp(fn2, p2));
            }
        }

        fNamePlusDLen = fname + dlen;
        writeSize = FILENAME_MAX - dlen;
        bufferFull = sw_memccpy_inc(
            (void **) &fNamePlusDLen,
            endFnamePtr,
            (void *) ent->d_name,
            '\0',
            &writeSize
        );
        if (bufferFull) {
            reportFullBuffer(LOGERROR, LogInfo);
            break;
        }

        isDir = (Bool) (stat(fname, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));

        if (match || isDir) {
            if (isDir && clearDir) {
                resSNP = snprintf(temp, sizeof temp, "%s/", fname);
                if (resSNP < 0 || (unsigned) resSNP >= (sizeof temp)) {
                    LogError(
                        LogInfo, LOGERROR, "Path name is too long: '%s'.", temp
                    );
                    goto closeDir;
                }

                passed = RemoveFilesHelper(
                    temp,
                    len1,
                    len2,
                    clearDir,
                    fn1,
                    fn2,
                    depth + 1,
                    maxDepth,
                    LogInfo
                );
            }

            /*
                Remove the file only if
                    * Clearing the directory entirely and we are a
                        directory
                    * We are not clearing the directory and we are not a
                      directory (.csv when SWNETCDF is enabled
                      or when STEPWAT2 removes files)
            */
            if ((((isDir && clearDir) || !isDir) && remove(fname) != 0) ||
                !passed) {

                passed = swFALSE;
                goto closeDir;
            }
        }
    }

closeDir: { closedir(dir); }

    return passed;
}

// NOLINTEND(misc-no-recursion)

/**
@brief Compose, store and count warning and error messages

@param[in,out] LogInfo Holds information on warnings and errors
@param[in] mode Indicator whether message is a warning or error
@param[in] fmt Message string with optional format specifications for ...
    arguments
@param[in] args A list of arguments of type `va_list`
*/
static void LogErrorHelper(
    LOG_INFO *LogInfo, const int mode, const char *fmt, va_list args
) {
    /* 9-Dec-03 (cwb) Modified to accept argument list similar
     *           to fprintf() so sprintf(errstr...) doesn't need
     *           to be called each time replacement args occur.
     */

    char outfmt[MAX_LOG_SIZE] = {0}; /* to prepend err type str */
    char buf[MAX_LOG_SIZE];
    char msgType[MAX_LOG_SIZE];
    int nextWarn = LogInfo->numWarnings;
    int expectedWriteSize;
    char *writePtr = msgType;

    if (LOGWARN & mode) {
        (void) sw_memccpy(writePtr, "WARNING: ", '\0', MAX_LOG_SIZE);
    } else if (LOGERROR & mode) {
        (void) sw_memccpy(writePtr, "ERROR: ", '\0', MAX_LOG_SIZE);
    }

    expectedWriteSize = snprintf(outfmt, MAX_LOG_SIZE, "%s%s\n", msgType, fmt);
    if (expectedWriteSize > MAX_LOG_SIZE) {
        // Silence gcc (>= 7.1) compiler flag `-Wformat-truncation=`, i.e.,
        // handle output truncation
#if defined(RSOILWAT)
        Rf_error("Programmer: message exceeds the maximum size.");
#else
        (void) fprintf(stderr, "Programmer: message exceeds the maximum size.");
#endif
#ifdef SWDEBUG
        exit(EXIT_FAILURE);
#endif
    }

    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    expectedWriteSize = vsnprintf(buf, MAX_LOG_SIZE, outfmt, args);
#ifdef SWDEBUG
    if (expectedWriteSize > MAX_LOG_SIZE) {
#if defined(RSOILWAT)
        Rf_error("Programmer: Injecting arguments to final message buffer "
                 "makes it exceed the maximum size.");
#else
        (void) fprintf(
            stderr,
            "Programmer: Injecting arguments to final message buffer "
            "makes it exceed the maximum size."
        );
#endif
        exit(EXIT_FAILURE);
    }
#else
    (void) expectedWriteSize; /* Silence clang-tidy
                                 clang-analyzer-deadcode.DeadStores */
#endif

    if (LOGWARN & mode) {
        if (nextWarn < MAX_MSGS) {
            (void) sw_memccpy(
                LogInfo->warningMsgs[nextWarn], buf, '\0', MAX_LOG_SIZE
            );
        }
        LogInfo->numWarnings++;
    } else if (LOGERROR & mode) {

#ifdef STEPWAT
        (void) fprintf(stderr, "%s", buf);

        // Consider updating STEPWAT2: instead of exiting/crashing, do catch
        // errors, recoil, and use `sw_write_warnings(); sw_fail_on_error()`
        // as SOILWAT2 >= 7.2.0
        exit(EXIT_FAILURE);
#else
        (void) sw_memccpy(LogInfo->errorMsg, buf, '\0', MAX_LOG_SIZE);
        LogInfo->stopRun = swTRUE;
#endif
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**************************************************************/

/*
@brief Compose, store and count warning and error messages

@param[in,out] LogInfo Holds information on warnings and errors
@param[in] mode Indicator whether message is a warning or error
@param[in] fmt Message string with optional format specifications for ...
    arguments
@param[in] ... Additional values that are injected into fmt
*/
void LogError(LOG_INFO *LogInfo, const int mode, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    LogErrorHelper(LogInfo, mode, fmt, args);

    va_end(args);
}

/*
@brief Similar to `LogError()`, compose, store and count warnings and error
    messages, with the addition of injecting suids into warning/error messages;
    this functionality is only available if it is mode SWMPI or SwNC

@param[in,out] LogInfo Holds information on warnings and errors
@param[in] mode Indicator whether message is a warning or error
@param[in] ncSuid Unique indentifier of the current suid being simulated to
    insert into the produced message
@param[in] sDom Specifies the program's domain is site-oriented
@param[in] fmt Message string with optional format specifications for ...
    arguments
@param[in] ... Additional values that are injected into fmt
*/
void LogErrorSuid(
    LOG_INFO *LogInfo,
    const int mode,
    size_t ncSuid[],
    Bool sDom,
    const char *fmt,
    ...
) {
    va_list args;
    char newFmt[MAX_LOG_SIZE] = "\0";
    int expectedWriteSize;
    /* tag_suid is 55:
    14 character for "(suid = [, ]) " + 40 character for 2 *
    ULONG_MAX + '\0' */
    char tag_suid[55] = "\0";

    va_start(args, fmt);

    if (!isnull(ncSuid)) {
        if (sDom) {
            expectedWriteSize =
                snprintf(tag_suid, 55, "(suid = %lu) ", ncSuid[0] + 1);
        } else {
            expectedWriteSize = snprintf(
                tag_suid,
                55,
                "(suid = [%lu, %lu]) ",
                ncSuid[1] + 1,
                ncSuid[0] + 1
            );
        }

        if (expectedWriteSize > MAX_LOG_SIZE) {
            // Silence gcc (>= 7.1) compiler flag `-Wformat-truncation=`, i.e.,
            // handle output truncation
#if defined(RSOILWAT)
            Rf_error(
                "Programmer: message exceeds the maximum size by adding suids."
            );
#else
            (void) fprintf(
                stderr,
                "Programmer: message exceeds the maximum size by adding suids."
            );
#endif
#ifdef SWDEBUG
            exit(EXIT_FAILURE);
#endif
        }

        expectedWriteSize =
            snprintf(newFmt, MAX_LOG_SIZE, "%s%s", tag_suid, fmt);

        if (expectedWriteSize > MAX_LOG_SIZE) {
            // Silence gcc (>= 7.1) compiler flag `-Wformat-truncation=`,
            // i.e., handle output truncation
#if defined(RSOILWAT)
            Rf_error("Programmer: message exceeds the maximum size by "
                     "adding suid tag.");
#else
            (void) fprintf(
                stderr,
                "Programmer: message exceeds the maximum size by adding "
                "suid tag."
            );
#endif
#ifdef SWDEBUG
            exit(EXIT_FAILURE);
#endif
        }
    }

    LogErrorHelper(LogInfo, mode, (!isnull(ncSuid)) ? newFmt : fmt, args);

    va_end(args);
}

/**
@brief Print a SOILWAT2 status message

@param[in] msg Message string.
*/
void sw_message(const char *msg) {
    char timeString[21];
    timeStringISO8601(timeString, sizeof timeString);

    sw_printf("SOILWAT2 (%s) %s\n", timeString, msg);
#if !defined(RSOILWAT)
    (void) fflush(stdout);
#endif
}

/**
@brief Convert string to largest value possible using the "size_t" integer
    with error handling

@note This function uses unsigned long long to make the maximum value
    consistant across the major platforms (i.e., Linux, Mac, and Windows);
    Assuming 64-bit systems, Windows uses LLP64 where Mac/Linux primarily
use the LP64 data model

This function implements cert-err34-c
"Detect errors when converting a string to a number".

@param[in] str Pointer to string to be converted.
@param[in] errMsg Pointer to string included in error message.
@param[out] LogInfo Holds information on warnings and errors
*/
size_t sw_strtosizet(const char *str, const char *errMsg, LOG_INFO *LogInfo) {
    unsigned long long resul = ULLONG_MAX;
    char *endStr;

    errno = 0;

    resul = strtoull(str, &endStr, 10);

    if (endStr == str || '\0' != *endStr) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: converting '%s' to unsigned long long integer failed.",
            errMsg,
            str
        );

    } else if (ULLONG_MAX == resul && ERANGE == errno) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: '%s' out of range of type unsigned long long integer.",
            errMsg,
            str
        );
    } else if (SIZE_MAX < resul) {
        LogError(
            LogInfo,
            LOGERROR,
            "Value larger than maximum size of an object (%zu < %zu).",
            SIZE_MAX,
            resul
        );
    }

    return (size_t) resul;
}

/**
@brief Convert string to long integer with error handling

This function implements cert-err34-c
"Detect errors when converting a string to a number".

@param[in] str Pointer to string to be converted.
@param[in] errMsg Pointer to string included in error message.
@param[out] LogInfo Holds information on warnings and errors
*/
long int sw_strtol(const char *str, const char *errMsg, LOG_INFO *LogInfo) {
    long int resl = LONG_MIN;
    char *endStr;

    errno = 0;

    resl = strtol(str, &endStr, 10);

    if (endStr == str || '\0' != *endStr) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: converting '%s' to long integer failed.",
            errMsg,
            str
        );

    } else if ((LONG_MIN == resl || LONG_MAX == resl) && ERANGE == errno) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: '%s' out of range of type long integer.",
            errMsg,
            str
        );
    }

    return resl;
}

/**
@brief Convert string to integer with error handling

This function implements cert-err34-c
"Detect errors when converting a string to a number".

@param[in] str Pointer to string to be converted.
@param[in] errMsg Pointer to string included in error message.
@param[out] LogInfo Holds information on warnings and errors
*/
int sw_strtoi(const char *str, const char *errMsg, LOG_INFO *LogInfo) {
    long int resl;
    int resi = INT_MIN;

    resl = sw_strtol(str, errMsg, LogInfo);

    if (!LogInfo->stopRun) {
        if (resl > INT_MAX || resl < INT_MIN) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s: '%s' out of range of type integer.",
                errMsg,
                str
            );

        } else {
            resi = (int) resl;
        }
    }

    return resi;
}

/**
@brief Convert string to double with error handling

This function implements cert-err34-c
"Detect errors when converting a string to a number".

@param[in] str Pointer to string to be converted.
@param[in] errMsg Pointer to string included in error message.
@param[out] LogInfo Holds information on warnings and errors
*/
double sw_strtod(const char *str, const char *errMsg, LOG_INFO *LogInfo) {
    double resd = HUGE_VAL;
    char *endStr;

    errno = 0;

    resd = strtod(str, &endStr);

    if (endStr == str || '\0' != *endStr) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: converting '%s' to double failed.",
            errMsg,
            str
        );

    } else if (HUGE_VAL == resd && ERANGE == errno) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: '%s' out of range of type double.",
            errMsg,
            str
        );
    }

    return resd;
}

/**************************************************************/
Bool GetALine(FILE *f, char buf[], int numChars) {
    /* Read a line of possibly commented input from the file *f.
     * Skip blank lines and comment lines.  Comments within the
     * line are removed and trailing whitespace is removed.
     */
    char *p;
    Bool not_eof = swFALSE;
    while (!isnull(fgets(buf, numChars, f))) {
        p = strchr(buf, '\n');
        if (!isnull(p)) {
            *p = '\0';
        }

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
    const char *c;
    long int l;
    char sep1 = '/';
    char sep2 = '\\';

    *outString = '\0';
    c = strrchr(p, (int) sep1);

    if (!c) {
        c = strrchr(p, (int) sep2);
    }

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
    const char *c;
    char sep1 = '/';
    char sep2 = '\\';

    c = strrchr(p, (int) sep1);
    if (!c) {
        c = strrchr(p, (int) sep2);
    }

    return (isnull(c) ? p : c + 1);
}

/**************************************************************/
FILE *OpenFile(const char *name, const char *mode, LOG_INFO *LogInfo) {
    FILE *fp;
    fp = fopen(name, mode);
    if (isnull(fp)) {
        // Report error if file couldn't be opened
        LogError(LogInfo, LOGERROR, "Cannot open file '%s'", name);
    }
    return (fp);
}

/**************************************************************/
void CloseFile(FILE **f, LOG_INFO *LogInfo) {
    /* This routine is a wrapper for the basic fclose() so
        it might be possible to add more code like error checking
        or other special features in the future.

        Currently, the FILE pointer is set to NULL so it could be
        used as a check for whether the file is opened or not.
        */
    if (*f == NULL) {
        LogError(
            LogInfo, LOGWARN, "CloseFile: file doesn't exist or isn't open!"
        );
        return;
    }

    if (fclose(*f) == EOF) {
        LogError(LogInfo, LOGERROR, "CloseFile: Could not close file.");
    }

    *f = NULL;
}

/**************************************************************/
Bool FileExists(const char *name) {
    /* return swFALSE if name is not a regular file
     * eg, directory, pipe, etc.
     */
    struct stat statbuf;
    Bool result = swFALSE;

    if (0 == stat(name, &statbuf)) {
        result = S_ISREG(statbuf.st_mode) ? swTRUE : swFALSE;
    }

    return (result);
}

/**************************************************************/
Bool DirExists(const char *dname) {
    /* test for existance of a directory
     * dname is name of directory, not including filename
     */

    struct stat statbuf;
    Bool result = swFALSE;

    if (0 == stat(dname, &statbuf)) {
        result = S_ISDIR(statbuf.st_mode) ? swTRUE : swFALSE;
    }

    return (result);
}

/**************************************************************/
Bool ChDir(const char *dname) {
    /* wrapper in case portability problems come up */

    return (chdir(dname) == 0) ? swTRUE : swFALSE;
}

/**************************************************************/
/* Mapping mdir() function to OS specific version */

#ifdef _WIN32 /* 32- and 64-bit Windows OS: Windows XP, Vista, 7, 8 */
#define mkdir(d, m) mkdir(d)
#elif _WIN64
#define mkdir(d, m) mkdir(d)
#elif __linux__ /* linux: Centos, Debian, Fedora, OpenSUSE, RedHat, Ubuntu */
#define mkdir(d, m) mkdir(d, m)
#elif __APPLE__ && __MACH__ /* (Mac) OS X, macOS, iOS, Darwin */
#define mkdir(d, m) mkdir(d, m)
#endif

// Errors are reported via LogInfo
void MkDir(const char *dname, LOG_INFO *LogInfo) {
    /* make a path with 'mkdir -p' -like behavior. provides an
     * interface for portability problems.
     * RELATIVE PATH ONLY solves problems like "C:\etc" and null
     * first element in absolute path.
     * if you need to make an absolute path, use ChDir() first.
     * if you care about mode of new dir, use mkdir(), not MkDir()
     *
     * Notes:
     * - portability issues seem to be quite problematic, at least
     *   between platforms and GCC and Borland.  The only common
     *   error code is EACCES, so if something else happens (and it
     *   well might in unix), more tests have to be included, perhaps
     *   with macros that test the compiler/platform.
     * - we're borrowing buffer to build the path to facilitate the
     *   -p behavior.
     */

    int i;
    int n;
    char *a[256] = {0}; /* points to each path element for mkdir -p behavior */
    char *c;            /* duplicate of dname so we don't change it */
    const char *delim = "\\/"; /* path separators */
    char buffer[MAX_ERROR];
    char *writePtr = buffer;
    char *endBuffer = buffer + sizeof buffer - 1;

    size_t startIndex = 0;
    size_t strLen = 0; // For `sw_strtok()`
    size_t writeSize = 256;
    Bool fullBuffer = swFALSE;

    if (isnull(dname)) {
        return;
    }

    c = Str_Dup(dname, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    /* parse path */
    n = 0;
    while (NULL != (a[n] = sw_strtok(c, &startIndex, &strLen, delim)) && n < 256
    ) {
        n++;
    }

    buffer[0] = '\0';
    for (i = 0; i < n; i++) {
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endBuffer, (void *) a[i], '\0', &writeSize
        );
        if (fullBuffer) {
            goto freeMem;
        }

        if (!DirExists(buffer)) {
            if (0 != mkdir(buffer, 0777)) {
                // directory failed to create -> report error
                LogError(
                    LogInfo, LOGERROR, "Failed to create directory '%s'", buffer
                );
                goto freeMem; // Exit function prematurely due to error
            }
        }

        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endBuffer, (void *) "/", '\0', &writeSize
        );
        if (fullBuffer) {
            goto freeMem;
        }
    }

freeMem:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }

    free(c);
}

#undef mkdir

/**************************************************************/
Bool RemoveFiles(
    const char *fspec, Bool clearDir, int maxDepth, LOG_INFO *LogInfo
) {
    /* delete files matching fspec. ASSUMES terminal element
     *   describes files, ie, is not a directory.
     * fspec may contain leading path (eg, here/and/now/files)
     * fspec can't be too complicated because I'm not including the
     *   full regexp library.  Specifically you can include a *
     *   anywhere in the terminal path member,
     *   eg: /here/now/fi*les, /here/now/files*
     *   or /here/now/files
     * Returns TRUE if all files removed, FALSE otherwise.
     */

    Bool result = swTRUE;
    char *baseName;
    char *fn1;
    char *fn2;

    size_t len1;
    size_t len2;
    size_t startIndex = 0;
    size_t strLen = 0; // For `sw_strtok()`

    if (isnull(fspec)) {
        return swTRUE;
    }

    baseName = Str_Dup(BaseName(fspec), LogInfo);

    if (strchr(baseName, '*')) {
        fn1 = sw_strtok(baseName, &startIndex, &strLen, "*");
        fn2 = sw_strtok(baseName, &startIndex, &strLen, "*");

        // Transfer extensions from fn1 to fn2 to catch the second
        // conditional of `RemoveFilesHelper()`
        if (fn1 && !fn2) {
            fn2 = fn1;
            fn1 = NULL;
        }
    } else {
        fn1 = baseName;
        fn2 = NULL;
    }
    len1 = (fn1) ? strlen(fn1) : 0;
    len2 = (fn2) ? strlen(fn2) : 0;

    result = RemoveFilesHelper(
        fspec, len1, len2, clearDir, fn1, fn2, 0, maxDepth, LogInfo
    );

    free(baseName);

    return result;
}

/**
@brief Copy a file

@param[in] from The file path of the source (original) file.
@param[in] to The file path to the file copy (destination).
@param[out] LogInfo Holds information on warnings and errors

@return swTRUE on success and swFALSE on failure.
*/
Bool CopyFile(const char *from, const char *to, LOG_INFO *LogInfo) {
    char buffer[4096]; // or any other constant that is a multiple of 512
    size_t n;
    FILE *ffrom;
    FILE *fto;

    ffrom = fopen(from, "r");
    if (ffrom == NULL) {
        LogError(
            LogInfo,
            LOGERROR,
            "CopyFile: error opening source file %s.\n",
            *from
        );
        return swFALSE; // Exit function prematurely due to error
    }

    fto = fopen(to, "w");
    if (fto == NULL) {
        LogError(
            LogInfo,
            LOGERROR,
            "CopyFile: error opening destination file %s.\n",
            to
        );
        return swFALSE; // Exit function prematurely due to error
    }

    while ((n = fread(buffer, 1, sizeof buffer, ffrom)) > 0) {
        if (fwrite(buffer, 1, n, fto) != n) {
            LogError(
                LogInfo, LOGERROR, "CopyFile: error while copying to %s.\n", to
            );
            (void) fclose(ffrom);
            (void) fclose(fto);
            return swFALSE; // Exit function prematurely due to error
        }
    }

    if (ferror(ffrom)) {
        LogError(
            LogInfo, LOGERROR, "CopyFile: error reading source file %s.\n", from
        );
        (void) fclose(ffrom);
        (void) fclose(fto);
        return swFALSE; // Exit function prematurely due to error
    }

    // clean up
    if (fclose(ffrom) == EOF) {
        LogError(
            LogInfo, LOGERROR, "CopyFile: error closing source file %s.\n", from
        );
        return swFALSE; // Exit function prematurely due to error
    }

    if (fclose(fto) == EOF) {
        LogError(
            LogInfo,
            LOGERROR,
            "CopyFile: error closing destination file %s.\n",
            to
        );
        return swFALSE; // Exit function prematurely due to error
    }

    return swTRUE;
}

/**
@brief Convert a key read-in from an input file to an index

@param[in] key Key found within the file to test for
@param[in] possibleKeys A list of possible keys that can be found
@param[in] numPossKeys Number of keys within `possibleKeys`
*/
int key_to_id(const char *key, const char **possibleKeys, int numPossKeys) {
    int id;

    for (id = 0; id < numPossKeys; id++) {
        if (strcmp(key, possibleKeys[id]) == 0) {
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
void set_hasKey(
    int keyID, const char **possibleKeys, Bool *hasKeys, LOG_INFO *LogInfo
) {
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
@param[in] possibleKeys A list of possible keys (used for error message).
@param[in] numKeys Number of keys.
@param[out] LogInfo Holds information on warnings and errors
*/
void check_requiredKeys(
    const Bool *hasKeys,
    const Bool *requiredKeys,
    const char **possibleKeys,
    int numKeys,
    LOG_INFO *LogInfo
) {
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
