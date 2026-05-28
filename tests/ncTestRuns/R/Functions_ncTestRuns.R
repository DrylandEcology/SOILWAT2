
#------ . ------
#------ Functions ------
countDims <- function(domSizes, isGridded) {
  stopifnot(grepl("x", domSizes, fixed = TRUE) == isGridded)

  res <- vector(mode = "list", length = length(domSizes))
  res[isGridded] <- strsplit(domSizes[isGridded], split = "x", fixed = TRUE) |>
    lapply(FUN = as.integer)
  res[!isGridded] <- as.integer(domSizes[!isGridded])
  res
}

countRuns <- function(nDomDims) {
  vapply(nDomDims, prod, FUN.VALUE = NA_real_) |> as.integer()
}


exampleSite <- function(crs = "WGS84") {
  c(lon = -105.58, lat = 39.59) |>
    sf::st_point() |>
    sf::st_sfc(crs = "WGS84") |>
    sf::st_transform(crs = crs)
}

locateExampleSite <- function(sites, crs = sf::st_crs(sites)) {
  stopifnot(requireNamespace("sf"), requireNamespace("units"))
  tmp <- sf::st_is_within_distance(
    sites,
    y = exampleSite(crs),
    dist = units::set_units(1L, "m"),
    sparse = FALSE
  )
  which(tmp[, 1L])
}

getRunIDs <- function(nRuns, kcrs, es, grids) {
  ids <- if (identical(nRuns, 1L)) {
    es[[kcrs]]
  } else if (identical(nRuns, 2L)) {
    c(2L, es[[kcrs]])
  } else if (identical(nRuns, 6L)) {
    seq_along(grids[[kcrs]])
  } else {
    stop("Not implemented number of runs: ", nRuns)
  }
  stopifnot(length(unique(ids)) == nRuns)
  ids
}

copyInputTemplateNC <- function(filename, template, crsType, list_xyvars) {
  if (identical(crsType, "projected")) {
    # Remove geographic variables and crs if projected
    res <- try(
      system2(
        command = "ncks",
        # -C = do not add associated variables (e.g., lat) to the extraction
        # -h = do not write to the history global attribute
        # -x = invert selection made by -v
        # -v = list of variables to operate on
        args = paste(
          "-C -h -x",
          "-v",
          paste(c(list_xyvars[["geographic"]], "crs_geogsc"), collapse = ","),
          template,
          filename
        )
      ),
      silent = TRUE
    )

    if (inherits(res, "try-error")) {
      stop(res)
    }

    res == 0L && is.null(attributes(res))

  } else {
    file.copy(
      from = template,
      to = filename,
      copy.mode = TRUE,
      copy.date = TRUE
    )
  }
}

copyDir <- function(from, to) {
  file.copy(
    from = from,
    to = dirname(to),
    overwrite = FALSE,
    recursive = TRUE,
    copy.mode = TRUE,
    copy.date = TRUE
  )

  if (!dir.exists(to)) {
    file.rename(
      from = file.path(dirname(to), basename(from)),
      to = to
    )
  }

  dir.exists(to)
}

# Prepended numbers are interpreted as prefixed units if there is no space
#' @examples
#' makePrefixedUnit("g g-1")
#' makePrefixedUnit("100 g g-1")
#'
makePrefixedUnit <- function(unit) {
  if (
    grepl(" ", unit, fixed = TRUE) &&
      !anyNA(suppressWarnings(as.integer(substr(unit, 1L, 1L))))
  ) {
    # remove first space (between number and unit)
    sub(" ", "", unit, fixed = TRUE)
  } else {
    unit
  }
}

convertUnits <- function(x, hasUnits = "1", newUnits = "1") {
  if (!identical(hasUnits, newUnits)) {
    stopifnot(requireNamespace("units"))

    units(x) <- units::as_units(makePrefixedUnit(hasUnits)) # set current units
    units(x) <- units::as_units(makePrefixedUnit(newUnits)) # transform
    x <- units::drop_units(x)
  }

  x
}


replaceOldNames <- function(x, newNames, oldNames) {
  ids <- match(x, oldNames, nomatch = 0L)
  x[ids > 0L] <- newNames[ids]
  x
}

appendToMessage <- function(hasMsg, newMsg) {
  if (isTRUE(nzchar(hasMsg, keepNA = TRUE))) {
    if (isTRUE(nzchar(newMsg, keepNA = TRUE))) {
      paste(hasMsg, newMsg, sep = " -- ")
    } else {
      hasMsg
    }
  } else {
    newMsg
  }
}

#' Arbitrarily chosen value to test feature to end simulation on any day of year
valueEarlyEndDate <- function() c(year = 2010L, doy = 25L)


#--- * SOILWAT2-related functions ------

copySW2Example <- function(from, to) {
  stopifnot(dir.exists(from))
  dir.create(to, showWarnings = FALSE, recursive = TRUE)

  # Copy "files.in"
  file.copy(from = file.path(from, "files.in"), to = to)

  # Copy "Input/"
  res <- copyDir(from = file.path(from, "Input"), to = file.path(to, "Input"))

  # Copy "Input_nc/"
  res <-
    res &&
    copyDir(from = file.path(from, "Input_nc"), to = file.path(to, "Input_nc"))

  # Exclude all *.nc
  tmp <- list.files(
    path = file.path(to, "Input_nc"),
    pattern = "*.nc$",
    full.names = TRUE,
    recursive = TRUE
  ) |>
    unlink()

  res && as.logical(tmp[[1L]] == 0L)
}

#' @param classic Logical value.
#'
#' @section Classic vs. non-classic files:
#'   * classic file lines: "value   # comment" where tag matches in the comment
#'   * non-classic file line: "tag value  # comment" where tag matches tag
setTxtInput <- function(filename, tag, value, classic = FALSE) {
  value <- paste(value, collapse = " ")
  # suppress warnings about incomplete final lines
  fin <- suppressWarnings(readLines(filename))
  idLine <- grep(
    pattern = if (isTRUE(classic)) tag else paste0("^", tag, " "),
    x = fin,
    ignore.case = TRUE
  )
  stopifnot(length(idLine) == 1L, idLine > 0L, idLine <= length(fin))
  posComment <- regexpr("#", text = fin[[idLine]], fixed = TRUE)
  res <- if (isTRUE(classic)) as.character(value) else paste(tag, value)
  fin[[idLine]] <- if (posComment > 0L) {
    paste0(
      res,
      strrep(" ", max(1L, posComment - 1L - nchar(res))),
      substr(fin[[idLine]], start = posComment, stop = 1000L)
    )
  } else {
    res
  }
  writeLines(fin, con = filename)
}


getCRSParam <- function(wkt, param) {
  stopifnot(requireNamespace("sf"))
  wkt <- if (inherits(wkt, "crs")) wkt$Wkt else as.character(wkt)
  ptxts <- gregexec('PARAMETER\\[[a-zA-Z0-9\\",_.-]+\\]', text = wkt)
  res <- regmatches(wkt, m = ptxts)[[1L]][1L, ]
  ids <- grep(param, x = res, fixed = TRUE)
  vapply(
    strsplit(gsub("]", "", res[ids], fixed = TRUE), split = ",", fixed = TRUE),
    function(x) as.numeric(x[[2L]]),
    FUN.VALUE = NA_real_
  )
}

readTSV <- function(filename) {
  utils::read.delim(
    file = filename,
    colClasses = "character",
    check.names = FALSE,
    blank.lines.skip = FALSE
  )
}

writeTSV <- function(x, filename) {
  # Preserve empty lines
  fcon <- file()
  on.exit(close(fcon))

  utils::write.table(
    x,
    file = fcon,
    sep = "\t",
    quote = FALSE,
    row.names = FALSE
  )
  xtmp <- readLines(fcon)
  ids <- which(xtmp == strrep("\t", times = ncol(x) - 1L))
  xtmp[ids] <- ""

  writeLines(xtmp, con = filename)
}


toggleNCInputTSV <- function(
  filename,
  inkeys = "all",
  sw2vars = NULL,
  ncFileNames = NULL,
  value = c(1L, 0L),
  extraCheck = NULL
) {
  stopifnot(value %in% c(1L, 0L))

  x <- readTSV(filename)

  inuse <- if (identical(inkeys, "all")) {
    nzchar(x[["SW2 input group"]], keepNA = TRUE)
  } else {
    tmp <- x[["SW2 input group"]] %in% inkeys
    if (!is.null(sw2vars)) {
      tmp <- tmp & x[["SW2 variable"]] %in% sw2vars
    }
    if (!is.null(ncFileNames)) {
      tmp <- tmp & basename(x[["ncFileName"]]) %in% basename(ncFileNames)
    }
    tmp
  }

  incheck <- if (
    !is.null(extraCheck) && all(c("cn", "pattern") %in% names(extraCheck))
  ) {
    grepl(pattern = extraCheck[["pattern"]], x = x[[extraCheck[["cn"]]]])
  } else {
    rep(TRUE, nrow(x))
  }

  hasvalue <- inuse & incheck & nzchar(x[["Do nc-input?"]])
  x[["Do nc-input?"]][hasvalue] <- value[[1L]]

  writeTSV(x, filename)
}

setNCInputTSV <- function(
  filename,
  testrun,
  inkeys,
  sw2vars = NULL,
  ncFileNames = NULL,
  values = NULL,
  list_xyvars = list(),
  list_crs = list()
) {
  x <- readTSV(filename)

  if (is.null(sw2vars)) {
    inkeys0 <- inkeys
    if (is.null(ncFileNames)) {
      tmp0 <- inkeys
      tmp1 <- x[["SW2 input group"]]
    } else {
      tmp0 <- paste(inkeys, basename(ncFileNames), sep = "-")
      tmp1 <- paste(
        x[["SW2 input group"]], basename(x[["ncFileName"]]), sep = "-"
      )
    }
    ids <- match(tmp1, tmp0, nomatch = 0L)

    inkeys <- inkeys0[ids]
    sw2vars <- x[["SW2 variable"]][ids > 0L]
    ncFileNames <- x[["ncFileName"]][ids > 0L]
  }

  stopifnot(
    identical(length(inkeys), length(sw2vars)),
    is.null(ncFileNames) || identical(length(inkeys), length(ncFileNames))
  )

  kcrs <- testrun[["domainCRS"]]

  values_default <- list(
    ncDomainType = testrun[["domainType"]],
    ncSiteName = list_xyvars[["site"]],
    ncCRSName =
      paste0("crs_", substr(kcrs, 1L, 4L), "sc"),
    ncCRSGridMappingName = list_crs[[kcrs]][["grid_mapping_name"]],
    ncXAxisName = list_xyvars[[kcrs]][[1L]],
    ncYAxisName = list_xyvars[[kcrs]][[2L]]
  )

  ids <- setdiff(names(values_default), names(values))
  values <- c(values_default[ids], values)

  for (k in seq_along(inkeys)) {
    tmp <- x[["SW2 input group"]] %in% inkeys[[k]] &
      x[["SW2 variable"]] %in% sw2vars[[k]]
    if (!is.null(ncFileNames)) {
      tmp <- tmp & x[["ncFileName"]] %in% ncFileNames[[k]]
    }
    idrow <- which(tmp)

    if (length(idrow) != 1L) {
      stop(
        "Could not identify row in nc-inputs.tsv: ",
        "k = ", k,
        ", inkey = ", inkeys[[k]],
        ", sw2var = ", sw2vars[[k]],
        if (!is.null(ncFileNames)) paste0(", ncFileName = ", ncFileNames[[k]])
      )
    }

    for (kv in seq_along(values)) {
      x[[names(values)[[kv]]]][[idrow]] <- values[[kv]]
    }
  }

  writeTSV(x, filename)
}

modifyNCUnitsTSV <- function(
  filename,
  unitsOfSOILWAT2ExampleInputs,
  adjustUnits = list(
    c(inkey = "inTopo", sw2var = "elevation", newUnit = "km"),
    c(inkey = "inWeather", sw2var = "temp_max", newUnit = "K"),
    c(inkey = "inClimate", sw2var = "r_humidity", newUnit = "1"),
    c(
      inkey = "inSoil",
      sw2var = "fractionVolBulk_gravel",
      newUnit = "0.01 cm3 cm-3"
    ),
    c(inkey = "inVeg", sw2var = "<veg>.litter", newUnit = "kg m-2"),
    c(inkey = "inVeg", sw2var = "shrub.biomass", newUnit = "kg m-2"),
    c(inkey = "inSite", sw2var = "Tsoil_constant", newUnit = "degF")
  )
) {
  x <- readTSV(filename)

  vars <- c(
    "SW2 input group", "SW2 variable", "SW2 units", "ncVarName", "ncVarUnits"
  )

  #--- Set units used SOILWA2 example inputs
  ids1 <- apply(x[, vars[1L:2L], drop = FALSE], 1L, paste, collapse = "-")
  has2 <- which(
    apply(unitsOfSOILWAT2ExampleInputs, 1L, function(x) any(nzchar(x)))
  )
  ids2 <- apply(
    unitsOfSOILWAT2ExampleInputs[has2, vars[1L:2L], drop = FALSE],
    MARGIN = 1L,
    FUN = paste,
    collapse = "-"
  )

  ids <- match(ids1, ids2, nomatch = 0L)
  x[ids > 0L, "ncVarUnits"] <-
    unitsOfSOILWAT2ExampleInputs[has2[ids], "inputUnits"]


  #--- Adjust units as requested for ncTestRuns
  res <- x[, vars, drop = FALSE]
  res[["ncVarUnitsModified"]] <- res[["ncVarUnits"]]

  for (k in seq_along(adjustUnits)) {
    idrow <- which(
      x[["SW2 input group"]] %in% adjustUnits[[k]][["inkey"]] &
        x[["SW2 variable"]] %in% adjustUnits[[k]][["sw2var"]]
    )

    if (length(idrow) != 1L) {
      stop(
        "Could not identify row in nc-inputs.tsv: ",
        "k = ", k, ", inkey = ", adjustUnits[[k]][["inkey"]],
        ", sw2var = ", adjustUnits[[k]][["sw2var"]]
      )
    }

    x[idrow, "ncVarUnits"] <- adjustUnits[[k]][["newUnit"]]
    res[idrow, "ncVarUnitsModified"] <- adjustUnits[[k]][["newUnit"]]
  }

  writeTSV(x, filename)

  res
}

getModifiedNCUnits <- function(x, inkey, ncvar) {
  idrow <- which(
    x[["SW2 input group"]] %in% inkey & x[["ncVarName"]] %in% ncvar
  )

  isdups <- duplicated(
    x[idrow, c("ncVarUnits", "ncVarUnitsModified"), drop = FALSE]
  )

  if (any(isdups)) {
    idrow <- idrow[!isdups]
  }

  if (length(idrow) != 1L) {
    stop(
      "Could not identify row in nc-inputs.tsv: ",
      "inkey = ", inkey, ", ncvar = ", ncvar
    )
  }

  as.list(x[idrow, c("ncVarUnits", "ncVarUnitsModified")])
}


detectMPIExecutor <- function() {
  executor <- NULL

  hasSrun <- try(
    system2(command = "command", args = "-v srun > /dev/null 2>&1"),
    silent = TRUE
  )

  if (isTRUE(all.equal(hasSrun, 0L))) {
    executor <- "srun"

  } else {
    hasMpirun <- try(
      system2(command = "command", args = "-v mpirun > /dev/null 2>&1"),
      silent = TRUE
    )
    if (isTRUE(all.equal(hasMpirun, 0L))) {
      executor <- "mpirun"
    }
  }

  executor
}


getSW2StartDay <- function(filename, variable = "start_day") {
  stopifnot(requireNamespace("RNetCDF"))

  xnc <- RNetCDF::open.nc(filename, write = TRUE)
  on.exit(RNetCDF::close.nc(xnc))

  RNetCDF::utcal.nc(
    unitstring = RNetCDF::att.get.nc(xnc, variable, "units"),
    value = RNetCDF::var.get.nc(xnc, variable),
    type = "c"
  )
}


invokeSW2 <- function(
  sw2,
  path_inputs,
  mode = c("nc", "mpi"),
  nTasks = NULL,
  mpiExecutor = NULL,
  renameDomainTemplate = FALSE,
  wallTimeSeconds = NULL,
  simulateCountDays = NULL,
  prepare = FALSE
) {
  mode <- match.arg(mode)
  isMPI <- identical(mode, "mpi")
  if (isMPI) {
    if (is.null(mpiExecutor)) {
      mpiExecutor <- detectMPIExecutor()
    }
    if (!mpiExecutor %in% c("mpirun", "srun")) {
      stop("mpiExecutor ", shQuote(mpiExecutor), " is not implemented.")
    }
  }

  msg <- NULL
  res <- withCallingHandlers(
    tryCatch(
      system2(
        command = if (isMPI) mpiExecutor else sw2,
        args = paste(
          if (isMPI && !is.null(nTasks)) paste("-n", nTasks),
          if (isMPI) paste0("./", sw2),
          "-d", path_inputs,
          "-f files.in",
          if (isTRUE(renameDomainTemplate)) "-r",
          if (isTRUE(is.finite(wallTimeSeconds))) paste("-t", wallTimeSeconds),
          if (isTRUE(prepare)) "-p",
          if (isTRUE(is.finite(simulateCountDays))) {
            paste("-s", simulateCountDays)
          }
        ),
        stdout = TRUE,
        stderr = TRUE
      ),
      error = function(e) {
        msg <<- c(msg, conditionMessage(e))
        NULL
      }
    ),
    warning = function(w) {
      msg <<- c(msg, conditionMessage(w))
      invokeRestart("muffleWarning")
    }
  )

  list(res, msg = msg)
}


runSW2 <- function(
  sw2,
  path_inputs,
  mode = c("nc", "mpi"),
  nTasks = NULL,
  mpiExecutor = NULL,
  renameDomainTemplate = FALSE,
  stopRestart = FALSE
) {
  res <- NULL

  if (isTRUE(stopRestart)) {
    # Start, stop, & restart
    # First step: prepare files
    res1 <- invokeSW2(
      sw2 = sw2,
      path_inputs = path_inputs,
      mode = mode,
      nTasks = nTasks,
      mpiExecutor = mpiExecutor,
      renameDomainTemplate = renameDomainTemplate,
      prepare = TRUE
    )

    progressMade1 <- getSW2StartDay(
      filename = file.path(path_inputs, "Input_nc", "cached_state.nc")
    )

    # Second step: first batch of time steps and stop
    res2 <- invokeSW2(
      sw2 = sw2,
      path_inputs = path_inputs,
      mode = mode,
      nTasks = nTasks,
      mpiExecutor = mpiExecutor,
      simulateCountDays = 1000L # fewer days than shortest test run
    )

    progressMade2 <- getSW2StartDay(
      filename = file.path(path_inputs, "Input_nc", "cached_state.nc")
    )

    # Third step: re-start simulation and complete
    res3 <- invokeSW2(
      sw2 = sw2,
      path_inputs = path_inputs,
      mode = mode,
      nTasks = nTasks,
      mpiExecutor = mpiExecutor
    )

    progressMade3 <- getSW2StartDay(
      filename = file.path(path_inputs, "Input_nc", "cached_state.nc")
    )

    # Determine outcome of start, stop, restart
    res <- if (progressMade1 == progressMade2) {
      list(NULL, msg = "Error: simulation did not start before early stop.")
    } else if (progressMade2 == progressMade3) {
      list(NULL, msg = "Error: simulation did not restart after early stop.")
    } else {
      list(
        c(res1[[1L]], res2[[1L]], res3[[1L]]),
        msg = appendToMessage(res1[["msg"]], res2[["msg"]]) |>
          appendToMessage(res3[["msg"]])
      )
    }

  } else {
    # Simulation without time limit
    res <- invokeSW2(
      sw2 = sw2,
      path_inputs = path_inputs,
      mode = mode,
      nTasks = nTasks,
      mpiExecutor = mpiExecutor,
      renameDomainTemplate = renameDomainTemplate
    )
  }

  res
}


#--- * Manipulate input netCDFs ------

# 4.3.2. Dimensionless Vertical Coordinate (deprecated)
# The units attribute is not required for dimensionless coordinates. For
# backwards compatibility with COARDS we continue to allow the units attribute
# to take one of the values: level, layer, or sigma_level. These values are not
# recognized by the UDUNITS package, and are considered a deprecated feature in
# the CF standard.

#' examples
#' createTestRunData(
#'   x = 1,
#'   otherValues = -273.15,
#'   dims = c(lon = 3L, lat = 2L),
#'   dimPermutation = c(2L, 1L),
#'   spDims = c(lon = 3L, lat = 2L),
#'   idExampleSite = 5L,
#'   usedUnits = list("degC", "K")
#' )
#'
#' createTestRunData(
#'   x = 1:12,
#'   otherValues = -(12:1),
#'   dims = c(time = 12L, lon = 3L, lat = 2L),
#'   dimPermutation = c(2L, 1L, 3L),
#'   spDims = c(lon = 3L, lat = 2L),
#'   idExampleSite = 5L
#' )
#'
#' createTestRunData(
#'   x = 1:12,
#'   otherValues = -(12:1),
#'   dims = c(time = 12L, site = 6L),
#'   dimPermutation = c(2L, 1L),
#'   spDims = c(site = 6L),
#'   idExampleSite = 5L
#' )
createTestRunData <- function(
  x,
  dims,
  dimPermutation,
  spDims,
  idExampleSite,
  otherValues = NULL,
  usedUnits = list(originalUnits = "1", newUnits = "1")
) {
  nonSpDims <- dims[setdiff(names(dims), names(spDims))]
  nNonSpDims <- length(nonSpDims)
  nSpElements <- prod(spDims)

  stopifnot(
    length(dims) == length(dimPermutation),
    all.equal(spDims, dims[names(spDims)]),
    all.equal(
      spDims, dims[length(dims) + seq(from = -length(spDims) + 1L, to = 0L)]
    ),
    nSpElements >= idExampleSite
  )

  if (nNonSpDims == 0L) {
    stopifnot(
      # no dimensions other than spatial
      length(x) == 1L,
      is.null(otherValues) || length(otherValues) == 1L
    )
  } else if (nNonSpDims == 1L) {
    stopifnot(
      identical(length(x), unname(nonSpDims)) || length(x) == 1L,
      is.null(otherValues) ||
        identical(length(otherValues), unname(nonSpDims)) ||
        length(otherValues) == 1L
    )
  } else {
    stopifnot(
      identical(dim(x), unname(nonSpDims)) || length(x) == 1L,
      is.null(otherValues) ||
        identical(dim(otherValues), unname(nonSpDims)) ||
        length(otherValues) == 1L
    )
  }

  res <- array(data = if (is.null(otherValues)) x else otherValues, dim = dims)

  if (nSpElements > 0L) {
    # Identify example site/gridcell
    xthk <- array(dim = spDims)
    xthk[seq_len(nSpElements)[idExampleSite][[1L]]] <- 1L
    ix <- which(xthk == 1L, arr.ind = TRUE)
    ithk <- if (nNonSpDims == 0L) {
      ix
    } else {
      cbind(
        # Indices for all values of non-spatial dimension
        expand.grid(lapply(nonSpDims, seq_len)) |> data.matrix(),
        # Repeat spatial index of example site/gridcell
        ix[rep(1L, prod(nonSpDims)), , drop = FALSE]
      )
    }

    # Add specific values for example site/gridcell
    res[ithk] <- x
  }


  # Permutate order of dimensions
  res <- aperm(res, perm = dimPermutation)

  # Adjust units
  if (!is.null(usedUnits)) {
    res <- convertUnits(
      res, hasUnits = usedUnits[[1L]], newUnits = usedUnits[[2L]]
    )
  }

  res
}


#' @param nMinSoilLayers An integer value. Minimum number of soil layers at a
#' site (grid cell). Note: this should be at least 3 if the vegetation
#' establishment output is activated (with default parameters).
createTestRunSoils <- function(
  soilData,
  dims,
  dimPermutation,
  idExampleSite,
  nSoilLayersExampleSite,
  nMinSoilLayers = 3L,
  type = c("standard", "variableSoilLayerNumber", "variableSoilLayerThickness"),
  mixNonExampleSiteValues = FALSE,
  usedUnits = list(originalUnits = "1", newUnits = "1"),
  seed = 127L
) {

  type <- match.arg(type)
  mixNonExampleSiteValues <- isTRUE(mixNonExampleSiteValues[[1L]])

  nSpElements <- prod(dims[-1L])
  stopifnot(
    length(dims) == length(dimPermutation),
    length(soilData) == dims[[1L]],
    length(soilData) >= nSoilLayersExampleSite,
    nSpElements >= idExampleSite,
    identical(names(dims)[[1L]], "vertical")
  )

  xIDs <- array(data = seq_len(nSpElements), dim = dims[-1L])

  # Determine number of soil layers for each site/gridcell
  # Initialize array to nMinSoilLayers
  xNSoilLayers <- array(data = as.integer(nMinSoilLayers), dim = dims[-1L])

  # Set default number of layers for example site
  xNSoilLayers[idExampleSite] <- nSoilLayersExampleSite

  if (nSpElements > 1L) {
    # We need more than the example site to vary soils
    idSites <- seq_len(nSpElements)[-idExampleSite]
    # Set maximum number of layers for first non-example site
    xNSoilLayers[idSites[[1L]]] <- length(soilData)
  }

  # Create data structure and fill with values depending on number of layers
  x <- array(data = NA_real_, dim = dims)

  if (mixNonExampleSiteValues) {
    set.seed(seed)
  }

  for (k in seq_len(prod(dims[-1L]))) {
    ks <- seq_len(xNSoilLayers[k])
    mids <- data.frame(which(xIDs == k, arr.ind = TRUE))
    mids[["ids"]] <- apply(mids, 1L, paste, collapse = "-")
    kids <- merge(
      expand.grid(c(list(vertical = ks), mids["ids"])),
      mids
    )[, -1L]
    x[data.matrix(kids)] <- if (mixNonExampleSiteValues && k != idExampleSite) {
      soilData[ks[sample.int(length(ks))]]
    } else {
      soilData[ks]
    }
  }

  # Permutate order of dimensions
  x <- aperm(x, perm = dimPermutation)

  # Adjust units
  if (!is.null(usedUnits)) {
    x <- convertUnits(
      x, hasUnits = usedUnits[[1L]], newUnits = usedUnits[[2L]]
    )
  }

  x
}


# Vary soil layer thickness of the first non-example site
varySoilThicknessArray <- function(hzthkArray, dims, idExampleSite) {
  spDims <- dims[-1L]
  nSpElements <- prod(spDims)

  if (nSpElements > 0L) {
    # Identify first non-example site/gridcell
    xthk <- array(dim = spDims)
    xthk[seq_len(nSpElements)[-idExampleSite][[1L]]] <- 1L
    ithk <- cbind(
      seq_len(dims[[1L]]),
      which(xthk == 1L, arr.ind = TRUE)[rep(1L, dims[[1L]]), ]
    )
    hzthkArray[ithk] <- 2.0 * hzthkArray[ithk] # double thickness
  }

  hzthkArray
}

calcDepthArrayFromThickness <- function(hzthkArray, dimPermCounts) {
  stopifnot(all.equal(dim(hzthkArray), dimPermCounts))

  res <- apply(
    hzthkArray,
    MARGIN = which(names(dimPermCounts) != "vertical"),
    cumsum
  )

  # apply() loses the name of the "vertical" dimension
  tmp <- names(dim(res))
  idsvert <- which(!nzchar(tmp) & !tmp %in% names(dimPermCounts))
  names(dim(res))[[idsvert]] <- "vertical"

  if (!all(names(dim(res)) == names(dimPermCounts))) {
    # apply rearranges dimensions -> permutate to desired dimension order
    res <- aperm(res, perm = match(names(dim(res)), names(dimPermCounts)))
  }

  stopifnot(all.equal(dim(res), dimPermCounts))

  res
}


#--- * Functions to work with testRun outputs ------

colorTestReport <- function(x) {
  stopifnot(requireNamespace("cli"))

  # note: `fixed = TRUE` does not work
  gsub("\\<ok\\>", cli::col_green("ok"), x = x) |>
    gsub("\\<failed\\>", cli::col_red("failed"), x = _) |>
    gsub("\\<missing\\>", cli::col_yellow("missing"), x = _)
}


printColoredDF <- function(x, vars) {
  res <- apply(
    rbind(vars, x[, vars, drop = FALSE]),
    MARGIN = 2L,
    FUN = format,
    justify = "right"
  )

  for (kr in seq_len(nrow(res))) {
    cat(
      colorTestReport(paste(res[kr, ], collapse = " ")),
      fill = TRUE
    )
  }
}

printReportRow <- function(x, colored = FALSE) {
  x <- do.call(sprintf, args = c(fmt = "%13s%12s%9s%23s%14s", as.list(x)))
  cat(msg = if (isTRUE(colored)) colorTestReport(x) else x, fill = TRUE)
}


findExampleSiteIndex <- function(id, domain) {
  which(domain == id, arr.ind = length(dim(domain)) == 2L)
}



readUnitsAttributeNC <- function(fname, var) {
  stopifnot(requireNamespace("RNetCDF"))

  nc <- RNetCDF::open.nc(fname)
  on.exit(RNetCDF::close.nc(nc), add = TRUE)

  RNetCDF::att.get.nc(nc, var, "units")
}


#' Convert calendar to spelling used by CFtime
cleanCalendar <- function(calendar) {
  calendar |>
    sub("allleap", "all_leap", x = _, fixed = TRUE) |>
    sub("365day", "365_day", x = _, fixed = TRUE) |>
    sub("366day", "366_day", x = _, fixed = TRUE)
}

timeStep <- function(x) {
  tmp <- diff(x) |>
    table() |>
    sort(decreasing = TRUE)
  tmp <- as.integer(names(tmp)[[1L]])

  if (tmp == 1L) {
    "day"
  } else if (tmp == 7L) {
    "week"
  } else if (tmp %in% 28L:31L) {
    "month"
  } else if (tmp %in% c(365L, 366L)) {
    "year"
  } else {
    stop("Time step not recognized.")
  }
}

#' Check time values
allEqualTimeValues <- function(
  timeValues,
  timeUnits,
  timeCalendar,
  timeBoundValues = NULL,
  startYear = NULL,
  endYear = NULL,
  earlyEndDate = NULL
) {
  if (is.null(startYear) && is.null(endYear)) return(TRUE)

  timeCalendar <- cleanCalendar(timeCalendar)
  acceptableCalendars <- c("standard", "gregorian", "proleptic_gregorian")

  stopifnot(
    requireNamespace("RNetCDF"),
    timeCalendar %in% acceptableCalendars
  )

  # Determine time step
  ts <- timeStep(timeValues)


  # Expected dates
  if (identical(ts, "week")) {
    # SOILWAT2 restarts the count of weeks for each year and
    # adds a partial week to complete the year
    years <- seq(startYear, min(endYear, earlyEndDate[["year"]]), by = 1L)
    n <- length(years)

    expStartDates <- as.POSIXct(paste0(years, "-01-01"), tz = "UTC")
    expEndDates <- as.POSIXct(paste0(years, "-12-31"), tz = "UTC")

    if (!is.null(earlyEndDate) && isTRUE(endYear >= earlyEndDate[["year"]])) {
      expEndDates[[n]] <- as.POSIXct(
        as.Date(paste(earlyEndDate, collapse = "-"), format = "%Y-%j"),
        tz = "UTC"
      )
    }

    expEndDates2 <- expEndDates + 86400L

    expectedTimeBounds <- list(
      lapply(
        seq_along(expStartDates),
        function(k) {
          seq(expStartDates[[k]], expEndDates[[k]], by = ts)
        }
      ) |>
        do.call(c, args = _),
      lapply(
        seq_along(expStartDates),
        function(k) {
          c(
            seq(expStartDates[[k]], expEndDates[[k]], by = ts)[-1L],
            expEndDates2[[k]]
          )
        }
      ) |>
        do.call(c, args = _)
    )

    # Remove a last partial week (unless end of year)
    netb <- length(expectedTimeBounds[[2L]])
    tmp <- as.POSIXlt(expectedTimeBounds[[2L]][[netb]])
    if (!(tmp$mon == 0L && tmp$mday == 1L)) {
      if (as.integer(diff(expectedTimeBounds[[2L]][c(netb - 1L, netb)])) < 7L) {
        expectedTimeBounds[[2L]] <- expectedTimeBounds[[2L]][-netb]
      }
    }

  } else {
    expStartDate <- as.POSIXct(paste0(startYear, "-01-01"), tz = "UTC")

    expEndDate <- if (
      is.null(earlyEndDate) || isTRUE(endYear < earlyEndDate[["year"]])
    ) {
      as.POSIXct(paste0(endYear, "-12-31"), tz = "UTC")
    } else {
      as.POSIXct(
        as.Date(paste(earlyEndDate, collapse = "-"), format = "%Y-%j"),
        tz = "UTC"
      )
    }

    expectedTimeBounds <- list(
      seq(expStartDate, expEndDate, by = ts),
      seq(expStartDate, expEndDate + 86400L, by = ts)[-1L]
    )
  }

  neds <- seq_len(min(lengths(expectedTimeBounds)))
  expectedTimeBounds <- lapply(expectedTimeBounds, function(x) x[neds])

  expectedDates <- rowMeans(
    cbind(expectedTimeBounds[[1L]], expectedTimeBounds[[2L]])
  ) |>
    as.POSIXct(tz = "UTC", origin = "1970-01-01")

  # Dates to check
  timeDates <- RNetCDF::utcal.nc(
    value = timeValues, unitstring = timeUnits, type = "c"
  )

  # Compare dates
  resMsg <- all.equal(expectedDates, timeDates)

  # Date bounds
  if (isTRUE(resMsg) && !is.null(timeBoundValues)) {
    # Date bounds to check
    timeBounds <- lapply(
      list(timeBoundValues[1L, ], timeBoundValues[2L, ]),
      function(x) {
        RNetCDF::utcal.nc(value = x, unitstring = timeUnits, type = "c")
      }
    )

    # Compare date bounds
    resMsg <- all.equal(expectedTimeBounds, timeBounds)
  }

  resMsg
}

#' Convert dates to the format YYYY-dayOfYear
datesToYearDoy <- function(x) {
  yrs <- strsplit(as.character(x), split = "-", fixed = TRUE) |>
    vapply(function(x) as.integer(x[[1L]]), FUN.VALUE = NA_integer_)
  dpy <- lapply(table(yrs), function(n) seq_len(n)) |> unlist()
  paste(yrs, dpy, sep = "-")
}


#' Identify shared dates even if different units or calendars
#'
#' @param methodLeapDay Method for leap days.
#'   - `"CF"` treats calendars as is in regards to leap days (February 29)
#'   - `"SW"` represents how SOILWAT2 translates calendars used in inputs
#'       to the internally used standard calendar (also used for output),
#'       see details.
#'
#' @section Details:
#'  - `"noleap"` calendars: SOILWAT2 interprets inputs for days 60 and 365
#'    during a year that has a leap day in the standard calendar (e.g., 1980)
#'    as February 29 and December 30.
#'    The `"noleap"` calendar interprets them as February 28 and December 31.
#'  - `"all_leap"` calendars: SOILWAT2 ignores inputs for day 366 during years
#'    that have no leap day in the standard calendar (e.g., 1981).
sharedDates <- function(
  timeValues1,
  timeUnits1,
  calendar1,
  timeValues2,
  timeUnits2,
  calendar2,
  methodLeapDay = c("CF", "SW2")
) {
  methodLeapDay <- match.arg(methodLeapDay)

  if (requireNamespace("CFtime", quietly = TRUE)) {
    calendar1 <- cleanCalendar(calendar1)
    calendar2 <- cleanCalendar(calendar2)

    t1 <- CFtime::CFtime(
      definition = timeUnits1, calendar = calendar1, offsets = timeValues1
    ) |>
      CFtime::as_timestamp(format = "date")

    t2 <- CFtime::CFtime(
      definition = timeUnits2, calendar = calendar2, offsets = timeValues2
    ) |>
      CFtime::as_timestamp(format = "date")

    if (!identical(calendar1, calendar2) && identical(methodLeapDay, "SW2")) {
      # Convert dates to SOILWAT2 compatible YYYY-dayOfYear
      t1 <- datesToYearDoy(t1)
      t2 <- datesToYearDoy(t2)
    }

  } else {
    stopifnot(requireNamespace("RNetCDF"))

    acceptableCalendars <- c("standard", "gregorian", "proleptic_gregorian")

    if (!all(c(calendar1, calendar2) %in% acceptableCalendars)) {
      stop(
        "Install R package CFtime for ncTestRuns with non-standard calendars.",
        call. = FALSE
      )
    }

    t1 <- RNetCDF::utcal.nc(
      value = timeValues1, unitstring = timeUnits1, type = "c"
    ) |>
      as.Date() |>
      as.integer()

    t2 <- RNetCDF::utcal.nc(
      value = timeValues2, unitstring = timeUnits2, type = "c"
    ) |>
      as.Date() |>
      as.integer()
  }

  tShared <- intersect(t1, t2)

  list(
    sharedDates1 = which(t1 %in% tShared),
    sharedDates2 = which(t2 %in% tShared)
  )
}

#' Subset time temporally
temporalSubsetNC <- function(x, xTime, usedTimeSteps) {
  stopifnot(setequal(names(x), names(xTime)))

  res <- x

  msgFmt <-
    "Cannot have a total of %d dimension(s) while time is at position %d."

  for (kv in names(x)) {
    if (!is.null(xTime[[kv]]) && !is.null(xTime[[kv]][["nDim"]])) {
      nDims <- 1L + length(dim(x[[kv]]))

      if (isTRUE(xTime[[kv]][["idDim"]] == 1L)) {
        res[[kv]] <- switch(
          EXPR = nDims,
          x[[kv]][usedTimeSteps],
          x[[kv]][usedTimeSteps, drop = FALSE],
          x[[kv]][usedTimeSteps, , drop = FALSE],
          x[[kv]][usedTimeSteps, , , drop = FALSE],
          x[[kv]][usedTimeSteps, , , , drop = FALSE],
          x[[kv]][usedTimeSteps, , , , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims - 1L, call. = FALSE)
        )
      } else if (isTRUE(xTime[[kv]][["idDim"]] == 2L)) {
        res[[kv]] <- switch(
          EXPR = nDims,
          stop(sprintf(msgFmt, 0L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 1L, xTime[[kv]][["idDim"]])),
          x[[kv]][, usedTimeSteps, drop = FALSE],
          x[[kv]][, usedTimeSteps, , drop = FALSE],
          x[[kv]][, usedTimeSteps, , , drop = FALSE],
          x[[kv]][, usedTimeSteps, , , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims - 1L, call. = FALSE)
        )

      } else if (isTRUE(xTime[[kv]][["idDim"]] == 3L)) {
        res[[kv]] <- switch(
          EXPR = nDims,
          stop(sprintf(msgFmt, 0L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 1L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 2L, xTime[[kv]][["idDim"]])),
          x[[kv]][, , usedTimeSteps, drop = FALSE],
          x[[kv]][, , usedTimeSteps, , drop = FALSE],
          x[[kv]][, , usedTimeSteps, , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims - 1L, call. = FALSE)
        )

      } else if (isTRUE(xTime[[kv]][["idDim"]] == 4L)) {
        res[[kv]] <- switch(
          EXPR = nDims,
          stop(sprintf(msgFmt, 0L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 1L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 2L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 3L, xTime[[kv]][["idDim"]])),
          x[[kv]][, , , usedTimeSteps, drop = FALSE],
          x[[kv]][, , , usedTimeSteps, , drop = FALSE],
          x[[kv]][, , , usedTimeSteps, , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims - 1L, call. = FALSE)
        )

      } else if (isTRUE(xTime[[kv]][["idDim"]] == 5L)) {
        res[[kv]] <- switch(
          EXPR = nDims,
          stop(sprintf(msgFmt, 0L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 1L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 2L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 3L, xTime[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 4L, xTime[[kv]][["idDim"]])),
          x[[kv]][, , , , usedTimeSteps, drop = FALSE],
          x[[kv]][, , , , usedTimeSteps, , drop = FALSE],
          x[[kv]][, , , , usedTimeSteps, , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims - 1L, call. = FALSE)
        )

      } else {
        stop(
          "Position of time dimension at ",
          xTime[[kv]][["idDim"]],
          " is not implemented."
        )
      }
    }
  }

  res
}

#' Subset to example site and subset vertically
subsetNC <- function(
  x,
  ref,
  xdom,
  xid,
  limitVerticalToRef = FALSE,
  refVertical = NULL,
  xVertical = NULL
) {
  res <- x

  sizeDom <- dim(xdom)
  if (is.null(sizeDom)) sizeDom <- length(xdom)
  isGridded <- length(sizeDom) == 2L
  nDimDom <- length(sizeDom)
  tagDom <- paste(sizeDom, collapse = "x")

  stopifnot(length(xid) == nDimDom)

  if (missing(ref) || is.null(ref)) {
    dim_ref <- NULL
    dim_x <- lapply(x, dim)
    vars_toSubset <- names(dim_x)

  } else {
    ids <- intersect(names(x), names(ref))
    dim_ref <- lapply(ref[ids], dim)
    dim_x <- lapply(x[ids], dim)

    dim_diffs <- vector(length = length(dim_x))
    for (k in seq_along(dim_x)) {
      dim_diffs[[k]] <- !identical(dim_ref[[k]], dim_x[[k]])
    }

    vars_toSubset <- names(dim_x)[dim_diffs]
  }

  msgFmt <- paste(
    "Cannot have a total of %d dimension(s)",
    "while vertical is at position %d."
  )

  for (kv in vars_toSubset) {
    nDims <- length(dim_x[[kv]])
    xv <- x[[kv]]

    # Subset to comparable soil layers with reference
    if (
      isTRUE(limitVerticalToRef) &&
        !is.null(xVertical[[kv]]) && !is.null(refVertical[[kv]]) &&
        !is.null(xVertical[[kv]][["nDim"]]) &&
        !is.null(refVertical[[kv]][["nDim"]]) &&
        xVertical[[kv]][["nDim"]] > refVertical[[kv]][["nDim"]]
    ) {
      usedVertical <- seq_len(refVertical[[kv]][["nDim"]])

      if (isTRUE(xVertical[[kv]][["idDim"]] == 1L)) {
        xv <- switch(
          EXPR = nDims,
          xv[usedVertical, drop = FALSE],
          xv[usedVertical, , drop = FALSE],
          xv[usedVertical, , , drop = FALSE],
          xv[usedVertical, , , , drop = FALSE],
          xv[usedVertical, , , , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims, call. = FALSE)
        )
      } else if (isTRUE(xVertical[[kv]][["idDim"]] == 2L)) {
        xv <- switch(
          EXPR = nDims,
          stop(sprintf(msgFmt, 1L, xVertical[[kv]][["idDim"]])),
          xv[, usedVertical, drop = FALSE],
          xv[, usedVertical, , drop = FALSE],
          xv[, usedVertical, , , drop = FALSE],
          xv[, usedVertical, , , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims, call. = FALSE)
        )

      } else if (isTRUE(xVertical[[kv]][["idDim"]] == 3L)) {
        xv <- switch(
          EXPR = nDims,
          stop(sprintf(msgFmt, 1L, xVertical[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 2L, xVertical[[kv]][["idDim"]])),
          xv[, , usedVertical, drop = FALSE],
          xv[, , usedVertical, , drop = FALSE],
          xv[, , usedVertical, , , drop = FALSE],
          xv[, , usedVertical, , , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims, call. = FALSE)
        )

      } else if (isTRUE(xVertical[[kv]][["idDim"]] == 4L)) {
        xv <- switch(
          EXPR = nDims,
          stop(sprintf(msgFmt, 1L, xVertical[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 2L, xVertical[[kv]][["idDim"]])),
          stop(sprintf(msgFmt, 3L, xVertical[[kv]][["idDim"]])),
          xv[, , , usedVertical, drop = FALSE],
          xv[, , , usedVertical, , drop = FALSE],
          xv[, , , usedVertical, , , drop = FALSE],
          xv[, , , usedVertical, , , , drop = FALSE],
          stop("Not implemented for dimensions n = ", nDims, call. = FALSE)
        )

      } else {
        stop(
          "Position of vertical dimension at ",
          xVertical[[kv]][["idDim"]],
          " is not implemented."
        )
      }
    }

    # Identify which dimensions in output identify spatial domain
    tagVar <- paste(dim_x[[kv]], collapse = "x")
    if (length(paste0(tagDom, "$")) > 1L) message(paste0(tagDom, "$"))
    ids <- gregexpr(pattern = paste0(tagDom, "$"), text = tagVar)[[1L]]

    if (isTRUE(ids[[1L]] < 0L)) {
      # domain dimensions are not the right-most dimensions -> transpose
      ids <- gregexpr(pattern = tagDom, text = tagVar)[[1L]]
      tmp <- gregexpr("x", text = substr(tagVar, 1L, ids), fixed = TRUE)[[1L]]
      id1 <- if (all(ids > 0L)) 1L + sum(tmp > 0L)
      idsDimDomain <- c(id1, if (isGridded) id1 + 1L)
      tmp <- seq_len(nDims)
      xv <- aperm(xv, perm = c(tmp[-idsDimDomain], idsDimDomain))
      dim_x[[kv]] <- dim(xv)
      tagVar <- paste(dim_x[[kv]], collapse = "x")
      if (length(paste0(tagDom, "$")) > 1L) message(paste0(tagDom, "$"))
      ids <- gregexpr(pattern = paste0(tagDom, "$"), text = tagVar)[[1L]]
    }

    tmp <- gregexpr("x", text = substr(tagVar, 1L, ids), fixed = TRUE)[[1L]]
    id1 <- if (all(ids > 0L)) 1L + sum(tmp > 0L)
    idsDimDomain <- c(id1, if (isGridded) id1 + 1L)

    # Implemented only if domain dimensions are right-most dimensions
    stopifnot(idsDimDomain[[length(idsDimDomain)]] == nDims)

    tmp <- if (isGridded) {
      switch(
        EXPR = nDims,
        stop("Gridded output should not have one dimension."),
        xv[xid[[1L]], xid[[2L]]],
        xv[, xid[[1L]], xid[[2L]]],
        xv[, , xid[[1L]], xid[[2L]]],
        xv[, , , xid[[1L]], xid[[2L]]],
        xv[, , , , xid[[1L]], xid[[2L]]]
      )
    } else {
      switch(
        EXPR = nDims,
        xv[xid],
        xv[, xid],
        xv[, , xid],
        xv[, , , xid],
        xv[, , , , xid]
      )
    }

    res[[kv]] <- if (is.null(dim_ref[[kv]]) || !any(dim_ref[[kv]] == 1L)) {
      tmp
    } else {
      array(tmp, dim = dim_ref[[kv]]) # add drop degenerate dimension
    }
  }

  res
}

getSitesFromNC <- function(fn) {
  stopifnot(requireNamespace("RNetCDF"))

  nc <- RNetCDF::open.nc(fn)
  on.exit(RNetCDF::close.nc(nc), add = TRUE)
  # collapse = T: 1x1 -> 1
  x <- RNetCDF::read.nc(nc, collapse = FALSE, unpack = TRUE)

  isProjected <- "crs_projsc" %in% names(x)
  var_crs <- if (isProjected) "crs_projsc" else "crs_geogsc"
  crs <- RNetCDF::att.get.nc(nc, var_crs, attribute = "crs_wkt") |>
    sf::st_crs()

  isSites <- "site" %in% names(x)

  vars_xy <- if (isProjected) c("x", "y") else c("lon", "lat")
  xyvals <- x[vars_xy]

  xy <- if (isSites) {
    data.frame(xyvals)
  } else {
    ds <- lapply(xyvals, dim)
    if (length(ds[[1L]]) == 2L && isTRUE(all.equal(ds[[1L]], ds[[2L]]))) {
      lapply(xyvals, as.vector) |> data.frame()
    } else {
      expand.grid(xyvals)
    }
  }

  as.matrix(xy) |>
    sf::st_multipoint() |>
    sf::st_sfc(crs = crs) |>
    sf::st_cast(to = "POINT")
}


listInputWeather <- function(var, intsv, path) {
  ids <- which(intsv[["SW2 variable"]] == var)[[1L]]

  fname <- list.files(
    path = file.path(path, dirname(intsv[ids, "ncFileName"])),
    pattern = paste0(
      "\\<",
      strsplit(
        basename(intsv[ids, "ncFileName"]), split = "%", fixed = TRUE
      )[[1L]][[1L]]
    ),
    full.names = TRUE
  )

  lookup <- list.files(
    path = file.path(path, "Input_nc", "inWeather"),
    pattern = "index_weather.nc",
    full.names = TRUE
  )

  list(
    fname = if (length(fname) > 1L) fname[[1L]] else fname,
    var = intsv[ids, "ncVarName"],
    units = intsv[ids, "ncVarUnits"],
    ncTAxisName = intsv[ids, "ncTAxisName"],
    lookup = if (length(lookup) > 1L) lookup[[1L]] else lookup
  )
}

listOutputWeather <- function(var, varTag, path) {
  fname <- list.files(
    path = path,
    pattern = paste0("^", varTag, "_[[:print:]]+_day.nc$"),
    full.names = TRUE
  )

  res <- list(
    fname = if (length(fname) > 1L) fname[[1L]] else fname,
    var = var
  )

  c(res, list(units = readUnitsAttributeNC(res[["fname"]], var)))
}


zeroOutNestedList <- function(x) {
  tmp <- unlist(as.relistable(x))
  tmp[] <- 0
  relist(tmp)
}

getDimInfoNC <- function(nc, vars, dimName) {
  stopifnot(requireNamespace("RNetCDF"))

  res <- vector(mode = "list", length = length(vars))
  names(res) <- vars

  nDims <- RNetCDF::file.inq.nc(nc)[["ndims"]]
  dimInfo <- lapply(
    seq_len(nDims) - 1L, function(id) RNetCDF::dim.inq.nc(nc, id)
  )

  idDim <- NULL
  nDim <- NULL

  for (k in seq_len(nDims)) {
    if (isTRUE(identical(dimInfo[[k]][["name"]], dimName))) {
      idDim <- dimInfo[[k]][["id"]]
      nDim <- dimInfo[[k]][["length"]]
      break
    }
  }

  if (is.null(idDim) && is.null(nDim)) return(res)

  for (k in seq_along(vars)) {
    tmp <- RNetCDF::var.inq.nc(nc, variable = vars[[k]])[["dimids"]]

    idDimRequested <- which(tmp == idDim)
    hasDim <- length(idDimRequested) > 0L

    res[[k]] <- list(
      idDim = if (hasDim) idDimRequested,
      nDim = if (hasDim) nDim
    )
  }

  res
}

compareNC <- function(
  fn,
  path,
  vars_required,
  vars_other,
  checkMethod = c("values", "valuesFirst365", "structure"),
  idExampleSite = 1L,
  limitVerticalToRef = FALSE,
  simStartYear = NULL,
  simEndYear = NULL,
  earlyEndDate = NULL,
  tolerance = sqrt(.Machine[["double.eps"]])
) {
  stopifnot(requireNamespace("RNetCDF"))
  checkMethod <- match.arg(checkMethod)

  resMsg <- NULL

  ncref <- RNetCDF::open.nc(fn)
  on.exit(RNetCDF::close.nc(ncref), add = TRUE)
  # collapse = T: 1x1 -> 1
  xref <- RNetCDF::read.nc(ncref, collapse = FALSE, unpack = TRUE)

  nc2 <- RNetCDF::open.nc(file.path(path, basename(fn)))
  on.exit(RNetCDF::close.nc(nc2), add = TRUE)
  x2 <- RNetCDF::read.nc(nc2, collapse = FALSE, unpack = TRUE)
  x2TimeUnits <- RNetCDF::att.get.nc(nc2, "time", attribute = "units")
  x2Calendar <- RNetCDF::att.get.nc(nc2, "time", attribute = "calendar")

  vars_shared <- intersect(names(xref), names(x2))
  vars_test <- setdiff(vars_shared, vars_other)

  if (all(vars_required %in% vars_shared) && length(vars_test) > 0L) {
    # Check time values of current simulation
    tmp <- regmatches(
      x = basename(fn), m = regexec("[0-9]{4}-[0-9]{4}", basename(fn))
    )
    yrs <- as.integer(strsplit(tmp[[1L]], split = "-", fixed = TRUE)[[1L]])

    resMsg <- allEqualTimeValues(
      timeValues = x2[["time"]],
      timeBoundValues = x2[["time_bnds"]],
      timeUnits = x2TimeUnits,
      timeCalendar = x2Calendar,
      startYear = max(simStartYear, yrs[[1L]]),
      endYear = min(simEndYear, yrs[[2L]]),
      earlyEndDate = earlyEndDate
    )

    if (isTRUE(resMsg)) {
      # Identify shared time and subset
      tmpTime <- sharedDates(
        timeValues1 = xref[["time"]],
        timeUnits1 = RNetCDF::att.get.nc(ncref, "time", attribute = "units"),
        calendar1 = RNetCDF::att.get.nc(ncref, "time", attribute = "calendar"),
        timeValues2 = x2[["time"]],
        timeUnits2 = x2TimeUnits,
        calendar2 = x2Calendar,
        methodLeapDay = "SW2"
      )

      if (identical(checkMethod, "valuesFirst365")) {
        ts <- timeStep(x2[["time"]])
        isStartYearLeap <- rSW2utils::isLeapYear(simStartYear)
        checkYear <- any(
          identical(ts, "year") && !isStartYearLeap,
          !identical(ts, "year")
        )
        if (identical(simStartYear, yrs[[1L]]) && checkYear) {
          tmpTime <- lapply(
            tmpTime,
            function(x) {
              switch(
                EXPR = ts,
                day = x[seq_len(365L)],
                week = x[seq_len(52L)],
                month = x[seq_len(11L + if (isStartYearLeap) 0L else 1L)],
                year = x[if (isStartYearLeap) 0L else 1L]
              )
            }
          )
        } else {
          checkMethod <- "structure"
        }
      }

      # Subset to shared time
      targetVals <- temporalSubsetNC(
        x = xref[vars_test],
        xTime = getDimInfoNC(ncref, vars = vars_test, dimName = "time"),
        usedTimeSteps = tmpTime[["sharedDates1"]]
      )
      currentVals <- temporalSubsetNC(
        x = x2[vars_test],
        xTime = getDimInfoNC(nc2, vars = vars_test, dimName = "time"),
        usedTimeSteps = tmpTime[["sharedDates2"]]
      )

      # Subset current simulation to match space + vertical of target
      currentVals <- subsetNC(
        currentVals,
        ref = targetVals,
        xdom = x2[["domain"]],
        xid = findExampleSiteIndex(idExampleSite, x2[["domain"]]),
        limitVerticalToRef = limitVerticalToRef,
        refVertical = if (limitVerticalToRef) {
          getDimInfoNC(ncref, vars = vars_test, dimName = "vertical")
        },
        xVertical = if (limitVerticalToRef) {
          getDimInfoNC(nc2, vars = vars_test, dimName = "vertical")
        }
      )

      # Compare current with target
      msg <- if (grepl("values", checkMethod, fixed = TRUE)) {
        all.equal(
          target = targetVals, current = currentVals, tolerance = tolerance
        )
      } else {
        # Don't check values --> set all values to 0
        all.equal(
          target = zeroOutNestedList(targetVals),
          current = zeroOutNestedList(currentVals),
          tolerance = tolerance
        )
      }

      resMsg <- if (isTRUE(msg)) {
        ""
      } else {
        paste(
          shQuote(basename(fn)), "is not equal to reference:", toString(msg)
        )
      }
    }

  } else {
    resMsg <- paste(
      shQuote(basename(fn)), "has missing variable(s):",
      toString(setdiff(vars_required, vars_shared))
    )
  }

  resMsg
}


compareNCWeather <- function(
  input,
  output,
  idExampleSite,
  tolerance = sqrt(.Machine[["double.eps"]])
) {
  stopifnot(
    requireNamespace("RNetCDF"),
    requireNamespace("units")
  )

  resMsg <- NULL

  if (length(input[["fname"]]) == 0L) {
    resMsg <- "No input."
  } else if (length(output[["fname"]]) == 0L) {
    resMsg <- "No output."
  }

  if (isTRUE(nzchar(resMsg))) return(resMsg)

  ncin <- RNetCDF::open.nc(input[["fname"]])
  on.exit(RNetCDF::close.nc(ncin), add = TRUE)
  xin <- RNetCDF::read.nc(ncin, collapse = FALSE, unpack = TRUE)

  xlk <- if (
    length(input[["lookup"]]) == 1L && file.exists(input[["lookup"]])
  ) {
    nclk <- RNetCDF::open.nc(input[["lookup"]])
    on.exit(RNetCDF::close.nc(nclk), add = TRUE)
    RNetCDF::read.nc(nclk, collapse = FALSE, unpack = TRUE)
  }

  ncout <- RNetCDF::open.nc(output[["fname"]])
  on.exit(RNetCDF::close.nc(ncout), add = TRUE)
  xout <- RNetCDF::read.nc(ncout, collapse = FALSE, unpack = TRUE)


  if (isTRUE(!input[["var"]] %in% names(xin))) {
    resMsg <- paste(
      shQuote(basename(input[["fname"]])),
      "has missing variable:",
      input[["var"]]
    )

  } else if (isTRUE(!output[["var"]] %in% names(xout))) {
    resMsg <- paste(
      shQuote(basename(output[["fname"]])),
      "has missing variable:",
      output[["var"]]
    )

  } else {
    # identify input domain (remove time dimension)
    inDims <- dim(xin[[input[["var"]]]])
    idTimeDim <- which(inDims == length(xin[[input[["ncTAxisName"]]]]))
    xin[["dom"]] <- array(
      seq_len(prod(inDims[-idTimeDim])),
      dim = inDims[-idTimeDim]
    )

    wIndex <- if (is.null(xlk)) {
      # input domain is identical to output domain
      findExampleSiteIndex(idExampleSite, xin[["domain"]])

    } else {
      # use index lookup to identify example site in input domain
      tmp <- subsetNC(
        xlk[c("x_index", "y_index")],
        ref = NULL,
        xdom = xlk[["domain"]],
        xid = findExampleSiteIndex(idExampleSite, xlk[["domain"]])
      )
      if (length(dim(xlk[["domain"]])) %in% 1L:2L) {
        array(
          data = 1L + c(tmp[[1L]], tmp[[2L]]), # convert base0 to base1
          dim = c(1L, 2L),
          dimnames = list(NULL, c("row", "col"))
        )
      }
    }

    # Identify shared time and subset
    inTimeName <- if (is.null(input[["ncTAxisName"]])) {
      "time"
    } else {
      input[["ncTAxisName"]]
    }
    outTimeName <- if (is.null(output[["ncTAxisName"]])) {
      "time"
    } else {
      input[["ncTAxisName"]]
    }

    tmpTime <- sharedDates(
      timeValues1 = xin[[inTimeName]],
      timeUnits1 = RNetCDF::att.get.nc(ncin, inTimeName, attribute = "units"),
      calendar1 = RNetCDF::att.get.nc(
        ncin, inTimeName, attribute = "calendar"
      ),
      timeValues2 = xout[[outTimeName]],
      timeUnits2 = RNetCDF::att.get.nc(ncout, outTimeName, attribute = "units"),
      calendar2 = RNetCDF::att.get.nc(
        ncout, outTimeName, attribute = "calendar"
      ),
      methodLeapDay = "SW2"
    )

    # Subset to shared time
    targetVals <- temporalSubsetNC(
      x = xin[input[["var"]]],
      xTime = getDimInfoNC(ncin, vars = input[["var"]], dimName = inTimeName),
      usedTimeSteps = tmpTime[["sharedDates1"]]
    )
    currentVals <- temporalSubsetNC(
      x = xout[output[["var"]]],
      xTime = getDimInfoNC(ncout, vars = output[["var"]], dimName = outTimeName),
      usedTimeSteps = tmpTime[["sharedDates2"]]
    )

    # Subset simulation to example site
    targetVals <- subsetNC(
      targetVals,
      ref = NULL,
      xdom = xin[["dom"]],
      xid = wIndex
    )[[input[["var"]]]]

    currentVals <- subsetNC(
      currentVals,
      ref = NULL,
      xdom = xout[["domain"]],
      xid = findExampleSiteIndex(idExampleSite, xout[["domain"]])
    )[[output[["var"]]]]

    # Convert units
    targetVals <- units::set_units(
      targetVals, value = input[["units"]], mode = "standard"
    ) |>
      units::set_units(value = output[["units"]], mode = "standard") |>
      units::drop_units()

    msg <- all.equal(
      target = targetVals,
      current = currentVals,
      tolerance = tolerance
    )

    if (isTRUE(msg)) {
      resMsg <- if (length(currentVals) >= 365L) {
        ""
      } else {
        paste(
          "Could not locate enough overlapping time to compare",
          "output",
          shQuote(output[["var"]]), "of", shQuote(basename(output[["fname"]])),
          "and",
          shQuote(input[["var"]]), "of", shQuote(basename(input[["fname"]]))
        )
      }

    } else {
      resMsg <- paste(
        "Output",
        shQuote(output[["var"]]), "of", shQuote(basename(output[["fname"]])),
        "is not equal to the input",
        shQuote(input[["var"]]), "of", shQuote(basename(input[["fname"]]))
      )
    }
  }

  resMsg
}

compareEqualityNCs <- function(
  dir1,
  dir2,
  tolerance = sqrt(.Machine[["double.eps"]])
) {
  stopifnot(requireNamespace("RNetCDF"))

  tag1 <- shQuote(file.path(basename(dirname(dir1)), basename(dir1)))
  tag2 <- shQuote(file.path(basename(dirname(dir2)), basename(dir2)))

  fnames1 <- list.files(path = dir1, pattern = ".nc$")
  fnames2 <- list.files(path = dir2, pattern = ".nc$")

  resMsg <- list()
  im <- 0L

  testFileNames <- if (all(basename(fnames1) %in% basename(fnames2))) {
    basename(fnames1)
  } else {
    im <- im + 1L
    resMsg[[im]] <- paste(
      "Directories differ in files:",
      "\n *", tag1, "contains files that", tag2, "does not contain:",
      toString(setdiff(basename(fnames1), basename(fnames2))),
      "\n *", tag2, "contains files that", tag1, "does not contain:",
      toString(setdiff(basename(fnames2), basename(fnames1)))
    )
    intersect(basename(fnames1), basename(fnames2))
  }

  for (k in seq_along(testFileNames)) {
    nc1 <- RNetCDF::open.nc(file.path(dir1, testFileNames[[k]]))
    nc2 <- RNetCDF::open.nc(file.path(dir2, testFileNames[[k]]))
    tmp <- all.equal(
      RNetCDF::read.nc(nc1), RNetCDF::read.nc(nc2), tolerance = tolerance
    )
    RNetCDF::close.nc(nc1)
    RNetCDF::close.nc(nc2)
    if (!isTRUE(tmp)) {
      im <- im + 1L
      resMsg[[im]] <- paste(
        "File", shQuote(testFileNames[[k]]),
        "differs between", tag1, "and", tag2, ":",
        tmp
      )
    }
  }

  if (im == 0L) TRUE else resMsg
}


#------ . ------
