
#------------------------------------------------------------------------------#
# Create a set of nc-based SOILWAT2 test runs
#
#   * Comprehensive set of spatial configurations of simulation domains and
#     input netCDFs (see tests/ncTestRuns/data-raw/metadata_testRuns.csv)
#   * (if available) Runs with daily inputs from external data sources
#     including Daymet, gridMET, and MACAv2METDATA
#   * One site/grid cell in the simulation domain is set up to correspond
#     to the reference run (which is equal to the "example" at tests/example);
#     the output of that site/grid cell is compared against the reference output
#
#
# Run this script as follows (command-line arguments are optional)
# ```
#   Rscript \
#       Rscript__ncTestRuns_01_createTestRuns.R \
#       --path-to-ncTestRuns=<...> \
#       --path-to-sw2=<...> \
#       --testRuns=<...>
# ```
#------------------------------------------------------------------------------#

#------ Requirements ------
stopifnot(
  requireNamespace("RNetCDF", quietly = TRUE),
  requireNamespace("sf", quietly = TRUE),
  requireNamespace("rSOILWAT2", quietly = TRUE),
  requireNamespace("units", quietly = TRUE)
)


#------ . ------
#------ Grab command line arguments (if any)
args <- commandArgs(trailingOnly = TRUE)

reqTestRuns <- if (any(ids <- grepl("--testRuns", args))) {
  sub("--testRuns", "", args[ids]) |>
    sub("=", "", x = _) |>
    trimws() |>
    as.integer()
} else {
  -1L
}

#------ Paths (possibly as command-line arguments) ------
dir_prj <- if (any(ids <- grepl("--path-to-ncTestRuns", args))) {
  sub("--path-to-ncTestRuns", "", args[ids]) |>
    sub("=", "", x = _) |>
    trimws()
} else {
  ".."
}

fname_sw2 <- if (any(ids <- grepl("--path-to-sw2", args))) {
  sub("--path-to-sw2", "", args[ids]) |>
    sub("=", "", x = _) |>
    trimws()
} else {
  file.path(dir_prj, "..", "..", "bin", "SOILWAT2")
}

stopifnot(file.exists(fname_sw2))


dir_dataraw <- file.path(dir_prj, "data-raw")
stopifnot(dir.exists(dir_dataraw))

dir_swex <- file.path(dir_prj, "..", "example")
stopifnot(dir.exists(dir_swex))

dir_swintxt <- file.path(dir_swex, "Input")
stopifnot(dir.exists(dir_swintxt))

dir_swinnc <- file.path(dir_swex, "Input_nc")
stopifnot(dir.exists(dir_swinnc))

dir_R <- file.path(dir_prj, "R")
stopifnot(dir.exists(dir_R))

dir_results <- file.path(dir_prj, "results")
dir.create(dir_results, recursive = TRUE, showWarnings = FALSE)

dir_domainTemplates <- file.path(dir_results, "domainTemplates")
dir.create(dir_domainTemplates, recursive = TRUE, showWarnings = FALSE)

dir_testRunsTemplates <- file.path(dir_results, "testRunsTemplates")
dir.create(dir_testRunsTemplates, recursive = TRUE, showWarnings = FALSE)


#------ . ------
#------ Load functions ------
toggleNCInputTSV <- NULL
countDims <- NULL
countRuns <- NULL
exampleSite <- NULL
locateExampleSite <- NULL
getRunIDs <- NULL
copyInputTemplateNC <- NULL
copySW2Example <- NULL
createMonthClimatologyAxisNC <- NULL
createPFTAxisNC <- NULL
createTimeAxisNC <- NULL
createVarNC <- NULL
createVerticalAxisNC <- NULL
createTestRunSoils <- NULL
createTestRunData <- NULL
varySoilThicknessArray <- NULL
calcDepthArrayFromThickness <- NULL
getCRSParam <- NULL
readTSV <- NULL
runSW2 <- NULL
setNCInputTSV <- NULL
modifyNCUnitsTSV <- NULL
getModifiedNCUnits <- NULL
setTxtInput <- NULL
updateGAttSourceVersion <- NULL
updateGAttFeatureType <- NULL
updateGAttFrequency <- NULL
writeTSV <- NULL

res <- lapply(
  list.files(path = dir_R, pattern = ".R$", full.names = TRUE),
  source
)


#------ . ------
#------ Load data ------

#--- * Specifications of test runs ------
listTestRuns <- utils::read.csv(
  file = file.path(dir_dataraw, "metadata_testRuns.csv")
)

nTotalTestRuns <- nrow(listTestRuns)

if (reqTestRuns > 0L) {
  stopifnot(reqTestRuns <= nrow(listTestRuns))
  ids <- which(listTestRuns[["testrun"]] %in% reqTestRuns)
  listTestRuns <- listTestRuns[ids, , drop = FALSE]
}

#--- Domain size
listTestRuns[["nDomDims"]] <- countDims(
  domSizes = listTestRuns[["domainSize"]],
  isGridded = listTestRuns[["domainType"]] == "xy"
)

listTestRuns[["nDomRuns"]] <- countRuns(listTestRuns[["nDomDims"]])

#--- Input size
listTestRuns[["nInputDims"]] <- countDims(
  domSizes = listTestRuns[["inputSize"]],
  isGridded = listTestRuns[["inputType"]] == "xy"
)

listTestRuns[["nInputRuns"]] <- countRuns(listTestRuns[["nInputDims"]])


#--- Domain CRS
sw_crs <- list(
  geographic = list(
    crs = sf::st_crs("WGS84"),
    grid_mapping_name = "latitude_longitude",
    long_name = "WGS84",
    datum = "WGS84"
  ),
  # Daymet
  projected = list(
    crs = sf::st_crs(
      paste(
        "+proj=lcc +lat_1=25 +lat_2=60 +x_0=0 +y_0=0 +lat_0=42.5 +lon_0=-100",
        "+a=6378137 +f=0.00335281066474748 +pm=0 +no_defs +units=m"
      )
    ),
    grid_mapping_name = "lambert_conformal_conic",
    long_name = "North American Daymet",
    datum = "WGS84"
  )
)

sw_xyvars <- list(
  site = "site",
  geographic = c("lon", "lat"),
  projected = c("x", "y")
)


#--- Domain sites and bounding box

# 6 sites/locations
#   * include the location used by SOILWAT2/test/example
#   * sites are organized on a regular grid
#   * span 4 gridmet and 4 daymet cells
sw_examplesite <- lapply(
  sw_crs,
  function(x) exampleSite(x[["crs"]])
)

res_xy <- list(
  geographic = c(0.0072932096, 0.0077086729),
  projected = c(600, 900)
)

sw_sites <- lapply(
  stats::setNames(nm = names(sw_crs)),
  function(gm) {
    swesc <- sf::st_coordinates(sw_examplesite[[gm]])
    expand.grid(
      x = swesc[1L, 1L] + c(-1, 0, 1) * res_xy[[gm]][[1L]],
      y = swesc[1L, 2L] + c(-1, 0) * res_xy[[gm]][[2L]]
    ) |>
      data.matrix() |>
      sf::st_multipoint() |>
      sf::st_sfc(crs = sw_crs[[gm]][["crs"]]) |>
      sf::st_cast(to = "POINT")
  }
)

id_examplesite <- lapply(
  stats::setNames(nm = names(sw_crs)),
  function(gm) {
    locateExampleSite(sw_sites[[gm]])
  }
)

sw_grids <- lapply(
  stats::setNames(nm = names(sw_sites)),
  function(gm) {
    sf::st_make_grid(
      sw_sites[[gm]],
      cellsize = res_xy[[gm]],
      offset =
        sf::st_bbox(sw_sites[[gm]])[c("xmin", "ymin")] - res_xy[[gm]] / 2,
      what = "polygons",
      n = c(3, 2)
    )
  }
)

# sf::st_bbox(sw_grids[["geographic"]])
# c(
#   xmin = -105.5909398144,
#   ymin = 39.57843699065,
#   xmax = -105.5690601856,
#   ymax = 39.59385433645
# )



#--- * Description of external weather datasets ------
inputWeatherTSV <- readTSV(
  filename = file.path(dir_dataraw, "metadata_weatherDatasets.tsv")
)

hasWeather <- vapply(
  unique(listTestRuns[["inWeather"]]),
  function(wd) {
    if (identical(wd, "sw2")) {
      TRUE
    } else {
      tmp <- list.files(
        path = file.path(dir_dataraw, "inWeather", wd, "data"),
        pattern = ".nc$",
        full.names = TRUE
      )
      length(tmp) >= 3L # at least tasmax, tasmin, pr
    }
  },
  FUN.VALUE = NA
)


#--- * Units of SOILWAT2 example inputs ------
unitsOfSOILWAT2ExampleInputs <- readTSV(
  filename = file.path(dir_dataraw, "unitsOfSOILWAT2ExampleInputs.tsv")
)


#--- * SOILWAT2 ------
swin <- rSOILWAT2::sw_exampleData


pfts_short <- c("Tree", "Shrub", "Forb", "Grass")
pfts <- c("Trees", "Shrubs", "Forbs", "Grasses")
npfts <- length(pfts)

veg_params <- c("fCover", "litter", "biomass", "pct_live", "lai_conv")

soilsDefault <- swin@soils@Layers
# Calculate SWRCp for "Campbell1974" with PTF "Cosby1984AndOthers"
pSWRCDefault <- rSOILWAT2::ptf_estimate(
  sand = soilsDefault[, "sand_frac"],
  clay = soilsDefault[, "clay_frac"],
  fcoarse = soilsDefault[, "gravel_content"],
  swrc_name = rSOILWAT2::swSite_SWRCflags(swin)[["swrc_name"]],
  ptf_name = rSOILWAT2::swSite_SWRCflags(swin)[["ptf_name"]]
)

nSoilLayersDefault <- nrow(soilsDefault)
hzthkDefault <- round(
  diff(c(0, soilsDefault[, "depth_cm", drop = TRUE])), digits = 0L
)

sweather <- list(
  standard = rSOILWAT2::dbW_weatherData_to_dataframe(swin@weatherHistory) |>
    rSOILWAT2::dbW_convert_to_GregorianYears()
)

sweather[["365_day"]] <- {
  ids366 <- which(sweather[["standard"]][["DOY"]] == 366L)
  sweather[["standard"]][-ids366, , drop = FALSE]
}

sweather[["366_day"]] <- {
  years <- unique(sweather[["standard"]][["Year"]])
  tmp <- array(
    dim = c(366L * length(years), ncol(sweather[["standard"]])),
    dimnames = list(NULL, colnames(sweather[["standard"]]))
  ) |>
    as.data.frame()

  tmp[["Year"]] <- rep(years, each = 366L)
  tmp[["DOY"]] <- rep(seq_len(366L), times = length(years))

  ids <- match(
    paste0(tmp[["Year"]], "-", tmp[["DOY"]]),
    paste0(
      sweather[["standard"]][["Year"]], "-", sweather[["standard"]][["DOY"]]
    ),
    nomatch = 0L
  )

  vars <- colnames(sweather[["standard"]])[-(1:2)]
  tmp[ids > 0L, vars] <- sweather[["standard"]][ids, vars, drop = FALSE]

  # Add extreme values for non-real 366th days
  tmp[ids == 0L, c("Tmax_C", "Tmin_C")] <- 50
  tmp[ids == 0L, "PPT_cm"] <- 50

  tmp
}

ntime <- lapply(sweather, nrow)
nmonths <- 12L


#------ . ------
#------ Create nc-testRuns ------

#--- Loop over testRuns ------
pb <- utils::txtProgressBar(max = nrow(listTestRuns), style = 3L)

for (k0 in seq_len(nrow(listTestRuns))) {

  #------ . ------
  # Skip a testRun if no external weather data
  if (isFALSE(hasWeather[[listTestRuns[k0, "inWeather"]]])) {
    utils::setTxtProgressBar(pb, value = k0)
    next
  }


  #--- * Create testRun ------
  tagk <- paste0(
    "testRun-", formatC(listTestRuns[k0, "testrun"], width = 2L, flag = 0L),
    "__",
    listTestRuns[k0, "tag"]
  )

  dir_testRun <- file.path(dir_testRunsTemplates, tagk)


  #--- ..** Simulation domain ------
  domCRS <- listTestRuns[k0, "domainCRS"]

  kDomIsGridded <- switch(
    EXPR = listTestRuns[k0, "domainType"],
    "xy" = TRUE,
    "s" = FALSE,
    stop("Not implemented domainType ", listTestRuns[k0, "domainType"])
  )

  nDomDims <- listTestRuns[k0, "nDomDims"][[1L]]
  nDomRuns <- as.integer(listTestRuns[k0, "nDomRuns"])
  stopifnot(nDomRuns <= length(sw_grids[[domCRS]]))

  ids <- getRunIDs(
    nRuns = nDomRuns, kcrs = domCRS, es = id_examplesite, grids = sw_grids
  )

  domBB <- sf::st_bbox(sw_grids[[domCRS]][ids, ])
  domSites <- sf::st_as_sf(sw_sites[[domCRS]][ids, ])


  #--- ..** Shift domain location ------
  if (!anyNA(listTestRuns[k0, "domainShift"])) {
    tmp <- strsplit(
      as.character(listTestRuns[k0, "domainShift"]), split = ";", fixed = TRUE
    )
    domainShift <- rep_len(as.integer(tmp[[1L]]), 2L)
    stopifnot(length(domainShift) > 0L)

    domBB[["xmin"]] <- domBB[["xmin"]] + domainShift[[1L]]
    domBB[["xmax"]] <- domBB[["xmax"]] + domainShift[[1L]]
    domBB[["ymin"]] <- domBB[["ymin"]] + domainShift[[2L]]
    domBB[["ymax"]] <- domBB[["ymax"]] + domainShift[[2L]]

    tmp_crs <- sf::st_crs(domSites)
    sf::st_geometry(domSites) <- sf::st_geometry(domSites) + domainShift
    sf::st_crs(domSites) <- tmp_crs
  }


  #--- ..** Input domain ------
  #--- ....*** Spatial dimensions ------
  inputSpDims <- listTestRuns[k0, "nInputDims"][[1L]]
  names(inputSpDims) <- switch(
    EXPR = listTestRuns[k0, "inputType"],
    "xy" = sw_xyvars[[listTestRuns[k0, "inputCRS"]]],
    "s" = sw_xyvars[["site"]],
    stop("Not implemented inputType ", listTestRuns[k0, "inputType"])
  )


  #--- ....*** Calendar ------
  calendar <- switch(
    EXPR = listTestRuns[k0, "calendarWeather"],
    "NA" = ,
    "standard" = "standard",
    "noleap" = "365_day",
    "allleap" = "366_day",
    stop(shQuote(listTestRuns[k0, "calendarWeather"]), " is not implemented.")
  )


  #--- ....*** Soils ------
  nExtraSoilLayers <- switch(
    EXPR = listTestRuns[k0, "inputSoilProfile"],
    "standard" = 0L,
    "variableSoilLayerNumber" = 2L,
    "variableSoilLayerThickness" = 2L,
    stop(shQuote(listTestRuns[k0, "inputSoilProfile"]), " is not implemented.")
  )

  nMaxSoilLayersTestRun <- nSoilLayersDefault + nExtraSoilLayers

  ids <- seq_len(nSoilLayersDefault)

  if (nExtraSoilLayers > 0L) {
    # Add two layers (here, repeat the two deepest layers)
    ids <- c(
      ids,
      nSoilLayersDefault +
        seq(from = 0L - nExtraSoilLayers + 1L, to = 0L, by = 1L)
    )
  }

  soilsTestRun <- soilsDefault[ids, , drop = FALSE]
  pSWRCTestRun <- pSWRCDefault[ids, , drop = FALSE]
  hzthkTestRun <- hzthkDefault[ids]


  #--- ....*** Overall dimensions and counts ------
  inDimCounts <- list(
    sp = inputSpDims,
    meteo = c(time = ntime[[calendar]], inputSpDims),
    clim = c(time = nmonths, inputSpDims),
    soil = c(vertical = nMaxSoilLayersTestRun, inputSpDims),
    soilPFT1 = c(pft = 1L, vertical = nMaxSoilLayersTestRun, inputSpDims),
    climPFT = c(pft = npfts, time = nmonths, inputSpDims),
    vegPFT = c(pft = npfts, inputSpDims)
  )

  inDimPerms <- lapply(inDimCounts, seq_along)

  if (identical(listTestRuns[k0, "inputDimOrder"], "mix")) {
    # don't mix inDimCounts because we need to permutate arrays that
    # are set up in original dimension order
    set.seed(143) # hand selected seed to produce "well" mixed permutations
    inDimPerms <- lapply(inDimPerms, sample)
  }

  inDimPermCounts <- mapply(
    function(x, p) x[p], inDimCounts, inDimPerms, SIMPLIFY = FALSE
  )

  inDimNames <- lapply(inDimPermCounts, names)


  #--- ....*** Identify example site among inputs ------
  ids <- getRunIDs(
    nRuns = as.integer(listTestRuns[k0, "nInputRuns"]),
    kcrs = listTestRuns[k0, "inputCRS"],
    es = id_examplesite,
    grids = sw_grids
  )
  inputSites <- sf::st_as_sf(sw_sites[[listTestRuns[k0, "inputCRS"]]][ids, ])
  idInputExampleSite <- locateExampleSite(inputSites)
  stopifnot(length(idInputExampleSite) == 1L)


  #--- ....*** Variable soil layers (number and/or depth) ------
  hzthkArrayTestRun <- createTestRunSoils(
    soilData = hzthkTestRun,
    dims = inDimCounts[["soil"]],
    dimPermutation = inDimPerms[["soil"]],
    idExampleSite = idInputExampleSite,
    nSoilLayersExampleSite = nSoilLayersDefault,
    type = listTestRuns[k0, "inputSoilProfile"],
    mixNonExampleSiteValues = FALSE
  )


  if (
    identical(listTestRuns[k0, "inputSoilProfile"], "variableSoilLayerThickness")
  ) {
    # Vary soil layer thickness of the first non-example site
    hzthkArrayTestRun <- varySoilThicknessArray(
      hzthkArrayTestRun,
      dims = inDimCounts[["soil"]],
      idExampleSite = idInputExampleSite
    )
  }


  hzdptArrayTestRun <- calcDepthArrayFromThickness(
    hzthkArray = hzthkArrayTestRun,
    dimPermCounts = inDimPermCounts[["soil"]]
  )


  #--- ..** Copy SOILWAT2/tests/example ------
  # Exclude all netCDF files
  stopifnot(copySW2Example(from = dir_swex, to = dir_testRun))

  dir_testrun_swinnc <- file.path(dir_testRun, "Input_nc")
  stopifnot(dir.exists(dir_testrun_swinnc))


  #--- ..** Set input text files ------

  #--- ....*** Set domain.in ------
  fname <- file.path(dir_testRun, "Input", "domain.in")

  setTxtInput(
    filename = fname,
    tag = "Domain",
    value = listTestRuns[k0, "domainType"]
  )

  if (kDomIsGridded) {
    setTxtInput(filename = fname, tag = "nDimX", value = nDomDims[[1L]])
    setTxtInput(filename = fname, tag = "nDimY", value = nDomDims[[2L]])
  } else {
    setTxtInput(filename = fname, tag = "nDimS", value = nDomDims[[1L]])
  }

  setTxtInput(
    filename = fname,
    tag = "crs_bbox",
    value = switch(
      EXPR = domCRS,
      geographic = "WGS84",
      projected = "North American Daymet"
    )
  )
  setTxtInput(filename = fname, tag = "xmin_bbox", value = domBB[["xmin"]])
  setTxtInput(filename = fname, tag = "ymin_bbox", value = domBB[["ymin"]])
  setTxtInput(filename = fname, tag = "xmax_bbox", value = domBB[["xmax"]])
  setTxtInput(filename = fname, tag = "ymax_bbox", value = domBB[["ymax"]])

  setTxtInput(
    filename = fname,
    tag = "StartYear",
    value = listTestRuns[k0, "simStartYear"]
  )
  setTxtInput(
    filename = fname,
    tag = "EndYear",
    value = listTestRuns[k0, "simEndYear"]
  )


  #--- ....*** Set desc_nc.in ------
  fname <- file.path(dir_testrun_swinnc, "desc_nc.in")

  setTxtInput(filename = fname, tag = "primary_crs", value = domCRS)

  setTxtInput(filename = fname, tag = "siteName", value = sw_xyvars[["site"]])

  setTxtInput(
    filename = fname,
    tag = "geo_XAxisName",
    value = sw_xyvars[["geographic"]][[1L]]
  )
  setTxtInput(
    filename = fname,
    tag = "geo_YAxisName",
    value = sw_xyvars[["geographic"]][[2L]]
  )

  setTxtInput(
    filename = fname,
    tag = "proj_XAxisName",
    value = sw_xyvars[["projected"]][[1L]]
  )
  setTxtInput(
    filename = fname,
    tag = "proj_YAxisName",
    value = sw_xyvars[["projected"]][[2L]]
  )

  if (identical(domCRS, "projected")) {
    setTxtInput(
      filename = fname, tag = "proj_long_name",
      value = sw_crs[["projected"]][["long_name"]]
    )
    setTxtInput(
      filename = fname,
      tag = "proj_grid_mapping_name",
      value = sw_crs[["projected"]][["grid_mapping_name"]]
    )
    setTxtInput(
      filename = fname, tag = "proj_crs_wkt",
      value = sw_crs[["projected"]][["crs"]]$Wkt
    )
    setTxtInput(
      filename = fname,
      tag = "proj_semi_major_axis",
      value = as.numeric(sw_crs[["projected"]][["crs"]]$SemiMajor)
    )
    setTxtInput(
      filename = fname,
      tag = "proj_inverse_flattening",
      value = as.numeric(sw_crs[["projected"]][["crs"]]$InvFlattening)
    )
    setTxtInput(
      filename = fname, tag = "proj_datum",
      value = sw_crs[["projected"]][["datum"]]
    )
    setTxtInput(
      filename = fname,
      tag = "proj_units",
      value = units::deparse_unit(sw_crs[["projected"]][["crs"]]$ud_unit)
    )
    setTxtInput(
      filename = fname,
      tag = "proj_standard_parallel",
      value = getCRSParam(
        wkt = sw_crs[["projected"]][["crs"]], param = "standard_parallel"
      )
    )
    setTxtInput(
      filename = fname,
      tag = "proj_longitude_of_central_meridian",
      value = getCRSParam(
        wkt = sw_crs[["projected"]][["crs"]], param = "central_meridian"
      )
    )
    setTxtInput(
      filename = fname,
      tag = "proj_latitude_of_projection_origin",
      value = getCRSParam(
        wkt = sw_crs[["projected"]][["crs"]], param = "latitude_of_origin"
      )
    )
  }


  #--- ....*** Set ncinputs.tsv ------
  fname_ncintsv <- file.path(
    dir_testrun_swinnc, "SW2_netCDF_input_variables.tsv"
  )

  toggleNCInputTSV(filename = fname_ncintsv, inkeys = "all", value = 0L)
  toggleNCInputTSV(
    filename = fname_ncintsv, inkeys = c("inDomain", "inSpatial"), value = 1L
  )

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = c("inDomain", "inDomain", "inSpatial", "inSpatial"),
    sw2vars = c("domain", "progress", "latitude", "longitude"),
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )

  valuesInputTSV <- list(
    ncDomainType = listTestRuns[k0, "inputType"],
    ncSiteName = sw_xyvars[["site"]],
    ncCRSName =
      paste0("crs_", substr(listTestRuns[k0, "inputCRS"], 1L, 4L), "sc"),
    ncCRSGridMappingName =
      sw_crs[[listTestRuns[k0, "inputCRS"]]][["grid_mapping_name"]],
    ncXAxisName = sw_xyvars[[listTestRuns[k0, "inputCRS"]]][[1L]],
    ncYAxisName = sw_xyvars[[listTestRuns[k0, "inputCRS"]]][[2L]]
  )


  #--- ......**** Modify nc-units in tsv ------
  usedUnits <- modifyNCUnitsTSV(fname_ncintsv, unitsOfSOILWAT2ExampleInputs)


  #--- ..** Create domain and templates ------
  fname_template <- file.path(dir_testrun_swinnc, "domain_template.nc")
  fname_domain <- file.path(dir_testrun_swinnc, "domain.nc")

  fname_domainTemplate <- file.path(
    dir_domainTemplates,
    paste0(
      strsplit(listTestRuns[k0, "tag"], split = "_")[[1L]][[1L]],
      ".nc"
    )
  )
  fname_inputTemplate <- file.path(
    dir_domainTemplates,
    paste0(
      paste(
        "dom",
        listTestRuns[k0, "inputType"],
        listTestRuns[k0, "inputSize"],
        substr(listTestRuns[k0, "inputCRS"], 1L, 4L),
        sep = "-"
      ),
      ".nc"
    )
  )


  #--- Use SOILWAT2 to create domain.nc
  if (!file.exists(fname_domain)) {
    res <- runSW2(sw2 = fname_sw2, path_inputs = dir_testRun)

    if (file.exists(fname_template)) {
      nc <- RNetCDF::open.nc(fname_template, write = TRUE)

      #--- ....*** Sites: fix location of sites ------
      # SOILWAT2 places them along a lower-left to upper-right diagonal of the
      # bounding box, but here we want to have different site locations
      if (!kDomIsGridded && nDomRuns > 1L) {
        crds <- sf::st_coordinates(domSites)

        for (kd in seq_len(2)) {
          RNetCDF::var.put.nc(
            nc, variable = sw_xyvars[[domCRS]][[kd]], data = crds[, kd]
          )
        }
      }

      #--- ....*** Projected CRS: add geographic coordinates ------
      if (identical(domCRS, "projected")) {
        ksitesGeogsc <- sf::st_transform(
          domSites, crs = sw_crs[["geographic"]][["crs"]]
        )
        crds <- sf::st_coordinates(ksitesGeogsc)

        for (kd in seq_len(2)) {
          RNetCDF::var.put.nc(
            nc,
            variable = sw_xyvars[["geographic"]][[kd]],
            data = array(crds[, kd], dim = nDomDims),
            count = nDomDims
          )
        }
      }

      RNetCDF::close.nc(nc)


      #--- ....*** Copy template to domain ------
      file.rename(from = fname_template, to = fname_domain) |> stopifnot()

    } else {
      # Copy from existing domain templates
      file.copy(
        from = fname_domainTemplate,
        to = fname_domain,
        copy.mode = TRUE,
        copy.date = TRUE
      ) |>
        stopifnot()
    }
  }


  #--- ....*** Copy domain to collection of templates ------
  if (!file.exists(fname_domainTemplate)) {
    file.copy(
      from = fname_domain,
      to = fname_domainTemplate,
      copy.mode = TRUE,
      copy.date = TRUE
    ) |>
      stopifnot()
  }

  stopifnot(file.exists(fname_inputTemplate))


  #--- ....*** Adjust domain to 0-360 longitude ------
  if (isTRUE(all.equal(listTestRuns[k0, "domainLonConvention"], 360))) {
    nc <- RNetCDF::open.nc(fname_domain, write = TRUE)

    tmp <- RNetCDF::var.get.nc(
      nc, variable = sw_xyvars[["geographic"]][[1L]], collapse = FALSE
    )
    RNetCDF::var.put.nc(
      nc, variable = sw_xyvars[["geographic"]][[1L]], data = 360 + tmp
    )

    hasBnds <- try(RNetCDF::var.inq.nc(nc, "lon_bnds"), silent = TRUE)
    if (!inherits(hasBnds, "try-error")) {
      RNetCDF::var.put.nc(
        nc,
        variable = "lon_bnds",
        data = 360 + RNetCDF::var.get.nc(nc, "lon_bnds", collapse = FALSE)
      )
    }

    RNetCDF::close.nc(nc)

    if (identical(listTestRuns[k0, "inputCRS"], "geographic")) {
      fname <- file.path(dir_testRun, "Input", "domain.in")
      setTxtInput(
        filename = fname, tag = "xmin_bbox", value = 360 + domBB[["xmin"]]
      )
      setTxtInput(
        filename = fname, tag = "xmax_bbox", value = 360 + domBB[["xmax"]]
      )
    }
  }


  #--- ....*** Adjust domain to subset ------
  if (!is.na(listTestRuns[k0, "domainSubset"])) {
    nc <- RNetCDF::open.nc(fname_domain, write = TRUE)

    tmp <- strsplit(listTestRuns[k0, "domainSubset"], split = ";", fixed = TRUE)
    domSubset <- as.integer(tmp[[1L]])
    stopifnot(length(domSubset) > 0L)

    domIds <- RNetCDF::var.get.nc(nc, variable = "domain", collapse = FALSE)
    domIds[!domIds %in% domSubset] <- NA

    RNetCDF::var.put.nc(nc, variable = "domain", data = domIds)

    RNetCDF::close.nc(nc)
  }


  #------ . ------
  #--- Inputs ------
  varType <- switch(
    EXPR = listTestRuns[k0, "inputVarType"],
    "double" = "NC_DOUBLE",
    "float" = "NC_FLOAT",
    stop(
      "Input variable type ", shQuote(listTestRuns[k0, "inputVarType"]),
      " is not implemented"
    )
  )

  #--- * inTopo ------
  dir_topo <- file.path(dir_testrun_swinnc, "inTopo")
  dir.create(dir_topo, recursive = TRUE, showWarnings = FALSE)

  fname_topo <- file.path(dir_topo, "topo.nc")

  if (!file.exists(fname_topo)) {
    nDigsTopo <- 2L
    IntrinsicSiteParams <- round(
      swin@site@IntrinsicSiteParams,
      digits = nDigsTopo
    )

    copyInputTemplateNC(
      fname_topo,
      template = fname_inputTemplate,
      crsType = listTestRuns[k0, "inputCRS"],
      list_xyvars = sw_xyvars
    )

    nc <- RNetCDF::open.nc(fname_topo, write = TRUE)
    updateGAttSourceVersion(nc)

    #--- ..** inTopo: elevation ------
    u <- getModifiedNCUnits(usedUnits, "inTopo", "elevation")
    createVarNC(
      nc,
      varname = "elevation",
      dimensions = inDimNames[["sp"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "elevation",
      data = createTestRunData(
        x = IntrinsicSiteParams[["Altitude"]],
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["sp"]],
        dimPermutation = inDimPerms[["sp"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["sp"]]
    )

    #--- ..** inTopo: slope ------
    u <- getModifiedNCUnits(usedUnits, "inTopo", "slope")
    createVarNC(
      nc,
      varname = "slope",
      dimensions = inDimNames[["sp"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::att.put.nc(
      nc,
      variable = "slope", name = "comment", type = "NC_CHAR",
      value = "no slope = 0, vertical surface = 90"
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "slope",
      data = createTestRunData(
        x = IntrinsicSiteParams[["Slope"]],
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["sp"]],
        dimPermutation = inDimPerms[["sp"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["sp"]]
    )

    #--- ..** inTopo: aspect ------
    u <- getModifiedNCUnits(usedUnits, "inTopo", "aspect")
    createVarNC(
      nc,
      varname = "aspect",
      dimensions = inDimNames[["sp"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::att.put.nc(
      nc,
      variable = "aspect", name = "comment", type = "NC_CHAR",
      value = paste(
        "surface azimuth angle (degrees): S=0, E=-90, N=180 or -180, W=90;",
        "ignored if slope = 0 or aspect takes a missing value"
      )
    )
    val_aspect <- IntrinsicSiteParams[["Aspect"]]
    RNetCDF::var.put.nc(
      nc,
      variable = "aspect",
      data = createTestRunData(
        x = if (rSOILWAT2::is_missing_weather(val_aspect)[1L, 1L]) {
          NA_real_
        } else {
          val_aspect
        },
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["sp"]],
        dimPermutation = inDimPerms[["sp"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["sp"]]
    )

    RNetCDF::close.nc(nc)
  }


  #--- ..** inTopo: set ncinputs.tsv ------
  toggleNCInputTSV(filename = fname_ncintsv, inkeys = "inTopo", value = 1L)

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inTopo",
    sw2vars = NULL,
    values = valuesInputTSV,
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inTopo",
    sw2vars = "indexSpatial",
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )


  #------ . ------
  #--- * inClimate ------
  nDigsClim <- 3L
  dir_climate <- file.path(dir_testrun_swinnc, "inClimate")
  dir.create(dir_climate, recursive = TRUE, showWarnings = FALSE)

  fname_climate <- file.path(dir_climate, "monthlyClimate.nc")

  if (!file.exists(fname_climate)) {
    Cloud <- swin@cloud@Cloud

    copyInputTemplateNC(
      fname_climate,
      template = fname_inputTemplate,
      crsType = listTestRuns[k0, "inputCRS"],
      list_xyvars = sw_xyvars
    )

    nc <- RNetCDF::open.nc(fname_climate, write = TRUE)
    updateGAttSourceVersion(nc)
    updateGAttFrequency(nc, frq = "month")
    updateGAttFeatureType(nc, type = "timeSeries")

    #--- ..** inClimate: month ------
    createMonthClimatologyAxisNC(
      nc,
      startYear = swin@years@StartYear,
      endYear = swin@years@EndYear
    )

    #--- ..** inClimate: cloudcov ------
    u <- getModifiedNCUnits(usedUnits, "inClimate", "cloudcov")
    createVarNC(
      nc,
      varname = "cloudcov",
      long_name = "mean monthly cloud cover",
      dimensions = inDimNames[["clim"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "cloudcov",
      data = createTestRunData(
        x = round(Cloud["SkyCoverPCT", , drop = TRUE], digits = nDigsClim),
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["clim"]],
        dimPermutation = inDimPerms[["clim"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["clim"]]
    )

    #--- ..** inClimate: windspeed ------
    u <- getModifiedNCUnits(usedUnits, "inClimate", "windspeed")
    createVarNC(
      nc,
      varname = "windspeed",
      long_name = "mean monthly wind speed",
      dimensions = inDimNames[["clim"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "windspeed",
      data = createTestRunData(
        x = round(Cloud["WindSpeed_m/s", , drop = TRUE], digits = nDigsClim),
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["clim"]],
        dimPermutation = inDimPerms[["clim"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["clim"]]
    )

    #--- ..** inClimate: r_humidity ------
    u <- getModifiedNCUnits(usedUnits, "inClimate", "r_humidity")
    createVarNC(
      nc,
      varname = "r_humidity",
      long_name = "mean monthly relative humidity",
      dimensions = inDimNames[["clim"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "r_humidity",
      data = createTestRunData(
        x = round(Cloud["HumidityPCT", , drop = TRUE], digits = nDigsClim),
        otherValues = 10,
        usedUnits = u,
        dims = inDimCounts[["clim"]],
        dimPermutation = inDimPerms[["clim"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["clim"]]
    )

    #--- ..** inClimate: snow_density ------
    u <- getModifiedNCUnits(usedUnits, "inClimate", "snow_density")
    createVarNC(
      nc,
      varname = "snow_density",
      long_name = "mean monthly density of snowpack",
      dimensions = inDimNames[["clim"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "snow_density",
      data = createTestRunData(
        x = round(
          Cloud["SnowDensity_kg/m^3", , drop = TRUE], digits = nDigsClim
        ),
        otherValues = 1, # snow_density of 0 throws now an error
        usedUnits = u,
        dims = inDimCounts[["clim"]],
        dimPermutation = inDimPerms[["clim"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["clim"]]
    )

    #--- ..** inClimate: n_rain_per_day ------
    u <- getModifiedNCUnits(usedUnits, "inClimate", "n_rain_per_day")
    createVarNC(
      nc,
      varname = "n_rain_per_day",
      long_name = "mean monthly number of rain events per day",
      dimensions = inDimNames[["clim"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "n_rain_per_day",
      data = createTestRunData(
        x = round(
          Cloud["RainEvents_per_day", , drop = TRUE], digits = nDigsClim
        ),
        otherValues = NULL,
        usedUnits = u,
        dims = inDimCounts[["clim"]],
        dimPermutation = inDimPerms[["clim"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["clim"]]
    )


    RNetCDF::close.nc(nc)
  }


  #--- ..** inClimate: set ncinputs.tsv ------
  toggleNCInputTSV(filename = fname_ncintsv, inkeys = "inClimate", value = 1L)

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inClimate",
    sw2vars = NULL,
    values = valuesInputTSV,
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inClimate",
    sw2vars = "indexSpatial",
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )


  #------ . ------
  #--- * inSoil-properties ------
  dir_soil <- file.path(dir_testrun_swinnc, "inSoil")
  dir.create(dir_soil, recursive = TRUE, showWarnings = FALSE)

  fname_soil <- file.path(dir_soil, "soil.nc")

  if (!file.exists(fname_soil)) {
    nDigsSoil <- 4L

    copyInputTemplateNC(
      fname_soil,
      template = fname_inputTemplate,
      crsType = listTestRuns[k0, "inputCRS"],
      list_xyvars = sw_xyvars
    )

    nc <- RNetCDF::open.nc(fname_soil, write = TRUE)
    updateGAttSourceVersion(nc)


    #--- ..** inSoil: vertical ------
    createVerticalAxisNC(nc, nMaxSoilLayersTestRun)

    #--- ..** inSoil: hzdpt ------
    createVarNC(
      nc,
      varname = "hzdpt",
      long_name = "depth to soil layer bottom",
      dimensions = inDimNames[["soil"]],
      units = "cm",
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "hzdpt",
      data = hzdptArrayTestRun,
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: hzthk ------
    createVarNC(
      nc,
      varname = "hzthk",
      long_name = "thickness (width) of soil layer",
      dimensions = inDimNames[["soil"]],
      units = "cm",
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "hzthk",
      data = hzthkArrayTestRun,
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: dbovendry ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "dbovendry")
    createVarNC(
      nc,
      varname = "dbovendry",
      long_name = "density of the bulk soil",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "dbovendry",
      data = createTestRunSoils(
        soilData =
          round(soilsTestRun[, "bulkDensity_g/cm^3", drop = TRUE], nDigsSoil),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: fragvol ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "fragvol")
    createVarNC(
      nc,
      varname = "fragvol",
      long_name = "coarse fragments in bulk soil",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "fragvol",
      data = createTestRunSoils(
        soilData =
          round(soilsTestRun[, "gravel_content", drop = TRUE], nDigsSoil),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: sandtotal ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "sandtotal")
    createVarNC(
      nc,
      varname = "sandtotal",
      long_name = "sand content in the less than 2 mm soil fraction",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "sandtotal",
      data = createTestRunSoils(
        soilData =
          round(soilsTestRun[, "sand_frac", drop = TRUE], nDigsSoil),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: claytotal ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "claytotal")
    createVarNC(
      nc,
      varname = "claytotal",
      long_name = "clay content in the less than 2 mm soil fraction",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "claytotal",
      data = createTestRunSoils(
        soilData =
          round(soilsTestRun[, "clay_frac", drop = TRUE], nDigsSoil),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: silttotal ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "silttotal")
    createVarNC(
      nc,
      varname = "silttotal",
      long_name = "silt content in the less than 2 mm soil fraction",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "silttotal",
      data = createTestRunSoils(
        soilData = round(
          1 - (
            soilsTestRun[, "sand_frac", drop = TRUE] +
              soilsTestRun[, "clay_frac", drop = TRUE]
          ),
          digits = nDigsSoil
        ),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: impermeability ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "impermeability")
    createVarNC(
      nc,
      varname = "impermeability",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "impermeability",
      data = createTestRunSoils(
        soilData =
          round(soilsTestRun[, "impermeability_frac", drop = TRUE], nDigsSoil),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: tsl ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "tsl")
    createVarNC(
      nc,
      varname = "tsl",
      long_name = "soil temperature",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "tsl",
      data = createTestRunSoils(
        soilData =
          round(soilsTestRun[, "soilTemp_c", drop = TRUE], nDigsSoil),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: som ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "som")
    createVarNC(
      nc,
      varname = "som",
      long_name = "soil organic matter",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "som",
      data = createTestRunSoils(
        soilData = rep(0, nMaxSoilLayersTestRun),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: evc ------
    u <- getModifiedNCUnits(usedUnits, "inSoil", "evc")
    createVarNC(
      nc,
      varname = "evc",
      long_name = "potential evaporation coefficient",
      dimensions = inDimNames[["soil"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "evc",
      data = createTestRunSoils(
        soilData =
          round(soilsTestRun[, "EvapBareSoil_frac", drop = TRUE], nDigsSoil),
        usedUnits = u,
        dims = inDimCounts[["soil"]],
        dimPermutation = inDimPerms[["soil"]],
        idExampleSite = idInputExampleSite,
        nSoilLayersExampleSite = nSoilLayersDefault,
        type = listTestRuns[k0, "inputSoilProfile"],
        mixNonExampleSiteValues = TRUE
      ),
      count = inDimPermCounts[["soil"]]
    )

    #--- ..** inSoil: pft ------
    createPFTAxisNC(nc, pfts)


    #--- ..** inSoil: trc ------
    createVarNC(
      nc,
      varname = "trc",
      long_name = "potential transpiration coefficient",
      dimensions = inDimNames[["soilPFT1"]],
      units = "1",
      vartype = varType
    )

    for (kpft in seq_len(npfts)) {
      cn <- paste0("transp", pfts_short[[kpft]], "_frac")
      trc <- round(soilsTestRun[, cn, drop = TRUE], nDigsSoil)
      kstart <- c(
        kpft,
        rep(1L, length = length(inDimPermCounts[["soilPFT1"]]) - 1L)
      )[inDimPerms[["soilPFT1"]]]

      RNetCDF::var.put.nc(
        nc,
        variable = "trc",
        data =
          array(trc, dim = inDimCounts[["soilPFT1"]]) |>
          aperm(perm = inDimPerms[["soilPFT1"]]),
        start = kstart,
        count = inDimPermCounts[["soilPFT1"]]
      )

      var <- paste0("trc_", tolower(pfts[[kpft]]))
      u <- getModifiedNCUnits(usedUnits, "inSoil", var)
      createVarNC(
        nc,
        varname = var,
        long_name = paste0(
          "potential ",
          tolower(pfts_short[[kpft]]),
          "-transpiration coefficient"
        ),
        dimensions = inDimNames[["soil"]],
        units = "1",
        vartype = varType
      )

      RNetCDF::var.put.nc(
        nc,
        variable = var,
        data = createTestRunSoils(
          soilData = trc,
          usedUnits = NULL,
          dims = inDimCounts[["soil"]],
          dimPermutation = inDimPerms[["soil"]],
          idExampleSite = idInputExampleSite,
          nSoilLayersExampleSite = nSoilLayersDefault,
          type = listTestRuns[k0, "inputSoilProfile"],
          mixNonExampleSiteValues = TRUE
        ),
        count = inDimPermCounts[["soil"]]
      )
    }

    RNetCDF::close.nc(nc)
  }



  #------ . ------
  #--- * inSoil-swrc ------
  fname_soil <- file.path(dir_soil, "swrcp.nc")

  if (!file.exists(fname_soil)) {
    copyInputTemplateNC(
      fname_soil,
      template = fname_inputTemplate,
      crsType = listTestRuns[k0, "inputCRS"],
      list_xyvars = sw_xyvars
    )

    nc <- RNetCDF::open.nc(fname_soil, write = TRUE)
    updateGAttSourceVersion(nc)


    #--- ..** inSoil: vertical ------
    createVerticalAxisNC(nc, nMaxSoilLayersTestRun)


    #--- ..** inSoil: swrc[i] ------
    for (k in seq_len(ncol(pSWRCTestRun))) {
      var <- paste0("swrcp", k)
      ln <- switch(
        EXPR = k,
        "air-entry suction",
        "saturated volumetric water content for the matric component",
        "slope of the linear log-log retention curve (b)",
        "saturated hydraulic conductivity",
        var,
        var
      )
      u <- switch(
        EXPR = k,
        "cm",
        "cm cm-1",
        "1",
        "cm day-1",
        "1",
        "1"
      )

      createVarNC(
        nc,
        varname = var,
        long_name = ln,
        dimensions = inDimNames[["soil"]],
        units = u,
        vartype = varType
      )
      RNetCDF::att.put.nc(
        nc,
        variable = var, name = "comment", type = "NC_CHAR",
        value = paste("Soil water release curve parameter", k, "(Campbell1974)")
      )
      RNetCDF::var.put.nc(
        nc,
        variable = var,
        data = createTestRunSoils(
          # No rounding of SWRCp inputs because reference uses internal PTF
          soilData = pSWRCTestRun[, k, drop = TRUE],
          usedUnits = NULL,
          dims = inDimCounts[["soil"]],
          dimPermutation = inDimPerms[["soil"]],
          idExampleSite = idInputExampleSite,
          nSoilLayersExampleSite = nSoilLayersDefault,
          type = listTestRuns[k0, "inputSoilProfile"],
          mixNonExampleSiteValues = TRUE
        ),
        count = inDimPermCounts[["soil"]]
      )
    }

    RNetCDF::close.nc(nc)
  }


  #------ . ------
  #--- ..** inSoil: set ncinputs.tsv ------
  toggleNCInputTSV(filename = fname_ncintsv, inkeys = "inSoil", value = 1L)

  if (!identical(listTestRuns[k0, "inputsProvideSWRCp", drop = TRUE], "yes")) {
    # Deactivate inputs from SWRCp
    toggleNCInputTSV(
      filename = fname_ncintsv,
      inkeys = rep("inSoil", ncol(pSWRCTestRun)),
      sw2vars = paste0("swrcpMineralSoil[", seq_len(ncol(pSWRCTestRun)), "]"),
      value = 0L
    )
  }

  if (identical(listTestRuns[k0, "pft", drop = TRUE], "dim")) {
    # Deactivate pft as variable (pft as dimension is activated)
    toggleNCInputTSV(
      filename = fname_ncintsv,
      inkeys = "inSoil",
      sw2vars = paste(pfts, "transp_coeff", sep = "."),
      value = 0L
    )

  } else if (identical(listTestRuns[k0, "pft", drop = TRUE], "var")) {
    # Deactivate pft as dimension (pft as variable is activated)
    toggleNCInputTSV(
      filename = fname_ncintsv,
      inkeys = "inSoil",
      sw2vars = "<veg>.transp_coeff",
      value = 0L
    )

  } else {
    stop(
      "pft test type ", shQuote(listTestRuns[k0, "pft", drop = TRUE]),
      " not implemented."
    )
  }

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inSoil",
    sw2vars = NULL,
    values = valuesInputTSV,
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inSoil",
    sw2vars = "indexSpatial",
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )


  #--- ..** inSoil: set siteparam.in ------
  fname <- file.path(dir_testRun, "Input", "siteparam.in")

  if (
    identical(listTestRuns[k0, "inputSoilProfile"], "variableSoilLayerThickness")
  ) {
    setTxtInput(
      filename = fname,
      tag = "# 0 = depth/thickness of soil layers vary among sites/gridcells$",
      value = 0L, # change to 0 from default 1
      classic = TRUE
    )
  }

  if (identical(listTestRuns[k0, "inputsProvideSWRCp", drop = TRUE], "yes")) {
    # Activate inputs from SWRCp
    setTxtInput(
      filename = fname,
      tag = "# Are SWRC parameters for the mineral soil component",
      value = 1L, # change to 1 from default 0
      classic = TRUE
    )
  }


  #--- ..** inSoil: set soils.in ------
  # Add extra soil layers so that text input file represent maximum number
  # of soil layers
  if (
    identical(listTestRuns[k0, "inputSoilProfile"], "variableSoilLayerNumber")
  ) {
    fname <- file.path(dir_testRun, "Input", "soils.in")

    fin <- suppressWarnings(readLines(fname))

    # Count number of soil layers in "soils.in"
    nhas <- intersect(
      grep("(^#)", x = trimws(fin), invert = TRUE),
      which(nchar(trimws(fin)) > 0L)
    ) |>
      unique() |>
      length()

    if (nhas == nSoilLayersDefault && nExtraSoilLayers > 0L) {
      lid <- max(which(nchar(fin) > 0L)) # last line with values
      ids1 <- seq_len(lid)
      idsExtra <- seq(from = 0L - nExtraSoilLayers + 1L, to = 0L, by = 1L)

      fextra <- strsplit(fin[lid + idsExtra], split = "[ ]+")
      fextra <- lapply(fextra, function(x) x[nchar(x) > 0L])
      stopifnot(length(unique(lengths(fextra))) == 1L)

      fextra <- mapply(
        function(sl, d) {
          c(as.character(d), sl[-1L])
        },
        fextra,
        cumsum(hzthkTestRun)[length(hzthkTestRun) + idsExtra],
        SIMPLIFY = FALSE
      )

      fin2 <- c(
        fin,
        vapply(fextra, paste, FUN.VALUE = NA_character_, collapse = " ")
      )

      writeLines(fin2, con = fname)
    }
  }


  #------ . ------
  #--- * inSite ------
  dir_site <- file.path(dir_testrun_swinnc, "inSite")
  dir.create(dir_site, recursive = TRUE, showWarnings = FALSE)

  #--- ..** inSite: tas-clim ------
  fname_tasclim <- file.path(dir_site, "tas-clim.nc")

  if (!file.exists(fname_tasclim)) {
    nDigsSite <- 2L
    tasclim <- round(
      swin@site@SoilTemperatureConstants[["ConstMeanAirTemp"]],
      digits = nDigsSite
    )

    copyInputTemplateNC(
      fname_tasclim,
      template = fname_inputTemplate,
      crsType = listTestRuns[k0, "inputCRS"],
      list_xyvars = sw_xyvars
    )

    nc <- RNetCDF::open.nc(fname_tasclim, write = TRUE)
    updateGAttSourceVersion(nc)

    u <- getModifiedNCUnits(usedUnits, "inSite", "tas")
    createVarNC(
      nc,
      varname = "tas",
      long_name = "mean temperature",
      dimensions = inDimNames[["sp"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "tas",
      data = createTestRunData(
        x = tasclim,
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["sp"]],
        dimPermutation = inDimPerms[["sp"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["sp"]]
    )

    RNetCDF::close.nc(nc)
  }


  #--- ..** inSite: set ncinputs.tsv ------
  toggleNCInputTSV(filename = fname_ncintsv, inkeys = "inSite", value = 1L)

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inSite",
    sw2vars = NULL,
    values = valuesInputTSV,
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inSite",
    sw2vars = "indexSpatial",
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )


  #------ . ------
  #--- * inVeg: pft as variables ------
  dir_veg <- file.path(dir_testrun_swinnc, "inVeg")
  dir.create(dir_veg, recursive = TRUE, showWarnings = FALSE)

  fname_veg <- file.path(dir_veg, "veg2.nc")


  if (!file.exists(fname_veg)) {
    nDigsVeg <- 4L

    copyInputTemplateNC(
      fname_veg,
      template = fname_inputTemplate,
      crsType = listTestRuns[k0, "inputCRS"],
      list_xyvars = sw_xyvars
    )

    nc <- RNetCDF::open.nc(fname_veg, write = TRUE)
    updateGAttSourceVersion(nc)
    updateGAttFrequency(nc, frq = "month")
    updateGAttFeatureType(nc, type = "timeSeries")

    #--- ..** inVeg: month ------
    createMonthClimatologyAxisNC(
      nc,
      startYear = swin@years@StartYear,
      endYear = swin@years@EndYear
    )

    #--- ..** inVeg: fcover_bg ------
    Composition <- swin@prod@Composition # doesn't reproduce if round

    u <- getModifiedNCUnits(usedUnits, "inVeg", "fcover_bg")
    createVarNC(
      nc,
      varname = "fcover_bg",
      long_name = "fractional cover of bare ground",
      dimensions = inDimNames[["sp"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )

    RNetCDF::var.put.nc(
      nc,
      variable = "fcover_bg",
      data = createTestRunData(
        x = Composition[["Bare Ground"]],
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["sp"]],
        dimPermutation = inDimPerms[["sp"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["sp"]]
    )


    #--- ..** inVeg: fcover_[veg] ------
    for (k in seq_along(pfts)) {
      var <- paste0("fcover_", tolower(pfts[[k]]))
      u <- getModifiedNCUnits(usedUnits, "inVeg", var)
      createVarNC(
        nc,
        varname = var,
        long_name = paste("fractional cover of", tolower(pfts[[k]])),
        dimensions = inDimNames[["sp"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )

      RNetCDF::var.put.nc(
        nc,
        variable = var,
        data = createTestRunData(
          x = Composition[[pfts[[k]]]],
          otherValues = if (k == 1L) 1 else 0,
          usedUnits = u,
          dims = inDimCounts[["sp"]],
          dimPermutation = inDimPerms[["sp"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["sp"]]
      )
    }

    monVeg <- swin@prod@MonthlyVeg

    #--- ..** inVeg: litter_[veg] ------
    for (k in seq_along(pfts)) {
      var <- paste0("litter_", tolower(pfts[[k]]))
      u <- getModifiedNCUnits(usedUnits, "inVeg", var)
      createVarNC(
        nc,
        varname = var,
        long_name = "litter",
        dimensions = inDimNames[["clim"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )

      RNetCDF::var.put.nc(
        nc,
        variable = var,
        data = createTestRunData(
          x = round(monVeg[[pfts[[k]]]][, "Litter"], nDigsVeg),
          otherValues = round(mean(monVeg[[pfts[[k]]]][, "Litter"]), nDigsVeg),
          usedUnits = u,
          dims = inDimCounts[["clim"]],
          dimPermutation = inDimPerms[["clim"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["clim"]]
      )
    }


    #--- ..** inVeg: biomass_[veg] ------
    for (k in seq_along(pfts)) {
      var <- paste0("biomass_", tolower(pfts[[k]]))
      u <- getModifiedNCUnits(usedUnits, "inVeg", var)
      createVarNC(
        nc,
        varname = var,
        long_name = "total biomass",
        dimensions = inDimNames[["clim"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )
      RNetCDF::var.put.nc(
        nc,
        variable = var,
        data = createTestRunData(
          x = round(monVeg[[pfts[[k]]]][, "Biomass"], nDigsVeg),
          otherValues = round(mean(monVeg[[pfts[[k]]]][, "Biomass"]), nDigsVeg),
          usedUnits = u,
          dims = inDimCounts[["clim"]],
          dimPermutation = inDimPerms[["clim"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["clim"]]
      )
    }

    #--- ..** inVeg: live_[veg] ------
    for (k in seq_along(pfts)) {
      var <- paste0("live_", tolower(pfts[[k]]))
      u <- getModifiedNCUnits(usedUnits, "inVeg", var)
      createVarNC(
        nc,
        varname = var,
        long_name = "fraction of biomass that is living",
        dimensions = inDimNames[["clim"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )
      RNetCDF::var.put.nc(
        nc,
        variable = var,
        # doesn't reproduce if rounded
        data = createTestRunData(
          x = monVeg[[pfts[[k]]]][, "Live_pct"],
          otherValues = mean(monVeg[[pfts[[k]]]][, "Live_pct"]),
          usedUnits = u,
          dims = inDimCounts[["clim"]],
          dimPermutation = inDimPerms[["clim"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["clim"]]
      )
    }

    #--- ..** inVeg: convLAI_[veg] ------
    for (k in seq_along(pfts)) {
      var <- paste0("convLAI_", tolower(pfts[[k]]))
      u <- getModifiedNCUnits(usedUnits, "inVeg", var)
      createVarNC(
        nc,
        varname = var,
        long_name = "biomass needed to produce LAI = 1",
        dimensions = inDimNames[["clim"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )
      RNetCDF::var.put.nc(
        nc,
        variable = var,
        data = createTestRunData(
          x = round(monVeg[[pfts[[k]]]][, "LAI_conv"], nDigsVeg),
          otherValues = 250,
          usedUnits = u,
          dims = inDimCounts[["clim"]],
          dimPermutation = inDimPerms[["clim"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["clim"]]
      )
    }

    RNetCDF::close.nc(nc)
  }


  #------ . ------
  #--- * inVeg: pft as dimension ------
  fname_veg <- file.path(dir_veg, "vegPFT.nc")

  if (!file.exists(fname_veg)) {
    copyInputTemplateNC(
      fname_veg,
      template = fname_inputTemplate,
      crsType = listTestRuns[k0, "inputCRS"],
      list_xyvars = sw_xyvars
    )

    nc <- RNetCDF::open.nc(fname_veg, write = TRUE)
    updateGAttSourceVersion(nc)
    updateGAttFrequency(nc, frq = "month")
    updateGAttFeatureType(nc, type = "timeSeries")


    #--- ..** inVeg: month ------
    createMonthClimatologyAxisNC(
      nc,
      startYear = swin@years@StartYear,
      endYear = swin@years@EndYear
    )


    #--- ..** inVeg: pft ------
    createPFTAxisNC(nc, pfts)



    #--- ..** inVeg: fcover_bg ------
    Composition <- swin@prod@Composition # doesn't reproduce if round

    u <- getModifiedNCUnits(usedUnits, "inVeg", "fcover_bg")
    createVarNC(
      nc,
      varname = "fcover_bg",
      long_name = "fractional cover of bare ground",
      dimensions = inDimNames[["sp"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "fcover_bg",
      data = createTestRunData(
        x = Composition[["Bare Ground"]],
        otherValues = 0,
        usedUnits = u,
        dims = inDimCounts[["sp"]],
        dimPermutation = inDimPerms[["sp"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["sp"]]
    )

    #--- ..** inVeg: fcover ------
    u <- getModifiedNCUnits(usedUnits, "inVeg", "fcover")
    createVarNC(
      nc,
      varname = "fcover",
      long_name = "fractional vegetation cover",
      dimensions = inDimNames[["vegPFT"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )

    RNetCDF::var.put.nc(
      nc,
      variable = "fcover",
      data = createTestRunData(
        x = Composition[pfts],
        otherValues = c(1, rep(0, npfts - 1L)),
        usedUnits = u,
        dims = inDimCounts[["vegPFT"]],
        dimPermutation = inDimPerms[["vegPFT"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["vegPFT"]]
    )


    #--- ..** inVeg: litter ------
    u <- getModifiedNCUnits(usedUnits, "inVeg", "litter")
    createVarNC(
      nc,
      varname = "litter",
      dimensions = inDimNames[["climPFT"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )

    RNetCDF::var.put.nc(
      nc,
      variable = "litter",
      data = createTestRunData(
        x = vapply(
          swin@prod@MonthlyVeg[pfts],
          function(x) round(x[, "Litter", drop = TRUE], nDigsVeg),
          FUN.VALUE = rep(NA_real_, nmonths)
        ) |> t(),
        otherValues = vapply(
          swin@prod@MonthlyVeg[pfts],
          function(x) {
            rep(round(mean(x[, "Litter", drop = TRUE]), nDigsVeg), nmonths)
          },
          FUN.VALUE = rep(NA_real_, nmonths)
        ) |> t(),
        usedUnits = u,
        dims = inDimCounts[["climPFT"]],
        dimPermutation = inDimPerms[["climPFT"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["climPFT"]]
    )


    #--- ..** inVeg: biomass ------
    u <- getModifiedNCUnits(usedUnits, "inVeg", "biomass")
    createVarNC(
      nc,
      varname = "biomass",
      long_name = "total biomass",
      dimensions = inDimNames[["climPFT"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "biomass",
      data = createTestRunData(
        x = vapply(
          swin@prod@MonthlyVeg[pfts],
          function(x) round(x[, "Biomass", drop = TRUE], nDigsVeg),
          FUN.VALUE = rep(NA_real_, nmonths)
        ) |> t(),
        otherValues = vapply(
          swin@prod@MonthlyVeg[pfts],
          function(x) {
            rep(round(mean(x[, "Biomass", drop = TRUE]), nDigsVeg), nmonths)
          },
          FUN.VALUE = rep(NA_real_, nmonths)
        ) |> t(),
        usedUnits = u,
        dims = inDimCounts[["climPFT"]],
        dimPermutation = inDimPerms[["climPFT"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["climPFT"]]
    )


    #--- ..** inVeg: live fraction ------
    u <- getModifiedNCUnits(usedUnits, "inVeg", "live")
    createVarNC(
      nc,
      varname = "live",
      long_name = "fraction of biomass that is living",
      dimensions = inDimNames[["climPFT"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "live",
      data = createTestRunData(
        # doesn't reproduce if round
        x = vapply(
          swin@prod@MonthlyVeg[pfts],
          function(x) x[, "Live_pct", drop = TRUE],
          FUN.VALUE = rep(NA_real_, nmonths)
        ) |> t(),
        otherValues = vapply(
          swin@prod@MonthlyVeg[pfts],
          function(x) {
            rep(mean(x[, "Live_pct", drop = TRUE]), nmonths)
          },
          FUN.VALUE = rep(NA_real_, nmonths)
        ) |> t(),
        usedUnits = u,
        dims = inDimCounts[["climPFT"]],
        dimPermutation = inDimPerms[["climPFT"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["climPFT"]]
    )


    #--- ..** inVeg: convLAI ------
    u <- getModifiedNCUnits(usedUnits, "inVeg", "convLAI")
    createVarNC(
      nc,
      varname = "convLAI",
      long_name = "biomass needed to produce LAI = 1",
      dimensions = inDimNames[["climPFT"]],
      units = u[["ncVarUnitsModified"]],
      vartype = varType
    )
    RNetCDF::var.put.nc(
      nc,
      variable = "convLAI",
      data = createTestRunData(
        x = vapply(
          swin@prod@MonthlyVeg[pfts],
          function(x) round(x[, "LAI_conv", drop = TRUE], nDigsVeg),
          FUN.VALUE = rep(NA_real_, nmonths)
        ) |> t(),
        otherValues = 250,
        usedUnits = u,
        dims = inDimCounts[["climPFT"]],
        dimPermutation = inDimPerms[["climPFT"]],
        spDims = inputSpDims,
        idExampleSite = idInputExampleSite
      ),
      count = inDimPermCounts[["climPFT"]]
    )


    RNetCDF::close.nc(nc)
  }


  #------ . ------
  #--- ..** inVeg: set ncinputs.tsv ------
  toggleNCInputTSV(filename = fname_ncintsv, inkeys = "inVeg", value = 1L)

  if (identical(listTestRuns[k0, "pft", drop = TRUE], "dim")) {
    # Deactivate pft as variable (pft as dimension is activated)
    toggleNCInputTSV(
      filename = fname_ncintsv,
      inkeys = "inVeg",
      sw2vars = c(
        "bareGround.fCover",
        apply(
          expand.grid(pfts, veg_params),
          MARGIN = 1L,
          FUN = paste,
          collapse = "."
        )
      ),
      value = 0L
    )

  } else if (identical(listTestRuns[k0, "pft", drop = TRUE], "var")) {
    # Deactivate pft as dimension (pft as variable is activated)
    toggleNCInputTSV(
      filename = fname_ncintsv,
      inkeys = "inVeg",
      sw2vars = c("bareGround.fCover", paste0("<veg>.", veg_params)),
      value = 0L
    )

  } else {
    stop(
      "pft test type ", shQuote(listTestRuns[k0, "pft", drop = TRUE]),
      " not implemented."
    )
  }

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inVeg",
    sw2vars = NULL,
    values = valuesInputTSV,
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inVeg",
    sw2vars = "indexSpatial",
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )


  #------ . ------
  #--- * inWeather ------
  dir_weather <- file.path(dir_testrun_swinnc, "inWeather")
  dir.create(dir_weather, recursive = TRUE, showWarnings = FALSE)

  stopifnot(
    listTestRuns[k0, "inWeather"] %in%
      c("sw2", "gridMET", "MACAv2METDATA", "Daymet")
  )

  useWeatherDatasets <- !identical(listTestRuns[k0, "inWeather"], "sw2")

  ids1 <- which(inputWeatherTSV[["dataset"]] == listTestRuns[k0, "inWeather"])
  tmp <- sw_crs[[listTestRuns[k0, "inputCRS"]]][["grid_mapping_name"]]
  ids2 <- which(inputWeatherTSV[["ncCRSGridMappingName"]] == tmp)
  metaWeather <- inputWeatherTSV[intersect(ids1, ids2), , drop = FALSE]
  stopifnot(names(valuesInputTSV) %in% names(metaWeather))

  if (useWeatherDatasets) {
    metaWeather[["ncFileName"]] <- file.path(
      "..", "..", "..", basename(dir_dataraw), "inWeather",
      listTestRuns[k0, "inWeather"], "data", metaWeather[["ncFileName"]]
    )
  }

  ids <- grep("^nc*", names(metaWeather))
  weatherValuesInputTSV <- metaWeather[, ids, drop = FALSE]

  vars_weather <- metaWeather[["SW2 variable"]]
  vars_climate_deactivate <- NULL
  desc_rsds <- NULL
  impute_weather <- NULL
  fix_weather <- NULL

  stopifnot(
    !useWeatherDatasets || length(vars_weather) > 0L,
    anyDuplicated(vars_weather) == 0L
  )


  if (identical(listTestRuns[k0, "inWeather"], "sw2")) {
    #--- ..** inWeather sw2 ------
    vars_weather <- c("temp_max", "temp_min", "ppt")
    desc_rsds <- 0L # daily global horizontal irradiation
    weatherValuesInputTSV <- do.call(
      what = rbind,
      args = lapply(seq_along(vars_weather), function(k) valuesInputTSV)
    )

    if (identical(calendar, "365_day")) {
      impute_weather <- 3L # use LOCF to handle fixed 365-day calendar of Daymet
    }

    fname_weather <- file.path(dir_weather, "weather.nc")

    if (!file.exists(fname_weather)) {
      nDigsMeteo <- 4L

      copyInputTemplateNC(
        fname_weather,
        template = fname_inputTemplate,
        crsType = listTestRuns[k0, "inputCRS"],
        list_xyvars = sw_xyvars
      )

      nc <- RNetCDF::open.nc(fname_weather, write = TRUE)
      updateGAttSourceVersion(nc)
      updateGAttFrequency(nc, frq = "day")
      updateGAttFeatureType(nc, type = "timeSeries")


      #--- ....*** inWeather sw2: time ------
      createTimeAxisNC(
        nc,
        startYear = sweather[[calendar]][1L, "Year", drop = TRUE],
        timeValues = seq_len(ntime[[calendar]]) - 0.5, # midday
        calendar = calendar
      )


      #--- ....*** inWeather sw2: tasmax ------
      u <- getModifiedNCUnits(usedUnits, "inWeather", "tasmax")
      createVarNC(
        nc,
        varname = "tasmax",
        long_name = "maximum air temperature",
        dimensions = inDimNames[["meteo"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )
      RNetCDF::att.put.nc(
        nc,
        variable = "tasmax", name = "units_metadata", type = "NC_CHAR",
        value = "temperature: on_scale"
      )
      RNetCDF::att.put.nc(
        nc,
        variable = "tasmax", name = "cell_method", type = "NC_CHAR",
        value = "time: maximum"
      )
      RNetCDF::var.put.nc(
        nc,
        variable = "tasmax",
        data = createTestRunData(
          x = round(
            sweather[[calendar]][, "Tmax_C", drop = TRUE],
            digits = nDigsMeteo
          ),
          otherValues = 20,
          usedUnits = u,
          dims = inDimCounts[["meteo"]],
          dimPermutation = inDimPerms[["meteo"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["meteo"]]
      )

      #--- ....*** inWeather sw2: tasmin ------
      u <- getModifiedNCUnits(usedUnits, "inWeather", "tasmin")
      createVarNC(
        nc,
        varname = "tasmin",
        long_name = "minimum air temperature",
        dimensions = inDimNames[["meteo"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )
      RNetCDF::att.put.nc(
        nc,
        variable = "tasmin",
        name = "units_metadata",
        type = "NC_CHAR",
        value = "temperature: on_scale"
      )
      RNetCDF::att.put.nc(
        nc,
        variable = "tasmin",
        name = "cell_method",
        type = "NC_CHAR",
        value = "time: minimum"
      )
      RNetCDF::var.put.nc(
        nc,
        variable = "tasmin",
        data = createTestRunData(
          x = round(
            sweather[[calendar]][, "Tmin_C", drop = TRUE],
            digits = nDigsMeteo
          ),
          otherValues = 5,
          usedUnits = u,
          dims = inDimCounts[["meteo"]],
          dimPermutation = inDimPerms[["meteo"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["meteo"]]
      )


      #--- ....*** inWeather sw2: pr ------
      u <- getModifiedNCUnits(usedUnits, "inWeather", "pr")
      createVarNC(
        nc,
        varname = "pr",
        long_name = "precipitation amount",
        dimensions = inDimNames[["meteo"]],
        units = u[["ncVarUnitsModified"]],
        vartype = varType
      )
      RNetCDF::att.put.nc(
        nc,
        variable = "pr",
        name = "cell_method",
        type = "NC_CHAR",
        value = "time: sum"
      )
      RNetCDF::var.put.nc(
        nc,
        variable = "pr",
        data = createTestRunData(
          x = round(
            10 * sweather[[calendar]][, "PPT_cm", drop = TRUE],
            digits = nDigsMeteo
          ),
          otherValues = 0,
          usedUnits = u,
          dims = inDimCounts[["meteo"]],
          dimPermutation = inDimPerms[["meteo"]],
          spDims = inputSpDims,
          idExampleSite = idInputExampleSite
        ),
        count = inDimPermCounts[["meteo"]]
      )


      RNetCDF::close.nc(nc)
    }
  }


  if (identical(listTestRuns[k0, "inWeather"], "gridMET")) {
    #--- ..** inWeather gridMET ------
    desc_rsds <- 1L # shortWaveRad is flux density over 24 hours
    vars_climate_deactivate <- c("cloudcov", "windspeed", "r_humidity")
    fix_weather <- "fixPERCENT"
  }


  if (identical(listTestRuns[k0, "inWeather"], "MACAv2METDATA")) {
    #--- ..** inWeather MACAv2METDATA ------
    desc_rsds <- 1L # shortWaveRad is flux density over 24 hours
    vars_climate_deactivate <- c("cloudcov", "windspeed", "r_humidity")
    fix_weather <- "fixPERCENT"
  }


  if (identical(listTestRuns[k0, "inWeather"], "Daymet")) {
    #--- ..** inWeather Daymet ------
    desc_rsds <- 2L # shortWaveRad is flux density over daylight period
    vars_climate_deactivate <- c("cloudcov", "r_humidity")
    impute_weather <- 3L # use LOCF to handle fixed 365-day calendar of Daymet
    fix_weather <- c("fixPERCENT", "fixMAXRSDS")
  }


  #--- . ------
  #--- ..** inWeather: set weathsetup.in ------
  fname <- file.path(dir_testRun, "Input", "weathsetup.in")

  if (!is.null(impute_weather)) {
    # Set missing weather imputation method
    setTxtInput(
      filename = fname,
      tag = "# 0 = use historical data only$",
      value = impute_weather,
      classic = TRUE
    )
  }

  if (!is.null(fix_weather)) {
    # Turn on corrections to daily weather input
    for (k in seq_along(fix_weather)) {
      setTxtInput(
        filename = fname,
        tag = switch(
          EXPR = fix_weather[[k]],
          fixMINMAX = "Swap min/max if min > max",
          fixPERCENT = "Reset percentages to 100% if > 100%",
          fixMAXRSDS = "Reset observed to extraterrestrial solar radiation"
        ),
        value = 1,
        classic = TRUE
      )
    }
  }

  if (!is.null(desc_rsds)) {
    # Set radiation description
    setTxtInput(
      filename = fname,
      tag = "Description of downward surface shortwave radiation",
      value = desc_rsds,
      classic = TRUE
    )
  }

  if (!is.null(vars_climate_deactivate)) {
    # Turn off mean monthly climate inputs
    for (kc in seq_along(vars_climate_deactivate)) {
      setTxtInput(
        filename = fname,
        tag = switch(
          EXPR = vars_climate_deactivate[[kc]],
          "cloudcov" = "# Sky cover$",
          "windspeed" = "# Wind speed$",
          "r_humidity" = "# Relative humidity$",
          stop(vars_climate_deactivate[[kc]], " is not implemented.")
        ),
        value = 0L,
        classic = TRUE
      )
    }
  }

  if (!is.null(vars_weather)) {
    # Turn on daily weather inputs
    for (kw in seq_along(vars_weather)) {
      setTxtInput(
        filename = fname,
        tag = switch(
          EXPR = vars_weather[[kw]],
          "temp_max" = "# Maximum daily temperature \\[C\\]$",
          "temp_min" = "# Minimum daily temperature \\[C\\]$",
          "ppt" = "# Precipitation \\[cm\\]$",
          "cloudcov" = "# Cloud cover \\[\\%\\]$",
          "windspeed" = "# Wind speed \\[m/s\\]$",
          "windspeed_east" = "# Wind speed eastward component \\[m/s\\]$",
          "windspeed_north" = "# Wind speed northward component \\[m/s\\]$",
          "r_humidity" = "# Relative humidity \\[\\%\\]$",
          "rmax_humidity" = "# Maximum relative humidity \\[\\%\\]$",
          "rmin_humidity" = "# Minimum relative humidity \\[\\%\\]$",
          "spec_humidity" = "# Specific humidity \\[\\%\\]$",
          "temp_dewpoint" = "# Dew point temperature \\[C\\]$",
          "actualVaporPressure" = "# Actual vapor pressure \\[kPa\\]$",
          "shortWaveRad" = "# Downward surface shortwave radiation",
          stop(vars_weather[[kc]], " is not implemented.")
        ),
        value = 1L,
        classic = TRUE
      )
    }
  }


  #--- ..** inWeather: set ncinputs.tsv ------
  toggleNCInputTSV(
    filename = fname_ncintsv, inkeys = "inWeather", sw2vars = NULL, value = 0L
  )

  toggleNCInputTSV(
    filename = fname_ncintsv,
    inkeys = rep("inWeather", times = 1L + length(vars_weather)),
    sw2vars = c("indexSpatial", vars_weather),
    value = 1L
  )

  if (length(vars_climate_deactivate) > 0L) {
    toggleNCInputTSV(
      filename = fname_ncintsv,
      inkeys = rep("inClimate", times = length(vars_climate_deactivate)),
      sw2vars = vars_climate_deactivate,
      value = 0L
    )
  }

  stopifnot(length(vars_weather) == nrow(weatherValuesInputTSV))

  for (kw in seq_along(vars_weather)) {
    setNCInputTSV(
      filename = fname_ncintsv,
      testrun = listTestRuns[k0, , drop = TRUE],
      inkeys = "inWeather",
      sw2vars = vars_weather[[kw]],
      values = weatherValuesInputTSV[kw, ],
      list_xyvars = sw_xyvars,
      list_crs = sw_crs
    )
  }

  setNCInputTSV(
    filename = fname_ncintsv,
    testrun = listTestRuns[k0, , drop = TRUE],
    inkeys = "inWeather",
    sw2vars = "indexSpatial",
    list_xyvars = sw_xyvars,
    list_crs = sw_crs
  )

  utils::setTxtProgressBar(pb, value = k0)
}

close(pb)


#------ . ------
#------ Report on creation of nc-testRuns ------
tmp <- list.dirs(dir_testRunsTemplates, recursive = FALSE)

cat(
  "Created ", min(length(tmp), nrow(listTestRuns)), " ncTestRuns ",
  "(requested ", nrow(listTestRuns), " out of ", nTotalTestRuns, ").",
  sep = "",
  fill = TRUE
)
#------ . ------
