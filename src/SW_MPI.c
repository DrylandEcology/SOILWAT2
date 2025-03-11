#include "include/SW_MPI.h"

#include "include/filefuncs.h"

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
@brief Throughout the program, various data will need to be sent around,
much of which, MPI does not provide default support for (not integer, double,
etc.), so this function will create custom MPI types based on program-defined
structs

@param[out] datatypes A list of custom MPI datatypes that will be created based
on various program-defined structs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_create_types(MPI_Datatype datatypes[], LOG_INFO *LogInfo) {
    int res = MPI_SUCCESS;

    int numItems[] = {5, 5, 5, 6, 5};
    int blockLens[][6] = {
        {1, 1, 1, 1, 1},    /* SW_DOMAIN */
        {1, 1, 1, 1, 1},    /* SW_SPINUP */
        {1, 1, 1, 1, 1},    /* SW_RUN_INPUTS */
        {1, 1, 1, 1, 1, 1}, /* SW_MPI_WallTime */
        {SW_OUTNKEYS,
         SW_OUTNKEYS,
         SW_OUTNPERIODS,
         1,
         SW_OUTNKEYS * SW_OUTNPERIODS} /* SW_OUT_DOM */
    };

    MPI_Datatype types[][6] = {
        {MPI_INT, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED
        }, /* SW_DOMAIN */
        {MPI_UNSIGNED, MPI_UNSIGNED, MPI_INT, MPI_INT, MPI_UNSIGNED
        },  /* SW_SPINUP */
        {}, /* SW_MPI_INPUTS */
        {MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_DOUBLE,
         MPI_UNSIGNED_LONG,
         MPI_UNSIGNED_LONG}, /* SW_MPI_WallTime */
        {MPI_INT, MPI_INT, MPI_UNSIGNED_LONG, MPI_INT, MPI_INT} /* SW_OUT_DOM */
    };
    MPI_Aint offsets[][6] = {/* SW_DOMAIN */
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
                              offsetof(SW_OUT_DOM, timeSteps)}
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
        eSW_MPI_WallTime,
        eSW_MPI_OutDomIO
    };

    int type;
    int typeIndex;
    int runTypeIndex;
    int covIndex;
    const int numTypes = 5;
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

                        /* Create a second type for */
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
            res = MPI_Type_free(&inTypes[vegprodIndex][0]);
            if (res != MPI_SUCCESS) {
                goto reportFail;
            }
        }
    }

    /* Free run input sub types */
    for (runTypeIndex = 0; runTypeIndex < numRunInTypes; runTypeIndex++) {
        res = MPI_Type_free(&types[eSW_MPI_Inputs][runTypeIndex]);
        if (res != MPI_SUCCESS) {
            goto reportFail;
        }
    }

    // TODO: Create custom MPI type for designations
    // TODO: Create custom MPI type for I/O requests

reportFail:
    if (res != MPI_SUCCESS) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred when attempting to create custom MPI types."
        );
    }
}
