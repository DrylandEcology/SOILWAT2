
#include <stdlib.h>
#include <stdio.h>
#include <netcdf.h>
#include <string.h>
#include <math.h>

void open_nc(const char *fileName, int *ncID);
void get_nc_dims(int ncFileID, size_t *x, size_t *y);
void allocate_dom_info(size_t lonSize, size_t latSize, int ***domVals);
void deallocate_dom_info(size_t latSize, int ***domVals);
void read_all_domain_vals(int ncFileID, const char *varName, int **domVals);
void replace_vals_to_1_and_0(
    size_t latSize, size_t lonSize, int **domVals, size_t *nActiveSites
);
