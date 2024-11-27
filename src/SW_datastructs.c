/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_datastructs.h"
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/rands.h"

#include <stdlib.h>
#include <string.h>

#define WITHIN_GRID 0
#define TOP 1
#define RIGHT 2
#define BOTTOM 3
#define LEFT 4

#define TOP_LEFT 5
#define TOP_RIGHT 6
#define BOTTOM_LEFT 7
#define BOTTOM_RIGHT 8

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Compare the longitude/x coordinates of two coordinate pairs
and give the result (-1 - elemOne < elemTwo, 1 - elemOne > elemTwo)

@param[in] elemOne A list containing the first longitude/x element to compare
@param[in] elemTwo A list containing the second longitude/x element to compare
@param[out] res Numerical description of the comparison between the two
elements
*/
static void compareElemX(double *elemOne, double *elemTwo, int *res) {
    *res = 0;

    if (!EQ(elemOne[1], elemTwo[1])) {
        *res = (LT(elemOne[1], elemTwo[1])) ? -1 : 1;
    }
}

/**
@brief Compare the latitude/y coordinates of two coordinate pairs
and give the result (-1 - elemOne < elemTwo, 1 - elemOne > elemTwo)

@param[in] elemOne A list containing the first latitude/y element to compare
@param[in] elemTwo A list containing the second latitude/y element to compare
@param[out] res Numerical description of the comparison between the two
elements
*/
static void compareElemY(double *elemOne, double *elemTwo, int *res) {
    *res = 0;

    if (!EQ(elemOne[0], elemTwo[0])) {
        *res = (LT(elemOne[0], elemTwo[0])) ? -1 : 1;
    }
}

/**
@brief Swap coordinate arrays within the bigger list of them

@param[in,out] locOne First location of an index array to be swapped
@param[in,out] locTwo Second location of an index array to be swapped
*/
static void swapCoords(double **locOne, double **locTwo) {
    double *temp = *locOne;
    *locOne = *locTwo;
    *locTwo = temp;
}

/**
@brief Swap index arrays within the bigger list of them

@param[in,out] locOne First location of an index array to be swapped
@param[in,out] locTwo Second location of an index array to be swapped
*/
static void swapIndices(unsigned int **locOne, unsigned int **locTwo) {
    unsigned int *temp = *locOne;
    *locOne = *locTwo;
    *locTwo = temp;
}

/**
@brief Get the pivot point within a chunk of list to sort while sorting
the specified chunk within the array

This function uses Hoare's partition scheme for using a pivot point that
is in the center of the specified chunk

@param[out] coords List of coordinate pairs to sort based on one of
the two dimensions
@param[out] indices List of indices in which the coordinates exist within the
input files that will be sorted with the coordinates to match coordinates
to the coorect indices
@param[in] left Lower bound of the sorting while recursing through the
function
@param[in] right Upper bound of the sorting while recursing through the
function
@param[in,out] lIndex Lower pointer (index) that will be used to swap
elements given a center pivot point
@param[in,out] rIndex Upper pointer (index) that will be used to swap
elements given a center pivot point
@param[in] compFunc Dynamic function pointer to say which dimension to compare
within the coordinate pairs
*/
static void get_pivot(
    double **coords,
    unsigned int **indices,
    int left,
    int right,
    int *lIndex,
    int *rIndex,
    void (*compFunc)(const double *, const double *, int *)
) {
    int pivot = left + (right - left) / 2;
    double *pivotVal = coords[pivot];
    int compResL;
    int compResR;

    while (*lIndex < *rIndex) {
        compFunc(coords[*lIndex], pivotVal, &compResL);
        compFunc(coords[*rIndex], pivotVal, &compResR);

        while (compResL < 0) {
            (*lIndex)++;
            compFunc(coords[*lIndex], pivotVal, &compResL);
        }

        while (compResR > 0) {
            (*rIndex)--;
            compFunc(coords[*rIndex], pivotVal, &compResR);
        }

        if (*lIndex < *rIndex) {
            swapCoords(&coords[*lIndex], &coords[*rIndex]);
            swapIndices(&indices[*lIndex], &indices[*rIndex]);

            (*lIndex)++;
            (*rIndex)--;
        }
    }
}

/**
@brief Recursively sort a given chunk of values within a coordinate pair
list by using the quick sort algorithm

@param[out] coords List of coordinate pairs to sort based on one of
the two dimensions
@param[out] indices List of indices in which the coordinates exist within the
input files that will be sorted with the coordinates to match coordinates
to the coorect indices
@param[in] left Lower bound of the sorting while recursing through the
function
@param[in] right Upper bound of the sorting while recursing through the
function
@param[in] compFunc Dynamic function pointer to tell the pivot function
(`get_pivot()`) which dimension to compare within the coordinate pairs
*/
// NOLINTBEGIN(misc-no-recursion)
static void quickSort(
    double **coords,
    unsigned int **indices,
    int left,
    int right,
    void (*compFunc)(const double *, const double *, int *)
) {
    int lIndex = left;
    int rIndex = right;

    if (right > left) {
        get_pivot(coords, indices, left, right, &lIndex, &rIndex, compFunc);

        quickSort(coords, indices, left, rIndex, compFunc);
        quickSort(coords, indices, rIndex + 1, right, compFunc);
    }
}

// NOLINTEND(misc-no-recursion)

/**
@brief Helper function to `createNode()` to copy the data to be added into the
tree to the newly created node

@param[in] src_coord Coordinate list to copy from
@param[in] src_indices Indices list to copy from
@param[out] dest_coord Coordinate list to copy into
@param[out] dest_indices Indices list to copy into
@param[in] src_maxDist Source maximum allowed distance from a node that is
on the edge of the domain
@param[in] dest_maxDist Destination maximum allowed distance from a node that
is on the edge of the domain
*/
static void copyData(
    const double src_coord[],
    const unsigned int src_indices[],
    const double src_maxDist,
    double dest_coord[],
    unsigned int dest_indices[],
    double *dest_maxDist
) {
    int index;

    for (index = 0; index < KD_NDIMS; index++) {
        dest_coord[index] = src_coord[index];
    }

    for (index = 0; index < KD_NINDICES; index++) {
        dest_indices[index] = src_indices[index];
    }

    *dest_maxDist = src_maxDist;
}

/**
@brief Create a new KD-tree node and fill it with the provided coordinates
and indices

@param[in] coords Coordinate to be added into the KD-tree
@param[in] indices List of indices in which the coordinates exist within the
input files
@param[out] LogInfo Holds information dealing with logfile output
*/
static SW_KD_NODE *createNode(
    double coords[], const unsigned int indices[], double maxDist, LOG_INFO *LogInfo
) {
    SW_KD_NODE *newNode = NULL;

    newNode =
        (SW_KD_NODE *) Mem_Malloc(sizeof(SW_KD_NODE), "createNode()", LogInfo);

    if (!LogInfo->stopRun) {
        copyData(
            coords,
            indices,
            maxDist,
            newNode->coords,
            newNode->indices,
            &newNode->maxDist
        );
        newNode->left = newNode->right = NULL;
    }

    return newNode;
}

/**
@brief Calculate the distance between two coordinate pairs

@note If the primary nc CRS is Geographical, we use Euclidean distance
calculations, otherwise, the primary CRS is projected and this function will use
the Great-circle distance calculation

@param[in] coordsOne Current coordinate pair
@param[in] coordsTwo Coordinate pair that we want to know the distance from the
current primary coordinate pair (coordsOne)
@param[in] primCRSIsGeo Specifies if the current primary CRS is Geographical

@return Distance between the two provided points
*/

// NOLINTBEGIN(misc-no-recursion)
static double calcDistance(
    const double coordsOne[], const double coordsTwo[], Bool primCRSIsGeo
) {
    double result = 0.0;
    double yDiff;
    double xDiff;
    double yMean;
    double K1;
    double K2;
    double cosRes;
    double cosResSquared;
    double cosResCubed;
    double T1;
    double T2;
    double T3;
    double T4;
    double T5;
    int point;

    if (primCRSIsGeo) {
        /* FCC distance calculation (<= 475 kilometers)
           with Chebyshev Polynomials to replace
           cos(x * yMean) where 1 <= x <= 5 (T1 through T5),
           in other words, Tn of cos(x) is the same as cos(nx) */
        yDiff = (coordsTwo[0] - coordsOne[0]);
        xDiff = (coordsTwo[1] - coordsOne[1]);
        yMean = ((coordsTwo[0] + coordsOne[0]) / 2) * deg_to_rad;
        cosRes = cos(yMean);
        cosResSquared = pow(cosRes, 2);
        cosResCubed = pow(cosRes, 3);

        T1 = cosRes;
        T2 = (2 * cosResSquared) - 1;
        T3 = (4 * cosResCubed) - (3 * cosRes);
        T4 = (8 * pow(cosRes, 4)) - (8 * cosResSquared) + 1;
        T5 = (16 * pow(cosRes, 5)) - (20 * cosResCubed) + (5 * cosRes);

        K1 = 111.13209 - (0.56605 * T2) + (0.00120 * T4);
        K2 = (111.41513 * T1) - (0.09455 * T3) + (0.00012 * T5);

        result = (K1 * yDiff) * (K1 * yDiff);
        result += ((K2 * xDiff) * (K2 * xDiff));
    } else {
        /* Euclidean distance calculation */
        for (point = 0; point < KD_NDIMS; point++) {
            result += (coordsTwo[point] - coordsOne[point]) *
                      (coordsTwo[point] - coordsOne[point]);
        }
    }

    return result;
}

// NOLINTEND(misc-no-recursion)

/**
@brief Calculate the maximum allowed distance outside of the grid
given edge node coordinates

@param[in] yCoords Individual latitude/y coordiantes
@param[in] xCoords Individual longitude/x coordiantes
@param[in] location Location gathered from `getCoordLocInGrid`
@param[in] indices A list of all the indices where each coordinate comes from
within the coordinate variables
@param[in] inPrimCRSIsGeo Specifies if the input coordinates from from a domain
that has a primary CRS of geographical

@return The maximum possibly distance outside the grid to be considered
"within" in
*/
static double calcMaxNodeDist(
    const double *yCoords,
    const double *xCoords,
    int location,
    const unsigned int indices[],
    Bool inPrimCRSIsGeo
) {
    double maxDist = -1.0;
    double tempDist = 0.0;
    long yIndexOffset = 0UL;
    long xIndexOffset = 0UL;
    double calcCoords[] = {yCoords[indices[0]], xCoords[indices[1]]};

    double neighborCoord[2] = {0}; /* [yCoord, xCoord] */

    if (location >= TOP_LEFT) {
        switch (location) {
        case TOP_LEFT:
            yIndexOffset = 1;
            xIndexOffset = 1;
            break;
        case TOP_RIGHT:
            yIndexOffset = 1;
            xIndexOffset = -1;
            break;
        case BOTTOM_LEFT:
            yIndexOffset = -1;
            xIndexOffset = 1;
            break;
        default: /* BOTTOM_RIGHT */
            yIndexOffset = -1;
            xIndexOffset = -1;
            break;
        }

        neighborCoord[0] = yCoords[indices[0] + yIndexOffset];
        neighborCoord[1] = xCoords[indices[1] + xIndexOffset];
    } else {
        if (location == LEFT || location == RIGHT) {
            xIndexOffset = (location == LEFT) ? 1 : -1;
            yIndexOffset = 1;
        } else { /* Location is TOP or BOTTOM */
            xIndexOffset = 1;
            yIndexOffset = (location == TOP) ? 1 : -1;
        }

        neighborCoord[0] = yCoords[indices[0] + yIndexOffset];
        neighborCoord[1] = xCoords[indices[1] + xIndexOffset];

        tempDist = calcDistance(neighborCoord, calcCoords, inPrimCRSIsGeo);

        if (location == LEFT || location == RIGHT) {
            yIndexOffset = -1;
        } else { /* Location is TOP or BOTTOM */
            xIndexOffset = -1;
        }

        neighborCoord[0] = yCoords[indices[0] + yIndexOffset];
        neighborCoord[1] = xCoords[indices[1] + xIndexOffset];
    }

    maxDist = calcDistance(neighborCoord, calcCoords, inPrimCRSIsGeo);
    maxDist = (GT(tempDist, maxDist)) ? tempDist : maxDist;

    return maxDist;
}

/**
@brief If the coordinates come from a grid domain, we need to know where
the location of the coordinates within the grid are, mainly we care about
if it the coordinate pair is on the edge of the grid to later calculate
the maximum distance allowed for a coordinate pair to be outside of the
grid

@param[in] yIndex Current latitude/y coordinate
@param[in] xIndex Current longitude/x coordinate
@param[in] ySize Size of the latitude/y dimension
@param[in] xSize Size of the longitude/x dimension
@param[out] posLocation Resulting position within the grid, default value is
`WITHIN_GRID`
*/
static void getCoordLocInGrid(
    const unsigned int yIndex,
    const unsigned int xIndex,
    const size_t ySize,
    const size_t xSize,
    int *posLocation
) {
    Bool onYEdge = (Bool) (yIndex == 0 || yIndex == ySize - 1);
    Bool onXEdge = (Bool) (xIndex == 0 || xIndex == xSize - 1);

    if (onXEdge && onYEdge) {
        if (xIndex == 0) {
            *posLocation = (yIndex == 0) ? TOP_LEFT : BOTTOM_LEFT;
        } else {
            *posLocation = (yIndex == 0) ? TOP_RIGHT : BOTTOM_RIGHT;
        }
    } else if (onXEdge) {
        *posLocation = (xIndex == 0) ? LEFT : RIGHT;
    } else if (onYEdge) {
        *posLocation = (yIndex == 0) ? TOP : BOTTOM;
    }
}

/**
@brief Construct a KD-tree, that holds the latitude/y and longitude/x
coordinate pairs to find the nearest neighbor to the programs spatial
domain; construct the tree determinisitically instead of randomly
putting points into the tree

@param[in] yxCoords Preconstructed coordinate pairs to add into the tree
@param[in] yCoords Individual latitude/y coordiantes
@param[in] xCoords Individual longitude/x coordiantes
@param[in] left Lower index of the coordinate pair list to sort/recursively
construct with
@param[in] right Upper index of the coordinate pair list to sort/recursively
construct with
@param[in] numY Number of unique latitude/y coordinates that exist in the
coordinate pairs, or in other words, size of the latitude/y dimension when
reading the input file coordinate variables
@param[in] numX Number of unique longitude/x coordinates that exist in the
coordinate pairs, or in other words, size of the longitude/x dimension when
reading the input file coordinate variables
@param[in] depth Recursive depth/level the tree is adding to which also
specifies the dimension of the coordinates we sort by/compare
@param[in] inIsGridded Specifies if the input coordinates come from a gridded
domain
@param[in] inPrimCRSIsGeo Specifies if the input coordinates from from a domain
that has a primary CRS of geographical
@param[in] indices A list of all the indices where each coordinate comes from
within the coordinate variables
@param[out] LogInfo Holds information dealing with logfile output

@return Left or right child node to the previous recursive call, if there is
no parent node, it is the root node of the entire tree (at depth 0)
*/
// NOLINTBEGIN(misc-no-recursion)
static SW_KD_NODE *constructTree(
    double **yxCoords,
    double *yCoords,
    double *xCoords,
    int left,
    int right,
    size_t numY,
    size_t numX,
    int depth,
    Bool inIsGridded,
    Bool inPrimCRSIsGeo,
    unsigned int **indices,
    LOG_INFO *LogInfo
) {
    SW_KD_NODE *newNode = NULL;

    int middle = left + (right - left) / 2;
    double maxDist = -1;
    int posLocation = WITHIN_GRID;
    void (*compFunc)(const double *, const double *, int *) =
        (depth % 2 == 0) ?
            (void (*)(const double *, const double *, int *)) compareElemY :
            (void (*)(const double *, const double *, int *)) compareElemX;

    if (left > right) {
        return NULL;
    }

    quickSort(yxCoords, indices, left, right, compFunc);

    if (inIsGridded) {
        getCoordLocInGrid(
            indices[middle][0], indices[middle][1], numY, numX, &posLocation
        );

        if (posLocation != WITHIN_GRID) {
            maxDist = calcMaxNodeDist(
                yCoords, xCoords, posLocation, indices[middle], inPrimCRSIsGeo
            );
        }
    }

    if (inPrimCRSIsGeo) {
        /* Convert the possibility of [0, 360] to [-180, 180] before
        inserting into the KD-tree */
        yxCoords[middle][1] = fmod(180.0 + yxCoords[middle][1], 360.0) - 180.0;
    }

    newNode = createNode(yxCoords[middle], indices[middle], maxDist, LogInfo);

    newNode->left = constructTree(
        yxCoords,
        yCoords,
        xCoords,
        left,
        middle - 1,
        numY,
        numX,
        depth + 1,
        inIsGridded,
        inPrimCRSIsGeo,
        indices,
        LogInfo
    );

    newNode->right = constructTree(
        yxCoords,
        yCoords,
        xCoords,
        middle + 1,
        right,
        numY,
        numX,
        depth + 1,
        inIsGridded,
        inPrimCRSIsGeo,
        indices,
        LogInfo
    );

    return newNode;
}

// NOLINTEND(misc-no-recursion)

/**
@brief Allocate memory for a list of coordinate pairs

@param[out] coords A list that will be filled with constructed coordinate pairs
given lat/lon or y/x coordinates
@param[out] indices A list that will be filled with indices from
the given input file's closest spatial points to the programs domain spatial
points
@param[in] numPoints Number of coordinate pairs to be accounted (allocated) for
@param[out] LogInfo Holds information dealing with logfile output
*/
static void alloc_coords_indices(
    double ***coords,
    unsigned int ***indices,
    size_t numPoints,
    LOG_INFO *LogInfo
) {
    size_t pair;

    *coords = (double **) Mem_Malloc(
        sizeof(double *) * numPoints, "alloc_coords_indices()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    *indices = (unsigned int **) Mem_Malloc(
        sizeof(unsigned int *) * numPoints, "alloc_coords_indices()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    for (pair = 0; pair < numPoints; pair++) {
        (*coords)[pair] = NULL;
        (*indices)[pair] = NULL;
    }

    for (pair = 0; pair < numPoints; pair++) {
        (*coords)[pair] = (double *) Mem_Malloc(
            sizeof(double) * 2, "alloc_coords_indices()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }

        (*coords)[pair][0] = 0.0;
        (*coords)[pair][1] = 0.0;

        (*indices)[pair] = (unsigned int *) Mem_Malloc(
            sizeof(unsigned int) * 2, "alloc_coords_indices()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }

        (*indices)[pair][0] = 0;
        (*indices)[pair][1] = 0;
    }
}

/**
@brief Deallocate structured coordinates used for creating a KD-tree

@param[in] numPoints Number of coordinate pairs
@param[out] coords Coordinate lists to be freed
@param[out] indices Index lists to be freed
*/
static void dealloc_coords_indices(
    size_t numPoints, double ***coords, unsigned int ***indices
) {
    size_t pair;

    for (pair = 0; pair < numPoints; pair++) {
        if (!isnull(*coords) && !isnull((*coords)[pair])) {
            free((void *) (*coords)[pair]);
            (*coords)[pair] = NULL;
        }

        if (!isnull(*indices) && !isnull((*indices)[pair])) {
            free((void *) (*indices)[pair]);
            (*indices)[pair] = NULL;
        }
    }

    if (!isnull(*coords)) {
        free((void *) *coords);
        *coords = NULL;
    }

    if (!isnull(*indices)) {
        free((void *) *indices);
        *indices = NULL;
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Recursively query a built KD-tree for the nearest neighbor
of a given coordinate pair that is within a maximum reasonable distance

@param[in] currNode Current node within the tree that is in question
@param[in] queryCoords Coordinates that the search is trying to find the
nearest neighbor of
@param[in] level Current level within the KD-tree
@param[in] primCRSIsGeo Specifies if the current CRS type is geographic
@param[out] bestNode Node of the nearest neighbor to the query coordinates
@param[in,out] bestDist Current best distance while searching
*/
// NOLINTBEGIN(misc-no-recursion)
void SW_DATA_queryTree(
    SW_KD_NODE *currNode,
    double queryCoords[],
    int level,
    Bool primCRSIsGeo,
    SW_KD_NODE **bestNode,
    double *bestDist
) {
    Bool wentLeft = swFALSE;
    int inspectIndex = level % KD_NDIMS;
    double oppDist = DBL_MAX;
    double *oppCoords = NULL;
    double currDist;

    if (isnull(currNode)) {
        return;
    }

    Bool goLeft =
        (Bool) LT(queryCoords[inspectIndex], currNode->coords[inspectIndex]);

    if (goLeft) {
        wentLeft = swTRUE;
        SW_DATA_queryTree(
            currNode->left,
            queryCoords,
            level + 1,
            primCRSIsGeo,
            bestNode,
            bestDist
        );
    } else {
        SW_DATA_queryTree(
            currNode->right,
            queryCoords,
            level + 1,
            primCRSIsGeo,
            bestNode,
            bestDist
        );
    }

    currDist = calcDistance(currNode->coords, queryCoords, primCRSIsGeo);

    if (LT(fabs(currDist), *bestDist) &&
        (EQ(currNode->maxDist, -1.0) || LE(currDist, currNode->maxDist))) {
        *bestNode = currNode;
        *bestDist = currDist;
    }

    /* Check to see if the other child branch holds a value that's closer
       than the current best option */
    oppCoords = (wentLeft) ? currNode->right->coords : currNode->left->coords;

    if (!isnull(oppCoords)) {
        oppDist = calcDistance(oppCoords, queryCoords, primCRSIsGeo);

        if (LT(oppDist, *bestDist)) {
            if (wentLeft) {
                SW_DATA_queryTree(
                    currNode->right,
                    queryCoords,
                    level + 1,
                    primCRSIsGeo,
                    bestNode,
                    bestDist
                );
            } else {
                SW_DATA_queryTree(
                    currNode->left,
                    queryCoords,
                    level + 1,
                    primCRSIsGeo,
                    bestNode,
                    bestDist
                );
            }
        }
    }
}

// NOLINTEND(misc-no-recursion)

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Recursively destroy the construct KD-tree

@param[in] currNode Current node within the tree being freed

@return Reference to the freed node (NULL)
*/
// NOLINTBEGIN(misc-no-recursion)
SW_KD_NODE *SW_DATA_destroyTree(SW_KD_NODE *currNode) {
    if (!isnull(currNode)) {
        currNode->left = SW_DATA_destroyTree(currNode->left);
        currNode->right = SW_DATA_destroyTree(currNode->right);

        free(currNode);
    }

    return NULL;
}

// NOLINTEND(misc-no-recursion)

/**
@brief Recursively add a new node into the KD-tree

@param[in] currNode Reference to the current node being examined
while adding the new information into the tree
@param[in] coords List of lat/lon or y/x coordinates to add into the tree
@param[in] indices List of indices in which the coordinates exist within the
input files
@param[in] level Current level within the tree
@param[in] maxDist Calculated maximum allowed distance from a node that is
on the edge of the domain
@param[out] LogInfo Holds information dealing with logfile output

@return Reference to the left or right child node of the parent
*/
// NOLINTBEGIN(misc-no-recursion)
SW_KD_NODE *SW_DATA_addNode(
    SW_KD_NODE *currNode,
    double coords[],
    const unsigned int indices[],
    double maxDist,
    int level,
    LOG_INFO *LogInfo
) {
    int inspectIndex = level % KD_NDIMS;

    if (!LogInfo->stopRun) {
        if (isnull(currNode)) {
            return createNode(coords, indices, maxDist, LogInfo);
        }
        if (LT(coords[inspectIndex], currNode->coords[inspectIndex])) {
            currNode->left = SW_DATA_addNode(
                currNode->left, coords, indices, maxDist, level + 1, LogInfo
            );
        } else {
            currNode->right = SW_DATA_addNode(
                currNode->right, coords, indices, maxDist, level + 1, LogInfo
            );
        }
    }

    return currNode;
}

// NOLINTEND(misc-no-recursion)

/**
@brief Construct a KD-tree given lat/lon or y/x coordinates

@param[out] treeRoot Root of the KD-tree that will be constructed
@param[in] yCoords Latitude or y coordinates from input files
@param[in] xCoords Longitude or x coordinates from input files
@param[in] ySize Amount of latitude or y coordinates being provided
@param[in] xSize Amount of longitude or x coordinates being provided
@param[in] inIsGridded Specifies that the provided input file is gridded
@param[in] has2DCoordVars Specifies if the coordinates that are being sent in
originated from coordinate variables that we 2D, i.e., in matrix form
@param[in] inPrimCRSIsGeo Specifies if the current CRS type is geographic
@param[in] yxConvs A list of two unit converters for the coordinate variables
(one for y and one for x)
@param[out] LogInfo Holds information dealing with logfile output
*/
void SW_DATA_create_tree(
    SW_KD_NODE **treeRoot,
    double *yCoords,
    double *xCoords,
    size_t ySize,
    size_t xSize,
    Bool inIsGridded,
    Bool has2DCoordVars,
    Bool inPrimCRSIsGeo,
    sw_converter_t *yxConvs[],
    LOG_INFO *LogInfo
) {
    double **coordPairs = NULL;
    size_t yIndex;
    size_t xIndex;
    size_t numPoints = ySize * xSize;
    size_t pair = 0;
    unsigned int **indices = NULL;

    alloc_coords_indices(&coordPairs, &indices, numPoints, LogInfo);
    if (LogInfo->stopRun) {
        goto freeInfo; /* Exit function prematurely due to error */
    }

    /* Check to see if the input coordinate variables are already
       in matrix form;
       If they are, we simply need to pair the coordinates one-to-one;
       Otherwise, we need to construct the grid from the coordinates
       meaning all coordinate pairs should be created */
    for (yIndex = 0; yIndex < ySize; yIndex++) {
        for (xIndex = 0; xIndex < xSize; xIndex++) {
#if defined(SWNETCDF) && defined(SWUDUNITS)
            if (!inPrimCRSIsGeo) {
                if (!isnull(yxConvs[0]) && xIndex == 0) {
                    yCoords[yIndex] =
                        cv_convert_double(yxConvs[0], yCoords[yIndex]);
                }

                if (!isnull(yxConvs[1]) && yIndex == 0) {
                    xCoords[xIndex] =
                        cv_convert_double(yxConvs[1], xCoords[xIndex]);
                }
            }
#endif
            if (has2DCoordVars) {
                coordPairs[pair][0] = yCoords[pair];
                coordPairs[pair][1] = xCoords[pair];

                indices[pair][0] = pair / xSize;
                indices[pair][1] = pair % xSize;
            } else {
                indices[pair][0] = yIndex;
                indices[pair][1] = xIndex;

                coordPairs[pair][0] = yCoords[yIndex];
                coordPairs[pair][1] = xCoords[xIndex];
            }

            pair++;
        }
    }

    *treeRoot = constructTree(
        coordPairs,
        yCoords,
        xCoords,
        0,
        (int) numPoints - 1,
        ySize,
        xSize,
        0,
        inIsGridded,
        inPrimCRSIsGeo,
        indices,
        LogInfo
    );

freeInfo:
    dealloc_coords_indices(numPoints, &coordPairs, &indices);
}
