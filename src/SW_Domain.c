
/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Domain.h"      // for SW_DOM_CheckProgress, SW_DOM_Cre...
#include "include/filefuncs.h"      // for LogError, CloseFile, key_to_id
#include "include/generic.h"        // for swTRUE, LOGERROR, swFALSE, Bool
#include "include/myMemory.h"       // for sw_memccpy_custom
#include "include/SW_datastructs.h" // for SW_DOMAIN, LOG_INFO
#include "include/SW_Defines.h"     // for LyrIndex, LARGE_VALUE, TimeInt
#include "include/SW_Files.h"       // for SW_F_deconstruct, SW_F_deepCopy
#include "include/SW_Output.h"      // for ForEachOutKey
#include "include/Times.h"          // for yearto4digit, Time_get_lastdoy_y
#include <stdio.h>                  // for sscanf, FILE
#include <stdlib.h>                 // for strtod, strtol
#include <string.h>                 // for strcmp, memcpy, memset

#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"

#if defined(SWMPI)
#include "include/SW_MPI.h"
#endif
#endif

#if defined(SOILWAT)
#include "include/rands.h" // for RandSeed
#endif


/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_DOM_IN_KEYS 19 // Number of possible keys within `domain.in`

/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */

#if defined(SWMPI)
/*
@brief Allocate helper arrays to keep track of starts/counts for
latitude and longitude dimensions

@param[in] nChunks A list of size NC_DIMS to hold the number of chunks that
will be contained in the latitude/site and longitude directions
@param[in] allocYX A flag specifying if both Y and X arrays should be
allocated, only Y if not
@param[in] alloc A flag specifying if the function is to allocate the given
arrays or to deallocate
@param[out] startY A list that will contain starting indices for each process
in the latitude or site direction; only return an allocated list
@param[out] startX A list that will contain starting indices for each process
in the longitude direction; only return an allocated list
@param[out] countY A list that will contain count sizes for each process
in the latitude or site direction; only return an allocated list
@param[out] countX A list that will contain count sizes for each process
in the longitude direction; only return an allocated list
@param[out] LogInfo Holds information dealing with logfile output
*/
static void alloc_dom_start_count(
    const size_t nChunks[],
    Bool allocYX,
    Bool alloc,
    size_t **startY,
    size_t **startX,
    size_t **countY,
    size_t **countX,
    LOG_INFO *LogInfo
) {
    if (alloc) {
        *startY = (size_t *) Mem_Malloc(
            sizeof(size_t) * nChunks[0], "get_sub_domains", LogInfo
        );
        checkReturn(LogInfo->stopRun);

        *countY = (size_t *) Mem_Malloc(
            sizeof(size_t) * nChunks[0], "get_sub_domains", LogInfo
        );
        checkReturn(LogInfo->stopRun);

        if (allocYX) {
            *startX = (size_t *) Mem_Malloc(
                sizeof(size_t) * nChunks[1], "get_sub_domains", LogInfo
            );
            checkReturn(LogInfo->stopRun);

            *countX = (size_t *) Mem_Malloc(
                sizeof(size_t) * nChunks[1], "get_sub_domains", LogInfo
            );
        }
    } else {
        if (!isnull(*startY)) {
            free((void *) *startY);
        }

        if (!isnull(*startX)) {
            free((void *) *startX);
        }

        if (!isnull(*countY)) {
            free((void *) *countY);
        }

        if (!isnull(*countX)) {
            free((void *) *countX);
        }
    }
}

/*
@brief Set the subdomain information for all processes and create a list
of active global suids from the local subdomain

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] nChunks A list of size NC_DIMS to hold the number of chunks that
will be contained in the lat and lon directions
@param[in] A list that is filled with starting indices for each process
in the latitude or site direction
@param[in] A list that is filled with starting indices for each process
in the longitude direction
@param[in] A list that is filled with count sizes for each process
in the latitude or site direction
@param[in] A list that is filled with count sizes for each process
in the longitude direction
@param[out] domStartIndexProg A list of size NC_DIMS to hold the start
index (site domain)/indices (gridded) of the subdomain for a process
@param[out] domCountsProg A list of size NC_DIMS to hold the counts
in each dimension(s) of the subdomain for a process, i.e., the size
of the subdomain
*/
static void assign_subdomain(
    int rank,
    Bool isSimDomDiscrete,
    const size_t nChunks[],
    const size_t *startY,
    const size_t *startX,
    const size_t *countY,
    const size_t *countX,
    size_t domStartIndexProg[],
    size_t domCountsProg[]
) {
    const size_t chunkYX =
        (isSimDomDiscrete) ? (size_t) rank : rank / nChunks[1];
    const size_t chunkX = (isSimDomDiscrete) ? 0 : rank % nChunks[1];

    domStartIndexProg[0] = startY[chunkYX];

    // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
    domStartIndexProg[1] = (isSimDomDiscrete) ? 0 : startX[chunkX];

    // NOLINTBEGIN(clang-analyzer-core.NullDereference)
    domCountsProg[0] = countY[chunkYX];
    domCountsProg[1] = (isSimDomDiscrete) ? 0 : countX[chunkX];
    // NOLINTEND(clang-analyzer-core.NullDereference)
}

/*
@brief Check that the generated subdomain start and count values

@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] nChunks A list of size NC_DIMS to hold the number of chunks that
will be contained in the lat and lon directions
@param[in] ysSize Size of the latitude/y or site dimension
@param[in] xSize Size of the longitude/x dimension
@param[in] startY A list that is filled with starting indices for each process
in the latitude or site direction
@param[in] startX A list that is filled with starting indices for each process
in the longitude direction
@param[in] countY A list that is filled with count sizes for each process
in the latitude or site direction
@param[in] countX A list that is filled with count sizes for each process
in the longitude direction
@param[out] LogInfo Holds information dealing with logfile output
*/
static void check_valid_subdomains(
    Bool isSimDomDiscrete,
    const size_t nChunks[],
    size_t ysSize,
    size_t xSize,
    const size_t *startY,
    const size_t *startX,
    const size_t *countY,
    const size_t *countX,
    LOG_INFO *LogInfo
) {
    const size_t nRowChunks = nChunks[0];
    const size_t nColChunks = nChunks[1];

    size_t index;
    size_t sum = 0;
    Bool fail = swFALSE;

    for (index = 0; index < nRowChunks && !fail; index++) {
        // Typically, a negative value would be tested for, however
        // if a negetive were to show, it would be close to max size_t
        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        if (startY[index] > ysSize || countY[index] > ysSize) {
            fail = swTRUE;
        }

        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        sum += countY[index];
    }

    fail = (Bool) (fail || (!isSimDomDiscrete && sum != ysSize));

    if (!isSimDomDiscrete && !isnull(startX) && !fail) {
        sum = 0;
        for (index = 0; index < nColChunks && !fail; index++) {
            // Typically, a negative value would be tested for, however
            // if a negetive were to show, it would be close to max size_t

            // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
            if (startX[index] > xSize || countX[index] > xSize) {
                fail = swTRUE;
            }

            // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
            sum += countX[index];
        }

        fail = (Bool) (fail || (!isSimDomDiscrete && sum != xSize));
    }

    if (fail) {
        LogError(
            LogInfo,
            LOGERROR,
            "Subdomains could not be distributed properly. Please "
            "try a different number of processes."
        );
    }
}

/*
@brief Calculate the start/count values for every chunk we will create

@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] nChunks A list of size NC_DIMS to hold the number of chunks that
will be contained in the lat and lon directions
@param[in] ysSize Size of the latitude/y or site dimension
@param[in] xSize Size of the longitude/x dimension
@param[out] startY A list that will be filled with starting indices for each
process in the latitude or site direction
@param[out] startX A list that will be filled with starting indices for each
process in the longitude direction
@param[out] countY A list that will be filled with count sizes for each process
in the latitude or site direction
@param[out] countX A list that will be filled with count sizes for each process
in the longitude direction
*/
static void calc_subdomain_start_counts(
    Bool isSimDomDiscrete,
    const size_t nChunks[],
    size_t ysSize,
    size_t xSize,
    size_t *startY,
    size_t *startX,
    size_t *countY,
    size_t *countX
) {
    const size_t rowRemainDef = ysSize / nChunks[0];
    const size_t colRemainDef = isSimDomDiscrete ? 0 : xSize / nChunks[1];

    size_t rowRemainder = ysSize % nChunks[0];
    size_t colRemainder = isSimDomDiscrete ? 0 : xSize % nChunks[1];
    size_t row;
    size_t col;

    // Calculate the starts for each chunk
    startY[0] = 0;

    // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
    countY[0] = ysSize;
    for (row = 1; row < nChunks[0]; row++) {
        startY[row] = startY[row - 1] + rowRemainDef;

        if (rowRemainder > 0) {
            startY[row]++;
            rowRemainder--;
        }
    }

    if (!isSimDomDiscrete) {
        // NOLINTBEGIN(clang-analyzer-core.NullDereference)
        startX[0] = 0;
        countX[0] = xSize;
        // NOLINTEND(clang-analyzer-core.NullDereference)

        for (col = 1; col < nChunks[1]; col++) {
            startX[col] = startX[col - 1] + colRemainDef;

            if (colRemainder > 0) {
                colRemainder--;
                startX[col]++;
            }
        }
    }

    // Calculate the counts for each chunk
    for (row = 1; row < nChunks[0]; row++) {
        countY[row - 1] = startY[row] - startY[row - 1];
    }
    countY[nChunks[0] - 1] = ysSize - startY[nChunks[0] - 1];

    if (!isSimDomDiscrete) {
        for (col = 1; col < nChunks[1]; col++) {
            countX[col - 1] = startX[col] - startX[col - 1];
        }
        countX[nChunks[1] - 1] = xSize - startX[nChunks[1] - 1];
    }
}

/*
@brief Divide a given gridded domain into subdomains/subrectangles
to provide to each process

@param[in] worldSize Total number of processes that the MPI run has created
(only relevant with SWMPI enabled)
@param[in] ySize Size of the latitude/y dimension
@param[in] xSize Size of the longitude/x dimension
@param[out] spaceChunks A list of size NC_DIMS to hold the default
chunk size when creating/writing to output files; return with the
array filled
@param[out] nChunks A list of size NC_DIMS to hold the number of chunks that
will be contained in the lat and lon directions
*/
static void divide_domain_subrects(
    size_t worldSize,
    size_t ySize,
    size_t xSize,
    size_t spaceChunks[],
    size_t nChunks[]
) {
    size_t bestNChunksY = worldSize;
    size_t bestNChunksX = 1;
    size_t height;
    size_t width;
    size_t bestDiff = bestNChunksY - bestNChunksX;
    size_t currDiff;

    for (height = 1; height < worldSize; height++) {
        width = worldSize / height;

        currDiff = height - width;
        if (worldSize % height == 0 &&
            abs((int) currDiff) < abs((int) bestDiff)) {

            bestNChunksY = height;
            bestNChunksX = width;

            bestDiff = bestNChunksY - bestNChunksX;
        }
    }

    nChunks[0] = bestNChunksY;
    nChunks[1] = bestNChunksX;

    spaceChunks[0] = ySize / bestNChunksY;
    spaceChunks[1] = xSize / bestNChunksX;
}
#endif

/**
@brief Calculate the program's subdomain for each process to control

@param[in] worldSize Total number of processes that the MPI run has created
(only relevant with SWMPI enabled)
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_subdomains(
    size_t worldSize,
    Bool isSimDomDiscrete,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
) {
    size_t sSize = SW_Domain->nDimS;
    size_t ySize = SW_Domain->nDimY;
    size_t xSize = SW_Domain->nDimX;

#if defined(SWMPI)
    const Bool allocate = swTRUE;
    const Bool deallocate = swFALSE;
    size_t *startsY = NULL;
    size_t *startsX = NULL;
    size_t *countsY = NULL;
    size_t *countsX = NULL;
    size_t nChunks[NC_DIMS] = {0};
    Bool allocBothArrs = (Bool) !isSimDomDiscrete;

    if ((isSimDomDiscrete && worldSize > sSize) ||
        (!isSimDomDiscrete && worldSize > ySize * xSize)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Too many processes spawned to properly divide the domain. "
            "Please use at most the number of sites worth of processes."
        );
        return;
    }

    // Check if domain lat/site or lon is divisible by worldSize
    if ((isSimDomDiscrete && worldSize <= sSize) ||
        (!isSimDomDiscrete && (worldSize <= ySize || worldSize <= xSize))) {

        if (isSimDomDiscrete) {
            nChunks[0] = worldSize;
            nChunks[1] = SW_Domain->spaceChunk[1] = 0;

            SW_Domain->spaceChunk[0] = sSize / worldSize;
        } else {
            nChunks[0] = (worldSize <= ySize) ? worldSize : 1;
            nChunks[1] =
                (worldSize <= xSize && worldSize > ySize) ? worldSize : 1;

            SW_Domain->spaceChunk[0] =
                (size_t) ceil((double) ySize / (double) nChunks[0]);
            SW_Domain->spaceChunk[1] =
                (size_t) ceil((double) xSize / (double) nChunks[1]);
        }
    } else {
        // Otherwise, the domain needs to be split into subrectangles
        divide_domain_subrects(
            worldSize, ySize, xSize, SW_Domain->spaceChunk, nChunks
        );
    }

    // Allocate start/count arrays
    // If method one above, only allocate one start and count
    // Otherwise, allocate two for start and count
    alloc_dom_start_count(
        nChunks,
        allocBothArrs,
        allocate,
        &startsY,
        &startsX,
        &countsY,
        &countsX,
        LogInfo
    );
    checkJumpToLabel(LogInfo->stopRun, freeMem);

    calc_subdomain_start_counts(
        isSimDomDiscrete,
        nChunks,
        (isSimDomDiscrete) ? sSize : ySize,
        xSize,
        startsY,
        startsX,
        countsY,
        countsX
    );

    // Check that the subdomains are properly divided into
    // Fail if not
    check_valid_subdomains(
        isSimDomDiscrete,
        nChunks,
        (isSimDomDiscrete) ? sSize : ySize,
        xSize,
        startsY,
        startsX,
        countsY,
        countsX,
        LogInfo
    );
    checkJumpToLabel(LogInfo->stopRun, freeMem);

    // Set the subdomains
    // Get the start and end values that pertain to the process
    assign_subdomain(
        SW_Domain->rank,
        isSimDomDiscrete,
        nChunks,
        startsY,
        startsX,
        countsY,
        countsX,
        SW_Domain->domStartIndex[eSW_InDomain],
        SW_Domain->domCounts[eSW_InDomain]
    );

    // Deallocate arrays
freeMem:
    // NOLINTNEXTLINE(readability-suspicious-call-argument)
    alloc_dom_start_count(
        nChunks,
        allocBothArrs,
        deallocate,
        &startsY,
        &startsX,
        &countsY,
        &countsX,
        LogInfo
    );
#elif defined(SWNETCDF)
    SW_Domain->domCounts[eSW_InDomain][0] = isSimDomDiscrete ? sSize : ySize;
    SW_Domain->domCounts[eSW_InDomain][1] = isSimDomDiscrete ? 0 : xSize;

    SW_Domain->domStartIndex[eSW_InDomain][0] =
        SW_Domain->domStartIndex[eSW_InDomain][1] = 0;
    SW_Domain->spaceChunk[0] =
        (size_t) (isSimDomDiscrete ? sqrt((double) sSize) : sqrt((double) ySize)
        );
    SW_Domain->spaceChunk[1] =
        (size_t) (isSimDomDiscrete ? 0 : sqrt((double) xSize));
#else
    (void) SW_Domain;
    (void) isSimDomDiscrete;
    (void) sSize;
    (void) ySize;
    (void) xSize;
#endif

#if !defined(SWMPI)
    (void) worldSize;
    (void) LogInfo;
#endif
}

/**
@brief Interface function to get the starting day of the simulation from
progress file

@param[in] cacheFileName Name of the program's generated cache file
@param[in] progTimeVarID Identifier of the variable within the target
netCDF that the progress day resides
@param[out] startDay Start day value read from progress file
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_start_sim_day(
    const char *cacheFileName, IntU *startDay, LOG_INFO *LogInfo
) {
    // Silence Clang Tidy attempting to set this to "const" for text/nc-mode
    *startDay = 1;

#if defined(SWNETCDF)
    if (FileExists(cacheFileName)) {
        SW_NCIN_get_start_sim_day((char *) cacheFileName, startDay, LogInfo);
    }
#else
    (void) cacheFileName;
    (void) LogInfo;
#endif
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Calculate the suid for the start gridcell/site position

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] suid Unique identifier for a simulation run
@param[out] ncSuid Unique indentifier of the first suid to run
    in relation to netCDFs
*/
void SW_DOM_calc_ncSuid(SW_DOMAIN *SW_Domain, size_t suid, size_t ncSuid[]) {

    if (SW_Domain->isSimDomDiscrete) {
        ncSuid[0] = suid;
        ncSuid[1] = 0;
    } else {
        ncSuid[0] = suid / SW_Domain->nDimX;
        ncSuid[1] = suid % SW_Domain->nDimX;
    }
}

/**
@brief Calculate the suid from a global point-of-view from subdomain
information

@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] startYS Start index of the Y dimension (gridded) or S dimension
(site-oriented) of the assigned subdomain
@param[in] startX Start index of the X dimension (gridded) of the assigned
subdomain
@param[in] actSiteIdx Active site index within the subdomain
@param[in] nCols Number of columns in the subdomain
@param[out] ncSuid Resulting global ncSuid calculation
*/
void SW_DOM_calc_suid_from_subdom(
    Bool isSimDomDiscrete,
    size_t startYS,
    size_t startX,
    size_t actSiteIdx,
    size_t nCols,
    size_t ncSuid[]
) {
    size_t ysOffset = isSimDomDiscrete ? actSiteIdx : actSiteIdx / nCols;
    size_t xOffset = isSimDomDiscrete ? 0 : actSiteIdx % nCols;

    ncSuid[0] = startYS + ysOffset;
    ncSuid[1] = isSimDomDiscrete ? 0 : startX + xOffset;
}

/**
@brief Calculate the number of suids in the given domain

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
*/
void SW_DOM_calc_nSUIDs(SW_DOMAIN *SW_Domain) {
    SW_Domain->nSUIDs = (SW_Domain->isSimDomDiscrete) ?
                            SW_Domain->nDimS :
                            SW_Domain->nDimX * SW_Domain->nDimY;
}

/**
@brief Check progress in domain

@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] ncSuid Current simulation unit identifier for which is used
    to get data from netCDF
@param[in,out] LogInfo Holds information dealing with logfile output

@return
TRUE if simulation for \p ncSuid has not been completed yet;
FALSE if simulation for \p ncSuid has been completed (i.e., skip).
*/
Bool SW_DOM_CheckProgress(
    int progFileID,
    int progVarID,
    size_t ncSuid[], // NOLINT(readability-non-const-parameter)
    LOG_INFO *LogInfo
) {
#if defined(SWNETCDF)
    return SW_NCIN_check_progress(progFileID, progVarID, ncSuid, LogInfo);
#else
    (void) progFileID;
    (void) progVarID;
    (void) ncSuid;
    (void) LogInfo;
#endif

    // return TRUE (due to lack of capability to track progress)
    return swTRUE;
}

/**
@brief Create an empty progress netCDF

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] LogInfo Holds information dealing with logfile output
*/
void SW_DOM_CreateProgress(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
#if defined(SWNETCDF)
    SW_NCIN_create_progress(SW_Domain, LogInfo);
#else
    (void) SW_Domain;
    (void) LogInfo;
#endif
}

/**
@brief Domain constructor for global variables which are constant between
    simulation runs.

@param[in] rng_seed Initial state for spinup RNG
@param[out] SW_Domain Struct of type SW_DOMAIN which
    holds constant temporal/spatial information for a set of simulation runs
*/
void SW_DOM_construct(size_t rng_seed, SW_DOMAIN *SW_Domain) {

/* Set seed of `spinup_rng`
  - SOILWAT2: set seed here
  - STEPWAT2: `main()` uses `Globals.randseed` to (re-)set for each iteration
  - rSOILWAT2: R API handles RNGs
*/
#if defined(SOILWAT)
    RandSeed(rng_seed, 1u, &SW_Domain->SW_SpinUp.spinup_rng);
#else
    (void) rng_seed; // Silence compiler flag `-Wunused-parameter`
#endif

    SW_Domain->nMaxSoilLayers = 0;
    SW_Domain->hasConsistentSoilLayerDepths = swFALSE;
    memset(
        &SW_Domain->depthsAllSoilLayers,
        0,
        sizeof(&SW_Domain->depthsAllSoilLayers[0]) * MAX_LAYERS
    );
    SW_Domain->nActiveSuidsProc = 1;
    SW_Domain->nActiveSuidsTot = 1;
    SW_Domain->nErrBeforeFail = 1;
    SW_Domain->startSimDay = SW_Domain->startstart;
    SW_Domain->endSimDay = 0;
    SW_Domain->nSitesInSubDom = 1;

#if defined(SWNETCDF)
    int inKey;
    int outKey;
    int pd;

    SW_Domain->spaceChunk[0] = SW_Domain->spaceChunk[1] = 0;

    ForEachNCInKey(inKey) {
        SW_Domain->domStartIndex[inKey][0] =
            SW_Domain->domStartIndex[inKey][1] = 0;
        SW_Domain->domCounts[inKey][0] = SW_Domain->domCounts[inKey][1] = 0;
    }

    ForEachOutKey(outKey) {
        ForEachOutPeriod(pd) {
            SW_Domain->OutDom.netCDFOutput.runOutFileIndex[inKey][pd] = 0;
            SW_Domain->OutDom.netCDFOutput.outTempStart[inKey][pd] = 0;
        }
    }

    SW_Domain->SW_PathInputs.weathStartFileIndex = 0;

    SW_Domain->fileSystemStripeSize = 0;
    SW_Domain->availMemory = 0;
#endif

    SW_OUTDOM_construct(&SW_Domain->OutDom);
}

/**
@brief Read `domain.in` and report any problems encountered when doing so

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_DOM_read(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {

    static const char *possibleKeys[NUM_DOM_IN_KEYS] = {
        "Domain",
        "nDimX",
        "nDimY",
        "nDimS",
        "StartYear",
        "EndYear",
        "StartDoy",
        "EndDoy",
        "crs_bbox",
        "xmin_bbox",
        "ymin_bbox",
        "xmax_bbox",
        "ymax_bbox",
        "SpinupMode",
        "SpinupScope",
        "SpinupDuration",
        "SpinupSeed",
        "SpatialTolerance",
        "MaxPercSimErrors"
    };
    static const Bool requiredKeys[NUM_DOM_IN_KEYS] = {
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swFALSE,
        swFALSE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
#if defined(SWMPI)
        swTRUE
#else
        swFALSE
#endif
    };
    Bool hasKeys[NUM_DOM_IN_KEYS] = {swFALSE};

    FILE *f;
    int y;
    int keyID;
    char inbuf[LARGE_VALUE];
    char *MyFileName;
    char key[17]; /* 17 - max key size plus null-terminating character */
    char value[LARGE_VALUE];
    int intRes = 0;
    int scanRes;
    double doubleRes = 0.;
    char *errDim = NULL;

    Bool doDoubleConv;

    MyFileName = SW_Domain->SW_PathInputs.txtInFiles[eDomain];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Set SW_DOMAIN
    while (GetALine(f, inbuf, LARGE_VALUE)) {
        scanRes = sscanf(inbuf, "%16s %s", key, value);

        if (scanRes < 2) {
            LogError(
                LogInfo, LOGERROR, "Invalid key-value pair in %s.", MyFileName
            );
            goto closeFile;
        }

        keyID = key_to_id(key, possibleKeys, NUM_DOM_IN_KEYS);

        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo);
        // set_hasKey() produces never an error, only possibly warnings

        /* Make sure we are not trying to convert a string with no numerical
         * value */
        if (keyID > 0 && keyID <= 18 && keyID != 8) {

            /* Check to see if the line number contains a double or integer
             * value */
            doDoubleConv = (Bool) ((keyID >= 9 && keyID <= 12) ||
                                   (keyID >= 17 && keyID <= 18));

            if (doDoubleConv) {
                doubleRes = sw_strtod(value, MyFileName, LogInfo);
            } else {
                intRes = sw_strtoi(value, MyFileName, LogInfo);

                /* Check to see if there are any unexpected negative or
                   zero values */
                if (intRes <= 0) {
                    if (keyID >= 1 && keyID <= 3) { /* > 0 */
                        if (keyID == 1) {
                            errDim = (char *) "X";
                        } else {
                            errDim = (keyID == 2) ? (char *) "Y" : (char *) "S";
                        }
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "%s: Dimension '%s' should be > 0.",
                            MyFileName,
                            errDim
                        );
                    } else if (keyID >= 4 && keyID <= 5) { /* >= 0 */
                        if (intRes < 0) {
                            LogError(
                                LogInfo,
                                LOGERROR,
                                "%s: Negative %s year (%d)",
                                MyFileName,
                                (keyID == 4) ? "start" : "end",
                                intRes
                            );
                        }
                    }
                }
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (keyID) {
        case 0: // Domain type
            if (strcmp(value, "xy") != 0 && strcmp(value, "s") != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Incorrect domain type %s."
                    " Please select from \"xy\" and \"s\".",
                    MyFileName,
                    value
                );
                goto closeFile;
            }
            SW_Domain->isSimDomDiscrete = (Bool) (strcmp(value, "s") == 0);
            break;
        case 1: // Number of X slots
            SW_Domain->nDimX = (size_t) intRes;
            break;
        case 2: // Number of Y slots
            SW_Domain->nDimY = (size_t) intRes;
            break;
        case 3: // Number of S slots
            SW_Domain->nDimS = (size_t) intRes;
            break;

        case 4: // Start year
            SW_Domain->startyr = yearto4digit((TimeInt) intRes);
            break;
        case 5: // End year
            SW_Domain->endyr = yearto4digit((TimeInt) intRes);
            break;
        case 6: // Start day of year
            SW_Domain->startstart = (TimeInt) intRes;
            break;
        case 7: // End day of year
            SW_Domain->endend = (TimeInt) intRes;
            break;

        case 8: // CRS box
            // Re-scan and get the entire value (including spaces)
            scanRes = sscanf(inbuf, "%9s %27[^\n]", key, value);

            if (scanRes < 2) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid key-value pair for CRS box in %s.",
                    MyFileName
                );
                goto closeFile;
            }

            (void) sw_memccpy(
                SW_Domain->crs_bbox, value, '\0', sizeof SW_Domain->crs_bbox
            );
            break;
        case 9: // Minimum x coordinate
            SW_Domain->min_x = doubleRes;
            break;
        case 10: // Minimum y coordinate
            SW_Domain->min_y = doubleRes;
            break;
        case 11: // Maximum x coordinate
            SW_Domain->max_x = doubleRes;
            break;
        case 12: // Maximum y coordinate
            SW_Domain->max_y = doubleRes;
            break;

        case 13: // Spinup Mode
            y = intRes;

            if (y != 1 && y != 2) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Incorrect Mode (%d) for spinup"
                    " Please select \"1\" or \"2\"",
                    MyFileName,
                    y
                );
                goto closeFile;
            }
            SW_Domain->SW_SpinUp.mode = y;
            break;
        case 14: // Spinup Scope
            SW_Domain->SW_SpinUp.scope = (TimeInt) intRes;
            break;
        case 15: // Spinup Duration
            SW_Domain->SW_SpinUp.duration = (TimeInt) intRes;

            // Set the spinup flag to true if duration > 0
            SW_Domain->SW_SpinUp.spinup =
                (Bool) (SW_Domain->SW_SpinUp.duration > 0);
            break;
        case 16: // Spinup Seed
            SW_Domain->SW_SpinUp.rng_seed = (size_t) intRes;
            break;
        case 17:
            SW_Domain->spatialTol = doubleRes;

            if (LT(SW_Domain->spatialTol, 0.0)) {
                LogError(LogInfo, LOGERROR, "Spatial tolerance must be >= 0.");
            }
            break;
        case 18:
            SW_Domain->maxPercSimErrors = doubleRes;

            if (LT(SW_Domain->maxPercSimErrors, 0.) ||
                GT(SW_Domain->maxPercSimErrors, 100.)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Max simulation errors percent must be between [0, 100]%%."
                );
            }
            break;

        case KEY_NOT_FOUND: // Unknown key

        default:
            LogError(
                LogInfo,
                LOGWARN,
                "%s: Ignoring an unknown key, %s",
                MyFileName,
                key
            );
            break;
        }
    }


    // Check if all required input was provided
    check_requiredKeys(
        hasKeys, requiredKeys, possibleKeys, NUM_DOM_IN_KEYS, LogInfo
    );
    if (LogInfo->stopRun) {
        goto closeFile;
    }

    if (SW_Domain->endyr < SW_Domain->startyr) {
        LogError(LogInfo, LOGERROR, "%s: Start Year > End Year", MyFileName);
        goto closeFile;
    }

    // Check if start day of year was not found
    keyID = key_to_id("StartDoy", possibleKeys, NUM_DOM_IN_KEYS);
    if (!hasKeys[keyID]) {
        LogError(LogInfo, LOGWARN, "Domain.in: Missing Start Day - using 1\n");
        SW_Domain->startstart = 1;
    } else if (SW_Domain->startstart > 1) {
        LogError(
            LogInfo,
            LOGWARN,
            "%s: StartDoy value is > 1, this is currently not fully supported "
            "and may result in unexpected outputs.",
            MyFileName
        );
    }

    // Check end day of year
    keyID = key_to_id("EndDoy", possibleKeys, NUM_DOM_IN_KEYS);
    if (SW_Domain->endend == 365 || SW_Domain->endend == 366 ||
        !hasKeys[keyID]) {
        // Make sure last day is correct for given last year
        SW_Domain->endend = Time_get_lastdoy_y(SW_Domain->endyr);
    }
    if (!hasKeys[keyID]) {
        LogError(
            LogInfo,
            LOGWARN,
            "Domain.in: Missing End Day - using %d\n",
            SW_Domain->endend
        );
    }

    // Check bounding box coordinates
    if (GT(SW_Domain->min_x, SW_Domain->max_x)) {
        LogError(LogInfo, LOGERROR, "Domain.in: bbox x-axis min > max.");
        goto closeFile;
    }

    if (GT(SW_Domain->min_y, SW_Domain->max_y)) {
        LogError(LogInfo, LOGERROR, "Domain.in: bbox y-axis min > max.");
        goto closeFile;
    }

    // Check if EndDoy is out of range
    if (SW_Domain->endend < 1 || SW_Domain->endend > 366) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: Invalid last day of simulation: %d is outside 1-366",
            MyFileName,
            SW_Domain->endend
        );
    }

    // Check if scope value is out of range
    if (SW_Domain->SW_SpinUp.scope < 1 ||
        SW_Domain->SW_SpinUp.scope > (SW_Domain->endyr - SW_Domain->startyr)) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: Invalid Scope (N = %d) for spinup",
            MyFileName,
            SW_Domain->SW_SpinUp.scope
        );
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Mark completion status of simulation run

@param[in] isFailure Did simulation run fail or succeed?
@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] start A list of calculated start values for when dealing
    with the netCDF library; simply ncSUID if SWMPI is not enabled
@param[in] count A list of count parts used for accessing/writing to
    netCDF files; simply {1, 0} or {1, 1} if SWMPI is not enabled
@param[in,out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_SetProgress(
    Bool isFailure,
    int progFileID,
    int progVarID,
    size_t start[], // NOLINT(readability-non-const-parameter)
    size_t count[], // NOLINT(readability-non-const-parameter)
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    const signed char mark = (isFailure) ? PRGRSS_FAIL : PRGRSS_DONE;

    SW_NCIN_set_progress(progFileID, progVarID, start, count, &mark, LogInfo);
#else
    (void) isFailure;
    (void) progFileID;
    (void) progVarID;
    (void) start;
    (void) count;
    (void) LogInfo;
#endif
}

/**
@brief Calculate range of suids to run simulations for

@param[in] worldSize Total number of processes that the MPI run has created
(only relevant with SWMPI enabled)
@param[in] runSimDayLen The number of days the simulations are to be run for
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_SimSet(
    int worldSize, TimeInt runSimDayLen, SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo
) {
    const TimeInt startTimeVal = 0;

    TimeInt endDay;
    TimeInt endDayCalc;
    TimeInt tempStartDoy;
    Bool simDomDiscrete = SW_Domain->isSimDomDiscrete;

    char *cacheFileName = SW_Domain->SW_PathInputs.txtInFiles[eNCCache];

#if defined(SOILWAT)
    if (LogInfo->printProgressMsg) {
        SW_MSG_ROOT("is identifying the simulation set ...", SW_Domain->rank);
    }
#endif

    get_start_sim_day(cacheFileName, &SW_Domain->startSimDay, LogInfo);
    checkReturn(LogInfo->stopRun);

    endDay = Time_years_to_days(
        SW_Domain->startyr,
        SW_Domain->endyr,
        SW_Domain->startstart,
        SW_Domain->endend
    );

    if (SW_Domain->startSimDay > endDay) {
        LogError(LogInfo, LOGERROR, "All simulation days completed.");

        return;
    }

    tempStartDoy = SW_Domain->startSimDay;
    SW_Domain->SW_ConstInfo.ModelSim.year =
        Time_sim_day_to_year(SW_Domain->startyr, &tempStartDoy);
    SW_Domain->SW_ConstInfo.ModelSim.doy = tempStartDoy;

    if (tempStartDoy == startTimeVal) {
        SW_Domain->SW_ConstInfo.ModelSim.doy = SW_Domain->startstart;
    }

    SW_Domain->endSimDay = endDay;
    if (runSimDayLen > 0) {
        // "endDayCalc" subtracts 1 to account for the inclusive
        // end condition of the loop in `SW_CTL_run_daily_timesteps()`
        endDayCalc = SW_Domain->startSimDay + runSimDayLen - 1;
        SW_Domain->endSimDay = (endDayCalc > endDay) ? endDay : endDayCalc;
    }

    get_subdomains((size_t) worldSize, simDomDiscrete, SW_Domain, LogInfo);

#if defined(SWNETCDF)
    SW_Domain->nSitesInSubDom = SW_Domain->domCounts[eSW_InDomain][0];
    SW_Domain->nSitesInSubDom *=
        simDomDiscrete ? 1 : SW_Domain->domCounts[eSW_InDomain][1];
#endif
}

void SW_DOM_deepCopy(SW_DOMAIN *source, SW_DOMAIN *dest, LOG_INFO *LogInfo) {

    memcpy(dest, source, sizeof(*dest));

    SW_OUTDOM_deepCopy(&source->OutDom, &dest->OutDom, LogInfo);

    SW_F_deepCopy(&source->SW_PathInputs, &dest->SW_PathInputs, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#if defined(SWNETCDF)
    size_t siteIdx;

    int inKey;

    SW_NC_deepCopy(
        source->nActiveSuidsProc,
        &dest->OutDom.netCDFOutput,
        &dest->netCDFInput,
        &source->OutDom.netCDFOutput,
        &source->netCDFInput,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    ForEachNCInKey(inKey) {
        if (!isnull(source->actSiteIdx[inKey])) {
            dest->actSiteIdx[inKey] = (size_t *) Mem_Malloc(
                sizeof(size_t) * source->nActiveSuidsProc,
                "SW_DOM_deepCopy",
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            for (siteIdx = 0; siteIdx < source->nActiveSuidsProc; siteIdx++) {
                dest->actSiteIdx[inKey][siteIdx] =
                    source->actSiteIdx[inKey][siteIdx];
            }
        }
    }
#endif
}

void SW_DOM_init_ptrs(SW_DOMAIN *SW_Domain) {

#if defined(SWNETCDF)
    int inKey;
#endif

    SW_OUTDOM_init_ptrs(&SW_Domain->OutDom);

    SW_F_init_ptrs(&SW_Domain->SW_PathInputs);

#if defined(SWNETCDF)
    SW_NCIN_init_ptrs(&SW_Domain->netCDFInput);

    SW_Domain->globDomSuids = NULL;

    ForEachNCInKey(inKey) { SW_Domain->actSiteIdx[inKey] = NULL; }
#endif
}

void SW_DOM_deconstruct(SW_DOMAIN *SW_Domain) {
    int key;
    unsigned int i;

    SW_F_deconstruct(&SW_Domain->SW_PathInputs);

#if defined(SWNETCDF)
    size_t site;

    SW_NC_deconstruct(&SW_Domain->OutDom.netCDFOutput);

    ForEachOutKey(key) {
        SW_NCOUT_dealloc_outputkey_var_info(&SW_Domain->OutDom, key);
    }

    SW_NCIN_deconstruct(&SW_Domain->netCDFInput);

    ForEachNCInKey(key) {
        if (!isnull(SW_Domain->actSiteIdx[key])) {
            free((void *) SW_Domain->actSiteIdx[key]);
            SW_Domain->actSiteIdx[key] = NULL;
        }
    }

    if (!isnull(SW_Domain->globDomSuids)) {
        for (site = 0; site < SW_Domain->nActiveSuidsProc; site++) {
            if (!isnull(SW_Domain->globDomSuids[site])) {
                free((void *) SW_Domain->globDomSuids[site]);
                SW_Domain->globDomSuids[site] = NULL;
            }
        }

        free((void *) SW_Domain->globDomSuids);
        SW_Domain->globDomSuids = NULL;
    }

    if (!isnull(SW_Domain->netCDFInput.progVals)) {
        free((void *) SW_Domain->netCDFInput.progVals);
        SW_Domain->netCDFInput.progVals = NULL;
    }
#endif
    ForEachOutKey(key) {
        for (i = 0; i < SW_NOUTCOLS; i++) {
            if (!isnull(SW_Domain->OutDom.colnames_OUT[key][i])) {
                free(SW_Domain->OutDom.colnames_OUT[key][i]);
                SW_Domain->OutDom.colnames_OUT[key][i] = NULL;
            }
        }
#ifdef RSOILWAT
        if (!isnull(SW_Domain->OutDom.outfile[key])) {
            free(SW_Domain->OutDom.outfile[key]);
            SW_Domain->OutDom.outfile[key] = NULL;
        }
#endif
    }
}

/**
@brief Identify soil profile information across simulation domain

Text-mode SOILWAT2 before v8.4.0 produced output of soil evaporation only
for those soil layers with potential evaporation coefficients larger than 0.
Since v8.4.0, all modes produce output for each soil layer.

@param[in] SW_netCDFIn Constant netCDF input file information
@param[in] SW_PathInputs
@param[out] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[out] nMaxSoilLayers Largest number of soil layers across
    simulation domain
@param[out] depthsAllSoilLayers Lower soil layer depths [cm] if
    consistent across simulation domain
@param[in] default_n_layers Default number of soil layer
@param[in] default_depths Default values of soil layer depths [cm]
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_soilProfile(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_PATH_INPUTS *SW_PathInputs,
    Bool hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    const double default_depths[],
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    if (SW_netCDFIn->readInVars[eSW_InSoil][0]) {
        SW_NCIN_soilProfile(
            SW_netCDFIn,
            hasConsistentSoilLayerDepths,
            nMaxSoilLayers,
            depthsAllSoilLayers,
            SW_PathInputs->numSoilVarLyrs,
            default_n_layers,
            default_depths,
            LogInfo
        );
    } else {
#endif

        // Assume default/template values are consistent
        *nMaxSoilLayers = default_n_layers;
        memcpy(
            depthsAllSoilLayers,
            default_depths,
            sizeof(default_depths[0]) * default_n_layers
        );

#if defined(SWNETCDF)
    }
#endif

    /* Cast unused variables to void to silence the compiler */
#if !defined(SWNETCDF)
    (void) SW_netCDFIn;
    (void) SW_PathInputs;
    (void) hasConsistentSoilLayerDepths;
    (void) LogInfo;
#endif
}

#if defined(SWNETCDF)
/**
@brief Calculate dynamic memory sizes within SW_DOMAIN

@param[in] SW_Domain SW_Domain Struct of type SW_DOMAIN holding
constant temporal/spatial information for a set of simulation runs

@return Total estimated size of dynamic memory allocated within SW_DOMAIN
*/
size_t SW_DOM_calc_dyn_mem(SW_DOMAIN *SW_Domain) {
    const size_t nSites = SW_Domain->nActiveSuidsProc;
    const TimeInt n_years = SW_Domain->endyr - SW_Domain->startyr + 1;

    SW_NETCDF_IN *SW_netCDFIn = &SW_Domain->netCDFInput;
    SW_PATH_INPUTS *SW_PathInputs = &SW_Domain->SW_PathInputs;

    int inKey;
    size_t totDomSize = 0;

    IntU file;

    // "globDomSuids" calculation
    totDomSize += (nSites * NC_DIMS * sizeof(size_t));

    // "actSiteIdx" calculation
    ForEachNCInKey(inKey) {
        if (SW_netCDFIn->readInVars[inKey][0]) {
            totDomSize += (nSites * sizeof(size_t));
        }
    }

    // SW_PATH_INPUTS memory
    for (file = 0; file < SW_NFILES; file++) {
        totDomSize +=
            (strlen(SW_PathInputs->txtInFiles[file] + 1) * sizeof(char));
    }

    totDomSize +=
        SW_NCIN_calc_dyn_mem(SW_netCDFIn, SW_PathInputs, nSites, n_years);
    totDomSize += SW_NCOUT_calc_output_sizes(SW_Domain);

    return totDomSize;
}
#endif

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
