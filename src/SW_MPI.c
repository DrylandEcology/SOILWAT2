#include "include/SW_MPI.h"
#include "include/SW_netCDF_Input.h"

#include "include/filefuncs.h"
#include "include/generic.h"
#include "include/myMemory.h"
#include "include/SW_Domain.h"
#include "include/SW_Files.h"
#include "include/SW_Markov.h"
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"
#include "include/SW_Output.h" // for ForEachOutKey, SW_ESTAB, pd2...
#include "include/Times.h"
#include <netcdf.h>
#include <stdlib.h>
#include <string.h>

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define FAIL_ALLOC_DESIG 0
#define FAIL_ROOT_SETUP 1
#define FAIL_ROOT_SEND 2
#define FAIL_ALLOC_SUIDS 3
#define FAIL_ALLOC_TSUIDS 4

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Handle OpenMPI error if one occurs

@param[in] mpiError Result value from an MPI function call that
    returned an error

@sideeffect Exit all instances of the MPI program
*/
void errorMPI(int mpiError) {
    char errorStr[FILENAME_MAX] = {'\0'};
    int errorLen = 0;

    MPI_Error_string(mpiError, errorStr, &errorLen);

    fprintf(stderr, "OpenMPI resulted in the error: %s\n", errorStr);

    MPI_Abort(MPI_COMM_WORLD, mpiError);
}

/**
@brief Deallocate helper memory that was allocated during the call to
    `SW_MPI_process_types()`

@param[in] numActiveSites Number of active sites that will be simulated
@param[in,out] designations A list of designations to fill and distribute
    amoung processes
@param[in,out] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in,out] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in,out] nodeNames A list of node names that contain processors
    when running on a computer (HPC or local)
@param[in,out] numProcsInNode A list of number of processes being run
    in the respective compute node
@param[in,out] numMaxProcsInNode A list holding the maximum
    number of ranks a node in `ranksInNodes` can hold
@param[in,out] ranksInNodes A list of ranks within a node for each
    compute node encountered
*/
static void deallocProcHelpers(
    int numActiveSites,
    SW_MPI_DESIGNATE ***designations,
    unsigned long ***activeSuids,
    unsigned long ****activeTSuids,
    char ***nodeNames,
    int **numProcsInNode,
    int **numMaxProcsInNode,
    int ***ranksInNodes
) {
    const int num2D = 4;
    const int num1D = 2;
    int var;
    int pair;

    void **oneDimFree[] = {
        (void **) numProcsInNode, (void **) numMaxProcsInNode
    };

    void ***twoDimFree[] = {
        (void ***) activeSuids,
        (void ***) nodeNames,
        (void ***) ranksInNodes,
        (void ***) designations
    };

    for (var = 0; var < num1D; var++) {
        if (!isnull((*oneDimFree[var]))) {
            free((*oneDimFree[var]));
            (*oneDimFree[var]) = NULL;
        }
    }

    for (var = 0; var < num2D; var++) {
        if (!isnull((*twoDimFree[var]))) {
            for (pair = 0; pair < numActiveSites; pair++) {
                if (!isnull(((*twoDimFree[var]))[pair])) {
                    free(((*twoDimFree[var]))[pair]);
                    ((*twoDimFree[var]))[pair] = NULL;
                }
            }

            free(((*twoDimFree[var])));
            ((*twoDimFree[var])) = NULL;
        }
    }

    if (!isnull(*activeTSuids)) {
        free((void *) *activeTSuids);
        *activeTSuids = NULL;
    }
}

/**
@brief Send filler information if an error occurs somewhere when
    attempting to designate processes to a job so we can exit the function

@param[in] desType Custom MPI datatype to send SW_MPI_DESIGNATE
@param[in] startRank Start rank to send to other process ranks if root process
@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] placement A value specifying which spot within the designation
    function an error occurred
@param[in] numCompProcs Number of compute processes in a node
@param[in] numActiveSites Number of active sites that will be simulated
@param[in] useIndexFile Specifies to create/use an index file
@param[in] useTSuid Specifies if we use translated SUIDs
*/
static void exitDesFunc(
    MPI_Datatype desType,
    int startRank,
    int rank,
    int worldSize,
    int placement,
    int numActiveSites,
    Bool *useIndexFile,
    Bool useTSuid
) {
    int destRank;
    int pair;
    char tempName[MAX_FILENAMESIZE];
    unsigned long pairBuff[2];
    int tempRankBuff[PROCS_PER_IO];

    SW_MPI_DESIGNATE temp;
    InKeys inKey;
    MPI_Request nullReq = MPI_REQUEST_NULL;

    // Check if rank is root
    if (rank == SW_MPI_ROOT) {
        if (placement == FAIL_ROOT_SETUP) {
            temp.procJob = SW_MPI_PROC_COMP;
            temp.nSuids = 0;
            temp.useTSuids = swFALSE;
            temp.nCompProcs = 0;
        }

        if (placement < FAIL_ROOT_SEND) {
            for (destRank = startRank; destRank < worldSize; destRank++) {
                SW_MPI_Recv(
                    desType,
                    tempName,
                    MAX_FILENAMESIZE,
                    destRank,
                    swTRUE,
                    0,
                    &nullReq
                );
            }
        }

        for (destRank = startRank; destRank < worldSize; destRank++) {
            SW_MPI_Send(desType, &temp, 1, destRank, swTRUE, 0, &nullReq);
        }
    } else {
        // Otherwise, we are a worker process - can only be I/O process
        // Check if we failed when allocating domain suids
        // or translated SUIDs

        // Read domain SUIDs into useless buffer
        for (pair = 0; pair < numActiveSites; pair++) {
            SW_MPI_Recv(
                MPI_UNSIGNED_LONG, pairBuff, 2, SW_MPI_ROOT, swTRUE, 0, &nullReq
            );
        }

        if (useTSuid) {
            ForEachNCInKey(inKey) {
                if (inKey > eSW_InSpatial && inKey != eSW_InSite &&
                    useIndexFile[inKey]) {

                    for (pair = 0; pair < numActiveSites; pair++) {
                        SW_MPI_Recv(
                            MPI_UNSIGNED_LONG,
                            pairBuff,
                            2,
                            SW_MPI_ROOT,
                            swTRUE,
                            0,
                            &nullReq
                        );
                    }
                }
            }
        }

        // Otherwise, we failed when allocating ranks
        // Read translated domain SUIDs into useless buffer
        SW_MPI_Recv(
            MPI_INT,
            tempRankBuff,
            PROCS_PER_IO,
            SW_MPI_ROOT,
            swTRUE,
            0,
            &nullReq
        );
    }
}

/**
@brief Calculate the number of I/O processes in a node

@param[in] numProcsInNode A list of number of processes being run
    in the respective compute node

@return The number of I/O processes that can reasonably be
    run within a compute node (i.e., one-to-one compute-to-IO ratio or greater)
*/
static int calcNumIOProcs(int numProcsInNode) {
    int numCompProcInNode = numProcsInNode - SW_MPI_NIO;

    return (numCompProcInNode / SW_MPI_NIO < 1.0) ? numProcsInNode / 2 :
                                                    SW_MPI_NIO;
}

/**
@brief Find which I/O process a compute process was assigned to

@param[in,out] desigs A list of designations to fill and distribute
    amoung processes
@param[in] rootDes Root process instance of a designation to fill rather
    than the first instance in `designations`
@param[in] node Current compute node we are searching for I/O assignment
@param[in] proc Current process we are searching for I/O match
@param[in] targetRank Rank to look for within an I/O process' list of
    assigned ranks
@param[in] ranksInNodes A list of ranks within a node for each
    compute node encountered
*/
static void findIOAssignment(
    SW_MPI_DESIGNATE **desigs,
    SW_MPI_DESIGNATE *rootDes,
    int node,
    int proc,
    int targetRank,
    int **ranksInNodes
) {
    int tempProc = 0;

    SW_MPI_DESIGNATE *tempDes;
    SW_MPI_DESIGNATE *targetDesig = &desigs[node][proc];

    targetDesig->ioRank = -1;

    while (targetDesig->ioRank == -1) {
        tempDes =
            (node == 0 && tempProc == 0) ? rootDes : &desigs[node][tempProc];
        if (tempDes->procJob == SW_MPI_PROC_COMP) {
            break;
        }

        for (int temp = 0; temp < tempDes->nCompProcs; temp++) {
            if (tempDes->ranks[temp] == targetRank) {
                targetDesig->ioRank = ranksInNodes[node][tempProc];
                break;
            }
        }

        tempProc++;
    }
}

/**
@brief Allocate room to store a list of domain SUIDs

@param[in] numActiveSites Number of active sites that will be
    simulated
@param[out] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[out] LogInfo Holds information on warnings and errors
*/
static void allocateActiveSuids(
    int numActiveSites, unsigned long ***activeSuids, LOG_INFO *LogInfo
) {
    const int nElemPerSuid = 2;
    int domIndex;

    *activeSuids = (unsigned long **) Mem_Malloc(
        sizeof(unsigned long *) * numActiveSites,
        "allocateActiveSuids()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (domIndex = 0; domIndex < numActiveSites; domIndex++) {
        (*activeSuids)[domIndex] = (unsigned long *) Mem_Malloc(
            sizeof(unsigned long) * nElemPerSuid,
            "allocateActiveSuids()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        memset((*activeSuids)[domIndex], 0, sizeof(unsigned long) * 2);
    }
}

/**
@brief Allocate room to store a list of translated domain SUIDs

@param[in] numActiveSites Number of active sites that will be
    simulated
@param[out] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[out] LogInfo Holds information on warnings and errors
*/
static void allocateActiveTSuids(
    unsigned long numActiveSites,
    unsigned long ***activeTSuids,
    LOG_INFO *LogInfo
) {
    const unsigned long nIndexVals = 2;
    unsigned long site;
    unsigned long col;

    *activeTSuids = (unsigned long **) Mem_Malloc(
        sizeof(unsigned long *) * numActiveSites,
        "allocateActiveTSuids()",
        LogInfo
    );
    for (site = 0; site < numActiveSites; site++) {
        (*activeTSuids)[site] = NULL;
    }

    for (site = 0; site < numActiveSites; site++) {
        (*activeTSuids)[site] = (unsigned long *) Mem_Malloc(
            sizeof(unsigned long) * nIndexVals,
            "allocateActiveTSuids()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        for (col = 0; col < nIndexVals; col++) {
            (*activeTSuids)[site][col] = 0;
        }
    }
}

/**
@brief Re/allocate compute node/process information

@param[in] oldCount Last size of the arrays
@param[in] newCount Updated size to reallocate to
@param[in,out] nodeNames A list of node names that contain processors
    when running on a computer (HPC or local)
@param[in,out] numProcsInNode A list of number of processes being run
    in the respective compute node node/local computer
@param[in,out] ranksInNodes A list of ranks within a node for each
    compute node encountered
@param[in,out] numMaxProcsInNode A list holding the maximum
    number of ranks a node in `ranksInNodes` can hold
@param[out] LogInfo Holds information on warnings and errors
*/
static void allocProcInfo(
    int oldCount,
    int newCount,
    char ***nodeNames,
    int **numProcsInNode,
    int ***ranksInNodes,
    int **numMaxProcsInNode,
    LOG_INFO *LogInfo
) {
    int fillIndex;

    if (isnull(*nodeNames)) {
        *nodeNames = (char **) Mem_Malloc(
            sizeof(char *) * newCount, "allocProcInfo()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        *numProcsInNode = (int *) Mem_Malloc(
            sizeof(int) * newCount, "allocProcInfo()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        *ranksInNodes = (int **) Mem_Malloc(
            sizeof(int *) * newCount, "allocProcInfo()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        *numMaxProcsInNode = (int *) Mem_Malloc(
            sizeof(int) * newCount, "allocProcInfo()", LogInfo
        );
    } else {
        *nodeNames = (char **) Mem_ReAlloc(
            *nodeNames, sizeof(char *) * newCount, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        *numProcsInNode = (int *) Mem_ReAlloc(
            *numProcsInNode, sizeof(int) * newCount, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        *ranksInNodes = (int **) Mem_ReAlloc(
            *ranksInNodes, sizeof(int *) * newCount, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        *numMaxProcsInNode = (int *) Mem_ReAlloc(
            *numMaxProcsInNode, sizeof(int) * newCount, LogInfo
        );
    }
    if (LogInfo->stopRun) {
        return;
    }

    for (fillIndex = oldCount; fillIndex < newCount; fillIndex++) {
        (*nodeNames)[fillIndex] = NULL;
        (*ranksInNodes)[fillIndex] = NULL;
        (*numProcsInNode)[fillIndex] = 0;
        (*numMaxProcsInNode)[fillIndex] = 0;
    }
}

/**
@brief Reallocate the list of ranks contained within a compute node

@param[in] oldCount Last size of the array
@param[in] newCount Updated size to reallocate to
@param[in,out] ranksInNode A list of rank numbers to be reallocated
@param[out] LogInfo Holds information on warnings and errors
*/
static void reallocRanks(
    int oldCount, int newCount, int **ranksInNode, LOG_INFO *LogInfo
) {
    int rank;

    if (isnull(*ranksInNode)) {
        *ranksInNode = (int *) Mem_Malloc(
            sizeof(int) * newCount, "reallocRanks()", LogInfo
        );
    } else {
        *ranksInNode =
            (int *) Mem_ReAlloc(*ranksInNode, sizeof(int) * newCount, LogInfo);
    }
    if (LogInfo->stopRun) {
        return;
    }

    for (rank = oldCount; rank < newCount; rank++) {
        (*ranksInNode)[rank] = 0;
    }
}

/**
@brief Fill information of a designation instance

@param[in] numCompProcs Number of global compute processes
@param[in] numCompProcsNode Number of compute processes in a node
@param[in] ioProcsInNode Number of I/O processes within a compute node
@param[in] numActiveSites Number of sites that was turned on
    by the program and/or user
@param[in] activeSiteSuids A list of program domain SUIDs that are
    to be simulated
@param[in] activeSiteTSuids A list of translated SUIDs
    (program domain to input domain) to be stored if used for
    each input key except domain and spatial
@param[in] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in] ranks A list of ranks within a node that we can specify that
    the I/O process handles
@param[in,out] rankAssign An index specifying where to start the assigning
    within `ranks` to the I/O process
@param[in,out] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in,out] leftOverCompProcs Specifies how many overflow compute
    processes we have if the number of compute to I/O does not
    divide evenly
@param[in,out] leftOverSuids Specifies how many overflow SUIDs we have
    if we were to assign if the number of compute processes and active sites
    does not divide evenly; return an updated number of left over SUIDs
@param[in,out] leftSuids Number of SUIDs that are left to be assigned
@param[in,out] suidAssign Placement within active SUID array we need
    to assign to the current I/O process; return the updated
    placement for the next assignment
@param[out] desig Designation instance that holds information about
    assigning a process to a job
@param[out] LogInfo Holds information on warnings and errors
*/
static void fillDesignationIO(
    int numCompProcs,
    int numCompProcsNode,
    int ioProcsInNode,
    int numActiveSites,
    unsigned long **activeSuids,
    int *ranks,
    int *rankAssign,
    unsigned long ***activeTSuids,
    int *leftOverCompProcs,
    int *leftOverSuids,
    int *leftSuids,
    int *suidAssign,
    SW_MPI_DESIGNATE *desig,
    LOG_INFO *LogInfo
) {
    int suid;
    int tSuid; /* For translated assignment */
    InKeys inKey;
    size_t numPosKeys = eSW_InClimate;
    int rank;

    desig->nCompProcs = numCompProcsNode / ioProcsInNode;

    if (*leftOverCompProcs > 0) {
        desig->nCompProcs++;
        (*leftOverCompProcs)--;
    }

    // Allocate and assign floor(# active sites / num
    // compute procs) + (one left over SUID, if necessary)
    // to the node or the rest of the SUIDs if this assignment
    // will complete the I/O assignments
    desig->nSuids = numActiveSites / numCompProcs;

    if (desig->nSuids > *leftSuids) {
        desig->nSuids = *leftSuids;
    } else if (*leftOverSuids > 0) {
        desig->nSuids++;
        (*leftOverSuids)--;
    }
    *leftSuids -= desig->nSuids;
    desig->domSuids = (unsigned long **) Mem_Malloc(
        sizeof(unsigned long *) * desig->nSuids,
        "SW_MPI_process_types()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (suid = 0; suid < desig->nSuids; suid++) {
        desig->domSuids[suid] = NULL;
    }

    for (suid = 0; suid < desig->nSuids; suid++) {
        desig->domSuids[suid] = (unsigned long *) Mem_Malloc(
            sizeof(unsigned long) * desig->nSuids,
            "fillDesignationIO()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    if (!isnull(activeTSuids)) {
        desig->domTSuids = (unsigned long ***) Mem_Malloc(
            sizeof(unsigned long **) * numPosKeys,
            "SW_MPI_get_activated_tsuids()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    // Add SUIDs/ranks to list of I/O
    ForEachNCInKey(inKey) {
        if (!isnull(activeTSuids[inKey])) {
            allocateActiveTSuids(
                desig->nSuids, &desig->domTSuids[inKey], LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            tSuid = *suidAssign;
            for (suid = 0; suid < desig->nSuids; suid++) {
                desig->domTSuids[inKey][suid][0] =
                    activeTSuids[inKey][tSuid][0];
                desig->domTSuids[inKey][suid][1] =
                    activeTSuids[inKey][tSuid][1];
                tSuid++;
            }
        }
    }

    for (suid = 0; suid < desig->nSuids; suid++) {
        desig->domSuids[suid][0] = activeSuids[*suidAssign][0];
        desig->domSuids[suid][1] = activeSuids[*suidAssign][1];
        (*suidAssign)++;
    }

    for (rank = 0; rank < desig->nCompProcs; rank++) {
        desig->ranks[rank] = ranks[*rankAssign];
        (*rankAssign)++;
    }
}

/**
@brief Designate all processes to a job (compute or I/O)

@param[in,out] designations A list of designations to fill and distribute
    amoung processes
@param[in,out] rootDes Root process instance of a designation to fill rather
    than the first instance in `designations`
@param[in] numProcsInNode A list of number of processes being run
    in the respective compute node node/local computer
@param[in] numNodes The number of compute nodes we are running instances of
    the program on
@param[in] numCompProcWorld Total number of compute processes to assign
    throughout the world of processes
@param[in] numActiveSites Number of active sites that will be
    simulated
@param[in] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in] ranksInNodes A list of ranks within a node for each
    compute node encountered
@param[in,out] leftSuids Number of SUIDs that are left to be assigned
@param[in,out] suidAssign Placement within active SUID array we need
    to assign to the current I/O process; return the updated
    placement for the next assignment
@param[out] LogInfo Holds information on warnings and errors
*/
static void designateProcesses(
    SW_MPI_DESIGNATE ***designations,
    SW_MPI_DESIGNATE *rootDes,
    int numProcsInNode[],
    int numNodes,
    int numCompProcWorld,
    int numActiveSites,
    unsigned long **activeSuids,
    unsigned long ***activeTSuids,
    int **ranksInNodes,
    int *leftSuids,
    int *suidAssign,
    LOG_INFO *LogInfo
) {
    int node;
    int proc;
    int numNodeProcs;
    int ioProcsLeft;
    int ioProcsInNode;
    int leftOverComp;
    int leftOverSuids;
    int compNodesAssign;

    SW_MPI_DESIGNATE *desig = NULL;

    // Allocate designation information for each compute node
    // # compute nodes by number of processes in compute node
    *designations = (SW_MPI_DESIGNATE **) Mem_Malloc(
        sizeof(SW_MPI_DESIGNATE *) * numNodes, "designateProcesses()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
    for (node = 0; node < numNodes; node++) {
        (*designations)[node] = NULL;
    }

    /* Calculate the number of overflow sites that will not be assigned
       unless we specifically make sure to assign them;
       Calculation:
        numSuidsPerComp = floor((# active sites) / nCompProcs)
        totAssignSuids = numSuidsPerComp * numCompProcWorld
        numOverflow = (# active sites) - totAssignSuids

        Note: This will result in a value within [0, # active sites - 1]
    */
    leftOverSuids = numActiveSites / numCompProcWorld;
    leftOverSuids *= numCompProcWorld;
    leftOverSuids = numActiveSites - leftOverSuids;

    for (node = 0; node < numNodes; node++) {
        (*designations)[node] = (SW_MPI_DESIGNATE *) Mem_Malloc(
            sizeof(SW_MPI_DESIGNATE) * numProcsInNode[node],
            "designateProcesses()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        numNodeProcs = numProcsInNode[node];

        // Go through the node's processes
        // Calculate the number of I/O processes within the node
        // DO NOT go over a ratio of one-to-one in compute-to-IO
        ioProcsLeft = ioProcsInNode = calcNumIOProcs(numNodeProcs);

        // Calculate how many overflow compute processes we need
        // to assign to I/O processes
        leftOverComp = (numNodeProcs - ioProcsInNode) / ioProcsInNode;
        leftOverComp = ioProcsInNode * leftOverComp;
        leftOverComp = (numNodeProcs - ioProcsInNode) - leftOverComp;
        compNodesAssign = ioProcsInNode; /* Skip past I/O assignments in node */
        for (proc = 0; proc < numNodeProcs; proc++) {
            desig = (node == 0 && proc == 0) ? rootDes :
                                               &(*designations)[node][proc];

            desig->procJob =
                (ioProcsLeft == 0) ? SW_MPI_PROC_COMP : SW_MPI_PROC_IO;

            if (desig->procJob == SW_MPI_PROC_IO) {
                fillDesignationIO(
                    numCompProcWorld,
                    numNodeProcs - ioProcsInNode,
                    ioProcsInNode,
                    numActiveSites,
                    activeSuids,
                    ranksInNodes[node],
                    &compNodesAssign,
                    activeTSuids,
                    &leftOverComp,
                    &leftOverSuids,
                    leftSuids,
                    suidAssign,
                    desig,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return;
                }

                ioProcsLeft--;
            }
        }
    }
}

/**
@brief Get and store the world of processors information to get
    a sense of where processors are located

@param[in] desType Custom MPI datatype to send SW_MPI_DESIGNATE
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] rootProcName Root process' processor/compute node name
@param[out] numNodes The number of unique nodes the program is running on
@param[out] nodeNames A list of unique node names
@param[out] numProcsInNode A list specifying the number of processes
    within each node
@param[out] numMaxProcsInNode A list specifying the size of each
    compute node's maxmimum determined amount of processes
@param[out] ranksInNodes A list specifying the rank values that are in
    a compute node
@param[iout] LogInfo Holds information on warnings and errors
*/
static void getProcInfo(
    MPI_Datatype desType,
    int worldSize,
    char rootProcName[],
    int *numNodes,
    char ***nodeNames,
    int **numProcsInNode,
    int **numMaxProcsInNode,
    int ***ranksInNodes,
    LOG_INFO *LogInfo
) {
    const int allocInc = 3; /* Can be anything > 0 but >= 5 may be too much */

    int destRank;
    int node;
    int oldSize = allocInc;
    int newSize = oldSize + allocInc;
    int oldSizeRanks = 0;
    Bool duplicate;
    char rankProc[FILENAME_MAX];

    MPI_Request nullReq = MPI_REQUEST_NULL;

    // Go through each rank and get their processor name
    for (destRank = 0; destRank < worldSize; destRank++) {
        duplicate = swFALSE;

        // Get node name
        if (destRank > SW_MPI_ROOT) {
            SW_MPI_Recv(
                MPI_CHAR,
                rankProc,
                MAX_FILENAMESIZE,
                destRank,
                swTRUE,
                0,
                &nullReq
            );
        } else {
            memcpy(rankProc, rootProcName, FILENAME_MAX);
        }

        // Add to a list of unique node names (if not already present)
        for (node = 0; node < *numNodes; node++) {
            duplicate = (Bool) (strcmp(rankProc, (*nodeNames)[node]) == 0);
            if (duplicate) {
                break;
            }
        }

        node = (duplicate) ? node : *numNodes;

        // If it was not unique
        if (!duplicate) {
            // Store the name to compare later

            // Resize storage arrays if needed
            if (*numNodes == newSize) {
                oldSize = newSize;
                newSize += allocInc;
                allocProcInfo(
                    oldSize,
                    newSize,
                    nodeNames,
                    numProcsInNode,
                    ranksInNodes,
                    numMaxProcsInNode,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    exitDesFunc(
                        desType,
                        destRank,
                        SW_MPI_ROOT,
                        worldSize,
                        FAIL_ALLOC_DESIG,
                        0,
                        NULL,
                        swFALSE
                    );
                    return;
                }
            }

            (*nodeNames)[node] = Str_Dup(rankProc, LogInfo);
            if (LogInfo->stopRun) {
                return;
            }

            (*numNodes)++;
        }
        (*numProcsInNode)[node]++;

        // Keep track of which ranks are in which nodes
        if ((*numProcsInNode)[node] >= (*numMaxProcsInNode)[node]) {
            oldSizeRanks = (*numMaxProcsInNode)[node];
            (*numMaxProcsInNode)[node] += allocInc;
            reallocRanks(
                oldSizeRanks,
                (*numMaxProcsInNode)[node],
                &(*ranksInNodes)[node],
                LogInfo
            );
            if (LogInfo->stopRun) {
                exitDesFunc(
                    desType,
                    destRank + 1,
                    SW_MPI_ROOT,
                    worldSize,
                    FAIL_ALLOC_DESIG,
                    0,
                    NULL,
                    swFALSE
                );
                return;
            }
        }
        (*ranksInNodes)[node][(*numProcsInNode)[node] - 1] = destRank;
    }
}

/**
@brief Get or send a processor designation to get ready for
    simulation

@param[in] desType Custom MPI datatype to send SW_MPI_DESIGNATE
@param[in] designations A list of designations to fill and distribute
    amoung processes
@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] numNodes The number of compute nodes we are running instances of
    the program on
@param[in] useIndexFile Specifies to create/use an index file
@param[in] numProcsInNode A list of number of processes being run
    in the respective compute node node/local computer
@param[in] ranksInNodes A list of ranks for every compute node
@param[in,out] SW_Designation Has two different purposes
    Root process: Used in the search process for I/O rank assignment
        for compute processes; does not change
    Other processes: Designation to be filled/recieved from the
        root process; returns the filled designation instance
@param[out] LogInfo Holds information on warnings and errors
*/
static void assignProcs(
    MPI_Datatype desType,
    SW_MPI_DESIGNATE **designations,
    int rank,
    int worldSize,
    int numNodes,
    Bool useIndexFile[],
    int numProcsInNode[],
    int **ranksInNodes,
    Bool useTranslated,
    SW_MPI_DESIGNATE *SW_Designation,
    LOG_INFO *LogInfo
) {
    int pair;
    int node;
    int proc;
    int sendRank;
    InKeys inKey;

    MPI_Request nullReq = MPI_REQUEST_NULL;
    SW_MPI_DESIGNATE *currDes = NULL;

    if (rank == SW_MPI_ROOT) {
        for (node = 0; node < numNodes; node++) {
            for (proc = 0; proc < numProcsInNode[node]; proc++) {
                sendRank = ranksInNodes[node][proc];

                if (sendRank > SW_MPI_ROOT) {
                    currDes = &designations[node][proc];

                    if (currDes->procJob == SW_MPI_PROC_COMP) {
                        findIOAssignment(
                            designations,
                            SW_Designation,
                            node,
                            proc,
                            sendRank,
                            ranksInNodes
                        );
                    } else {
                        currDes->useTSuids = useTranslated;
                    }

                    // Send their designation
                    SW_MPI_Send(
                        desType, currDes, 1, sendRank, swTRUE, 0, &nullReq
                    );

                    if (currDes->procJob == SW_MPI_PROC_IO) {
                        for (pair = 0; pair < currDes->nSuids; pair++) {
                            SW_MPI_Send(
                                MPI_UNSIGNED_LONG,
                                currDes->domSuids[pair],
                                2,
                                sendRank,
                                swTRUE,
                                0,
                                &nullReq
                            );
                        }

                        if (useTranslated) {
                            ForEachNCInKey(inKey) {
                                if (inKey <= eSW_InSpatial ||
                                    !useIndexFile[inKey] ||
                                    inKey == eSW_InSite) {
                                    continue;
                                }

                                for (pair = 0; pair < currDes->nSuids; pair++) {
                                    SW_MPI_Send(
                                        MPI_UNSIGNED_LONG,
                                        currDes->domTSuids[inKey][pair],
                                        2,
                                        sendRank,
                                        swTRUE,
                                        0,
                                        &nullReq
                                    );
                                }
                            }
                        }

                        SW_MPI_Send(
                            MPI_INT,
                            currDes->ranks,
                            PROCS_PER_IO,
                            sendRank,
                            swTRUE,
                            0,
                            &nullReq
                        );
                    }
                }
            }
        }
    } else {
        SW_MPI_Recv(
            desType, SW_Designation, 1, SW_MPI_ROOT, swTRUE, 0, &nullReq
        );

        if (SW_Designation->procJob == SW_MPI_PROC_IO) {
            allocateActiveSuids(
                SW_Designation->nSuids, &SW_Designation->domSuids, LogInfo
            );
            if (LogInfo->stopRun) {
                exitDesFunc(
                    desType,
                    0,
                    rank,
                    worldSize,
                    FAIL_ALLOC_SUIDS,
                    SW_Designation->nSuids,
                    useIndexFile,
                    SW_Designation->useTSuids
                );
                return;
            }

            if (SW_Designation->useTSuids) {
                useIndexFile[eSW_InDomain] = swFALSE;
                useIndexFile[eSW_InSpatial] = swFALSE;
                useIndexFile[eSW_InSoil] = swTRUE;
                useIndexFile[eSW_InSite] = swFALSE;
                useIndexFile[eSW_InVeg] = swTRUE;
                useIndexFile[eSW_InWeather] = swTRUE;
                useIndexFile[eSW_InClimate] = swTRUE;

                SW_Designation->domTSuids = (unsigned long ***) Mem_Malloc(
                    sizeof(unsigned long **) * SW_NINKEYSNC,
                    "SW_MPI_process_types()",
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    exitDesFunc(
                        desType,
                        0,
                        rank,
                        worldSize,
                        FAIL_ALLOC_TSUIDS,
                        SW_Designation->nSuids,
                        useIndexFile,
                        SW_Designation->useTSuids
                    );
                    return;
                }

                ForEachNCInKey(inKey) {
                    if (inKey == eSW_InDomain || inKey == eSW_InSite ||
                        !useIndexFile[inKey]) {

                        continue;
                    }

                    allocateActiveTSuids(
                        SW_Designation->nSuids,
                        &SW_Designation->domTSuids[inKey],
                        LogInfo
                    );
                    if (LogInfo->stopRun) {
                        exitDesFunc(
                            desType,
                            0,
                            rank,
                            worldSize,
                            FAIL_ALLOC_TSUIDS,
                            SW_Designation->nSuids,
                            NULL,
                            SW_Designation->useTSuids
                        );
                        return;
                    }
                }
            }

            for (pair = 0; pair < SW_Designation->nSuids; pair++) {
                SW_MPI_Recv(
                    MPI_UNSIGNED_LONG,
                    SW_Designation->domSuids[pair],
                    2,
                    SW_MPI_ROOT,
                    swTRUE,
                    0,
                    &nullReq
                );
            }

            if (SW_Designation->useTSuids) {
                ForEachNCInKey(inKey) {
                    if (SW_Designation->useTSuids && useIndexFile[inKey]) {

                        for (pair = 0; pair < SW_Designation->nSuids; pair++) {
                            SW_MPI_Recv(
                                MPI_UNSIGNED_LONG,
                                SW_Designation->domTSuids[inKey][pair],
                                2,
                                SW_MPI_ROOT,
                                swTRUE,
                                0,
                                &nullReq
                            );
                        }
                    }
                }
            }

            SW_MPI_Recv(
                MPI_INT,
                SW_Designation->ranks,
                PROCS_PER_IO,
                SW_MPI_ROOT,
                swTRUE,
                0,
                &nullReq
            );
        }
    }
}

/**
@brief Broadcast a message/type to all processes

@param[in] datatype Custom MPI datatype that will be sent
@param[in] buffer Location of memory that will be written to
@param[in] count Number of items to recv from the buffer
@param[in] srcRank Source rank that information will be sent from
with any message tag values
@param[in] comm MPI communicator to broadcast a message to
*/
static void SW_Bcast(
    MPI_Datatype datatype, void *buffer, int count, int srcRank, MPI_Comm comm
) {
    int mpiRes = MPI_SUCCESS;

    mpiRes = MPI_Bcast(buffer, count, datatype, srcRank, comm);

    if (mpiRes != MPI_SUCCESS) {
        errorMPI(mpiRes);
    }
}

/**
@brief Wrapper function to send/receive information about a string
    that will be allocated and sent/stored

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] buffer Buffer to send (root) and allocate (all other processes)
@param[in] send Flag specifying (root) if the string should be sent (not NULL)
@param[in] comm MPI communicator to broadcast a message to
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_dynamic_string(
    int rank, char **buffer, Bool send, MPI_Comm comm, LOG_INFO *LogInfo
) {
    size_t strLen = 0;

    if (rank == SW_MPI_ROOT) {
        strLen = (send) ? strlen(*buffer) + 1 : 0;
        SW_Bcast(MPI_UNSIGNED_LONG, &strLen, 1, SW_MPI_ROOT, comm);
    } else {
        SW_Bcast(MPI_UNSIGNED_LONG, &strLen, 1, SW_MPI_ROOT, comm);

        if (strLen > 0) {
            *buffer = (char *) Mem_Malloc(
                sizeof(char) * strLen, "get_dynamic_string()", LogInfo
            );
        }
    }
    if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
        return;
    }

    if (strLen > 0) {
        SW_Bcast(MPI_CHAR, *buffer, strLen, SW_MPI_ROOT, comm);
    }
}

/**
@brief Broadcast information about SW_NETCDF_IN

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] comm MPI communicator to broadcast a message to
@param[in,out] netCDFIn Struct of type SW_NETCDF_IN that will be distributed
    (root process) and received (all others)
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_NC_info(
    int rank, MPI_Comm comm, SW_NETCDF_IN *netCDFIn, LOG_INFO *LogInfo
) {
    int startVar;
    int var;
    int att;
    char *str;
    InKeys inKey;
    Bool useKey = swFALSE;

    if (rank > SW_MPI_ROOT) {
        SW_NCIN_alloc_input_var_info(netCDFIn, LogInfo);
    }
    SW_Bcast(MPI_INT, netCDFIn->ncDomVarIDs, SW_NVARDOM, SW_MPI_ROOT, comm);

    ForEachNCInKey(inKey) {
        if (rank == SW_MPI_ROOT) {
            useKey =
                (Bool) (netCDFIn->readInVars[inKey][0] && inKey > eSW_InDomain);
        }

        SW_Bcast(MPI_INT, &useKey, 1, SW_MPI_ROOT, comm);

        if (!useKey) {
            netCDFIn->readInVars[inKey][0] = swFALSE;
            continue;
        }

        SW_Bcast(
            MPI_INT,
            netCDFIn->readInVars[inKey],
            numVarsInKey[inKey] + 1,
            SW_MPI_ROOT,
            comm
        );

        startVar = 0;
        while (startVar < numVarsInKey[inKey] &&
               !netCDFIn->readInVars[inKey][startVar + 1]) {

            startVar++;
        }

        if (rank > SW_MPI_ROOT) {
            SW_NCIN_allocDimVar(
                numVarsInKey[inKey], &netCDFIn->dimOrderInVar[inKey], LogInfo
            );
        }
        if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
            return;
        }

        // Go through all enabled variables within the current key
        // and copy
        for (var = startVar; var < numVarsInKey[inKey]; var++) {
            if (!netCDFIn->readInVars[inKey][var + 1]) {
                continue;
            }

            for (att = 0; att < NUM_INPUT_INFO; att++) {
                str = netCDFIn->inVarInfo[inKey][var][att];
                get_dynamic_string(
                    rank,
                    &netCDFIn->inVarInfo[inKey][var][att],
                    (Bool) (!isnull(str)),
                    comm,
                    LogInfo
                );
                if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
                    return;
                }
            }

            str = netCDFIn->units_sw[inKey][var];
            get_dynamic_string(
                rank,
                &netCDFIn->units_sw[inKey][var],
                (Bool) (!isnull(str)),
                comm,
                LogInfo
            );
            if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
                return;
            }

            SW_Bcast(
                MPI_INT,
                netCDFIn->dimOrderInVar[inKey][var],
                MAX_NDIMS,
                SW_MPI_ROOT,
                comm
            );
        }
    }
}

/**
@brief Wrapper function to create groups and communications
between compute and I/O processes

@param[in] numRanks Number of ranks within the new communicator
@param[in] ranks A list of ranks that will be within the
    new communicator
@param[out] groupComm New MPI group communicator
*/
static void mpi_create_group_comms(
    int numRanks, int *ranks, MPI_Comm *groupComm
) {
    int res = MPI_SUCCESS;
    MPI_Group worldGroup;
    MPI_Group newGroup;

    res = MPI_Comm_group(MPI_COMM_WORLD, &worldGroup);
    if (res != MPI_SUCCESS) {
        goto reportFail;
    }

    res = MPI_Group_incl(worldGroup, numRanks, ranks, &newGroup);
    if (res != MPI_SUCCESS) {
        goto reportFail;
    }

    res = MPI_Comm_create_group(MPI_COMM_WORLD, newGroup, 0, groupComm);
    if (res != MPI_SUCCESS) {
        goto reportFail;
    }

reportFail:
    if (res != MPI_SUCCESS) {
        errorMPI(res);
    }
}

/**
@brief Create custom communicators between compute and I/O processes so
    they can act independently of one another if necessary

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] rankJob Assigned job of a specific rank/process (compute or I/O)
@param[in] numIOProcsTot Total number of I/O processes across the
    communicator MPI_COMM_WORLD
@param[in] designations A list of designations to fill and distribute
    amoung processes
@param[in] ranksInNodes A list of ranks within a node for each
    compute node encountered
@param[in] numNodes The number of compute nodes we are running instances of
    the program on
@param[in] numProcsInNode A list of number of processes being run
    in the respective compute node
@param[out] groupComm New MPI group communicator
@param[out] rootCompComm New MPI group communicator for the root process
    so it can specifically send things to all compute processes and
    I/O
@param[out] LogInfo Holds information on warnings and errors
*/
static void create_groups(
    int rank,
    int worldSize,
    int rankJob,
    int numIOProcsTot,
    SW_MPI_DESIGNATE **designations,
    int **ranksInNodes,
    int numNodes,
    int *numProcsInNode,
    MPI_Comm *groupComm,
    MPI_Comm *rootCompComm,
    LOG_INFO *LogInfo
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;

    int numCompProcs = worldSize - numIOProcsTot + 1;
    int *ranksInComp = NULL;
    int *ranksInIO = NULL;
    int compIndex = 0;
    int ioIndex = 0;
    int node;
    int *buff;
    int numElem;
    int procJob;
    int rankIndex;
    int sendRank;

    // Broadcast number of compute and I/O processors in MPI_COMM_WORLD
    SW_Bcast(MPI_INT, &numCompProcs, 1, SW_MPI_ROOT, MPI_COMM_WORLD);
    SW_Bcast(MPI_INT, &numIOProcsTot, 1, SW_MPI_ROOT, MPI_COMM_WORLD);

    // Allocate rank information to use it when letting MPI
    // know which ranks are in a group
    if (rank == SW_MPI_ROOT || rankJob == SW_MPI_PROC_COMP) {
        ranksInComp = (int *) Mem_Malloc(
            sizeof(int) * numCompProcs, "create_groups()", LogInfo
        );
    }
    SW_MPI_check_setup_status(LogInfo->stopRun, MPI_COMM_WORLD);
    if (LogInfo->stopRun) {
        return;
    }

    if (rank == SW_MPI_ROOT || rankJob == SW_MPI_PROC_IO) {
        ranksInIO = (int *) Mem_Malloc(
            sizeof(int) * numIOProcsTot, "create_groups()", LogInfo
        );
    }
    SW_MPI_check_setup_status(LogInfo->stopRun, MPI_COMM_WORLD);
    if (LogInfo->stopRun) {
        return;
    }

    // Setup/send rank information for each group
    if (rank == SW_MPI_ROOT) {
        // Make a list of ranks within compute and I/O processes
        for (node = 0; node < numNodes; node++) {
            for (rankIndex = 0; rankIndex < numProcsInNode[node]; rankIndex++) {
                sendRank = ranksInNodes[node][rankIndex];
                procJob = (sendRank == SW_MPI_ROOT) ?
                              rankJob :
                              designations[node][rankIndex].procJob;

                if (procJob == SW_MPI_PROC_COMP) {
                    ranksInComp[compIndex] = sendRank;
                    compIndex++;
                } else {
                    if (sendRank == SW_MPI_ROOT) {
                        ranksInComp[compIndex] = sendRank;
                        compIndex++;
                    }
                    ranksInIO[ioIndex] = sendRank;
                    ioIndex++;
                }
            }
        }

        // Send rank list(s) to respective processes so they
        // can be apart of the group creation
        for (node = 0; node < numNodes; node++) {
            for (rankIndex = 0; rankIndex < numProcsInNode[node]; rankIndex++) {
                sendRank = ranksInNodes[node][rankIndex];
                procJob = (sendRank == SW_MPI_ROOT) ?
                              rankJob :
                              designations[node][rankIndex].procJob;
                buff = (procJob == SW_MPI_PROC_COMP) ? ranksInComp : ranksInIO;
                numElem = (procJob == SW_MPI_PROC_COMP) ? numCompProcs :
                                                          numIOProcsTot;

                if (sendRank > SW_MPI_ROOT) {
                    SW_MPI_Send(
                        MPI_INT, buff, numElem, sendRank, swTRUE, 0, &nullReq
                    );
                }
            }
        }
    }
    buff = (rankJob == SW_MPI_PROC_COMP) ? ranksInComp : ranksInIO;
    numElem = (rankJob == SW_MPI_PROC_COMP) ? numCompProcs : numIOProcsTot;

    if (rank > SW_MPI_ROOT) {
        SW_MPI_Recv(MPI_INT, buff, numElem, SW_MPI_ROOT, swTRUE, 0, &nullReq);
    }

    /* Create I/O and compute communicators;
       put the root process into both so it can
       properly spread information that is only for compute and I/O
       processes
    */
    if (rankJob == SW_MPI_PROC_IO) {
        mpi_create_group_comms(numElem, buff, groupComm);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rankJob == SW_MPI_PROC_COMP || rank == SW_MPI_ROOT) {
        mpi_create_group_comms(
            (rank > SW_MPI_ROOT) ? numElem : numCompProcs,
            (rank > SW_MPI_ROOT) ? buff : ranksInComp,
            (rank > SW_MPI_ROOT) ? groupComm : rootCompComm
        );
    }
}

/**
@brief Send input path information to all I/O processes

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] comm MPI communicator to broadcast a message to
@param[in] nYears Number of years within the simulation
@param[in] netCDFIn Struct of type SW_NETCDF_IN that will be distributed
    (root process) and received (all others)
@param[out] pathInputs Inputs Struct of type SW_PATH_INPUTS which
    holds basic information about input files and values
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_path_info(
    int rank,
    MPI_Comm comm,
    TimeInt nYears,
    SW_NETCDF_IN *netCDFIn,
    SW_PATH_INPUTS *pathInputs,
    LOG_INFO *LogInfo
) {
    InKeys inKey;
    int var;
    int startVar;
    unsigned int file;

    // Get number of weather files and days in year for those years
    SW_Bcast(MPI_INT, &pathInputs->ncNumWeatherInFiles, 1, SW_MPI_ROOT, comm);

    if (rank > SW_MPI_ROOT) {
        pathInputs->numDaysInYear = (unsigned int *) Mem_Malloc(
            sizeof(unsigned int) * nYears, "get_path_info()", LogInfo
        );
    }
    if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
        return;
    }

    SW_Bcast(
        MPI_UNSIGNED, pathInputs->numDaysInYear, nYears, SW_MPI_ROOT, comm
    );

    get_dynamic_string(
        rank, &pathInputs->txtInFiles[eLog], swTRUE, comm, LogInfo
    );

    if (netCDFIn->readInVars[eSW_InWeather][0] && rank > SW_MPI_ROOT) {
        SW_NCIN_allocate_startEndYrs(
            &pathInputs->ncWeatherInStartEndYrs,
            pathInputs->ncNumWeatherInFiles,
            LogInfo
        );
    }
    if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
        return;
    }

    ForEachNCInKey(inKey) {
        if (!netCDFIn->readInVars[inKey][0]) {
            continue;
        }

        startVar = 0;

        while (startVar < numVarsInKey[inKey] &&
               !netCDFIn->readInVars[inKey][startVar + 1]) {
            startVar++;
        }

        // Allocate and send/receive variable information
        if (rank > SW_MPI_ROOT) {
            SW_NCIN_alloc_sim_var_information(
                numVarsInKey[inKey],
                inKey,
                swFALSE,
                &pathInputs->inVarIDs[inKey],
                &pathInputs->inVarTypes[inKey],
                &pathInputs->hasScaleAndAddFact[inKey],
                &pathInputs->scaleAndAddFactVals[inKey],
                &pathInputs->missValFlags[inKey],
                NULL,
                &pathInputs->numSoilVarLyrs,
                LogInfo
            );
        }
        if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
            return;
        }

        if (inKey == eSW_InSoil) {
            SW_Bcast(
                MPI_UNSIGNED_LONG,
                pathInputs->numSoilVarLyrs,
                numVarsInKey[inKey],
                SW_MPI_ROOT,
                comm
            );
        }

        SW_Bcast(
            MPI_INT,
            pathInputs->inVarIDs[inKey],
            numVarsInKey[inKey],
            SW_MPI_ROOT,
            comm
        );
        SW_Bcast(
            MPI_INT,
            pathInputs->inVarTypes[inKey],
            numVarsInKey[inKey],
            SW_MPI_ROOT,
            comm
        );
        SW_Bcast(
            MPI_INT,
            pathInputs->hasScaleAndAddFact[inKey],
            numVarsInKey[inKey],
            SW_MPI_ROOT,
            comm
        );

        // Transfer information that is kept track for variables in a key
        for (var = startVar; var < numVarsInKey[inKey]; var++) {
            if (!netCDFIn->readInVars[inKey][var + 1]) {
                continue;
            }

            // Get 2D array information for variables
            SW_Bcast(
                MPI_DOUBLE,
                pathInputs->scaleAndAddFactVals[inKey][var],
                2,
                SW_MPI_ROOT,
                comm
            );
            SW_Bcast(
                MPI_INT,
                pathInputs->missValFlags[inKey][var],
                SIM_INFO_NFLAGS,
                SW_MPI_ROOT,
                comm
            );

            if (isnull(pathInputs->doubleMissVals[inKey])) {
                SW_NCIN_alloc_miss_vals(
                    numVarsInKey[inKey],
                    &pathInputs->doubleMissVals[inKey],
                    LogInfo
                );
            }
            if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
                return;
            }

            SW_Bcast(
                MPI_DOUBLE,
                pathInputs->doubleMissVals[inKey][var],
                2,
                SW_MPI_ROOT,
                comm
            );
        }

        // Copy weather start/end years
        if (inKey == eSW_InWeather) {
            for (file = 0; file < pathInputs->ncNumWeatherInFiles; file++) {
                SW_Bcast(
                    MPI_UNSIGNED,
                    pathInputs->ncWeatherInStartEndYrs[file],
                    2,
                    SW_MPI_ROOT,
                    comm
                );
            }
        }
    }
}

/**
@brief Wrapper for MPI function to free a type and throw a
    warning if a free throws an error

@param[in,out] type
@param[out] LogInfo
*/
static void free_type(MPI_Datatype *type, LOG_INFO *LogInfo) {
    int res = MPI_SUCCESS;

    res = MPI_Type_free(type);

    if (res != MPI_SUCCESS) {
        LogError(LogInfo, LOGWARN, "Could not free a custom MPI type.");
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Intiialize the MPI program by getting basic information about
the rank, world size, and processor/node name, while setting the
MPI handler method to return from a function call rather than
crashing the program

@param[in] argc Number of command-line provided inputs
@param[in] argv List of command-line provided inputs
@param[out] rank Process number known to MPI for the current process (aka rank)
@param[out] worldSize Total number of processes that the MPI run has created
@param[out] procName Name of the processor/node the current processes is
running on
*/
void SW_MPI_initialize(
    int *argc, char ***argv, int *rank, int *worldSize, char *procName
) {
    int procNameSize = 0;

    MPI_Init(argc, argv);

    MPI_Comm_rank(MPI_COMM_WORLD, rank);
    MPI_Comm_size(MPI_COMM_WORLD, worldSize);

    MPI_Get_processor_name(procName, &procNameSize);

    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
}

/**
@brief Conclude the program run by finalizing/freeing anything that's
been initialized/created through MPI within the program run
*/
void SW_MPI_finalize() { MPI_Finalize(); }

/**
@brief Wrapper for MPI-provided function `MPI_Send()` and reduces
the ambiguity of how the information gets sent (no longer buffer or
synchronous)

@param[in] datatype Custom MPI datatype that will be sent
@param[in] buffer Location of memory that holds the memory that
we will be sending
@param[in] count Number of items to send from the buffer
@param[in] destRank Destination rank that information will be sent to
@param[in] sync Specifies if the send should be synchronous or
asynchronous
@param[in] tag Unique identifier for a message that does not interact
with any message tag values
@param[in,out] request Type MPI_Request that holds information on a
previous asynchronous send
*/
void SW_MPI_Send(
    MPI_Datatype datatype,
    void *buffer,
    int count,
    int destRank,
    Bool sync,
    int tag,
    MPI_Request *request
) {
    int result = 0;

    if (sync) {
        result =
            MPI_Ssend(buffer, count, datatype, destRank, tag, MPI_COMM_WORLD);
    } else {
        if (*request != MPI_REQUEST_NULL) {
            result = MPI_Wait(request, MPI_STATUS_IGNORE);
            if (result != MPI_SUCCESS) {
                goto error;
            }

            result = MPI_Request_free(request);
            if (result != MPI_SUCCESS) {
                goto error;
            }
        }

        result = MPI_Issend(
            buffer, count, datatype, destRank, tag, MPI_COMM_WORLD, request
        );
    }

error: {
    if (result != MPI_SUCCESS) {
        errorMPI(result);
    }
}
}

/**
@brief Wrapper for MPI-provided function `MPI_Recv()` and reduces
the ambiguity of how the information gets sent (no longer buffer or
synchronous)

@param[in] datatype Custom MPI datatype that will be sent
@param[in] buffer Location of memory that will be written to
@param[in] count Number of items to recv from the buffer
@param[in] srcRank Source rank that information will be received from
@param[in] sync Specifies if the receive should be synchronous or
asynchronous
@param[in] tag Unique identifier for a message that does not interact
with any message tag values
@param[in,out] request Type MPI_Request that holds information on a
previous asynchronous receive
*/
void SW_MPI_Recv(
    MPI_Datatype datatype,
    void *buffer,
    int count,
    int srcRank,
    Bool sync,
    int tag,
    MPI_Request *request
) {
    int result = 0;

    if (sync) {
        result = MPI_Recv(
            buffer,
            count,
            datatype,
            srcRank,
            tag,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );
    } else {
        if (*request != MPI_REQUEST_NULL) {
            result = MPI_Wait(request, MPI_STATUS_IGNORE);
            if (result != MPI_SUCCESS) {
                goto error;
            }

            result = MPI_Request_free(request);
            if (result != MPI_SUCCESS) {
                goto error;
            }
        }

        result = MPI_Irecv(
            buffer, count, datatype, srcRank, tag, MPI_COMM_WORLD, request
        );
    }

error: {
    if (result != MPI_SUCCESS) {
        errorMPI(result);
    }
}
}

/**
@brief Throughout the program, various data will need to be sent around,
much of which, MPI does not provide default support for (not integer, double,
etc.), so this function will create custom MPI types based on program-defined
structs

@param[out] datatypes A list of custom MPI datatypes that will be created based
on various program-defined structs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_create_types(MPI_Datatype datatypes[], LOG_INFO *LogInfo) {
    int res;
    int numItems[] = {5, 5, 5, 5, 6, 5, 18};
    int blockLens[][19] = {
        {1, 1, 1, 1, 1},    /* SW_DOMAIN */
        {1, 1, 1, 1, 1},    /* SW_SPINUP */
        {1, 1, 1, 1, 1},    /* SW_RUN_INPUTS */
        {1, 1, 1, 1, 1},    /* SW_MPI_DESIGNATE */
        {1, 1, 1, 1, 1, 1}, /* SW_MPI_WallTime */
        {SW_OUTNKEYS,
         SW_OUTNKEYS,
         SW_OUTNPERIODS,
         1,
         SW_OUTNKEYS * SW_OUTNPERIODS}, /* SW_OUT_DOM */
        {MAX_FILENAMESIZE,
         MAX_SPECIESNAMELEN + 1,
         1,
         1,
         1,
         1,
         1,
         1,
         1,
         1,
         1,
         2,
         1,
         1,
         1,
         1,
         1,
         1} /* SW_VEGESTAB_INPUTS */
    };

    MPI_Datatype types[][18] = {
        {MPI_INT, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED
        }, /* SW_DOMAIN */
        {MPI_UNSIGNED, MPI_UNSIGNED, MPI_INT, MPI_INT, MPI_UNSIGNED
        },  /* SW_SPINUP */
        {}, /* SW_MPI_INPUTS */
        {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_UNSIGNED
        }, /* SW_MPI_DESIGNATE */
        {MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_UNSIGNED_LONG,
         MPI_UNSIGNED_LONG}, /* SW_MPI_WallTime */
        {MPI_INT, MPI_INT, MPI_UNSIGNED_LONG, MPI_INT, MPI_INT
        }, /* SW_OUT_DOM */
        {MPI_CHAR,
         MPI_CHAR,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_UNSIGNED,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE} /* SW_VEGESTAB_INPUTS */
    };
    MPI_Aint offsets[][18] = {
        /* SW_DOMAIN */
        {offsetof(SW_DOMAIN, hasConsistentSoilLayerDepths),
         offsetof(SW_DOMAIN, nMaxSoilLayers),
         offsetof(SW_DOMAIN, nMaxEvapLayers),
         offsetof(SW_DOMAIN, startyr),
         offsetof(SW_DOMAIN, endyr)},

        /* SW_SPINUP */
        {offsetof(SW_SPINUP, scope),
         offsetof(SW_SPINUP, duration),
         offsetof(SW_SPINUP, mode),
         offsetof(SW_SPINUP, rng_seed),
         offsetof(SW_SPINUP, spinup)},

        /* SW_RUN_INPUTS */
        {offsetof(SW_RUN_INPUTS, SkyRunIn),
         offsetof(SW_RUN_INPUTS, ModelRunIn),
         offsetof(SW_RUN_INPUTS, SoilRunIn),
         offsetof(SW_RUN_INPUTS, VegProdRunIn),
         offsetof(SW_RUN_INPUTS, SiteRunIn)},

        /* SW_MPI_DESIGNATE */
        {offsetof(SW_MPI_DESIGNATE, procJob),
         offsetof(SW_MPI_DESIGNATE, ioRank),
         offsetof(SW_MPI_DESIGNATE, nCompProcs),
         offsetof(SW_MPI_DESIGNATE, nSuids),
         offsetof(SW_MPI_DESIGNATE, useTSuids)},

        /* SW_MPI_WallTime */
        {offsetof(SW_WALLTIME, timeMean),
         offsetof(SW_WALLTIME, timeSD),
         offsetof(SW_WALLTIME, timeMin),
         offsetof(SW_WALLTIME, timeMax),
         offsetof(SW_WALLTIME, nTimedRuns),
         offsetof(SW_WALLTIME, nUntimedRuns)},

        /* SW_OUT_DOM */
        {offsetof(SW_OUT_DOM, sumtype),
         offsetof(SW_OUT_DOM, use),
         offsetof(SW_OUT_DOM, nrow_OUT),
         offsetof(SW_OUT_DOM, used_OUTNPERIODS),
         offsetof(SW_OUT_DOM, timeSteps)},

        /* SW_VEGESTAB_INPUTS */
        {offsetof(SW_VEGESTAB_INFO_INPUTS, sppFileName),
         offsetof(SW_VEGESTAB_INFO_INPUTS, sppname),
         offsetof(SW_VEGESTAB_INFO_INPUTS, vegType),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_pregerm_days),
         offsetof(SW_VEGESTAB_INFO_INPUTS, max_pregerm_days),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_wetdays_for_germ),
         offsetof(SW_VEGESTAB_INFO_INPUTS, max_drydays_postgerm),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_wetdays_for_estab),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_days_germ2estab),
         offsetof(SW_VEGESTAB_INFO_INPUTS, max_days_germ2estab),
         offsetof(SW_VEGESTAB_INFO_INPUTS, estab_lyrs),
         offsetof(SW_VEGESTAB_INFO_INPUTS, bars),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_swc_germ),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_swc_estab),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_temp_germ),
         offsetof(SW_VEGESTAB_INFO_INPUTS, max_temp_germ),
         offsetof(SW_VEGESTAB_INFO_INPUTS, min_temp_estab),
         offsetof(SW_VEGESTAB_INFO_INPUTS, max_temp_estab)}
    };

    int numItemsInStructs[] = {6, 6, 12, 2, 1};

    MPI_Datatype inTypes[][12] = {
        {MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE
        },
        {MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_UNSIGNED},
        {MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE},
        {},
        {MPI_DOUBLE}
    };

    int inBlockLens[][12] = {
        {MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS + 1},
        {1, 1, 1, 1, 1, 1},
        {MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         NVEGTYPES * MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS,
         MAX_LAYERS * SWRC_PARAM_NMAX},
        {NVEGTYPES, 1},
        {1}
    };

    MPI_Aint inOffsets[][12] = {
        {offsetof(SW_SKY_INPUTS, cloudcov),
         offsetof(SW_SKY_INPUTS, windspeed),
         offsetof(SW_SKY_INPUTS, r_humidity),
         offsetof(SW_SKY_INPUTS, snow_density),
         offsetof(SW_SKY_INPUTS, n_rain_per_day),
         offsetof(SW_SKY_INPUTS, snow_density_daily)},
        {offsetof(SW_MODEL_RUN_INPUTS, longitude),
         offsetof(SW_MODEL_RUN_INPUTS, latitude),
         offsetof(SW_MODEL_RUN_INPUTS, elevation),
         offsetof(SW_MODEL_RUN_INPUTS, slope),
         offsetof(SW_MODEL_RUN_INPUTS, aspect),
         offsetof(SW_MODEL_RUN_INPUTS, isnorth)},
        {offsetof(SW_SOIL_RUN_INPUTS, width),
         offsetof(SW_SOIL_RUN_INPUTS, depths),
         offsetof(SW_SOIL_RUN_INPUTS, soilDensityInput),
         offsetof(SW_SOIL_RUN_INPUTS, evap_coeff),
         offsetof(SW_SOIL_RUN_INPUTS, transp_coeff),
         offsetof(SW_SOIL_RUN_INPUTS, fractionVolBulk_gravel),
         offsetof(SW_SOIL_RUN_INPUTS, fractionWeightMatric_sand),
         offsetof(SW_SOIL_RUN_INPUTS, fractionWeightMatric_clay),
         offsetof(SW_SOIL_RUN_INPUTS, fractionWeight_om),
         offsetof(SW_SOIL_RUN_INPUTS, impermeability),
         offsetof(SW_SOIL_RUN_INPUTS, avgLyrTempInit),
         offsetof(SW_SOIL_RUN_INPUTS, swrcpMineralSoil)},
        {offsetof(SW_VEGPROD_RUN_INPUTS, veg),
         offsetof(SW_VEGPROD_RUN_INPUTS, bare_cov)},
        {offsetof(SW_SITE_RUN_INPUTS, Tsoil_constant)}
    };

    int numItemsInCov[] = {1, 5};
    MPI_Datatype covTypes[][5] = {
        {MPI_DOUBLE},
        {MPI_DATATYPE_NULL, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE}
    };
    int covBlockLens[][5] = {
        {1}, {1, MAX_MONTHS, MAX_MONTHS, MAX_MONTHS, MAX_MONTHS}
    };
    MPI_Aint covOffsets[][5] = {
        {offsetof(CoverType, fCover)},
        {offsetof(VegType, cov),
         offsetof(VegType, litter),
         offsetof(VegType, biomass),
         offsetof(VegType, pct_live),
         offsetof(VegType, lai_conv)}
    };

    int typeIndices[] = {
        eSW_MPI_Domain,
        eSW_MPI_Spinup,
        eSW_MPI_Inputs,
        eSW_MPI_Designate,
        eSW_MPI_WallTime,
        eSW_MPI_OutDomIO,
        eSW_MPI_VegEstabIn
    };

    int type;
    int typeIndex;
    int runTypeIndex;
    int covIndex;
    const int numTypes = 7;
    const int numRunInTypes = 5;
    const int numCovTypes = 2;
    const int vegprodIndex = 3;

    for (type = 0; type < numTypes; type++) {
        typeIndex = typeIndices[type];

        if (typeIndex == eSW_MPI_Inputs) {
            /* Create custom MPI subtypes for bigger SW_RUN_INPUTS */
            for (runTypeIndex = 0; runTypeIndex < numRunInTypes;
                 runTypeIndex++) {

                if (runTypeIndex == vegprodIndex) {
                    /* Create custom MPI subtypes for VegType and CovType */
                    for (covIndex = 0; covIndex < numCovTypes; covIndex++) {
                        res = MPI_Type_create_struct(
                            numItemsInCov[covIndex],
                            covBlockLens[covIndex],
                            covOffsets[covIndex],
                            covTypes[covIndex],
                            (covIndex == 0) ? &covTypes[1][0] :
                                              &inTypes[vegprodIndex][0]
                        );
                        if (res != MPI_SUCCESS) {
                            goto reportFail;
                        }

                        res = MPI_Type_commit(
                            (covIndex == 0) ? &covTypes[1][0] :
                                              &inTypes[vegprodIndex][0]
                        );
                        if (res != MPI_SUCCESS) {
                            goto reportFail;
                        }

                        /* Copy CoverType to second type placement
                           for bare cover */
                        if (covIndex == 1) {
                            inTypes[vegprodIndex][1] = inTypes[vegprodIndex][0];
                        }
                    }
                }

                res = MPI_Type_create_struct(
                    numItemsInStructs[runTypeIndex],
                    inBlockLens[runTypeIndex],
                    inOffsets[runTypeIndex],
                    inTypes[runTypeIndex],
                    &types[type][runTypeIndex]
                );
                if (res != MPI_SUCCESS) {
                    goto reportFail;
                }
                res = MPI_Type_commit(&types[type][runTypeIndex]);
                if (res != MPI_SUCCESS) {
                    goto reportFail;
                }
            }
        }

        res = MPI_Type_create_struct(
            numItems[type],
            blockLens[type],
            offsets[type],
            types[type],
            &datatypes[typeIndex]
        );
        if (res != MPI_SUCCESS) {
            goto reportFail;
        }
        res = MPI_Type_commit(&datatypes[typeIndex]);
        if (res != MPI_SUCCESS) {
            goto reportFail;
        }

        if (typeIndex == eSW_MPI_Inputs) {
            free_type(&inTypes[vegprodIndex][0], LogInfo);
            free_type(&inTypes[vegprodIndex][1], LogInfo);
        }
    }

    /* Free run input sub types */
    for (runTypeIndex = 0; runTypeIndex < numRunInTypes; runTypeIndex++) {
        free_type(&types[eSW_MPI_Inputs][runTypeIndex], LogInfo);
    }

reportFail:
    if (res != MPI_SUCCESS) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred when attempting to create custom MPI types."
        );
    }
}

/**
@brief Setup MPI processes with basic information from domain and
    template(s)

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] procName Name of the processor the process is being run on
    (agnostic of if on HPC or local computer)
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in,out] sw_template Template SW_RUN for the function to use as a
    reference for local versions of SW_RUN; root process will send necessary
    information to other processes
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_setup(
    int rank,
    int worldSize,
    char *procName,
    SW_DOMAIN *SW_Domain,
    SW_RUN *sw_template,
    LOG_INFO *LogInfo
) {
    if (SW_MPI_check_setup_status(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    SW_MPI_process_types(SW_Domain, procName, worldSize, rank, LogInfo);
    if (SW_MPI_check_setup_status(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    SW_MPI_template_info(
        rank,
        &SW_Domain->SW_Designation,
        sw_template,
        SW_Domain->datatypes[eSW_MPI_Inputs],
        SW_Domain->datatypes[eSW_MPI_Spinup],
        SW_Domain->datatypes[eSW_MPI_VegEstabIn],
        LogInfo
    );
    if (SW_MPI_check_setup_status(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    SW_MPI_domain_info(SW_Domain, rank, LogInfo);
}

/**
@brief Once the setup is complete in the root process, send template
information to all other processes

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in,out] SW_Run SW_RUN template what needs to be copied to
    all compute processes with some information pertaining to I/O
@param[in] inRunType Custom MPI type for transfering data for SW_RUN_INPUTS
@param[in] spinupType Custom MPI type for transfering data for SW_SPINUP
@param[in] vegEstabType Custom MPI type for transfering data for
    SW_VEGESTAB_INFO_INPUTS
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_template_info(
    int rank,
    SW_MPI_DESIGNATE *desig,
    SW_RUN *SW_Run,
    MPI_Datatype inRunType,
    MPI_Datatype spinupType,
    MPI_Datatype vegEstabType,
    LOG_INFO *LogInfo
) {
    const int numStructs = 6; /* Do not include veg estab */
    const int swIndex = 4;
    const int vegEstabIndex = 6;
    const int num2DMarkov = 8;
    const int num1DMarkov = 3;

    IntUS vCount;
    int structType;
    int var;
    int numElem[] = {3, 24, 6, 5, 7, 42, 2};
    void **markov2DBuffer[] = {
        (void **) &SW_Run->MarkovIn.wetprob,
        (void **) &SW_Run->MarkovIn.dryprob,
        (void **) &SW_Run->MarkovIn.avg_ppt,
        (void **) &SW_Run->MarkovIn.std_ppt,
        (void **) &SW_Run->MarkovIn.cfxw,
        (void **) &SW_Run->MarkovIn.cfxd,
        (void **) &SW_Run->MarkovIn.cfnw,
        (void **) &SW_Run->MarkovIn.cfnd,
    };
    void *markov1DBuffer[] = {
        (void *) SW_Run->MarkovIn.u_cov,
        (void *) SW_Run->MarkovIn.v_cov,
        (void *) &SW_Run->MarkovIn.ppt_events
    };
    void *buffers[][42] = {
        {(void *) &SW_Run->CarbonIn.use_wue_mult,
         (void *) &SW_Run->CarbonIn.use_bio_mult,
         (void *) SW_Run->CarbonIn.ppm},
        {(void *) &SW_Run->WeatherIn.use_snow,
         (void *) &SW_Run->WeatherIn.use_weathergenerator_only,
         (void *) &SW_Run->WeatherIn.generateWeatherMethod,
         (void *) &SW_Run->WeatherIn.pct_snowdrift,
         (void *) &SW_Run->WeatherIn.pct_snowRunoff,
         (void *) SW_Run->WeatherIn.scale_precip,
         (void *) SW_Run->WeatherIn.scale_temp_max,
         (void *) SW_Run->WeatherIn.scale_temp_min,
         (void *) SW_Run->WeatherIn.scale_skyCover,
         (void *) SW_Run->WeatherIn.scale_wind,
         (void *) SW_Run->WeatherIn.scale_rH,
         (void *) SW_Run->WeatherIn.scale_actVapPress,
         (void *) SW_Run->WeatherIn.scale_shortWaveRad,
         (void *) SW_Run->WeatherIn.name_prefix,
         (void *) &SW_Run->WeatherIn.rng_seed,
         (void *) &SW_Run->WeatherIn.use_cloudCoverMonthly,
         (void *) &SW_Run->WeatherIn.use_windSpeedMonthly,
         (void *) &SW_Run->WeatherIn.use_humidityMonthly,
         (void *) SW_Run->WeatherIn.dailyInputFlags,
         (void *) SW_Run->WeatherIn.dailyInputIndices,
         (void *) &SW_Run->WeatherIn.n_input_forcings,
         (void *) &SW_Run->WeatherIn.desc_rsds,
         (void *) &SW_Run->WeatherIn.n_years,
         (void *) &SW_Run->WeatherIn.startYear},
        {(void *) &SW_Run->VegProdIn.vegYear,
         (void *) &SW_Run->VegProdIn.isBiomAsIf100Cover,
         (void *) &SW_Run->VegProdIn.use_SWA,
         (void *) SW_Run->VegProdIn.critSoilWater,
         (void *) SW_Run->VegProdIn.rank_SWPcrits,
         (void *) &SW_Run->VegProdIn.veg_method},
        {(void *) &SW_Run->ModelIn.SW_SpinUp,
         (void *) &SW_Run->ModelIn.startyr,
         (void *) &SW_Run->ModelIn.endyr,
         (void *) &SW_Run->ModelIn.startstart,
         (void *) &SW_Run->ModelIn.endend},
        {(void *) &SW_Run->SoilWatIn.hist_use,
         (void *) &SW_Run->SoilWatIn.hist.method,
         (void *) &SW_Run->SoilWatIn.hist.yr.first,
         (void *) &SW_Run->SoilWatIn.hist.yr.last,
         (void *) &SW_Run->SoilWatIn.hist.yr.total,
         (void *) SW_Run->SoilWatIn.hist.swc,
         (void *) SW_Run->SoilWatIn.hist.std_err},
        {(void *) SW_Run->SiteIn.site_swrc_name,
         (void *) SW_Run->SiteIn.site_ptf_name,
         (void *) &SW_Run->SiteIn.use_soil_temp,
         (void *) &SW_Run->SiteIn.methodSurfaceTemperature,
         (void *) &SW_Run->SiteIn.site_swrc_type,
         (void *) &SW_Run->SiteIn.site_swrc_name,
         (void *) &SW_Run->SiteIn.t1Param1,
         (void *) &SW_Run->SiteIn.t1Param2,
         (void *) &SW_Run->SiteIn.t1Param3,
         (void *) &SW_Run->SiteIn.csParam1,
         (void *) &SW_Run->SiteIn.csParam2,
         (void *) &SW_Run->SiteIn.shParam,
         (void *) &SW_Run->SiteIn.bmLimiter,
         (void *) &SW_Run->SiteIn.stDeltaX,
         (void *) &SW_Run->SiteIn.stMaxDepth,
         (void *) &SW_Run->SiteIn.depthSapric,
         (void *) &SW_Run->SiteIn.type_soilDensityInput,
         (void *) &SW_Run->SiteIn.reset_yr,
         (void *) &SW_Run->SiteIn.deepdrain,
         (void *) &SW_Run->SiteIn.inputsProvideSWRCp,
         (void *) &SW_Run->SiteIn.evap.range,
         (void *) &SW_Run->SiteIn.evap.slope,
         (void *) &SW_Run->SiteIn.evap.xinflec,
         (void *) &SW_Run->SiteIn.evap.yinflec,
         (void *) &SW_Run->SiteIn.transp.range,
         (void *) &SW_Run->SiteIn.transp.slope,
         (void *) &SW_Run->SiteIn.transp.xinflec,
         (void *) &SW_Run->SiteIn.transp.yinflec,
         (void *) &SW_Run->SiteIn.slow_drain_coeff,
         (void *) &SW_Run->SiteIn.pet_scale,
         (void *) &SW_Run->SiteIn.TminAccu2,
         (void *) &SW_Run->SiteIn.TmaxCrit,
         (void *) &SW_Run->SiteIn.lambdasnow,
         (void *) &SW_Run->SiteIn.RmeltMin,
         (void *) &SW_Run->SiteIn.RmeltMax,
         (void *) &SW_Run->SiteIn.percentRunoff,
         (void *) &SW_Run->SiteIn.percentRunon,
         (void *) &SW_Run->SiteIn.SWCInitVal,
         (void *) &SW_Run->SiteIn.SWCWetVal,
         (void *) &SW_Run->SiteIn.SWCMinVal},
        {(void *) &SW_Run->VegEstabSim.use, (void *) &SW_Run->VegEstabSim.count}
    };
    MPI_Datatype types[][42] = {
        {MPI_INT, MPI_INT, MPI_DOUBLE}, /* SW_CARBON_INPUTS */
        {MPI_INT,      MPI_INT,      MPI_UNSIGNED, MPI_DOUBLE,  MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,  MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,   MPI_CHAR,    MPI_INT,
         MPI_INT,      MPI_INT,      MPI_INT,      MPI_INT,     MPI_UNSIGNED,
         MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED
        }, /* SW_WEATHER_INPUTS */
        {MPI_INT, MPI_DOUBLE, MPI_INT, MPI_INT, MPI_INT, MPI_INT
        }, /* SW_VEGPROD_INPUTS */
        {spinupType, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED
        }, /* SW_MODEL_INPUTS */
        {MPI_INT, MPI_INT, MPI_UNSIGNED, MPI_DOUBLE, MPI_DOUBLE
        }, /* SW_SOILWAT_INPUTS */
        {MPI_CHAR,     MPI_CHAR,   MPI_INT,    MPI_UNSIGNED, MPI_UNSIGNED,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,
         MPI_UNSIGNED, MPI_INT,    MPI_INT,    MPI_INT,      MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE}, /* SW_SITE_INPUTS */
        {MPI_INT, MPI_UNSIGNED}     /* SW_VEGPROD_SIM */
    };
    int count[][42] = {
        {1, 1, MAX_NYEAR}, /* SW_CARBON_INPUTS */
        {1,
         1,
         1,
         1,
         1,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_FILENAMESIZE - 5,
         1,
         1,
         1,
         1,
         MAX_INPUT_COLUMNS,
         1,
         1,
         1,
         1},                                /* SW_WEATHER_INPUTS */
        {1, NVEGTYPES, NVEGTYPES, 1, 1, 1}, /* SW_VEGPROD_INPUTS */
        {1, 1, 1, 1, 1},                    /* SW_MODEL_INPUTS */
        {1, 1, 1, 1, MAX_DAYS * MAX_LAYERS, MAX_DAYS * MAX_LAYERS
        }, /* SW_SOILWAT_INPUTS */
        {64, 64, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1,  1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
        },     /* SW_SITE_INPUTS */
        {1, 1} /* SW_VEGPROD_SIM */
    };
    int markov1DCount[] = {MAX_WEEKS * 2, MAX_WEEKS * 2 * 2, 1};

    MPI_Comm comm =
        (rank == SW_MPI_ROOT) ? desig->rootCompComm : desig->groupComm;

    // Send input information to all processes
    if (desig->procJob == SW_MPI_PROC_COMP || rank == SW_MPI_ROOT) {
        for (structType = 0; structType < numStructs; structType++) {
            for (var = 0; var < numElem[structType]; var++) {
                SW_Bcast(
                    types[structType][var],
                    buffers[structType][var],
                    count[structType][var],
                    SW_MPI_ROOT,
                    comm
                );

                if (structType == swIndex && !SW_Run->SoilWatIn.hist_use) {
                    break;
                }
            }
        }
    }

    // Markov inputs if enabled
    if (SW_Run->WeatherIn.generateWeatherMethod == 2) {
        if (rank > SW_MPI_ROOT) {
            SW_MKV_construct(SW_Run->WeatherIn.rng_seed, &SW_Run->MarkovIn);
            allocateMKV(&SW_Run->MarkovIn, LogInfo);
        }
        if (SW_MPI_check_setup_status(LogInfo->stopRun, comm)) {
            return;
        }

        for (var = 0; var < num2DMarkov; var++) {
            SW_Bcast(
                MPI_DOUBLE, *markov2DBuffer[var], MAX_DAYS, SW_MPI_ROOT, comm
            );
        }

        for (var = 0; var < num1DMarkov; var++) {
            SW_Bcast(
                (var == 2) ? MPI_INT : MPI_DOUBLE,
                markov1DBuffer[var],
                markov1DCount[var],
                SW_MPI_ROOT,
                comm
            );
        }
    }

    /* Vegetation establishment information
       We must do it outside of the major loop to have
       all processes take place */
    SW_Bcast(
        types[vegEstabIndex][0],
        buffers[vegEstabIndex][0],
        count[vegEstabIndex][0],
        SW_MPI_ROOT,
        MPI_COMM_WORLD
    );

    if (SW_Run->VegEstabSim.use) {
        SW_Bcast(
            types[vegEstabIndex][1],
            buffers[vegEstabIndex][1],
            count[vegEstabIndex][1],
            SW_MPI_ROOT,
            MPI_COMM_WORLD
        );

        for (vCount = 0; vCount < SW_Run->VegEstabSim.count; vCount++) {
            SW_Bcast(
                vegEstabType,
                &SW_Run->VegEstabIn.parms[vCount],
                1,
                SW_MPI_ROOT,
                MPI_COMM_WORLD
            );
        }
    }

    // SW_RUN_INPUTS
    SW_Bcast(inRunType, &SW_Run->RunIn, 1, SW_MPI_ROOT, MPI_COMM_WORLD);
}

/**
@brief Once the setup is complete in the root process, send domain
information to all other processes; all others receive

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] rank Process number known to MPI for the current process (aka rank)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_domain_info(SW_DOMAIN *SW_Domain, int rank, LOG_INFO *LogInfo) {
    // rootCompComm - not used if not root process
    MPI_Comm *rootCompComm = &SW_Domain->SW_Designation.rootCompComm;
    MPI_Comm *groupComm = &SW_Domain->SW_Designation.groupComm;
    MPI_Datatype *types = SW_Domain->datatypes;
    int procJob = SW_Domain->SW_Designation.procJob;
    TimeInt nYears;

    // Send/get soil/temporal information
    if (procJob == SW_MPI_PROC_IO || rank == SW_MPI_ROOT) {
        SW_Bcast(types[eSW_MPI_Domain], SW_Domain, 1, SW_MPI_ROOT, *groupComm);
    }

    // Send/get Spinup information - compute processes + rank only
    if (procJob == SW_MPI_PROC_COMP || rank == SW_MPI_ROOT) {
        SW_Bcast(
            types[eSW_MPI_Spinup],
            &SW_Domain->SW_SpinUp,
            1,
            SW_MPI_ROOT,
            (rank == SW_MPI_ROOT) ? *rootCompComm : *groupComm
        );
    }

    // Send/get netCDF information - I/O processes only
    if (procJob == SW_MPI_PROC_IO) {
        get_NC_info(rank, *groupComm, &SW_Domain->netCDFInput, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }
    }

    // Send/get path information
    if (procJob == SW_MPI_PROC_IO || rank == SW_MPI_ROOT) {
        nYears = SW_Domain->endyr - SW_Domain->startyr + 1;
        get_path_info(
            rank,
            *groupComm,
            nYears,
            &SW_Domain->netCDFInput,
            &SW_Domain->SW_PathInputs,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    // Send/get domain information - SW_PATH_OUTPUTS will wait until later
    SW_Bcast(
        types[eSW_MPI_OutDomIO],
        &SW_Domain->OutDom,
        1,
        SW_MPI_ROOT,
        MPI_COMM_WORLD
    );

    SW_Bcast(
        MPI_INT,
        SW_Domain->OutDom.use_OutPeriod,
        SW_OUTNPERIODS,
        SW_MPI_ROOT,
        MPI_COMM_WORLD
    );
}

/**
@brief Before we proceed to the next important section of the program,
we must do a check-in with all processes to make sure no errors occurred

@param[in] stopRun A flag specifying if an error occurred and stop the run
@param[in] comm MPI communicator to broadcast a message to
*/
Bool SW_MPI_check_setup_status(Bool stopRun, MPI_Comm comm) {
    int fail = (stopRun) ? 1 : 0;
    int failProgram = 0;

    MPI_Allreduce(&fail, &failProgram, 1, MPI_INT, MPI_SUM, comm);

    return (Bool) (failProgram > 0);
}

/**
@brief Find the active sites within the provided domain so we do not
try to simulate/assign to compute processes

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[out] numActiveSites Number of sites that was found in
    the progress input file that is enabled
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_root_find_active_sites(
    SW_DOMAIN *SW_Domain,
    unsigned long ***activeSuids,
    int *numActiveSites,
    LOG_INFO *LogInfo
) {
    signed char *prog = NULL;
    int progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprog];
    Bool sDom = (Bool) (strcmp(SW_Domain->DomainType, "s") == 0);
    size_t numSites =
        (sDom) ? SW_Domain->nDimS : SW_Domain->nDimY * SW_Domain->nDimX;
    unsigned long progIndex;
    int progFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCprog];

    *numActiveSites = 0;

    prog = (signed char *) Mem_Malloc(
        sizeof(signed char) * numSites,
        "SW_MPI_root_find_active_sites()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    /* Read all progress values */
    if (nc_get_var_schar(progFileID, progVarID, prog) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not read all of the progress variable values."
        );
        goto freeMem;
    }

    /* Go through the entirety of the progress values and keep track of
       how many are ready to be run */
    for (progIndex = 0; progIndex < numSites; progIndex++) {
        *numActiveSites += (prog[progIndex] == PRGRSS_READY) ? 1 : 0;
    }

    allocateActiveSuids(*numActiveSites, activeSuids, LogInfo);
    if (LogInfo->stopRun) {
        goto freeMem;
    }

    /* Go through the progress values again and calculate/store
       the active domain SUIDs */
    for (progIndex = 0; progIndex < numSites; progIndex++) {
        if (prog[progIndex] == PRGRSS_READY) {
            SW_DOM_calc_ncSuid(SW_Domain, progIndex, (*activeSuids)[progIndex]);
        }
    }

freeMem:
    free((void *) prog);
}

/**
@brief Get translated SUIDs and store them for workload distribution

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in] numActiveSites Number of active sites that will be simulated
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_get_activated_tsuids(
    SW_DOMAIN *SW_Domain,
    unsigned long **activeSuids,
    unsigned long ****activeTSuids,
    unsigned long numActiveSites,
    LOG_INFO *LogInfo
) {
    Bool inSDom;
    size_t nSites;
    unsigned int *sxIndexVals = NULL;
    unsigned int *yIndexVals = NULL;
    char ****varInfo = SW_Domain->netCDFInput.inVarInfo;
    Bool **readInVars = SW_Domain->netCDFInput.readInVars;
    Bool *useIndexFile = SW_Domain->netCDFInput.useIndexFile;
    char ***ncInFiles = SW_Domain->SW_PathInputs.ncInFiles;
    size_t numPosKeys = eSW_LastInKey;
    int index;
    int site;
    InKeys inKey;
    int varNum;
    int fileID;
    char *ncFileName;
    int varID;
    unsigned long *indexCell;
    unsigned long *domSuid;

    (*activeTSuids) = (unsigned long ***) Mem_Malloc(
        sizeof(unsigned long **) * numPosKeys,
        "SW_MPI_get_activated_tsuids()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (index = 0; index < (int) numPosKeys; index++) {
        (*activeTSuids)[index] = NULL;
    }

    ForEachNCInKey(inKey) {
        if (inKey == eSW_InDomain || !readInVars[inKey][0] ||
            !useIndexFile[inKey]) {
            continue;
        }

        varNum = 0;
        while (!readInVars[varNum + 1]) {
            varNum++;
        }

        ncFileName = ncInFiles[inKey][0];
        SW_NC_open(ncFileName, NC_NOWRITE, &fileID, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }

        inSDom = (Bool) (strcmp(varInfo[inKey][varNum][INDOMTYPE], "s") == 0);
        nSites =
            (inSDom) ? SW_Domain->nDimS : SW_Domain->nDimX * SW_Domain->nDimY;
        sxIndexVals = (unsigned int *) Mem_Malloc(
            sizeof(unsigned int) * nSites,
            "SW_MPI_get_activated_tsuids()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
        if (!inSDom) {
            yIndexVals = (unsigned int *) Mem_Malloc(
                sizeof(unsigned int) * nSites,
                "SW_MPI_get_activated_tsuids()",
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto freeMem;
            }
        }

        varID = -1;
        SW_NC_get_vals(
            fileID,
            &varID,
            (inSDom) ? "s_index" : "x_index",
            sxIndexVals,
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        if (!inSDom) {
            varID = -1;
            SW_NC_get_vals(fileID, &varID, "y_index", yIndexVals, LogInfo);
            if (LogInfo->stopRun) {
                goto freeMem;
            }
        }

        allocateActiveTSuids(numActiveSites, &(*activeTSuids)[inKey], LogInfo);
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        for (site = 0; site < (int) numActiveSites; site++) {
            domSuid = activeSuids[site];
            indexCell = (*activeTSuids)[inKey][site];

            /*
                Translate a domain suid for a site into a translated suid
                since we are using an index file
                e.g., [0, 3] -> [1, 0]
             */
            if (inSDom) {
                indexCell[0] = sxIndexVals[domSuid[0]];
            } else {
                indexCell[0] = yIndexVals[domSuid[0]];
                indexCell[1] = sxIndexVals[domSuid[1]];
            }
        }
    }

freeMem:
    if (!isnull(sxIndexVals)) {
        free(sxIndexVals);
    }

    if (!isnull(yIndexVals)) {
        free(yIndexVals);
    }
}

/**
@brief Assign processes to compute or I/O jobs

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] procName Name of the processor the process is being run on
    (agnostic of if on HPC or local computer)
@param[in] worldSize Number of processes that was created that make up
    the world
@param[in] rank Process number known to MPI for the current process (aka rank)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_process_types(
    SW_DOMAIN *SW_Domain,
    char *procName,
    int worldSize,
    int rank,
    LOG_INFO *LogInfo
) {
    unsigned long ***activeTSuids = NULL;
    unsigned long **activeSuids = NULL;
    int numActiveSites = 0;
    MPI_Request nullReq = MPI_REQUEST_NULL;

    int startOldSize = 0;
    int startNewSize = 3;
    int numNodes = 0;
    int node;
    char **nodeNames = NULL;
    int *numProcsInNode = NULL;
    int *numMaxProcsInNode = NULL;
    int **ranksInNodes = NULL;
    int numIOProcsTot = 0;
    int suidAssign = 0;
    int leftSuids = 0;
    int pair;
    Bool useTranslated = swFALSE;

    SW_MPI_DESIGNATE **designations = NULL;
    MPI_Comm *rootCompComm =
        (rank == SW_MPI_ROOT) ? &SW_Domain->SW_Designation.rootCompComm : NULL;
    MPI_Datatype desType = SW_Domain->datatypes[eSW_MPI_Designate];

    // Spread the index file creation flags across the world;
    // necessary if we use translated SUIDs and we have not
    // received them yet
    SW_Bcast(
        MPI_INT,
        SW_Domain->netCDFInput.useIndexFile,
        SW_NINKEYSNC,
        SW_MPI_ROOT,
        MPI_COMM_WORLD
    );

    // Check if the process is not the root
    if (rank != SW_MPI_ROOT) {
        // Send processor name to the root
        SW_MPI_Send(
            MPI_CHAR,
            procName,
            MAX_FILENAMESIZE,
            SW_MPI_ROOT,
            swTRUE,
            0,
            &nullReq
        );

        // Get processor assignment
        assignProcs(
            desType,
            NULL,
            rank,
            worldSize,
            0,
            SW_Domain->netCDFInput.useIndexFile,
            NULL,
            NULL,
            swTRUE,
            &SW_Domain->SW_Designation,
            LogInfo
        );
    }
    // Otherwise, we are root
    else {
        for (pair = 0; pair < SW_NINKEYSNC && !useTranslated; pair++) {
            useTranslated = SW_Domain->netCDFInput.useIndexFile[pair];
        }

        SW_MPI_root_find_active_sites(
            SW_Domain, &activeSuids, &numActiveSites, LogInfo
        );
        if (LogInfo->stopRun) {
            exitDesFunc(
                desType,
                1,
                SW_MPI_ROOT,
                worldSize,
                FAIL_ROOT_SETUP,
                0,
                NULL,
                swFALSE
            );
            goto freeMem;
        }

        SW_MPI_get_activated_tsuids(
            SW_Domain, activeSuids, &activeTSuids, numActiveSites, LogInfo
        );
        if (LogInfo->stopRun) {
            exitDesFunc(
                desType,
                1,
                SW_MPI_ROOT,
                worldSize,
                FAIL_ROOT_SETUP,
                0,
                NULL,
                swFALSE
            );
            goto freeMem;
        }

        allocProcInfo(
            startOldSize,
            startNewSize,
            &nodeNames,
            &numProcsInNode,
            &ranksInNodes,
            &numMaxProcsInNode,
            LogInfo
        );
        if (LogInfo->stopRun) {
            exitDesFunc(
                desType,
                1,
                SW_MPI_ROOT,
                worldSize,
                FAIL_ROOT_SETUP,
                0,
                NULL,
                swFALSE
            );
            goto freeMem;
        }

        getProcInfo(
            desType,
            worldSize,
            procName,
            &numNodes,
            &nodeNames,
            &numProcsInNode,
            &numMaxProcsInNode,
            &ranksInNodes,
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        // Check if any nodes have a count of 1
        for (node = 0; node < numNodes; node++) {
            if (numProcsInNode[node] == 1) {
                // Fail as we need more than one process in a node
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Must have at least 2 processes within a node "
                    " (%s has 1).",
                    nodeNames[node]
                );
                exitDesFunc(
                    desType,
                    1,
                    SW_MPI_ROOT,
                    worldSize,
                    FAIL_ROOT_SETUP,
                    0,
                    NULL,
                    swFALSE
                );
                goto freeMem;
            }

            numIOProcsTot += calcNumIOProcs(numProcsInNode[node]);
        }

        if (numActiveSites < worldSize - numIOProcsTot) {
            LogError(
                LogInfo,
                LOGERROR,
                "Less active sites to simulate than compute processes. "
                "Attempted to create %d compute processes with a maximum "
                "of %d I/O processes per node, %d in total.",
                worldSize - numIOProcsTot,
                SW_MPI_NIO,
                numIOProcsTot
            );
            exitDesFunc(
                desType,
                1,
                SW_MPI_ROOT,
                worldSize,
                FAIL_ROOT_SETUP,
                0,
                NULL,
                swFALSE
            );
            goto freeMem;
        }

        leftSuids = numActiveSites;
        designateProcesses(
            &designations,
            &SW_Domain->SW_Designation,
            numProcsInNode,
            numNodes,
            worldSize - numIOProcsTot,
            numActiveSites,
            activeSuids,
            activeTSuids,
            ranksInNodes,
            &leftSuids,
            &suidAssign,
            LogInfo
        );
        if (LogInfo->stopRun) {
            exitDesFunc(
                desType,
                1,
                SW_MPI_ROOT,
                worldSize,
                FAIL_ROOT_SEND,
                0,
                NULL,
                swFALSE
            );
            goto freeMem;
        }

        // Send designation to processes
        SW_Domain->SW_Designation.useTSuids = useTranslated;
        assignProcs(
            desType,
            designations,
            rank,
            worldSize,
            numNodes,
            SW_Domain->netCDFInput.useIndexFile,
            numProcsInNode,
            ranksInNodes,
            useTranslated,
            &SW_Domain->SW_Designation,
            LogInfo
        );
    }

    create_groups(
        rank,
        worldSize,
        SW_Domain->SW_Designation.procJob,
        numIOProcsTot,
        designations,
        ranksInNodes,
        numNodes,
        numProcsInNode,
        &SW_Domain->SW_Designation.groupComm,
        rootCompComm,
        LogInfo
    );

freeMem:
    deallocProcHelpers(
        SW_Domain->SW_Designation.nSuids,
        &designations,
        &activeSuids,
        &activeTSuids,
        &nodeNames,
        &numProcsInNode,
        &numMaxProcsInNode,
        &ranksInNodes
    );
}
