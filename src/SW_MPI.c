#include "include/SW_MPI.h"
#include "include/SW_netCDF_Input.h"

#include "include/filefuncs.h"
#include "include/generic.h"
#include "include/myMemory.h"
#include "include/SW_Domain.h"
#include "include/SW_Files.h"
#include "include/SW_Main_lib.h"
#include "include/SW_Markov.h"
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"
#include "include/SW_Output.h"          // for ForEachOutKey, SW_ESTAB, pd2...
#include "include/SW_Output_outarray.h" // for SW_OUT_construct_outarray
#include "include/SW_Weather.h"         // for SW_WTH_allocateAllWeather
#include "include/Times.h"
#include <netcdf.h>
#include <netcdf_par.h>
#include <signal.h> // for signal
#include <stdlib.h>
#include <string.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile sig_atomic_t runSims = 1;

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define FAIL_ALLOC_DESIG 0
#define FAIL_ROOT_SETUP 1
#define FAIL_ROOT_SEND 2
#define FAIL_ALLOC_SUIDS 3
#define FAIL_ALLOC_TSUIDS 4

#define REQ_OUT_LOG 0
#define REQ_OUT_NC 1
#define REQ_OUT_BOTH 2

#define COMP_COMPLETE 0

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Handle an interrupt provided by the OS to stop the program;
the current supported interrupts are terminations (SIGTERM) and
interrupts (SIGINT, commonly CTRL+C on the keyboard)

@param[in] signal Numerical value of the signal that was recieved
(currently not used)
*/
static void handle_interrupt(int signal) {
    (void) signal; /* Silence compiler */
    runSims = 0;
}

/**
@brief Handle OpenMPI error if one occurs

@param[in] mpiError Result value from an MPI function call that
    returned an error

@sideeffect Exit all instances of the MPI program
*/
void errorMPI(int mpiError) {
    char errorStr[MAX_FILENAMESIZE] = {'\0'};
    int errorLen = 0;

    MPI_Error_string(mpiError, errorStr, &errorLen);

    SW_MPI_Fail(SW_MPI_FAIL_MPI, mpiError, errorStr);
}

/**
@brief Deallocate helper memory that was allocated during the call to
    `SW_MPI_process_types()`

@param[in] numActiveSites Number of active sites that will be simulated
@param[in] maxNodes Maximum number of nodes that were allocated
@param[in] numDes Number of designations that were allocated
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
    int maxNodes,
    int numDes,
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
    int numVals;

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
        if (!isnull((void *) (*oneDimFree[var]))) {
            free((*oneDimFree[var]));
            (*oneDimFree[var]) = NULL;
        }
    }

    for (var = 0; var < num2D; var++) {
        numVals = (var >= 1 && var <= 2) ? maxNodes : numActiveSites;
        numVals = (var == 3) ? numDes : numVals;
        if (!isnull(*twoDimFree[var])) {
            for (pair = 0; pair < numVals; pair++) {
                if (!isnull((*twoDimFree[var])[pair])) {
                    free((void *) (*twoDimFree[var])[pair]);
                    (*twoDimFree[var])[pair] = NULL;
                }
            }
            free((void *) (*twoDimFree[var]));
            (*twoDimFree[var]) = NULL;
        }
    }

    if (!isnull(*activeTSuids)) {
        free((void *) *activeTSuids);
        *activeTSuids = NULL;
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

@param[in] numCompProcsNode Number of compute processes in a node
@param[in] ioProcsInNode Number of I/O processes within a compute node
@param[in] numIOProcsTot Total number of I/O processes in the MPI world
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
    int numCompProcsNode,
    int ioProcsInNode,
    int numIOProcsTot,
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
    int rank;

    desig->nCompProcs = numCompProcsNode / ioProcsInNode;

    if (*leftOverCompProcs > 0) {
        desig->nCompProcs++;
        (*leftOverCompProcs)--;
    }

    // Allocate and assign floor(# active sites / num
    // I/O procs) + (one left over SUID, if necessary)
    // to the node or the rest of the SUIDs if this assignment
    // will complete the I/O assignments
    desig->nSuids = numActiveSites / numIOProcsTot;

    if (desig->nSuids > *leftSuids) {
        desig->nSuids = *leftSuids;
    } else if (*leftOverSuids > 0) {
        desig->nSuids++;
        (*leftOverSuids)--;
    }
    *leftSuids -= desig->nSuids;
    desig->domSuids = (unsigned long **) Mem_Malloc(
        sizeof(unsigned long *) * desig->nSuids, "fillDesignationIO()", LogInfo
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
@param[in] numIOProcsTot Total number of I/O processes in the MPI world
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
    int numIOProcsTot,
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
                    numNodeProcs - ioProcsInNode,
                    ioProcsInNode,
                    numIOProcsTot,
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

@param[in] worldSize Total number of processes that the MPI run has created
@param[in] rootProcName Root process' processor/compute node name
@param[out] numNodes The number of unique nodes the program is running on
@param[out] maxNumNodes Maximum number of nodes that were in use
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
    int worldSize,
    char rootProcName[],
    int *numNodes,
    int *maxNumNodes,
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
                    return;
                }

                *maxNumNodes = newSize;
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

    // Transfer and allocate information to I/O processes before
    // starting the loop to simplify error handling across processes
    // when allocating memory
    if (SW_Designation->procJob == SW_MPI_PROC_IO) {
        if (rank > SW_MPI_ROOT) {
            allocateActiveSuids(
                SW_Designation->nSuids, &SW_Designation->domSuids, LogInfo
            );
            if (LogInfo->stopRun) {
                goto reportError;
            }
        }

        if (SW_Designation->useTSuids && rank > SW_MPI_ROOT) {
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
                    goto reportError;
                }
            }
        }
    }

reportError:
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

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
    if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
        if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
                if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
            if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
@brief Helper function for `create_groups` to create communicators
    that contains an I/O process and the I/O process' respectively assigned
    compute process ranks

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] rankJob Assigned job of a specific rank/process (compute or I/O)
@param[in,out] desig Designation instance that holds information about
    assigning a process to a job
@param[out] LogInfo Holds information on warnings and errors
*/
static void create_iocomp_comms(
    int rank, int rankJob, SW_MPI_DESIGNATE *desig, LOG_INFO *LogInfo
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;
    int *ranksInIOCompComm = NULL;

    int rankIndex;
    int sendRank;
    int *ranksPerIO = desig->ranks;

    // Include IO process itself for this function only - it will be
    // new rank 0 in the new communicator for I/O to child compute
    // processes
    int numRanksForIO = desig->nCompProcs + 1;

    if (rankJob == SW_MPI_PROC_IO) {
        ranksInIOCompComm = (int *) Mem_Malloc(
            sizeof(int) * numRanksForIO, "create_iocomp_comms()", LogInfo
        );
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            goto freeMem;
        }

        ranksInIOCompComm[0] = rank;

        for (rankIndex = 1; rankIndex < numRanksForIO; rankIndex++) {
            sendRank = ranksPerIO[rankIndex - 1];
            SW_MPI_Send(
                MPI_INT, &numRanksForIO, 1, sendRank, swTRUE, 0, &nullReq
            );

            // Store this send rank in the list to send
            ranksInIOCompComm[rankIndex] = sendRank;
        }
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            goto freeMem;
        }

        for (rankIndex = 1; rankIndex < numRanksForIO; rankIndex++) {
            sendRank = ranksPerIO[rankIndex - 1];

            SW_MPI_Send(
                MPI_INT,
                ranksInIOCompComm,
                numRanksForIO,
                sendRank,
                swTRUE,
                0,
                &nullReq
            );
        }
    } else {
        // Check that the I/O processes have been able to allocate
        // their group/communicator rank list
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            goto freeMem;
        }

        SW_MPI_Recv(
            MPI_INT, &numRanksForIO, 1, desig->ioRank, swTRUE, 0, &nullReq
        );

        ranksInIOCompComm = (int *) Mem_Malloc(
            sizeof(int) * numRanksForIO, "create_iocomp_comms()", LogInfo
        );
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            goto freeMem;
        }

        SW_MPI_Recv(
            MPI_INT,
            ranksInIOCompComm,
            numRanksForIO,
            desig->ioRank,
            swTRUE,
            0,
            &nullReq
        );
    }

    mpi_create_group_comms(
        numRanksForIO, ranksInIOCompComm, &desig->ioCompComm
    );

freeMem:
    if (!isnull(ranksInIOCompComm)) {
        free(ranksInIOCompComm);
    }
}

/**
@brief Create custom communicators between compute and I/O processes so
    they can act independently of one another if necessary

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Total number of processes that the MPI run has created
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
@param[in,out] desig Designation instance that holds information about
    assigning a process to a job
@param[out] LogInfo Holds information on warnings and errors
*/
static void create_groups(
    int rank,
    int worldSize,
    int numIOProcsTot,
    SW_MPI_DESIGNATE **designations,
    int **ranksInNodes,
    int numNodes,
    int *numProcsInNode,
    SW_MPI_DESIGNATE *desig,
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
    int rankJob = desig->procJob;

    MPI_Comm *groupComm = &desig->groupComm;
    MPI_Comm *rootCompComm =
        (rank == SW_MPI_ROOT) ? &desig->rootCompComm : NULL;

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
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        goto freeMem;
    }

    if (rank == SW_MPI_ROOT || rankJob == SW_MPI_PROC_IO) {
        ranksInIO = (int *) Mem_Malloc(
            sizeof(int) * numIOProcsTot, "create_groups()", LogInfo
        );
    }
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        goto freeMem;
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

    if (rankJob == SW_MPI_PROC_COMP || rank == SW_MPI_ROOT) {
        mpi_create_group_comms(
            (rank > SW_MPI_ROOT) ? numElem : numCompProcs,
            (rank > SW_MPI_ROOT) ? buff : ranksInComp,
            (rank > SW_MPI_ROOT) ? groupComm : rootCompComm
        );
    }

    // Create io-comp communicators
    create_iocomp_comms(rank, rankJob, desig, LogInfo);

freeMem:
    // Free allocated memory
    if (!isnull(ranksInComp)) {
        free(ranksInComp);
    }

    if (!isnull(ranksInIO)) {
        free(ranksInIO);
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
    SW_Bcast(
        MPI_UNSIGNED, &pathInputs->ncNumWeatherInFiles, 1, SW_MPI_ROOT, comm
    );
    SW_Bcast(
        MPI_UNSIGNED, &pathInputs->weathStartFileIndex, 1, SW_MPI_ROOT, comm
    );

    if (rank > SW_MPI_ROOT) {
        pathInputs->numDaysInYear = (unsigned int *) Mem_Malloc(
            sizeof(unsigned int) * nYears, "get_path_info()", LogInfo
        );
    }
    if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
    if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
        if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
            if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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
@brief Helper function to SW_MPI_open_files to open input files in for
    parallel I/O

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] comm MPI communicator to broadcast a message to
@param[in] netCDFIn Constant netCDF input file information
@param[in,out] pathInputs Inputs Struct of type SW_PATH_INPUTS which
    holds basic information about input files and values
@param[out] LogInfo Holds information on warnings and errors
*/
static void open_input_files(
    int rank,
    MPI_Comm comm,
    SW_NETCDF_IN *netCDFIn,
    SW_PATH_INPUTS *pathInputs,
    LOG_INFO *LogInfo
) {
    InKeys inKey;
    int var;
    int domVar;
    unsigned int numFiles;
    unsigned int file;
    int *id;
    char *fileName;
    Bool skipVar;
    Bool stop = swFALSE;
    Bool useWeathFileArray;

    ForEachNCInKey(inKey) {
        if (!netCDFIn->readInVars[inKey][0] || inKey == eSW_InDomain) {
            if (inKey == eSW_InDomain) {
                // Reopen domain and progress file
                for (domVar = 0; domVar < SW_NVARDOM; domVar++) {
                    if (rank == SW_MPI_ROOT) {
                        fileName = pathInputs->ncInFiles[eSW_InDomain][domVar];
                    }
                    get_dynamic_string(rank, &fileName, swTRUE, comm, LogInfo);
                    if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                        return;
                    }

                    SW_NC_open_par(
                        fileName,
                        (domVar == vNCdom) ? NC_NOWRITE : NC_WRITE,
                        comm,
                        &pathInputs->ncDomFileIDs[domVar],
                        LogInfo
                    );
                    if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                        return;
                    }

                    if (rank > SW_MPI_ROOT) {
                        free(fileName);
                    }
                }
            }
            continue;
        }

        pathInputs->openInFileIDs[inKey] = (int **) Mem_Malloc(
            sizeof(int *) * numVarsInKey[inKey], "SW_MPI_open_files()", LogInfo
        );
        if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
            return;
        }

        for (var = 0; var < numVarsInKey[inKey]; var++) {
            pathInputs->openInFileIDs[inKey][var] = NULL;
        }

        for (var = 0; var < numVarsInKey[inKey]; var++) {
            skipVar = (Bool) (!netCDFIn->readInVars[inKey][var + 1] ||
                              ((var == 0 && !netCDFIn->useIndexFile[inKey])));
            if (skipVar) {
                continue;
            }

            numFiles =
                (inKey != eSW_InWeather) ? 1 : pathInputs->ncNumWeatherInFiles;

            pathInputs->openInFileIDs[inKey][var] = (int *) Mem_Malloc(
                sizeof(int) * numFiles, "SW_MPI_open_files()", LogInfo
            );
            if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                return;
            }

            for (file = 0; file < numFiles; file++) {
                if (inKey == eSW_InWeather &&
                    file < pathInputs->weathStartFileIndex) {

                    // Do not open prior year weather file(s)
                    continue;
                }
                useWeathFileArray = (Bool) (inKey == eSW_InWeather && var > 0);

                if (rank == SW_MPI_ROOT) {
                    fileName = (!useWeathFileArray) ?
                                   pathInputs->ncInFiles[inKey][var] :
                                   pathInputs->ncWeatherInFiles[var][file];
                }
                get_dynamic_string(rank, &fileName, swTRUE, comm, LogInfo);
                if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                    stop = swTRUE;
                    goto freeFile;
                }

                id = &pathInputs->openInFileIDs[inKey][var][file];
                SW_NC_open_par(fileName, NC_NOWRITE, comm, id, LogInfo);
                if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                    stop = swTRUE;
                    goto freeFile;
                }

            freeFile:
                if (rank > SW_MPI_ROOT) {
                    free(fileName);
                    fileName = NULL;
                }
                if (stop) {
                    return;
                }
            }
        }
    }
}

/**
@brief Generate log file names and open them to be written to in the future;
    the generated log files will have the name of the format:
        <rank>_<job>_<logfile name> where "logfile name" contains the extension
        e.g., 0_IO_logfile.log or 20_COMP_logfile.log

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] logfileName Base logfile name that the user provided in `files.in`
@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] LogInfo Holds information on warnings and errors
*/
static void open_logfiles(
    int rank, char *logfileName, SW_MPI_DESIGNATE *desig, LOG_INFO *LogInfo
) {
    int numLogs = desig->nCompProcs + 1; // + 1 for current I/O process
    int log;
    int resSNP = 0;
    char logBuffer[MAX_FILENAMESIZE] = "\0";
    char dir[MAX_FILENAMESIZE] = "\0";
    const char *baseName = BaseName(logfileName);

    LogInfo->logfps = (FILE **) Mem_Malloc(
        sizeof(FILE *) * numLogs, "open_logfiles()", LogInfo
    );
    LogInfo->numFiles = numLogs;

    DirName(logfileName, dir);

    for (log = 0; log < numLogs; log++) {
        snprintf(
            logBuffer,
            MAX_FILENAMESIZE,
            "%s%d_%s_%s",
            dir,
            (log > 0) ? desig->ranks[log - 1] : rank,
            (log > 0) ? "COMP" : "IO",
            baseName
        );
        if (resSNP < 0 || (unsigned int) resSNP > sizeof(logBuffer)) {
            LogError(
                LogInfo,
                LOGERROR,
                "Could not create log file names in rank %d.",
                rank
            );
            return;
        }

        LogInfo->logfps[log] = OpenFile(logBuffer, "w", LogInfo);
        if (LogInfo->stopRun) {
            return;
        }
    }
}

/**
@brief Helper function to SW_MPI_open_files to open input files;
    including log files

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] logFileName Base logfile name that the user provided in `files.in`
@param[in] comm MPI communicator to broadcast a message to
@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] pathOutputs Constant netCDF output file information
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
static void open_output_files(
    int rank,
    char *logFileName,
    MPI_Comm comm,
    SW_MPI_DESIGNATE *desig,
    SW_PATH_OUTPUTS *pathOutputs,
    SW_OUT_DOM *OutDom,
    LOG_INFO *LogInfo
) {
    OutKey outKey;
    OutPeriod pd;
    unsigned int file;
    int *id;
    char *fileName = NULL;

    SW_Bcast(MPI_UNSIGNED, &pathOutputs->numOutFiles, 1, SW_MPI_ROOT, comm);

    ForEachOutKey(outKey) {
        if (OutDom->nvar_OUT[outKey] > 0 && OutDom->use[outKey]) {
            ForEachOutPeriod(pd) {
                if (OutDom->use_OutPeriod[pd]) {
                    pathOutputs->openOutFileIDs[outKey][pd] =
                        (int *) Mem_Malloc(
                            sizeof(int) * pathOutputs->numOutFiles,
                            "open_output_files()",
                            LogInfo
                        );
                    if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                        return;
                    }

                    for (file = 0; file < pathOutputs->numOutFiles; file++) {
                        if (rank == SW_MPI_ROOT) {
                            fileName =
                                pathOutputs->ncOutFiles[outKey][pd][file];
                        }
                        get_dynamic_string(
                            rank, &fileName, swTRUE, comm, LogInfo
                        );
                        if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                            if (rank > SW_MPI_ROOT) {
                                free(fileName);
                            }
                            return;
                        }

                        id = &pathOutputs->openOutFileIDs[outKey][pd][file];

                        SW_NC_open_par(fileName, NC_WRITE, comm, id, LogInfo);
                        if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                            if (rank > SW_MPI_ROOT) {
                                free(fileName);
                            }
                            return;
                        }
                        if (rank > SW_MPI_ROOT) {
                            free(fileName);
                        }
                    }
                }
            }
        }
    }

    open_logfiles(rank, logFileName, desig, LogInfo);
}

/**
@brief Wrapper for MPI function to free a type and throw a
    warning if a free throws an error

@param[in,out] type Custom MPI type to attempt to free
@param[out] LogInfo Holds information on warnings and errors
*/
static void free_type(MPI_Datatype *type, LOG_INFO *LogInfo) {
    int res = MPI_SUCCESS;

    res = MPI_Type_free(type);

    if (res != MPI_SUCCESS) {
        LogError(LogInfo, LOGWARN, "Could not free a custom MPI type.");
    }
}

/**
@brief Allocate enough input structs to distribute across compute processes

@param[in] defInputs Default inputs that will be use if input flags
    are not turned on
@param[in] numInputs Number of inputs to allocate for
@param[in] readWeather A flag specifying if weather will be read in;
    if so, we will allocate weather
@param[in] n_years Number of years in simulation
@param[in] allocSoils A flag specifying if temporary soil storage should be
    allocated
@param[out] elevations A list of elevations to hold more than one
    site's worth of values
@param[out] tempMonthlyVals A list of monthly values to hold more than one
    site's worth of values
@param[out] tempSilt A list of temporary silt values, holding more than one
    site's worth of values
@param[out] tempSoilVals A list of temporary soil values, holding more than
    one site's worth of values
@param[out] tempWeather A list of temporary weather values, holding more than
    one site's worth of values
@param[out] tempSoils A list of soil values, holding more than one site's worth
    of values
@param[out] inputs A list of SW_RUN_INPUTS that will be distributed sent
    to compute processes
@param[out] LogInfo Holds information on warnings and errors
*/
static void alloc_inputs(
    SW_RUN_INPUTS *defInputs,
    int numInputs,
    Bool readWeather,
    int n_years,
    Bool allocSoils,
    double **elevations,
    double **tempMonthlyVals,
    double **tempSilt,
    double **tempSoilVals,
    double **tempWeather,
    SW_SOIL_RUN_INPUTS **tempSoils,
    SW_RUN_INPUTS **inputs,
    LOG_INFO *LogInfo
) {
    int input;

    *inputs = (SW_RUN_INPUTS *) Mem_Malloc(
        sizeof(SW_RUN_INPUTS) * numInputs, "alloc_inputs()", LogInfo
    );

    memset(*inputs, 0, sizeof(SW_RUN_INPUTS) * numInputs);

    for (input = 0; input < numInputs; input++) {
        memcpy(&((*inputs)[input]), defInputs, sizeof(SW_RUN_INPUTS));
    }

    for (input = 0; input < numInputs; input++) {
        (*inputs)[input].weathRunAllHist = NULL;

        if (readWeather) {
            SW_WTH_allocateAllWeather(
                &(*inputs)[input].weathRunAllHist, n_years, LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
    }

    *elevations = (double *) Mem_Malloc(
        sizeof(double) * numInputs, "alloc_inputs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    *tempMonthlyVals = (double *) Mem_Malloc(
        sizeof(double) * numInputs * MAX_MONTHS, "alloc_inputs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    *tempSilt = (double *) Mem_Malloc(
        sizeof(double) * numInputs * MAX_LAYERS, "alloc_inputs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    *tempSoilVals = (double *) Mem_Malloc(
        sizeof(double) * numInputs * MAX_LAYERS * SWRC_PARAM_NMAX,
        "alloc_inputs()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    *tempWeather = (double *) Mem_Malloc(
        sizeof(double) * numInputs * MAX_DAYS, "alloc_inputs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    if (allocSoils) {
        *tempSoils = (SW_SOIL_RUN_INPUTS *) Mem_Malloc(
            sizeof(SW_SOIL_RUN_INPUTS) * numInputs, "alloc_inputs()", LogInfo
        );
    }
}

/**
@brief Allocate space for start and count indices to determine the
    continuous writing/reading

@param[in] numSuids Number of SUIDs that will be assigned, this should be
    the product of the <number of compute processors for the I/O process>
    and N_SUID_ASSIGN
@param[in] nCompProcs Number of compute processes that are assigned to
    the I/O process
@param[in] useIndexFile Specifies to create/use an index file
@param[in] readInVars Specifies which variables are to be read-in as input
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] OutRun Struct of type SW_OUT_RUN that holds output
    information that may change throughout simulation runs
@param[out] starts A list of start pairs used for accessing/writing to
    netCDF files
@param[out] counts A list of count parts used for accessing/writing to
    netCDF files
@param[out] distSUIDs A list of SUIDs that's a subset of an I/O processor's
    assigned SUIDs
@param[out] distTSuids A list of translated (index file) SUIDs that's a subset
    of an I/O processor's assigned SUIDs
@param[out] tempDistSuids A list of copied SUIDs that's a subset of an I/O
    processor's assigned SUIDs
@param[out] sendInputs A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[out] succFlags A list of success flags that are accumulated
    through N_ITER_BEFORE_OUT iterations of output gathering specifying
    if the simulation for a suid was successfully run
@param[out] logs A list of LOG_INFO instances that will be used for
    getting log information from compute processes
@param[out] LogInfo Holds information on warnings and errors
*/
static void alloc_IO_info(
    int numSuids,
    int nCompProcs,
    Bool useIndexFile[],
    Bool **readInVars,
    SW_OUT_DOM *OutDom,
    SW_OUT_RUN *OutRun,
    size_t **starts[],
    size_t **counts[],
    size_t ***distSUIDs,
    size_t **distTSuids[],
    size_t ***tempDistSuids,
    int **sendInputs,
    Bool **succFlags,
    LOG_INFO **logs,
    LOG_INFO *LogInfo
) {
    int suid;
    int allocIndex;
    const int num1DArr = 1;
    InKeys inKey;
    int numVals;

    int **alloc1DArr[] = {sendInputs};

    ForEachNCInKey(inKey) {
        numVals =
            (inKey == eSW_InDomain) ? numSuids * N_ITER_BEFORE_OUT : numSuids;

        starts[inKey] = (size_t **) Mem_Malloc(
            sizeof(size_t *) * numVals, "alloc_IO_info()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
        for (allocIndex = 0; allocIndex < numVals; allocIndex++) {
            starts[inKey][allocIndex] = NULL;
        }

        counts[inKey] = (size_t **) Mem_Malloc(
            sizeof(size_t *) * numVals, "alloc_IO_info()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
        for (allocIndex = 0; allocIndex < numVals; allocIndex++) {
            starts[inKey][allocIndex] = NULL;
        }

        for (allocIndex = 0; allocIndex < numVals; allocIndex++) {
            starts[inKey][allocIndex] = (size_t *) Mem_Malloc(
                sizeof(size_t) * 2, "alloc_IO_info()", LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            counts[inKey][allocIndex] = (size_t *) Mem_Malloc(
                sizeof(size_t) * 2, "alloc_IO_info()", LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            starts[inKey][allocIndex][0] = starts[inKey][allocIndex][1] = 0;
            counts[inKey][allocIndex][0] = counts[inKey][allocIndex][1] = 0;
        }
    }

    for (allocIndex = 0; allocIndex < num1DArr; allocIndex++) {
        (*alloc1DArr[allocIndex]) =
            Mem_Malloc(sizeof(int) * nCompProcs, "alloc_IO_info()", LogInfo);
        if (LogInfo->stopRun) {
            return;
        }
    }

    *distSUIDs = (size_t **) Mem_Malloc(
        sizeof(size_t *) * numSuids * N_ITER_BEFORE_OUT,
        "alloc_IO_info()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (suid = 0; suid < numSuids * N_ITER_BEFORE_OUT; suid++) {
        (*distSUIDs)[suid] = NULL;
    }

    for (suid = 0; suid < numSuids * N_ITER_BEFORE_OUT; suid++) {
        (*distSUIDs)[suid] = (size_t *) Mem_Malloc(
            sizeof(size_t) * 2, "alloc_IO_info()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        (*distSUIDs)[suid][0] = (*distSUIDs)[suid][1] = 0;
    }

    *tempDistSuids = (size_t **) Mem_Malloc(
        sizeof(size_t *) * numSuids, "alloc_IO_info()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (suid = 0; suid < numSuids; suid++) {
        (*tempDistSuids)[suid] = NULL;
    }

    for (suid = 0; suid < numSuids; suid++) {
        (*tempDistSuids)[suid] = (size_t *) Mem_Malloc(
            sizeof(size_t) * 2, "alloc_IO_info()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        (*tempDistSuids)[suid][0] = (*tempDistSuids)[suid][1] = 0;
    }

    *succFlags = (Bool *) Mem_Malloc(
        sizeof(Bool) * numSuids * N_ITER_BEFORE_OUT, "alloc_IO_info()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
    for (suid = 0; suid < numSuids * N_ITER_BEFORE_OUT; suid++) {
        *succFlags[suid] = swFALSE;
    }

    ForEachNCInKey(inKey) {
        if (inKey > eSW_InDomain && useIndexFile[inKey] &&
            readInVars[inKey][0]) {

            distTSuids[inKey] = (size_t **) Mem_Malloc(
                sizeof(size_t *) * numSuids, "alloc_IO_info()", LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            for (suid = 0; suid < numSuids; suid++) {
                distTSuids[inKey][suid] = NULL;
            }

            for (suid = 0; suid < numSuids; suid++) {
                distTSuids[inKey][suid] = (size_t *) Mem_Malloc(
                    sizeof(size_t) * 2, "alloc_IO_info()", LogInfo
                );
                if (LogInfo->stopRun) {
                    return;
                }

                distTSuids[inKey][suid][0] = 0;
                distTSuids[inKey][suid][1] = 1;
            }
        }
    }

    *logs = (LOG_INFO *) Mem_Malloc(
        sizeof(LOG_INFO) * numSuids, "alloc_IO_info()", LogInfo
    );

    SW_OUT_construct_outarray(
        N_SUID_ASSIGN * nCompProcs * N_ITER_BEFORE_OUT, OutDom, OutRun, LogInfo
    );
}

/**
@brief Deallocate information that was allocated for I/O operations

@param[in] numSuids Number of SUIDs that will be assigned, this should be
    the product of the <number of compute processors for the I/O process>
    and N_SUID_ASSIGN
@param[out] OutRun Struct of type SW_OUT_RUN that holds output
    information that may change throughout simulation runs
@param[out] starts A list of start pairs used for accessing/writing to
    netCDF files
@param[out] counts A list of count parts used for accessing/writing to
    netCDF files
@param[out] distSUIDs A list of SUIDs that's a subset of an I/O processor's
    assigned SUIDs
@param[out] sendLens A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[out] tempDistSuids A list of copied SUIDs that's a subset of an I/O
    processor's assigned SUIDs
@param[out] sendInputs A list of values specifying how many inputs each compute
    process will receive
@param[out] elevations A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[out] tempMonthlyVals A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[out] tempSilt A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[out] tempSWRCPVals A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[out] tempSoils A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[out] succFlags A list of success flags that are accumulated
    through N_ITER_BEFORE_OUT iterations of output gathering specifying
    if the simulation for a suid was successfully run
@param[out] logs A list of LOG_INFO instances that will be used for
    getting log information from compute processes
*/
static void dealloc_IO_info(
    int numSuids,
    SW_OUT_RUN *OutRun,
    size_t **starts[],
    size_t **counts[],
    size_t ***distSUIDs,
    size_t **distTSuids[],
    size_t ***tempDistSuids,
    int **sendInputs,
    double **elevations,
    double **tempMonthlyVals,
    double **tempSilt,
    double **tempSWRCPVals,
    SW_SOIL_RUN_INPUTS **tempSoils,
    Bool **succFlags,
    LOG_INFO **logs
) {
    InKeys inKey;
    size_t ***deallocStartCount[2] = {NULL};
    const int numDeallocStartCount = 2;
    int dealloc;
    int suid;
    void **dealloc1D[] = {
        (void **) sendInputs,
        (void **) logs,
        (void **) elevations,
        (void **) tempMonthlyVals,
        (void **) tempSilt,
        (void **) tempSWRCPVals,
        (void **) tempSoils,
        (void **) succFlags
    };
    const int numDealloc1D = 8;

    ForEachNCInKey(inKey) {
        deallocStartCount[0] = &starts[inKey];
        deallocStartCount[1] = &counts[inKey];

        for (dealloc = 0; dealloc < numDeallocStartCount; dealloc++) {
            if (!isnull(*deallocStartCount[dealloc])) {
                for (suid = 0; suid < numSuids; suid++) {
                    if (!isnull((*deallocStartCount[dealloc])[suid])) {
                        free((void *) (*deallocStartCount[dealloc])[suid]);
                        (*deallocStartCount[dealloc])[suid] = NULL;
                    }
                }

                free((void *) *deallocStartCount[dealloc]);
                (*deallocStartCount[dealloc]) = NULL;
            }

            if (!isnull(distTSuids[inKey])) {
                for (suid = 0; suid < numSuids; suid++) {
                    if (!isnull(distTSuids[inKey][suid])) {
                        free((void *) distTSuids[inKey][suid]);
                        distTSuids[inKey][suid] = NULL;
                    }
                }

                free((void *) distTSuids[inKey]);
                distTSuids[inKey] = NULL;
            }
        }
    }

    if (!isnull(*distSUIDs)) {
        for (suid = 0; suid < numSuids; suid++) {
            if (!isnull((*distSUIDs)[suid])) {
                free((void *) (*distSUIDs)[suid]);
                (*distSUIDs)[suid] = NULL;
            }

            free((void *) *distSUIDs);
            *distSUIDs = NULL;
        }
    }

    if (!isnull(*tempDistSuids)) {
        for (suid = 0; suid < numSuids; suid++) {
            if (!isnull((*tempDistSuids)[suid])) {
                free((void *) (*tempDistSuids)[suid]);
                (*tempDistSuids)[suid] = NULL;
            }

            free((void *) *tempDistSuids);
            *tempDistSuids = NULL;
        }
    }

    for (dealloc = 0; dealloc < numDealloc1D; dealloc++) {
        if (!isnull(*(dealloc1D[dealloc]))) {
            free((void *) *(dealloc1D[dealloc]));
            *(dealloc1D[dealloc]) = NULL;
        }
    }

    SW_OUT_deconstruct_outarray(OutRun);
}

/**
@brief Helper to calculate the contiguous reading/writings available
    based on the domain SUIDs

This function must result in a number of writes between
[1, num compute processes]

@param[in] suids A list of domain SUIDs whose data will be distributed
    as the next batch of input; can be domain or translated (index)
    SUIDs
@param[in] numDomSuids Number of domain SUIDs that were given
@param[in] sDom Specifies the program's domain is site-oriented
@param[in] spatialVars1D Specifies if the input key "eSW_InSpatial" inputs
    are 1- or 2-dimensional; this will impact the order of "start" and "count"
    values
@param[in] useSuccFlags A flag specifying if the function should take the
    success flags of simulation runs into account to make contiguous writes
    of the same value (pass or fail)
@param[in] succFlags A list of flags specifying if runs for SUIDs are to be
    used (NULL if not used)
@param[out] numWrites The number of writes that must be performed
    by the calling function to output all simulated information
    for the sites
@param[out] starts A list of calculated start values for when dealing
    with the netCDF library
@param[out] counts A list of calculated count values for when dealing
    with the netCDF library
*/
static void get_contiguous_counts(
    size_t **suids,
    int numDomSuids,
    Bool sDom,
    int nSuidsLeft,
    Bool spatialVars1D,
    Bool useSuccFlags,
    Bool *succFlags,
    int *numWrites,
    size_t **starts,
    size_t **counts
) {
    // Intial state
    // prevX = x[0]
    // prevY = y[0]
    // start = [prevX, 0] or [prevY, prevX]
    // count = [1, 1]
    // number of values
    // Note: currYX and prevYX below are used for either Y (gridded)
    // or X (sites)
    size_t prevYX = suids[0][0];
    size_t prevX = (sDom) ? 0 : suids[0][1];
    int writeIndex = 0;
    int suidIndex;
    int numContVals = 1;
    size_t *suid;
    int xIndex = (sDom || spatialVars1D) ? 0 : 1;
    Bool prevFlag = swTRUE;
    Bool currFlag = swTRUE;

    starts[0][0] = prevYX;
    starts[0][1] = prevX;

    counts[0][0] = (spatialVars1D) ? numDomSuids : 1;
    counts[0][1] = (sDom || spatialVars1D) ? 1 : N_SUID_ASSIGN;

    if (useSuccFlags) {
        prevFlag = succFlags[0];
    }

    // Loop through selected domain SUIDs
    for (suidIndex = 1; suidIndex < numDomSuids && nSuidsLeft > 0;
         suidIndex++) {
        suid = suids[suidIndex];

        if (useSuccFlags) {
            currFlag = succFlags[suidIndex];
        }

        // Check if x is not prevX + 1 or y is not prevYX + 1;
        // This means we found a place that is not contiguous
        // in the domain SUIDs
        if (((sDom && suid[0] != prevYX + 1) ||
             (!sDom && (suid[0] != prevYX || suid[1] != prevX + 1))) ||
            (useSuccFlags && prevFlag != currFlag)) {

            // Set the count for X to number of SUIDs
            counts[writeIndex][xIndex] = numContVals;

            writeIndex++;

            // Prepare next start by using this SUID's values
            // so we can start the contiguous site/gridcell finding
            // from this point
            numContVals = 0;

            prevYX = suids[suidIndex][0];
            prevX = (sDom) ? 0 : suids[suidIndex][1];

            starts[writeIndex][0] = prevYX;
            starts[writeIndex][1] = prevX;
            counts[suidIndex][0] = (spatialVars1D) ? numDomSuids : 1;
            prevFlag = currFlag;

            nSuidsLeft--;
        }

        numContVals++;
    }

    counts[writeIndex][xIndex] = numContVals;

    *numWrites = writeIndex + 1;
}

/**
@brief Spread read inputs across compute processes

The list of inputs contains N_SUID_ASSIGN for each process and will
be distributed in order to the compute processes; so, compute process
1 will get the [0, N_SUID_ASSIGN), then 2 will get [N_SUID_ASSIGN, N_SUID_ASSIGN
* 2), so on and so forth

This function should be called after the main loop of the I/O process so that
any final compute processes can be sent a signal to finish

@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] inputType Custom MPI data type for sending SW_RUN_INPUTS
@param[in] weathHistType Custom MPI type for transfering data for
    SW_WEATHER_HIST
@param[in] inputs A list of input structs that will be distributed across
    compute processes
@param[in] numInputs Total number of inputs that will be spread to
    compute processes
@param[in] estVeg A flag specifying if we vegetation is to be estimated
@param[in,out] sendEstVeg A flag specifying if we have already sent
    the flag to compute processes; return an updated flag value of swFALSE
@param[in] readWeather A flag specifying if weather will be read in;
    if so, send weather
@param[in] sendInputs A list of values specifying how many inputs each compute
    process will receive
@param[in] n_years Number of years in simulation
@param[in] finalize A flag specifying if this will be the last call
*/
static void spread_inputs(
    SW_MPI_DESIGNATE *desig,
    MPI_Datatype inputType,
    MPI_Datatype weathHistType,
    SW_RUN_INPUTS *inputs,
    int numInputs,
    Bool estVeg,
    Bool *sendEstVeg,
    Bool readWeather,
    int sendInputs[],
    int n_years,
    Bool finalize
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;

    Bool extraSetupCheck;
    int comp;
    int numSendIn;
    int destRank;
    int suid = 0;
    int send = 0;
    int sendSum = 0;

    if (*sendEstVeg) {
        SW_Bcast(MPI_INT, &estVeg, 1, SW_GROUP_ROOT, desig->ioCompComm);
        *sendEstVeg = swFALSE;
    }

    for (comp = 0; comp < desig->nCompProcs && sendSum == 0; comp++) {
        sendSum += sendInputs[comp + 1];
    }

    // Send amount information so compute processes know how
    // many inputs to accept then send inputs of that amount to processes
    for (comp = 0; comp < desig->nCompProcs; comp++) {
        if (!finalize || sendInputs[comp + 1] > COMP_COMPLETE) {
            numSendIn = COMP_COMPLETE;
            destRank = desig->ranks[comp];

            if (numInputs > 0) {
                numSendIn =
                    (numInputs > N_SUID_ASSIGN) ? N_SUID_ASSIGN : numInputs;
            }

            sendInputs[comp + 1] = (finalize) ? 0 : numSendIn;

            SW_MPI_Send(
                MPI_INT, &sendInputs[comp + 1], 1, destRank, swTRUE, 0, &nullReq
            );

            if (!finalize || sendInputs[comp + 1] > COMP_COMPLETE) {
                for (send = 0; send < sendInputs[comp + 1]; send++) {
                    SW_MPI_Send(
                        inputType,
                        &inputs[suid + send],
                        1,
                        destRank,
                        swTRUE,
                        0,
                        &nullReq
                    );
                }

                // When a compute process is done with their workload(s) but
                // other compute processes may have more work, so the completed
                // process will have to participate in the setup fail
                extraSetupCheck = (Bool) (sendSum > 0 && sendInputs[comp + 1] ==
                                                             COMP_COMPLETE);

                if (sendInputs[comp + 1] == COMP_COMPLETE) {
                    SW_MPI_Send(
                        MPI_INT,
                        &extraSetupCheck,
                        1,
                        destRank,
                        swTRUE,
                        0,
                        &nullReq
                    );
                }

                if (readWeather) {
                    for (send = 0; send < sendInputs[comp + 1]; send++) {
                        SW_MPI_Send(
                            weathHistType,
                            inputs[suid].weathRunAllHist,
                            n_years,
                            destRank,
                            swTRUE,
                            0,
                            &nullReq
                        );
                    }
                }

                suid += sendInputs[comp + 1];
            }

            numInputs -= numSendIn;
        }
    }
}

/**
@brief Calculate contiguous read/write indices for every input netCDF key
    including a key specifically for program's domain

@note The idea behind doing this is the experimentally determined fact that
    each read/write of data is more worth the time when the action gathers more
    data at once; this means if we do < N reads/writes sequentially, where N is
the number of sites/gridcells, this should save time when reading/writing
    comparatively

@param[in] nSuids Number of SUIDs that will be assigned
@param[in] nSuidsLeft Number of SUIDs that are to be assigned; this
    is only used when an I/O process is on the last assignments of
    SUIDs (i.e., we attempt to assign more SUIDs than we have left)
@param[in] domWrite Base index to read from in `distSUIDs` since
    `distSUIDs` will hold N_ITER_BEFORE_OUT worth of SUIDs
@param[in] useIndexFile Specifies to create/use an index file
@param[in] readInVars Specifies which variables are to be read-in as input
@param[in] distSUIDs A list of domain SUIDs whose data will be distributed
    as the next batch of input
@param[in] distTSUIDs A list of domain translated SUIDs whose data will be
    distributed as the next batch of input
@param[in] sDoms A list of flags that specify the domain type of every
    netCDF input key (if used); specifies all other keys in `start` and
    `count` including the program's domain
@param[in] spatialVars1D Specifies if the input key "eSW_InSpatial" inputs
    are 1- or 2-dimensional; this will impact the order of "start" and "count"
    values
@param[out] numWrites A list of values that specify how many reads/writes a
    specific start-count group would need to cover all SUIDs; this should be
    between [1, nSuids]; the best case being 1 and worst being nSuids
@param[out] starts A list of size SW_NINKEYSNC specifying the start
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array
@param[out] counts A list of size SW_NINKEYSNC specifying the count
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array; counts match placements with `start`
    indices for each key
*/
static void calculate_contiguous_allkeys(
    int nSuids,
    int nSuidsLeft,
    int domWrite,
    Bool useIndexFile[],
    Bool *readInVars[],
    size_t **distSUIDs,
    size_t **distTSUIDs[],
    Bool sDoms[],
    Bool spatialVars1D,
    int numWrites[],
    size_t **starts[],
    size_t **counts[]
) {
    InKeys inKey;
    Bool useIndex;
    Bool useTranslated;
    int suid;
    Bool oneDSpatial;

    for (inKey = 0; inKey < SW_NINKEYSNC; inKey++) {
        useIndex = (inKey != eSW_InDomain) ? useIndexFile[inKey] : swFALSE;
        oneDSpatial = (Bool) (inKey == eSW_InSpatial && spatialVars1D);

        if (inKey == eSW_InDomain || readInVars[inKey][0]) {
            if (inKey == eSW_InDomain || useIndex) {
                useTranslated = (Bool) (inKey != eSW_InDomain && useIndex);

                get_contiguous_counts(
                    (useTranslated) ? distTSUIDs[inKey] : &distSUIDs[domWrite],
                    nSuids,
                    sDoms[inKey],
                    nSuidsLeft,
                    oneDSpatial,
                    swFALSE,
                    NULL,
                    &numWrites[inKey],
                    starts[inKey],
                    counts[inKey]
                );
            } else {
                numWrites[inKey] = numWrites[eSW_InDomain];

                for (suid = 0; suid < numWrites[inKey]; suid++) {
                    starts[inKey][suid][0] =
                        starts[eSW_InDomain][domWrite + suid][0];
                    starts[inKey][suid][1] =
                        starts[eSW_InDomain][domWrite + suid][1];

                    counts[inKey][suid][0] =
                        counts[eSW_InDomain][domWrite + suid][0];
                    counts[inKey][suid][1] =
                        counts[eSW_InDomain][domWrite + suid][1];
                }
            }
        }
    }
}

/**
@brief Write log messages to respective log files for compute processes

@param[in] numRecv Number of log files received
@param[in] logfp A list of pointers to open logfiles respective to
    the I/O process and all compute processes it controls
@param[in] logs A list of LOG_INFO instances that will be used for
    getting log information from compute processes
*/
static void write_logs(int numRecv, FILE *logfp, LOG_INFO *logs) {
    int log = 0;

    for (log = 0; log < numRecv; log++) {
        logs[log].logfp = logfp;

        sw_write_warnings("\n", &logs[log]);
    }
}

/**
@brief Order received output from compute processes

@param[in] recvRank Rank to receive outputs from
@param[in] procIndex Index of the processes we are reading output from
@param[in] writeLens Number of inputs that were distributed to the process
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] main_p_OUT Array of accumulated output values throughout
    simulation runs
*/
static void arrange_output(
    int recvRank,
    int procIndex,
    int writeLens[],
    SW_OUT_DOM *OutDom,
    size_t offsetMult[],
    double *main_p_OUT[][SW_OUTNPERIODS]
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;

    int pdIndex;
    OutKey key;
    OutPeriod pd;

    size_t writeIndex;
    size_t oneSiteSize;

    // Loop through output keys
    ForEachOutKey(key) {
        // Loop through output periods
        for (pdIndex = 0; pdIndex < OutDom->used_OUTNPERIODS; pdIndex++) {
            pd = OutDom->timeSteps[key][pdIndex];

            // Check if output period is required
            if (OutDom->use[key] && pd != eSW_NoTime) {
                // Calculate start index within given inputs
                // i.e., main = <input index> * <size of 1 site output> *
                // procIndex
                oneSiteSize = OutDom->nrow_OUT[pd] *
                              (OutDom->ncol_OUT[key] + ncol_TimeOUT[pd]);
                writeIndex = oneSiteSize * offsetMult[procIndex + 1];

                // Receive information
                SW_MPI_Recv(
                    MPI_DOUBLE,
                    &main_p_OUT[key][pd][writeIndex],
                    oneSiteSize * writeLens[procIndex + 1],
                    recvRank,
                    swTRUE,
                    0,
                    &nullReq
                );
            }
        }
    }
}

/**
@brief Get a request and subsequent information from a compute process

This function will receive three pieces of information
    1) Simulation run statuses
        - Flag results collected throughout the simulation runs
        - Specifies if a specific run failed for succeeded
        - Used to write output and update the progress file properly
            * If a simulation run failed, do not output it and report it
    2) Request type
        - Three different types of requests
            * REQ_OUT_NC - Only receive output information to be written to file
            * REQ_OUT_LOG - Only receive log information to be written to log
                            files; only occurs when all simulations fail
            * REQ_OUT_BOTH - Both take action above
    3) Source rank
        - Rank of the process that sent information to I/O process
            * I/O process is not aware of the sender by default due to
                MPI_ANY_SOURCE

Handle the different request types accordingly
    1) Log request - Accept log information from the sender compute process
        and report logs to the respective log file

    2) Output request - Accept output infromation from the sender compute
        process and store it in a larger `p_OUT` to flush at a later time

@param[in] numCompProcs Number of compute processes that have been assigned to
    an I/O process
@param[in] recvLens A list of lengths that will be used to specify how many
    inputs to receive from a specific process
@param[in] reqType Custom MPI type that mimics SW_MPI_REQUEST
@param[in] logType Custom MPI type that mimics LOG_INFO
@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] logs A list of LOG_INFO instances that will be used for
    getting log information from compute processes
@param[in] logfps A list of pointers to open logfiles respective to
    the I/O process and all compute processes it controls
@param[out] succFlags Accumulator array of flags specifying how respective
    simulation runs went
@param[out] main_p_OUT Array of accumulated output values throughout
    simulation all runs
*/
static void get_comp_results(
    int numCompProcs,
    int recvLens[],
    MPI_Datatype reqType,
    MPI_Datatype logType,
    SW_MPI_DESIGNATE *desig,
    SW_OUT_DOM *OutDom,
    LOG_INFO *logs,
    FILE *logfps[],
    Bool *succFlags,
    double *main_p_OUT[][SW_OUTNPERIODS]
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;

    Bool logReq;
    Bool outReq;
    size_t offsetMult[PROCS_PER_IO] = {recvLens[0]};

    int destRank;
    int rankIndex;
    int rank;
    int comp;
    int succ;
    int numProcResponse = 0;
    int targetRepsonses = numCompProcs;

    for (comp = 1; comp < numCompProcs; comp++) {
        offsetMult[comp] = offsetMult[comp - 1] + recvLens[comp];
        targetRepsonses += (recvLens[comp] > 0) ? 1 : 0;
    }

    while (numProcResponse < targetRepsonses) {
        SW_MPI_REQUEST req;
        SW_MPI_Recv(reqType, &req, 1, MPI_ANY_SOURCE, swTRUE, 0, &nullReq);

        destRank = req.sourceRank;
        logReq = (Bool) (req.requestType == REQ_OUT_LOG ||
                         req.requestType == REQ_OUT_BOTH);
        outReq = (Bool) (req.requestType == REQ_OUT_NC ||
                         req.requestType == REQ_OUT_BOTH);

        rankIndex = 0;
        rank = desig->ranks[rankIndex];
        while (rank != destRank) {
            rankIndex++;
            rank = desig->ranks[rankIndex];
        }

        for (succ = 0; succ < recvLens[rankIndex]; succ++) {
            succFlags[succ + offsetMult[rankIndex + 1]] = req.runStatus[succ];
        }

        if (logReq) {
            SW_MPI_Recv(
                logType,
                logs,
                recvLens[rankIndex + 1],
                destRank,
                swTRUE,
                0,
                &nullReq
            );

            write_logs(recvLens[rankIndex + 1], logfps[rankIndex + 1], logs);
        }

        if (outReq) {
            arrange_output(
                destRank, rankIndex, recvLens, OutDom, offsetMult, main_p_OUT
            );
        }

        numProcResponse++;
    }
}

/**
@brief Get the next batch of SUIDs for input distribution

@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] numIterSuids The number of SUIDs that will be assigned
    this iteration of input distribution
@param[in] input Base input index ranging from [0, # total suids)
@param[in] useIndexFile Specifies to create/use an index file
@param[in] readInVars Specifies which variables are to be read-in as input
@param[out] distSUIDs A list of SUIDs that's a subset of an I/O processor's
    assigned SUIDs
@param[out] distTSUIDs A list of domain translated SUIDs whose data will be
    distributed as the next batch of input
*/
static void get_next_suids(
    SW_MPI_DESIGNATE *desig,
    int numIterSuids,
    int input,
    Bool useIndexFile[],
    Bool *readInVars[],
    size_t *distSUIDs[],
    size_t **distTSUIDs[]
) {
    InKeys inKey;
    int suid;

    for (suid = 0; suid < numIterSuids; suid++) {
        distSUIDs[suid][0] = desig->domSuids[input + suid][0];
        distSUIDs[suid][1] = desig->domSuids[input + suid][1];

        ForEachNCInKey(inKey) {
            if (inKey > eSW_InDomain && useIndexFile[inKey] &&
                readInVars[inKey][0]) {

                distTSUIDs[inKey][suid][0] =
                    desig->domTSuids[inKey][input + suid][0];
                distTSUIDs[inKey][suid][1] =
                    desig->domTSuids[inKey][input + suid][1];
            }
        }
    }
}

/**
@brief Wrapper function to calculate output starts/counts and
    output all values

@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] distSUIDs A list of domain SUIDs whose data will be distributed
    as the next batch of input
@param[in] numSuids Number of SUIDs that will be assigned, this should be
    the product of the <number of compute processors for the I/O process>
    and N_SUID_ASSIGN
@param[in] siteDom Specifies that the programs domain has sites, otherwise
    it is gridded
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] succFlags Accumulator array of flags specifying how respective
    simulation runs went
@param[in] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
    holds basic information about output files and values
@param[out] starts A list of size SW_NINKEYSNC specifying the start
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array
@param[out] counts A list of size SW_NINKEYSNC specifying the count
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array; counts match placements with `start`
    indices for each key
@param[out] main_p_OUT Array of accumulated output values throughout
    simulation all runs
@param[out] LogInfo Holds information on warnings and errors
*/
static void write_outputs(
    int progFileID,
    int progVarID,
    size_t **distSUIDs,
    int numSuids,
    Bool siteDom,
    SW_OUT_DOM *OutDom,
    Bool succFlags[],
    SW_PATH_OUTPUTS *SW_PathOutputs,
    size_t **starts,
    size_t **counts,
    double *main_p_OUT[][SW_OUTNPERIODS],
    LOG_INFO *LogInfo
) {
    int numWrites = 0;
    int write;
    int numSites = 0;

    get_contiguous_counts(
        distSUIDs,
        numSuids,
        siteDom,
        numSuids,
        swFALSE,
        swTRUE,
        succFlags,
        &numWrites,
        starts,
        counts
    );

    // Output accumulated output
    SW_NCOUT_write_output(
        OutDom,
        main_p_OUT,
        SW_PathOutputs->numOutFiles,
        NULL,
        NULL,
        numWrites,
        starts,
        counts,
        SW_PathOutputs->openOutFileIDs,
        siteDom,
        LogInfo
    );

    // Update progress file statuses
    for (write = 0; write < numWrites; write++) {
        SW_NCIN_set_progress(
            (Bool) !succFlags[numSites],
            progFileID,
            progVarID,
            starts[write],
            counts[write],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        numSites += (siteDom) ? counts[write][0] : counts[write][1];
    }

    for (write = 0; write < numSuids; write++) {
        succFlags[numSites] = swTRUE;
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Intiialize the MPI program by getting basic information about
the rank, world size, and processor/node name, while setting the
MPI handler method to return from a function call rather than
crashing the program and finally initializing MPI information in SW_DOMAIN

@param[in] argc Number of command-line provided inputs
@param[in] argv List of command-line provided inputs
@param[out] rank Process number known to MPI for the current process (aka rank)
@param[out] worldSize Total number of processes that the MPI run has created
@param[out] procName Name of the processor/node the current processes is
running on
@param[out] desig Designation instance that holds information about
    assigning a process to a job
@param[out] datatypes A list of custom MPI datatypes that will be created based
on various program-defined structs
*/
void SW_MPI_initialize(
    int *argc,
    char ***argv,
    int *rank,
    int *worldSize,
    char *procName,
    SW_MPI_DESIGNATE *desig,
    MPI_Datatype datatypes[]
) {
    int procNameSize = 0;
    int type;
    InKeys key;

    MPI_Init(argc, argv);

    MPI_Comm_rank(MPI_COMM_WORLD, rank);
    MPI_Comm_size(MPI_COMM_WORLD, worldSize);

    MPI_Get_processor_name(procName, &procNameSize);

    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    for (type = 0; type < SW_MPI_NTYPES; type++) {
        datatypes[type] = MPI_DATATYPE_NULL;
    }

    ForEachNCInKey(key) { desig->domTSuids[key] = NULL; }

    desig->domSuids = NULL;
    desig->nSuids = 0;
    desig->nCompProcs = 0;
    desig->useTSuids = swFALSE;
    desig->procJob = SW_MPI_PROC_COMP;
    desig->groupComm = MPI_COMM_NULL;
    desig->ioCompComm = MPI_COMM_NULL;
    desig->rootCompComm = MPI_COMM_NULL;

    desig->nTotCompProcs = 0;
    desig->nTotIOProcs = 0;
    desig->ioRank = 0;
}

/**
@brief Conclude the program run by finalizing/freeing anything that's
been initialized/created through MPI within the program run
*/
void SW_MPI_finalize() { MPI_Finalize(); }

/**
@brief Free communicators and types when finishing the program

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in,out] desig Designation instance that holds information about
    assigning a process to a job
@param[in,out] types A list of custom MPI datatypes used throughout the program
@param[out] LogInfo Holds information on warnings and errors; this function
    will use this but no errors will be reported
*/
void SW_MPI_free_comms_types(
    int rank, SW_MPI_DESIGNATE *desig, MPI_Datatype types[], LOG_INFO *LogInfo
) {
    const int numComms = 3;
    int comm;
    int type;
    MPI_Comm comms[] = {
        desig->rootCompComm, desig->groupComm, desig->ioCompComm
    };

    for (type = 0; type < SW_MPI_NTYPES; type++) {
        if (types[type] != MPI_DATATYPE_NULL) {
            free_type(&types[type], LogInfo);
        }
    }

    for (comm = 0; comm < numComms; comm++) {
        if (((comm == 0 && rank == SW_MPI_ROOT) || comm > 0) &&
            comms[comm] != MPI_COMM_NULL) {

            MPI_Comm_free(&comms[comm]);
        }
    }
}

/**
@brief Trigger an abort error when a fatal error occurs

Options for failing the program
    - NetCDF read/write error
    - Too many simulation errors
    - OpenMPI error

Moving forward, an option for further development of this part of the
program is to allow for a hardcoded number of netCDF read errors

@param[in] failType Reason why the program failed
@param[in] errorCode MPI error code if MPI caused the error (not used
    otherwise)
@param[in] mpiErrStr String representation of MPI error (not used
    if MPI did not cause the error)
*/
void SW_MPI_Fail(int failType, int errorCode, char *mpiErrStr) {
    const char *ncFail = "SOILWAT2 failed due to a netCDF error.";
    const char *compFail =
        "SOILWAT2 failed due to too many errors during simulations.";
    const char *mpiFailAdd = "SOILWAT2 failed due to an OpenMPI problem:";
    char mpiFail[FILENAME_MAX] = "\0";

    char *failStr;
    int failCode = failType;

    switch (failType) {
    case SW_MPI_FAIL_NETCDF:
        failStr = (char *) ncFail;
        break;
    case SW_MPI_FAIL_COMP_ERR:
        failStr = (char *) compFail;
        break;
    default: // SW_MPI_FAIL_MPI
        snprintf(
            mpiFail,
            sizeof(mpiFail),
            "%s \"%s\".",
            (char *) mpiFailAdd,
            mpiErrStr
        );
        failStr = mpiFail;
        failCode = errorCode;
        break;
    }

    fprintf(stderr, "An error occured: %s\n", failStr);

    MPI_Abort(MPI_COMM_WORLD, failCode);
}

/**
@brief Deconstruct MPI-related information in domain

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
*/
void SW_MPI_deconstruct(SW_DOMAIN *SW_Domain) {
    SW_MPI_DESIGNATE *desig = &SW_Domain->SW_Designation;
    int procJob = desig->procJob;
    int suid;
    InKeys inKey;

    if (procJob == SW_MPI_PROC_IO) {
        if (!isnull(desig->domSuids)) {
            for (suid = 0; suid < desig->nSuids; suid++) {
                if (!isnull(desig->domSuids[suid])) {
                    free((void *) desig->domSuids[suid]);
                    desig->domSuids[suid] = NULL;
                }
            }

            free((void *) desig->domSuids);
            desig->domSuids = NULL;
        }

        if (!isnull(desig->domTSuids)) {
            ForEachNCInKey(inKey) {
                if (!isnull(desig->domTSuids[inKey])) {
                    for (suid = 0; suid < desig->nSuids; suid++) {
                        if (!isnull(desig->domTSuids[inKey][suid])) {
                            free((void *) desig->domTSuids[inKey][suid]);
                            desig->domTSuids[inKey][suid] = NULL;
                        }
                    }

                    free((void *) desig->domTSuids[inKey]);
                    desig->domTSuids[inKey] = NULL;
                }
            }
        }
    }
}

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
    int numItems[] = {5, 5, 5, 5, 6, 5, 18, 3, 7, 9};
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
         1},                   /* SW_VEGESTAB_INPUTS */
        {1, N_SUID_ASSIGN, 1}, /* SW_MPI_REQUEST */
        {MAX_LOG_SIZE, MAX_MSGS * MAX_LOG_SIZE, 1, 1, 1, 1, 1}, /* LOG_INFO */
        {MAX_DAYS,
         MAX_DAYS,
         MAX_DAYS,
         MAX_DAYS,
         MAX_DAYS,
         MAX_DAYS,
         MAX_DAYS,
         MAX_DAYS,
         MAX_DAYS} /* SW_WEATHER_HIST */
    };

    MPI_Datatype types[][18] = {
        {MPI_INT, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED
        }, /* SW_DOMAIN */
        {MPI_UNSIGNED, MPI_UNSIGNED, MPI_INT, MPI_INT, MPI_UNSIGNED
        }, /* SW_SPINUP */
        {MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL}, /* SW_RUN_INPUTS */
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
         MPI_DOUBLE},                /* SW_VEGESTAB_INPUTS */
        {MPI_INT, MPI_INT, MPI_INT}, /* SW_MPI_REQUEST */
        {MPI_CHAR,
         MPI_CHAR,
         MPI_INT,
         MPI_UNSIGNED_LONG,
         MPI_UNSIGNED_LONG,
         MPI_INT,
         MPI_INT}, /* LOG_INFO */
        {MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE} /* SW_WEATHER_HIST */
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
         offsetof(SW_VEGESTAB_INFO_INPUTS, max_temp_estab)},

        /* SW_MPI_REQUEST */
        {offsetof(SW_MPI_REQUEST, sourceRank),
         offsetof(SW_MPI_REQUEST, runStatus),
         offsetof(SW_MPI_REQUEST, requestType)},

        /* LOG_INFO */
        {offsetof(LOG_INFO, errorMsg),
         offsetof(LOG_INFO, warningMsgs),
         offsetof(LOG_INFO, numWarnings),
         offsetof(LOG_INFO, numDomainWarnings),
         offsetof(LOG_INFO, numDomainErrors),
         offsetof(LOG_INFO, stopRun),
         offsetof(LOG_INFO, QuietMode)},

        /* SW_WEATHER_HIST */
        {offsetof(SW_WEATHER_HIST, temp_max),
         offsetof(SW_WEATHER_HIST, temp_min),
         offsetof(SW_WEATHER_HIST, temp_avg),
         offsetof(SW_WEATHER_HIST, ppt),
         offsetof(SW_WEATHER_HIST, cloudcov_daily),
         offsetof(SW_WEATHER_HIST, windspeed_daily),
         offsetof(SW_WEATHER_HIST, r_humidity_daily),
         offsetof(SW_WEATHER_HIST, shortWaveRad),
         offsetof(SW_WEATHER_HIST, actualVaporPressure)}
    };

    // Input type information
    int numItemsInStructs[] = {6, 6, 12, 5, 2};

    MPI_Datatype inTypes[][12] = {
        {MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE
        },
        {MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_INT},
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
        {MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL,
         MPI_DATATYPE_NULL},
        {MPI_DOUBLE, MPI_UNSIGNED}
    };

    int inBlockLens[][12] = {
        {MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_MONTHS,
         MAX_DAYS + 1},
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
        {1, 1, 1, 1, 1},
        {1, 1}
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
        {offsetof(SW_VEGPROD_RUN_INPUTS, veg[0]),
         offsetof(SW_VEGPROD_RUN_INPUTS, veg[1]),
         offsetof(SW_VEGPROD_RUN_INPUTS, veg[2]),
         offsetof(SW_VEGPROD_RUN_INPUTS, veg[3]),
         offsetof(SW_VEGPROD_RUN_INPUTS, bare_cov)},
        {offsetof(SW_SITE_RUN_INPUTS, Tsoil_constant),
         offsetof(SW_SITE_RUN_INPUTS, n_layers)}
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
        {offsetof(CoverTypeRunIn, fCover)},
        {offsetof(VegTypeRunIn, cov),
         offsetof(VegTypeRunIn, litter),
         offsetof(VegTypeRunIn, biomass),
         offsetof(VegTypeRunIn, pct_live),
         offsetof(VegTypeRunIn, lai_conv)}
    };

    int type;
    int runType;
    int covIndex;
    int veg;
    const int numRunInTypes = 5;
    const int numCovTypes = 2;
    const int vegprodIndex = 3;

    for (type = 0; type < SW_MPI_NTYPES; type++) {
        if (type == eSW_MPI_Inputs) {
            /* Create custom MPI subtypes for bigger SW_RUN_INPUTS */
            for (runType = 0; runType < numRunInTypes; runType++) {
                if (runType == vegprodIndex) {
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
                           for bare cover and make multiple instances of
                           "veg" to send NVEGTYPE instances */
                        if (covIndex == 1) {
                            for (veg = 1; veg < NVEGTYPES; veg++) {
                                MPI_Type_dup(
                                    inTypes[vegprodIndex][0],
                                    &inTypes[vegprodIndex][veg]
                                );
                            }
                            MPI_Type_dup(
                                covTypes[1][0], &inTypes[vegprodIndex][4]
                            );
                            free_type(&covTypes[1][0], LogInfo);
                        }
                    }
                }

                res = MPI_Type_create_struct(
                    numItemsInStructs[runType],
                    inBlockLens[runType],
                    inOffsets[runType],
                    inTypes[runType],
                    &types[type][runType]
                );
                if (res != MPI_SUCCESS) {
                    goto reportFail;
                }
                res = MPI_Type_commit(&types[type][runType]);
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
            &datatypes[type]
        );
        if (res != MPI_SUCCESS) {
            goto reportFail;
        }
        res = MPI_Type_commit(&datatypes[type]);
        if (res != MPI_SUCCESS) {
            goto reportFail;
        }

        if (type == eSW_MPI_Inputs) {
            for (veg = 0; veg < NVEGTYPES + 1; veg++) {
                free_type(&inTypes[vegprodIndex][veg], LogInfo);
            }
        }
    }

    /* Free run input sub types */
    for (runType = 0; runType < numRunInTypes; runType++) {
        free_type(&types[eSW_MPI_Inputs][runType], LogInfo);
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
    Bool getWeather = swFALSE;

    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    if (rank == SW_MPI_ROOT) {
        getWeather = !SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];
    }

    SW_MPI_process_types(SW_Domain, procName, worldSize, rank, LogInfo);
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    SW_MPI_template_info(
        rank,
        &SW_Domain->SW_Designation,
        sw_template,
        SW_Domain->datatypes[eSW_MPI_Inputs],
        SW_Domain->datatypes[eSW_MPI_Spinup],
        SW_Domain->datatypes[eSW_MPI_VegEstabIn],
        SW_Domain->datatypes[eSW_MPI_WeathHist],
        getWeather,
        LogInfo
    );
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
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
@param[in] weathHistType Custom MPI type for transfering data for
    SW_WEATHER_HIST
@param[in] getWeather Specifies if the root program should spread default
    weather inputs to processes
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_template_info(
    int rank,
    SW_MPI_DESIGNATE *desig,
    SW_RUN *SW_Run,
    MPI_Datatype inRunType,
    MPI_Datatype spinupType,
    MPI_Datatype vegEstabType,
    MPI_Datatype weathHistType,
    Bool getWeather,
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
    int numElem[] = {3, 24, 5, 5, 7, 44, 2};
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
    void *buffers[][44] = {
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
         (void *) &SW_Run->VegProdIn.veg_method,
         (void *) &SW_Run->VegProdIn.bare_cov.albedo},
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
         (void *) &SW_Run->SiteIn.SWCMinVal,
         (void *) &SW_Run->SiteSim.n_transp_rgn,
         (void *) SW_Run->SiteSim.TranspRgnDepths},
        {(void *) &SW_Run->VegEstabIn.use, (void *) &SW_Run->VegEstabIn.count}
    };
    MPI_Datatype types[][44] = {
        {MPI_INT, MPI_INT, MPI_DOUBLE}, /* SW_CARBON_INPUTS */
        {MPI_INT,      MPI_INT,      MPI_UNSIGNED, MPI_DOUBLE,  MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,  MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,   MPI_CHAR,    MPI_INT,
         MPI_INT,      MPI_INT,      MPI_INT,      MPI_INT,     MPI_UNSIGNED,
         MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED
        }, /* SW_WEATHER_INPUTS */
        {MPI_INT, MPI_DOUBLE, MPI_INT, MPI_INT, MPI_DOUBLE
        }, /* SW_VEGPROD_INPUTS */
        {spinupType, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED
        }, /* SW_MODEL_INPUTS */
        {MPI_INT, MPI_INT, MPI_UNSIGNED, MPI_DOUBLE, MPI_DOUBLE
        }, /* SW_SOILWAT_INPUTS */
        {MPI_CHAR,     MPI_CHAR,   MPI_INT,      MPI_UNSIGNED, MPI_UNSIGNED,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,
         MPI_UNSIGNED, MPI_INT,    MPI_INT,      MPI_INT,      MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_DOUBLE,   MPI_DOUBLE,   MPI_DOUBLE,
         MPI_DOUBLE,   MPI_DOUBLE, MPI_UNSIGNED, MPI_DOUBLE
        },                      /* SW_SITE_INPUTS */
        {MPI_INT, MPI_UNSIGNED} /* SW_VEGPROD_SIM */
    };
    int count[][44] = {
        /* SW_CARBON_INPUTS */
        {1, 1, MAX_NYEAR},

        /* SW_WEATHER_INPUTS */
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
         1,
         1},

        /* SW_VEGPROD_INPUTS */
        {1, NVEGTYPES, 1, 1, 1},

        /* SW_MODEL_INPUTS */
        {1, 1, 1, 1, 1},

        /* SW_SOILWAT_INPUTS */
        {1, 1, 1, 1, MAX_DAYS * MAX_LAYERS, MAX_DAYS * MAX_LAYERS},

        /* SW_SITE_INPUTS */
        {64, 64, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1,  1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         1,  1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, MAX_TRANSP_REGIONS},

        /* SW_VEGPROD_SIM */
        {1, 1}
    };
    int markov1DCount[] = {MAX_WEEKS * 2, MAX_WEEKS * 2 * 2, 1};
    int veg;
    int vegIn;
    const int numNumTemplateVeg = 28;

    MPI_Comm comm =
        (rank == SW_MPI_ROOT) ? desig->rootCompComm : desig->groupComm;

    // Send input information to all processes
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

    if (desig->procJob == SW_MPI_PROC_COMP || rank == SW_MPI_ROOT) {
        ForEachVegType(veg) {
            void *sendBuff[] = {
                (void *) &SW_Run->VegProdIn.veg[veg].cnpy.xinflec,
                (void *) &SW_Run->VegProdIn.veg[veg].cnpy.yinflec,
                (void *) &SW_Run->VegProdIn.veg[veg].cnpy.range,
                (void *) &SW_Run->VegProdIn.veg[veg].cnpy.slope,
                (void *) &SW_Run->VegProdIn.veg[veg].canopy_height_constant,
                (void *) &SW_Run->VegProdIn.veg[veg].veg_kSmax,
                (void *) &SW_Run->VegProdIn.veg[veg].veg_kdead,
                (void *) &SW_Run->VegProdIn.veg[veg].lit_kSmax,
                (void *) &SW_Run->VegProdIn.veg[veg].EsTpartitioning_param,
                (void *) &SW_Run->VegProdIn.veg[veg].Es_param_limit,
                (void *) &SW_Run->VegProdIn.veg[veg].shade_scale,
                (void *) &SW_Run->VegProdIn.veg[veg].shade_deadmax,
                (void *) &SW_Run->VegProdIn.veg[veg].tr_shade_effects.xinflec,
                (void *) &SW_Run->VegProdIn.veg[veg].tr_shade_effects.yinflec,
                (void *) &SW_Run->VegProdIn.veg[veg].tr_shade_effects.range,
                (void *) &SW_Run->VegProdIn.veg[veg].tr_shade_effects.slope,
                (void *) &SW_Run->VegProdIn.veg[veg].maxCondroot,
                (void *) &SW_Run->VegProdIn.veg[veg].swpMatric50,
                (void *) &SW_Run->VegProdIn.veg[veg].shapeCond,
                (void *) &SW_Run->VegProdIn.veg[veg].SWPcrit,
                (void *) &SW_Run->VegProdIn.critSoilWater[veg],
                (void *) &SW_Run->VegProdIn.veg[veg].co2_bio_coeff1,
                (void *) &SW_Run->VegProdIn.veg[veg].co2_bio_coeff2,
                (void *) &SW_Run->VegProdIn.veg[veg].co2_wue_coeff1,
                (void *) &SW_Run->VegProdIn.veg[veg].co2_wue_coeff2,
                (void *) &SW_Run->VegProdIn.veg[veg].cov.albedo,
                (void *) &SW_Run->VegProdIn.rank_SWPcrits[veg],
                (void *) &SW_Run->VegProdIn.veg[veg].flagHydraulicRedistribution
            };

            for (vegIn = 0; vegIn < numNumTemplateVeg; vegIn++) {
                SW_Bcast(
                    (vegIn < numNumTemplateVeg - 2) ? MPI_DOUBLE : MPI_INT,
                    sendBuff[vegIn],
                    1,
                    SW_MPI_ROOT,
                    MPI_COMM_WORLD
                );
            }
        }
    }

    // Markov inputs if enabled
    if ((desig->procJob == SW_MPI_PROC_COMP || rank == SW_MPI_ROOT) &&
        SW_Run->WeatherIn.generateWeatherMethod == 2) {

        if (rank > SW_MPI_ROOT) {
            SW_MKV_construct(SW_Run->WeatherIn.rng_seed, &SW_Run->MarkovIn);
            allocateMKV(&SW_Run->MarkovIn, LogInfo);
        }
        if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
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

    if (SW_Run->VegEstabIn.use) {
        SW_Bcast(
            types[vegEstabIndex][1],
            buffers[vegEstabIndex][1],
            count[vegEstabIndex][1],
            SW_MPI_ROOT,
            MPI_COMM_WORLD
        );

        for (vCount = 0; vCount < SW_Run->VegEstabIn.count; vCount++) {
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

    if (desig->procJob == SW_MPI_PROC_COMP || rank == SW_MPI_ROOT) {
        SW_Bcast(MPI_INT, &getWeather, 1, SW_MPI_ROOT, comm);

        if (getWeather) {
            if (rank > SW_MPI_ROOT) {
                SW_WTH_allocateAllWeather(
                    &SW_Run->RunIn.weathRunAllHist,
                    SW_Run->WeatherIn.n_years,
                    LogInfo
                );
            }
            if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                return;
            }

            SW_Bcast(
                weathHistType,
                SW_Run->RunIn.weathRunAllHist,
                SW_Run->WeatherIn.n_years,
                SW_MPI_ROOT,
                comm
            );
        }
    }
}

/**
@brief Send netCDF output information to I/O processes from root

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] comm MPI communicator to broadcast a message to
@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_ncout_info(
    int rank, MPI_Comm comm, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    OutKey outKey;
    IntUS var;
    IntUS nVars;
    const int unitsIndex = 4;
    const int varNameIndex = 1;
    char *attStr;
    Bool sendStr = swTRUE;

    if (rank > SW_MPI_ROOT) {
        SW_NCOUT_alloc_output_var_info(OutDom, LogInfo);
    }
    if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
        return;
    }

    SW_Bcast(
        MPI_INT, &OutDom->netCDFOutput.strideOutYears, 1, SW_MPI_ROOT, comm
    );
    SW_Bcast(
        MPI_INT, &OutDom->netCDFOutput.baseCalendarYear, 1, SW_MPI_ROOT, comm
    );

    ForEachOutKey(outKey) {
        nVars = OutDom->nvar_OUT[outKey];

        if (nVars == 0 || !OutDom->use[outKey]) {
            continue;
        }

        SW_Bcast(
            MPI_INT,
            OutDom->netCDFOutput.reqOutputVars[outKey],
            nVars,
            SW_MPI_ROOT,
            comm
        );

        for (var = 0; var < nVars; var++) {
            if (!OutDom->netCDFOutput.reqOutputVars[outKey][var]) {
                continue;
            }

            attStr =
                OutDom->netCDFOutput.outputVarInfo[outKey][var][unitsIndex];

            sendStr = (Bool) (!isnull(attStr));
            get_dynamic_string(rank, &attStr, sendStr, comm, LogInfo);
            if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                return;
            }

            attStr =
                OutDom->netCDFOutput.outputVarInfo[outKey][var][varNameIndex];
            sendStr = (Bool) (!isnull(attStr));
            get_dynamic_string(rank, &attStr, sendStr, comm, LogInfo);
            if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                return;
            }

            attStr = OutDom->netCDFOutput.units_sw[outKey][var];
            sendStr = (Bool) (!isnull(attStr));
            get_dynamic_string(rank, &attStr, sendStr, comm, LogInfo);
            if (SW_MPI_setup_fail(LogInfo->stopRun, comm)) {
                return;
            }
        }
    }
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
    SW_Bcast(types[eSW_MPI_Domain], SW_Domain, 1, SW_MPI_ROOT, MPI_COMM_WORLD);

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
@brief Opens input and output files for I/O processes for parallel
    read and write; this includes log file pointers

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in,out] pathInputs Struct of type SW_PATH_INPUTS which
    holds basic information about input files and values
@param[in] netCDFIn Constant netCDF input file information
@param[in,out] pathOutputs Constant netCDF output file information
@param[in] OutDom Constant netCDF input file information
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_open_files(
    int rank,
    SW_MPI_DESIGNATE *desig,
    SW_PATH_INPUTS *pathInputs,
    SW_NETCDF_IN *netCDFIn,
    SW_PATH_OUTPUTS *pathOutputs,
    SW_OUT_DOM *OutDom,
    LOG_INFO *LogInfo
) {
    MPI_Comm comm = desig->groupComm;
    char *logFileName = pathInputs->txtInFiles[eLog];

    if (rank == SW_MPI_ROOT) {
        nc_close(pathInputs->ncDomFileIDs[vNCdom]);
        nc_close(pathInputs->ncDomFileIDs[vNCprog]);
    }

    open_input_files(rank, comm, netCDFIn, pathInputs, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    open_output_files(
        rank, logFileName, comm, desig, pathOutputs, OutDom, LogInfo
    );
}

/**
@brief Close all opened netCDF files

@param[in] openOutFileIDs A list of open output netCDF file IDs
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] numOutFiles Number of output files for each
    output key/period
*/
void SW_MPI_close_out_files(
    int *openOutFileIDs[][SW_OUTNPERIODS], SW_OUT_DOM *OutDom, int numOutFiles
) {
    OutKey outKey;
    OutPeriod pd;
    int file;

    ForEachOutKey(outKey) {
        if (!isnull(openOutFileIDs[outKey])) {
            if (OutDom->nvar_OUT[outKey] > 0 && OutDom->use[outKey]) {
                ForEachOutPeriod(pd) {
                    if (!isnull(openOutFileIDs[outKey][pd])) {
                        if (OutDom->use_OutPeriod[pd]) {
                            for (file = 0; file < numOutFiles; file++) {
                                nc_close(openOutFileIDs[outKey][pd][file]);
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
@brief Close all opened netCDF files

@param[in] openInFileIDs A list of open input netCDF file IDs
@param[in] readInVars Specifies which variables are to be read-in as input
@param[in] useIndexFile Specifies to create/use an index file
@param[in] numWeathFiles Number of weather files that were created
*/
void SW_MPI_close_in_files(
    int **openInFileIDs[],
    Bool **readInVars,
    Bool useIndexFile[],
    int numWeathFiles
) {
    InKeys inKey;
    Bool skipVar;
    int numFiles;
    int varNum;
    int file;

    ForEachNCInKey(inKey) {
        if (!readInVars[inKey][0] || inKey == eSW_InDomain ||
            isnull(openInFileIDs[inKey])) {
            continue;
        }

        numFiles = (inKey == eSW_InWeather) ? numWeathFiles : 1;

        for (varNum = 0; varNum < numVarsInKey[inKey]; varNum++) {
            skipVar = (Bool) (!readInVars[inKey][varNum + 1] ||
                              (varNum == 0 && !useIndexFile[inKey]) ||
                              isnull(openInFileIDs[inKey][varNum]));

            if (!skipVar) {
                for (file = 0; file < numFiles; file++) {
                    nc_close(openInFileIDs[inKey][varNum][file]);
                }
            }
        }
    }
}

/**
@brief Before we proceed to the next important section of the program,
we must do a check-in with all processes to make sure no errors occurred

@param[in] stopRun A flag specifying if an error occurred and stop the run
@param[in] comm MPI communicator to broadcast a message to
*/
Bool SW_MPI_setup_fail(Bool stopRun, MPI_Comm comm) {
    int fail = (stopRun) ? 1 : 0;
    int failProgram = 0;

    MPI_Allreduce(&fail, &failProgram, 1, MPI_INT, MPI_SUM, comm);

    return (Bool) (failProgram > 0);
}

/**
@brief Report any log information that has been created throughout the
program run either through I/O processes or the root process

Assuming setup did not fail, average domain simulation information from
compute processes; if setup did fail, report any log information to the
general log file the root process holds

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] size Number of processors (world size) within the
    communicator MPI_COMM_WORLD
@param[in] wtType Custom MPI datatype for SW_WALLTIME
@param[in] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] failedSetup Specifies if the setup process failed
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_MPI_report_log(
    int rank,
    int size,
    MPI_Datatype wtType,
    SW_WALLTIME *SW_WallTime,
    SW_DOMAIN *SW_Domain,
    Bool failedSetup,
    LOG_INFO *LogInfo
) {
    SW_MPI_DESIGNATE *desig = &SW_Domain->SW_Designation;
    MPI_Datatype logType = SW_Domain->datatypes[eSW_MPI_Log];
    MPI_Request req = MPI_REQUEST_NULL;
    int numRanks = 0;
    double prevMean = 0.0;
    double runSqr = 0.0;
    int destProcJob = SW_MPI_PROC_COMP;
    int destRank;
    Bool reportLog = (Bool) (LogInfo->stopRun || LogInfo->numWarnings > 0);
    Bool destReport = swFALSE;
    char warnHeader[MAX_FILENAMESIZE] = "\0";
    FILE *tempFilePtr = LogInfo->logfp;

    if (rank == SW_MPI_ROOT) {
        if (reportLog) {
            if (!isnull(LogInfo->logfps)) {
                tempFilePtr = LogInfo->logfp;
                LogInfo->logfp = LogInfo->logfps[0];
            }

            sw_write_warnings("\n", LogInfo);
            LogInfo->logfp = tempFilePtr;
        }

        /* Get timing information to average in root processes */
        for (destRank = 1; destRank < size; destRank++) {
            SW_WALLTIME rankWT;
            LOG_INFO procLog;

            SW_MPI_Recv(MPI_INT, &destProcJob, 1, destRank, swTRUE, 0, &req);

            if (destProcJob == SW_MPI_PROC_COMP && !failedSetup) {
                SW_MPI_Recv(wtType, &rankWT, 1, destRank, swTRUE, 0, &req);
                numRanks++;

                prevMean = SW_WallTime->timeMean;
                SW_WallTime->timeMean = get_running_mean(
                    numRanks, SW_WallTime->timeMean, rankWT.timeMean
                );
                SW_WallTime->timeMax = get_running_mean(
                    numRanks, SW_WallTime->timeMax, rankWT.timeMean
                );
                SW_WallTime->timeMin = get_running_mean(
                    numRanks, SW_WallTime->timeMin, rankWT.timeMean
                );
                runSqr = get_running_sqr(
                    prevMean, SW_WallTime->timeMean, rankWT.timeMean
                );
                SW_WallTime->nTimedRuns += rankWT.nTimedRuns;
                SW_WallTime->nUntimedRuns += rankWT.nUntimedRuns;
            } else if (failedSetup) {
                SW_MPI_Recv(MPI_INT, &destReport, 1, destRank, swTRUE, 0, &req);

                if (destReport) {
                    // Get logs that need to be reported to general
                    // log file
                    sw_init_logs(LogInfo->logfp, &procLog);

                    SW_MPI_Recv(
                        logType, &procLog, 1, destRank, swTRUE, 0, &req
                    );

                    snprintf(
                        warnHeader, MAX_FILENAMESIZE, "\n(Rank %d) ", destRank
                    );

                    sw_write_warnings(warnHeader, &procLog);

                    LogInfo->numDomainErrors += procLog.numDomainErrors;
                    LogInfo->numDomainWarnings += procLog.numDomainWarnings;
                }
            }
        }

        if (!failedSetup) {
            SW_WallTime->timeSD = final_running_sd(numRanks, runSqr);

            SW_WT_ReportTime(*SW_WallTime, LogInfo);
        }
    } else {
        /* Send process job to root process */
        SW_MPI_Send(MPI_INT, &desig->procJob, 1, SW_MPI_ROOT, swTRUE, 0, &req);

        if (desig->procJob == SW_MPI_PROC_COMP && !failedSetup) {
            /* Send timing information to the root process to average it */
            SW_MPI_Send(wtType, SW_WallTime, 1, SW_MPI_ROOT, swTRUE, 0, &req);

        } else if (failedSetup) {
            // Send information to the root process
            SW_MPI_Send(MPI_INT, &reportLog, 1, SW_MPI_ROOT, swTRUE, 0, &req);

            if (reportLog) {
                SW_MPI_Send(logType, LogInfo, 1, SW_MPI_ROOT, swTRUE, 0, &req);
            }
        }
    }
}

/**
@brief Find the active sites within the provided domain so we do not
try to simulate/assign to compute processes

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[out] numActiveSites Number of active sites that will be simulated
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_root_find_active_sites(
    SW_DOMAIN *SW_Domain,
    unsigned long ***activeSuids,
    int *numActiveSites,
    LOG_INFO *LogInfo
) {
    int suid = 0;
    signed char *prog = NULL;
    int progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprog];
    Bool sDom = SW_Domain->netCDFInput.siteDoms[eSW_InDomain];
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
            SW_DOM_calc_ncSuid(SW_Domain, progIndex, (*activeSuids)[suid]);
            suid++;
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
    Bool sProgDom = SW_Domain->netCDFInput.siteDoms[eSW_InDomain];
    size_t nSites;
    unsigned int *sxIndexVals = NULL;
    unsigned int *yIndexVals = NULL;
    Bool **readInVars = SW_Domain->netCDFInput.readInVars;
    Bool *useIndexFile = SW_Domain->netCDFInput.useIndexFile;
    char ***ncInFiles = SW_Domain->SW_PathInputs.ncInFiles;
    size_t numPosKeys = eSW_LastInKey;
    int index;
    int site;
    InKeys inKey;
    int fileID = -1;
    char *ncFileName;
    int varID;
    unsigned long *indexCell;
    unsigned long *domSuid;
    size_t offset;

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

        fileID = -1;

        ncFileName = ncInFiles[inKey][0];
        SW_NC_open(ncFileName, NC_NOWRITE, &fileID, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }

        inSDom = SW_Domain->netCDFInput.siteDoms[inKey];
        nSites =
            (sProgDom) ? SW_Domain->nDimS : SW_Domain->nDimX * SW_Domain->nDimY;
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
                offset = (sProgDom) ?
                             domSuid[0] :
                             ((site / SW_Domain->nDimX) * SW_Domain->nDimX) +
                                 domSuid[1];

                indexCell[0] = yIndexVals[offset];
                indexCell[1] = sxIndexVals[offset];
            }
        }

        nc_close(fileID);
    }

freeMem:
    if (!isnull(sxIndexVals)) {
        free(sxIndexVals);
    }

    if (!isnull(yIndexVals)) {
        free(yIndexVals);
    }

    if (fileID > -1) {
        nc_close(fileID);
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
    int maxNodes = startNewSize;
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
    SW_MPI_DESIGNATE *desig = &SW_Domain->SW_Designation;
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
        // Check that the root process information allocation occured
        // with no problems
        if (SW_MPI_setup_fail(swFALSE, MPI_COMM_WORLD)) {
            return;
        }

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

        if (SW_MPI_setup_fail(swFALSE, MPI_COMM_WORLD)) {
            return;
        }

        // Get processor assignment
        assignProcs(
            desType,
            NULL,
            rank,
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
            goto reportError;
        }

        SW_MPI_get_activated_tsuids(
            SW_Domain, activeSuids, &activeTSuids, numActiveSites, LogInfo
        );
        if (LogInfo->stopRun) {
            goto reportError;
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
            goto reportError;
        }

    reportError:
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            return;
        }

        getProcInfo(
            worldSize,
            procName,
            &numNodes,
            &maxNodes,
            &nodeNames,
            &numProcsInNode,
            &numMaxProcsInNode,
            &ranksInNodes,
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto checkForError;
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
                goto checkForError;
            }

            numIOProcsTot += calcNumIOProcs(numProcsInNode[node]);
        }
        desig->nTotIOProcs = numIOProcsTot;
        desig->nTotCompProcs = worldSize - numIOProcsTot;

        if (numActiveSites == 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "No active sites to simulate."
            );
        } else if (numActiveSites < worldSize - numIOProcsTot) {
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
        }
        if (LogInfo->stopRun ||
            SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            goto checkForError;
        }

        leftSuids = numActiveSites;
        designateProcesses(
            &designations,
            desig,
            numProcsInNode,
            numNodes,
            desig->nTotCompProcs,
            numIOProcsTot,
            numActiveSites,
            activeSuids,
            activeTSuids,
            ranksInNodes,
            &leftSuids,
            &suidAssign,
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto checkForError;
        }

        // Send designation to processes
        desig->useTSuids = useTranslated;
        assignProcs(
            desType,
            designations,
            rank,
            numNodes,
            SW_Domain->netCDFInput.useIndexFile,
            numProcsInNode,
            ranksInNodes,
            useTranslated,
            desig,
            LogInfo
        );
    }

checkForError:
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        goto freeMem;
    }

    create_groups(
        rank,
        worldSize,
        numIOProcsTot,
        designations,
        ranksInNodes,
        numNodes,
        numProcsInNode,
        desig,
        LogInfo
    );

freeMem:
    deallocProcHelpers(
        desig->nSuids,
        maxNodes,
        worldSize,
        &designations,
        &activeSuids,
        &activeTSuids,
        &nodeNames,
        &numProcsInNode,
        &numMaxProcsInNode,
        &ranksInNodes
    );
}

/**
@brief Organize output data when completing a simulation run into
    a bigger output buffer to later send to an I/O process

Process designation: Compute

@param[in] runNum Current run number the compute process is on,
    should be between [0, N_SUID_ASSIGN - 1]
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] src_p_OUT Source of accumulated output values throughout
    simulation runs
@param[out] dest_p_OUT Destination of accumulated output values throughout
    simulation runs
*/
void SW_MPI_store_outputs(
    int runNum,
    SW_OUT_DOM *OutDom,
    double *src_p_OUT[][SW_OUTNPERIODS],
    double *dest_p_OUT[][SW_OUTNPERIODS]
) {
    size_t startIndex;
    size_t oneSiteSize;
    OutKey outKey;
    OutPeriod timeStep;
    int ipd;

    ForEachOutKey(outKey) {
        for (ipd = 0; ipd < OutDom->used_OUTNPERIODS; ipd++) {
            timeStep = OutDom->timeSteps[outKey][ipd];

            if (OutDom->use[outKey] && timeStep != eSW_NoTime) {
                oneSiteSize =
                    OutDom->nrow_OUT[timeStep] *
                    (OutDom->ncol_OUT[outKey] + ncol_TimeOUT[timeStep]);
                startIndex = oneSiteSize * runNum;

                memcpy(
                    &dest_p_OUT[outKey][timeStep][startIndex],
                    src_p_OUT[outKey][timeStep],
                    sizeof(double) * oneSiteSize
                );
            }
        }
    }
}

/**
@brief Send information to I/O processes when a compute
    process is some with their assigned workload

This function will send three pieces of information
    1) Simulation run statuses
        - Flag results collected throughout the simulation runs
        - Specifies if a specific run failed for succeeded
        - Used to write output and update the progress file properly
            * If a simulation run failed, do not output it and report it
    2) Request type
        - Three different types of requests
            * REQ_OUT_NC - Only send output information to be written to file
            * REQ_OUT_LOG - Only send log information to be written to log
                            files; only occurs when all simulations fail
            * REQ_OUT_BOTH - Both take action above
    3) Source rank
        - Rank of the process that sent information to I/O process
            * I/O process is not aware of the sender by default due to
                MPI_ANY_SOURCE

Further improvement upon this functionality could be sending outputs
for the current batch of outputs while doing other operations
(asynchronous)

Process designation: Compute

@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] numInputs Number of inputs that were sent to an I/O process
@param[in] ioRank Destination rank of the I/O process to send to
@param[in] reqTypeMPI Custom MPI type that mimics SW_MPI_REQUEST
@param[in] logType Custom MPI type that mimics LOG_INFO
@param[in] runStatuses A list of run statuses for each simulation
    (success or fail)
@param[in] reportLog A flag specifying that something was reported within
    the simulations and must be sent
@param[in] logs A list of LOG_INFO instances to send to the I/O process
    if the `reportLog` flag is turned on
@param[in] p_OUT Array of accumulated output values throughout
    simulation runs
*/
void SW_MPI_send_results(
    SW_OUT_DOM *OutDom,
    int rank,
    int numInputs,
    int ioRank,
    MPI_Datatype reqTypeMPI,
    MPI_Datatype logType,
    Bool runStatuses[],
    Bool reportLog,
    LOG_INFO logs[],
    double *p_OUT[][SW_OUTNPERIODS]
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;
    int reqType = REQ_OUT_NC;
    Bool succRun = swFALSE;
    int run;
    SW_MPI_REQUEST req;
    OutKey outKey;
    int timeStep;
    int ipd;
    size_t sendSize = 0;

    for (run = 0; run < N_SUID_ASSIGN; run++) {
        req.runStatus[run] = runStatuses[run];
        succRun = (Bool) (succRun || runStatuses[run]);
    }

    if (succRun && reportLog) {
        reqType = REQ_OUT_BOTH;
    } else if (reportLog) {
        reqType = REQ_OUT_LOG;
    }

    req.sourceRank = rank;
    req.requestType = reqType;

    SW_MPI_Send(reqTypeMPI, &req, 1, ioRank, swTRUE, 0, &nullReq);

    if (reportLog) {
        SW_MPI_Send(logType, logs, numInputs, ioRank, swTRUE, 0, &nullReq);
    }

    if (succRun) {
        ForEachOutKey(outKey) {
            for (ipd = 0; ipd < OutDom->used_OUTNPERIODS; ipd++) {
                timeStep = OutDom->timeSteps[outKey][ipd];

                if (OutDom->use[outKey] && timeStep != eSW_NoTime) {
                    sendSize =
                        OutDom->nrow_OUT[timeStep] *
                        (OutDom->ncol_OUT[outKey] + ncol_TimeOUT[timeStep]);
                    sendSize *= numInputs;

                    SW_MPI_Send(
                        MPI_DOUBLE,
                        p_OUT[outKey][timeStep],
                        sendSize,
                        ioRank,
                        swTRUE,
                        0,
                        &nullReq
                    );

                    memset(
                        p_OUT[outKey][timeStep], 0, sizeof(double) * sendSize
                    );
                }
            }
        }
    }
}

/**
@brief Handle the receiving of inputs from an I/O process

This function will receive up to N_SUID_ASSIGN inputs from the
assigned I/O process

Further improvement upon this functionality could be getting inputs
for the next batch of inputs during simulation runs (asynchronous)

Process designation: Compute

@param[in] getWeather Specifies if we get weather from I/O process;
    if not, we copy it from the SW_RUN template
@param[in] n_years Number of years of weather to receive if we do
    get weather from the I/O process
@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] inputType Custom MPI data type for sending SW_RUN_INPUTS
@param[in] weathHistType Custom MPI type for transfering data for
    SW_WEATHER_HIST
@param[out] inputs A list of SW_RUN_INPUTS that will be filled by an
    I/O process
@param[out] numInputs Number of inputs that were sent to this process
@param[out] estVeg A flag specifying if the process needs to estiamte
    vegetation
@param[in,out] getEstVeg Specifies if the function needs to get the `estVeg`
    flag from it's I/O process
@param[out] extraFailCheck Specifies if, when a compute process is done with
    all workloads, the compute process should take part in an extra call to
    `SW_MPI_setup_fail()`
*/
void SW_MPI_get_inputs(
    Bool getWeather,
    int n_years,
    SW_MPI_DESIGNATE *desig,
    MPI_Datatype inputType,
    MPI_Datatype weathHistType,
    SW_RUN_INPUTS inputs[],
    int *numInputs,
    Bool *estVeg,
    Bool *getEstVeg,
    Bool *extraFailCheck
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;
    int input;

    if (*getEstVeg) {
        SW_Bcast(MPI_INT, estVeg, 1, SW_GROUP_ROOT, desig->ioCompComm);
        *getEstVeg = swFALSE;
    }

    SW_MPI_Recv(MPI_INT, numInputs, 1, desig->ioRank, swTRUE, 0, &nullReq);

    if (numInputs == 0) {
        SW_MPI_Recv(
            MPI_INT, extraFailCheck, 1, desig->ioRank, swTRUE, 0, &nullReq
        );
    }

    if (*numInputs > 0) {
        for (input = 0; input < *numInputs; input++) {
            SW_MPI_Recv(
                inputType, &inputs[input], 1, desig->ioRank, swTRUE, 0, &nullReq
            );
        }

        if (getWeather) {
            for (input = 0; input < *numInputs; input++) {
                SW_MPI_Recv(
                    weathHistType,
                    inputs[input].weathRunAllHist,
                    n_years,
                    desig->ioRank,
                    swTRUE,
                    0,
                    &nullReq
                );
            }
        }
    }
}

/**
@brief Handle I/O requests from assigned compute processes

This function will execute the following operations as main functionality
    - Assign SUIDs to compute processes
    - Read inputs for said SUIDs
    - Distribute inputs to assigned compute processes
    - Write output if large buffer to hold all results is full of previous
        SUID outputs
    - Get current simulation outputs
    - Repeat until there are no outputs left
    - Output any residual outputs that weren't enough to qualify as "full"

Further improvement upon this functionality could be sending/receiving
inputs/outputs for the next batch of inputs/outputs while doing other operations
(asynchronous)

This function also pre-calculates starts and counts for reading/writing
from the netCDF library to have as many contiguous reads/writes as possible

@note Process designations: I/O

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] sw Comprehensive struct of type SW_RUN containing all
    information in the simulation
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] setupFail Specifies if the process failed in the setup phase
    (SWMPI only)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_handle_IO(
    int rank,
    SW_RUN *sw,
    SW_DOMAIN *SW_Domain,
    Bool *setupFail,
    LOG_INFO *LogInfo
) {
    // Initialize variables
    // Store number `nCompProcs` in variable (SW_MPI_DESIGNATE)
    // Variable(s) for storing SUID array index/tracker
    // Extra input storage
    SW_MPI_DESIGNATE *desig = &SW_Domain->SW_Designation;
    MPI_Datatype reqType = SW_Domain->datatypes[eSW_MPI_Req];
    MPI_Datatype logType = SW_Domain->datatypes[eSW_MPI_Log];
    MPI_Datatype weathHistType = SW_Domain->datatypes[eSW_MPI_WeathHist];
    MPI_Datatype inputType = SW_Domain->datatypes[eSW_MPI_Inputs];
    int progFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCprog];
    int progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprog];
    int input = 0;
    int numSuidsTot = desig->nCompProcs * N_SUID_ASSIGN;
    int numIterSuids;
    Bool *useIndexFile = SW_Domain->netCDFInput.useIndexFile;
    Bool **readInVars = SW_Domain->netCDFInput.readInVars;
    Bool constSoilDepths = SW_Domain->hasConsistentSoilLayerDepths;
    int dim;
    int dimSum = 0;
    Bool sendEstVeg = swTRUE;
    Bool estVeg = SW_Domain->netCDFInput.readInVars[eSW_InVeg][0];
    Bool readWeather = SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];
    Bool readSoils = SW_Domain->netCDFInput.readInVars[eSW_InSoil][0];
    Bool spatialVars1D = swFALSE;
    int nSuids = desig->nSuids;
    const int oneDimSum = -4;
    int inputsLeft = nSuids;
    int n_years = sw->WeatherIn.n_years;
    Bool allocSoils = (Bool) (!constSoilDepths && readSoils);
    int numIterations = 0;
    int succFlagWrite;
    int distSUIDWrite;
    int iterNumSuids = 0;
    int suid;

    SW_RUN_INPUTS *inputs = NULL;
    int *sendInputs = NULL;
    int numWrites[SW_NINKEYSNC] = {0};
    size_t **starts[SW_NINKEYSNC] = {NULL};
    size_t **counts[SW_NINKEYSNC] = {NULL};
    size_t **distSUIDs = NULL; // Subset of all assigned suids (domain)
    size_t **tempDistSuids = NULL;
    size_t **distTSUIDs[SW_NINKEYSNC] = {NULL};
    double *elevations = NULL;
    double *tempMonthlyVals = NULL;
    double *tempSilt = NULL;
    double *tempSoilVals = NULL;
    double *tempWeather = NULL;
    SW_SOIL_RUN_INPUTS *tempSoils = NULL;
    Bool *succFlags = NULL;
    LOG_INFO *logs = NULL;
    Bool errorCaused = swFALSE;
    size_t temp;

    // Determine if spatial variables are one- or two-dimensional
    // This impacts the order of "start" and "count" values for spatial key
    for (dim = 0; dim < MAX_NDIMS; dim++) {
        dimSum += SW_Domain->netCDFInput.dimOrderInVar[eSW_InSpatial][1][dim];
    }
    spatialVars1D = (Bool) (dimSum == oneDimSum);

    alloc_IO_info(
        numSuidsTot,
        desig->nCompProcs,
        useIndexFile,
        readInVars,
        &SW_Domain->OutDom,
        &sw->OutRun,
        starts,
        counts,
        &distSUIDs,
        distTSUIDs,
        &tempDistSuids,
        &sendInputs,
        &succFlags,
        &logs,
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto checkStatus;
    }

    alloc_inputs(
        &sw->RunIn,
        numSuidsTot,
        readWeather,
        n_years,
        allocSoils,
        &elevations,
        &tempMonthlyVals,
        &tempSilt,
        &tempSoilVals,
        &tempWeather,
        &tempSoils,
        &inputs,
        LogInfo
    );

checkStatus:
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        goto freeMem;
    }
    *setupFail = swFALSE;

    /* Set up interrupt handlers so if the program is interrupted
    during simulation, we can exit smoothly and not abruptly */
    (void) signal(SIGINT, handle_interrupt);
    (void) signal(SIGTERM, handle_interrupt);

    // Loop until all SUIDs have been assigned or an interrupt occurs
    for (input = 0; input < nSuids && runSims;) {
        numIterSuids =
            (input + numSuidsTot > nSuids) ? nSuids - input : numSuidsTot;
        iterNumSuids += numIterSuids;
        succFlagWrite = numIterations * desig->nCompProcs * N_SUID_ASSIGN;
        distSUIDWrite = succFlagWrite;

        numIterations = (numIterations + 1) % N_ITER_BEFORE_OUT;

        if (numIterations == 0) {
            for (suid = 0; suid < numIterSuids; suid++) {
                tempDistSuids[suid][0] = distSUIDs[suid][0];
                tempDistSuids[suid][1] = distSUIDs[suid][1];
            }
        }

        get_next_suids(
            desig,
            numIterSuids,
            input,
            useIndexFile,
            readInVars,
            &distSUIDs[distSUIDWrite],
            distTSUIDs
        );

        // Calculate contiguous start/end/num writes for each input
        // key including plain domain SUIDs (index 0);
        // If the input domain is the same as the program's, copy the
        // starts, counts, and num writes to the input key
        calculate_contiguous_allkeys(
            numIterSuids,
            desig->nSuids,
            distSUIDWrite,
            useIndexFile,
            readInVars,
            distSUIDs,
            distTSUIDs,
            SW_Domain->netCDFInput.siteDoms,
            spatialVars1D,
            numWrites,
            starts,
            counts
        );

        // Do not attempt to read inputs because I/O process needs
        // to let it's other processes know that there was an error
        if (!LogInfo->stopRun) {
            SW_NCIN_read_inputs(
                sw,
                SW_Domain,
                NULL,
                starts,
                counts,
                SW_Domain->SW_PathInputs.openInFileIDs,
                numWrites,
                numIterSuids,
                tempMonthlyVals,
                elevations,
                tempSilt,
                tempSoilVals,
                tempWeather,
                tempSoils,
                inputs,
                LogInfo
            );
            if (LogInfo->stopRun) {
                errorCaused = swTRUE;
            }
        }

        // Make sure all processes did not throw a fatal error
        // before continuing
        if (SW_MPI_setup_fail(LogInfo->stopRun, desig->ioCompComm)) {
            goto freeMem;
        }

        // Distribute N_SUID_ASSIGN amount of run inputs to compute processes
        spread_inputs(
            desig,
            inputType,
            weathHistType,
            inputs,
            inputsLeft,
            estVeg,
            &sendEstVeg,
            readWeather,
            sendInputs,
            n_years,
            swFALSE
        );

        // Check if we need to read the next batch of inputs &
        // Check if outputs are full
        if (numIterations == 0 && input > 0) {
            // Get multi-iteration start/count values for writing output
            // in as little contiguous writes as possible

            // The way this function is written currently makes it so that
            // by the time we write output, the first
            // N_SUID_ASSIGN SUIDs will be over written, so we need
            // to temporarily swap the newly written N_SUID_ASSIGN SUIDs
            // with the previous SUIDs
            for (suid = 0; suid < numIterSuids; suid++) {
                temp = tempDistSuids[suid][0];
                tempDistSuids[suid][0] = distSUIDs[suid][0];
                distSUIDs[suid][0] = temp;

                temp = tempDistSuids[suid][1];
                tempDistSuids[suid][1] = distSUIDs[suid][1];
                distSUIDs[suid][1] = temp;
            }

            write_outputs(
                progFileID,
                progVarID,
                distSUIDs,
                iterNumSuids - numIterSuids,
                SW_Domain->netCDFInput.siteDoms[eSW_InDomain],
                &SW_Domain->OutDom,
                succFlags,
                &sw->SW_PathOutputs,
                starts[eSW_InDomain],
                counts[eSW_InDomain],
                sw->OutRun.p_OUT,
                LogInfo
            );
            if (LogInfo->stopRun) {
                errorCaused = swTRUE;
            }

            for (suid = 0; suid < numIterSuids; suid++) {
                distSUIDs[suid][0] = tempDistSuids[suid][0];
                distSUIDs[suid][1] = tempDistSuids[suid][1];
            }

            iterNumSuids = numIterSuids;
        }

        // Loop through all requests sent by the compute process
        // or we haven't received input from all processes
        // Get dynamically allocated output memory from
        // each compute process if simulations were successful
        get_comp_results(
            desig->nCompProcs,
            sendInputs,
            reqType,
            logType,
            desig,
            &SW_Domain->OutDom,
            logs,
            LogInfo->logfps,
            &succFlags[succFlagWrite],
            sw->OutRun.p_OUT
        );

        input += numSuidsTot;
        inputsLeft -= numSuidsTot;
    }

    if (runSims) {
        if (SW_MPI_setup_fail(LogInfo->stopRun, desig->ioCompComm)) {
            goto freeMem;
        }

        // Make sure no compute process gets stuck waiting for input
        spread_inputs(
            desig,
            inputType,
            weathHistType,
            inputs,
            inputsLeft,
            estVeg,
            &sendEstVeg,
            readWeather,
            sendInputs,
            n_years,
            swTRUE
        );

        if ((numIterations == 0 && input == numSuidsTot) ||
            numIterations < N_ITER_BEFORE_OUT) {

            write_outputs(
                progFileID,
                progVarID,
                distSUIDs,
                iterNumSuids - numIterSuids,
                SW_Domain->netCDFInput.siteDoms[eSW_InDomain],
                &SW_Domain->OutDom,
                succFlags,
                &sw->SW_PathOutputs,
                starts[eSW_InDomain],
                counts[eSW_InDomain],
                sw->OutRun.p_OUT,
                LogInfo
            );
        }
    }

freeMem:
#if defined(SOILWAT)
    if (runSims == 0) {
        SW_MSG_ROOT("Program was killed early. Shutting down...", rank);
    }
#endif

    dealloc_IO_info(
        numSuidsTot,
        &sw->OutRun,
        starts,
        counts,
        &distSUIDs,
        distTSUIDs,
        &tempDistSuids,
        &sendInputs,
        &elevations,
        &tempMonthlyVals,
        &tempSilt,
        &tempSoilVals,
        &tempSoils,
        &succFlags,
        &logs
    );

    if (errorCaused || !runSims) {
        // Report error
        if (LogInfo->stopRun) {
            LogInfo->logfp = LogInfo->logfps[0];
            sw_write_warnings("\n", LogInfo);

            SW_MPI_Fail(SW_MPI_FAIL_NETCDF, 0, NULL);
        }
    }
}
