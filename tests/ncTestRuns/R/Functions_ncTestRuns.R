
#------ . ------
#------ Functions ------
countDims <- function(domSizes, isGridded) {
  stopifnot(grepl("x", domSizes, fixed = TRUE) == isGridded)

  res <- vector(mode = "list", length = length(domSizes))
  res[isGridded] <- strsplit(domSizes[isGridded], split = "x", fixed = TRUE) |>
    lapply(FUN = function(x) as.integer(x))
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
    sites, y = exampleSite(crs), dist = units::set_units(1, "m"), sparse = FALSE
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
    res <- system2(
      command = "ncks",
      # -C = do not add associated variables (e.g., lat) to the extraction list
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
    )
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
    sub(" ", "", unit) # remove first space (between number and unit)
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
  line <- grep(
    pattern = if (isTRUE(classic)) tag else paste0("^", tag, " "),
    x = fin,
    ignore.case = TRUE
  )
  stopifnot(length(line) == 1L, line > 0L, line <= length(fin))
  posComment <- regexpr("#", text = fin[[line]], fixed = TRUE)
  res <- if (isTRUE(classic)) as.character(value) else paste(tag, value)
  fin[[line]] <- if (posComment > 0L) {
    paste0(
      res,
      paste(rep(" ", max(1, posComment - 1L - nchar(res))), collapse = ""),
      substr(fin[[line]], start = posComment, stop = 1e3)
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
  res <- regmatches(wkt, m = ptxts)[[1L]][1L,]
  ids <- grep(param, x = res, fixed = TRUE)
  vapply(
    strsplit(gsub("]", "", res[ids]), split = ",", fixed = TRUE),
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
  ids <- which(xtmp == paste(rep("\t", times = ncol(x) - 1L), collapse = ""))
  xtmp[ids] <- ""

  writeLines(xtmp, con = filename)
}


toggleNCInputTSV <- function(
  filename, inkeys = "all", sw2vars = NULL, value = c(1L, 0L), extraCheck = NULL
) {
  stopifnot(value %in% c(1L, 0L))

  x <- readTSV(filename)

  inuse <- if (identical(inkeys, "all")) {
    nchar(x[["SW2 input group"]]) > 0L
  } else {
    tmp <- x[["SW2 input group"]] %in% inkeys
    if (is.null(sw2vars)) {
      tmp
    } else {
      tmp & x[["SW2 variable"]] %in% sw2vars
    }
  }

  incheck <- if (
    !is.null(extraCheck) && all(c("cn", "pattern") %in% names(extraCheck))
  ) {
    grepl(pattern = extraCheck[["pattern"]], x = x[[extraCheck[["cn"]]]])
  } else {
    rep(TRUE, nrow(x))
  }

  hasvalue <- inuse & incheck & x[["Do nc-input?"]] != ""
  x[["Do nc-input?"]][hasvalue] <- value[[1L]]

  writeTSV(x, filename)
}

setNCInputTSV <- function(
  filename,
  testrun,
  inkeys,
  sw2vars = NULL,
  values = NULL,
  list_xyvars = list(),
  list_crs = list()
) {
  x <- readTSV(filename)

  if (is.null(sw2vars)) {
    inkeys0 <- inkeys
    ids <- match(x[["SW2 input group"]], inkeys0, nomatch = 0L)

    inkeys <- inkeys0[ids]
    sw2vars <- x[["SW2 variable"]][ids > 0L]
  }

  stopifnot(identical(length(inkeys), length(sw2vars)))

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
    idrow <- which(
      x[["SW2 input group"]] %in% inkeys[[k]] &
        x[["SW2 variable"]] %in% sw2vars[[k]]
    )

    if (length(idrow) != 1L) {
      stop(
        "Could not identify row in nc-inputs.tsv: ",
        "k = ", k, ", inkey = ", inkeys[[k]], ", sw2var = ", sw2vars[[k]]
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
    c(inkey = "inVeg", sw2var = "Shrubs.biomass", newUnit = "kg m-2"),
    c(inkey = "inSite", sw2var = "Tsoil_constant", newUnit = "degF")
  )
) {
  x <- readTSV(filename)

  vars <- c(
    "SW2 input group", "SW2 variable", "SW2 units", "ncVarName", "ncVarUnits"
  )
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

  if (length(idrow) != 1L) {
    stop(
      "Could not identify row in nc-inputs.tsv: ",
      "inkey = ", inkey, ", ncvar = ", ncvar
    )
  }

  as.list(x[idrow, c("ncVarUnits", "ncVarUnitsModified")])
}


runSW2 <- function(sw2, path_inputs, renameDomainTemplate = FALSE) {
  msg <- NULL
  res <- withCallingHandlers(
    tryCatch(
      system2(
        command = sw2,
        args = paste(
          "-d", path_inputs,
          "-f files.in",
          if (isTRUE(renameDomainTemplate)) "-r"
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


#--- * Manipulate input netCDFs ------

updateGAttSourceVersion <- function(nc) {
  stopifnot(requireNamespace("RNetCDF"))
  RNetCDF::att.put.nc(
    nc,
    variable = "NC_GLOBAL", name = "source", type = "NC_CHAR",
    value = "SOILWAT2v8.1.0"
  )
}

updateGAttFrequency <- function(nc, frq) {
  stopifnot(requireNamespace("RNetCDF"))
  RNetCDF::att.put.nc(
    nc,
    variable = "NC_GLOBAL", name = "frequency", type = "NC_CHAR",
    value = frq
  )
}

updateGAttFeatureType <- function(nc, type) {
  stopifnot(requireNamespace("RNetCDF"))
  RNetCDF::att.put.nc(
    nc,
    variable = "NC_GLOBAL", name = "featureType", type = "NC_CHAR",
    value = type
  )
}

# 4.3.2. Dimensionless Vertical Coordinate (deprecated)
# The units attribute is not required for dimensionless coordinates. For
# backwards compatibility with COARDS we continue to allow the units attribute
# to take one of the values: level, layer, or sigma_level. These values are not
# recognized by the UDUNITS package, and are considered a deprecated feature in
# the CF standard.
createVerticalAxisNC <- function(nc, nlyrs) {
  stopifnot(requireNamespace("RNetCDF"))
  RNetCDF::dim.def.nc(nc, dimname = "vertical", dimlength = nlyrs)
  RNetCDF::var.def.nc(
    nc,
    varname = "vertical", vartype = "NC_INT", dimensions = "vertical"
  )
  # RNetCDF::att.put.nc(
  #   nc, variable = "vertical", name = "units", type = "NC_CHAR",
  #   value = "layer"
  # )
  RNetCDF::att.put.nc(
    nc,
    variable = "vertical", name = "long_name", type = "NC_CHAR",
    value = "soil layer"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "vertical", name = "axis", type = "NC_CHAR",
    value = "Z"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "vertical", name = "standard_name", type = "NC_CHAR",
    value = "depth"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "vertical", name = "positive", type = "NC_CHAR",
    value = "down"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "vertical", name = "comment", type = "NC_CHAR",
    value = "dimensionless vertical coordinate"
  )
  RNetCDF::var.put.nc(
    nc,
    variable = "vertical",
    data = seq_len(nlyrs)
  )
}


createPFTAxisNC <- function(nc, pfts) {
  stopifnot(requireNamespace("RNetCDF"))
  RNetCDF::dim.def.nc(nc, dimname = "pft", dimlength = length(pfts))
  RNetCDF::var.def.nc(
    nc,
    varname = "pft", vartype = "NC_STRING", dimensions = "pft"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "pft", name = "standard_name", type = "NC_CHAR",
    value = "biological_taxon_name"
  )
  RNetCDF::var.put.nc(nc, variable = "pft", data = pfts)
}

createTimeAxisNC <- function(nc, startYear, timeValues, calendar = "standard") {
  stopifnot(requireNamespace("RNetCDF"))
  RNetCDF::dim.def.nc(nc, dimname = "time", dimlength = length(timeValues))
  RNetCDF::var.def.nc(
    nc,
    varname = "time", vartype = "NC_FLOAT", dimensions = "time"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "time", name = "units", type = "NC_CHAR",
    value = paste0("days since ", startYear, "-01-01 00:00:00")
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "time", name = "long_name", type = "NC_CHAR",
    value = "time"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "time", name = "axis", type = "NC_CHAR",
    value = "T"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "time", name = "standard_name", type = "NC_CHAR",
    value = "time"
  )
  RNetCDF::att.put.nc(
    nc,
    variable = "time", name = "calendar", type = "NC_CHAR",
    value = as.character(calendar)
  )
  RNetCDF::var.put.nc(nc, variable = "time", data = timeValues)
}


createMonthClimatologyAxisNC <- function(nc, startYear, endYear) {
  stopifnot(requireNamespace("RNetCDF"))
  time_unit <- paste0("days since ", startYear, "-01-01 00:00:00")

  tv <- RNetCDF::utinvcal.nc(
    time_unit,
    value = seq(
      from = as.Date(paste0(startYear, "-01-16")),
      to = as.Date(paste0(startYear, "-12-16")),
      by = "month"
    ) |> as.POSIXct() # mid-month
  )

  createTimeAxisNC(
    nc,
    startYear = startYear,
    timeValues = tv
  )

  RNetCDF::att.put.nc(
    nc,
    variable = "time", name = "climatology", type = "NC_CHAR",
    value = "climatology_bounds"
  )

  res <- try(RNetCDF::dim.inq.nc(nc, "bnds"), silent = TRUE)
  if (inherits(res, "try-error") && grepl("Invalid dimension", res)) {
    RNetCDF::dim.def.nc(nc, dimname = "bnds", dimlength = 2L)
  }

  RNetCDF::var.def.nc(
    nc,
    varname = "climatology_bounds", vartype = "NC_DOUBLE",
    dimensions = c("bnds", "time")
  )


  cbnds <- rbind(
    start = RNetCDF::utinvcal.nc(
      time_unit,
      value = as.POSIXct(
        as.Date(paste0(startYear, "-", seq_len(12L), "-01"))
      )
    ),
    end = RNetCDF::utinvcal.nc(
      time_unit,
      value = as.POSIXct(
        c(
          as.Date(paste0(endYear, "-", seq_len(12L)[-1L], "-01")),
          as.Date(paste0(endYear + 1L, "-01-01"))
        ) - 1L
      )
    )
  )

  RNetCDF::var.put.nc(
    nc,
    variable = "climatology_bounds",
    data = cbnds,
    count = c(2L, 12L)
  )
}


createVarNC <- function(
    nc, varname, dimensions, units, long_name = varname,
  vartype = c("NC_DOUBLE", "NC_FLOAT")
) {
  stopifnot(requireNamespace("RNetCDF"))
  vartype <- match.arg(vartype)

  RNetCDF::var.def.nc(
    nc,
    varname = varname, vartype = vartype, dimensions = dimensions
  )
  RNetCDF::att.put.nc(
    nc,
    variable = varname, name = "long_name", type = "NC_CHAR",
    value = long_name
  )
  RNetCDF::att.put.nc(
    nc,
    variable = varname, name = "units", type = "NC_CHAR",
    value = units
  )
}


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
  seed = 127
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
  xNSoilLayers <- array(data = as.integer(nMinSoilLayers), dim = dims[-1])

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
    hzthkArray[ithk] <- 2 * hzthkArray[ithk] # double thickness
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
  idsvert <- which(nchar(tmp) == 0L & !tmp %in% names(dimPermCounts))
  names(dim(res))[[idsvert]] <- "vertical"

  if (!all(names(dim(res)) == names(dimPermCounts))) {
    # apply rearranges dimensions -> permutate to desired dimension order
    res <- aperm(res, perm = match(names(dim(res)), names(dimPermCounts)))
  }

  stopifnot(all.equal(dim(res), dimPermCounts))

  res
}


#--- * Functions to work with testRun outputs ------

appendToMessage <- function(hasMsg, newMsg) {
  if (isTRUE(nchar(hasMsg) > 0L)) {
    if (isTRUE(nchar(newMsg) > 0L)) {
      paste(hasMsg, newMsg, sep = " -- ")
    } else {
      hasMsg
    }
  } else {
    newMsg
  }
}

colorTestReport <- function(x) {
  stopifnot(requireNamespace("cli"))

  gsub("\\<ok\\>", cli::col_green("ok"), x = x) |>
    gsub("\\<failed\\>", cli::col_red("failed"), x = _) |>
    gsub("\\<missing\\>", cli::col_yellow("missing"), x = _)
}


printColoredDF <- function(x, vars) {
  res <- apply(
    rbind(vars, res[, vars, drop = FALSE]),
    MARGIN = 2L,
    FUN = format,
    justify = "right"
  )

  for (kr in seq_len(nrow(res))) {
    cat(
      colorTestReport(paste0(res[kr, ], collapse = " ")),
      fill = TRUE
    )
  }
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

  for (kv in vars_toSubset) {
    nDims <- length(dim_x[[kv]])
    xv <- x[[kv]]

    # Subset to comparable soil layers with reference
    if (
      isTRUE(limitVerticalToRef) &&
        !is.null(xVertical[[kv]]) && !is.null(refVertical[[kv]]) &&
        !is.null(xVertical[[kv]][["nVertical"]]) &&
        !is.null(refVertical[[kv]][["nVertical"]]) &&
        xVertical[[kv]][["nVertical"]] > refVertical[[kv]][["nVertical"]]
    ) {
      usedVertical <- seq_len(refVertical[[kv]][["nVertical"]])

      if (isTRUE(xVertical[[kv]][["idDimVertical"]] == 1L)) {
        xv <- switch(
          EXPR = nDims,
          xv[usedVertical, drop = FALSE],
          xv[usedVertical, , drop = FALSE],
          xv[usedVertical, , , drop = FALSE],
          xv[usedVertical, , , , drop = FALSE],
          xv[usedVertical, , , , , drop = FALSE]
        )
      } else if (isTRUE(xVertical[[kv]][["idDimVertical"]] == 2L)) {
        xv <- switch(
          EXPR = nDims,
          stop(
            "Cannot have a total of one dimension but vertical is at position 2."
          ),
          xv[, usedVertical, drop = FALSE],
          xv[, usedVertical, , drop = FALSE],
          xv[, usedVertical, , , drop = FALSE],
          xv[, usedVertical, , , , drop = FALSE]
        )

      } else {
        stop(
          "Position of vertical dimension at ",
          xVertical[[kv]][["idDimVertical"]],
          " is not implemented."
        )
      }
    }

    # Identify which dimensions in output identify spatial domain
    tagVar <- paste(dim_x[[kv]], collapse = "x")
    if (length(paste0(tagDom, "$")) > 1) print(paste0(tagDom, "$"))
    ids <- gregexpr(pattern = paste0(tagDom, "$"), text = tagVar)[[1L]]

    if (isTRUE(ids[[1L]] < 0)) {
      # domain dimensions are not the right-most dimensions -> transpose
      ids <- gregexpr(pattern = tagDom, text = tagVar)[[1L]]
      tmp <- gregexpr("x", text = substr(tagVar, 1L, ids), fixed = TRUE)[[1L]]
      id1 <- if (all(ids > 0L)) 1L + sum(tmp > 0L)
      idsDimDomain <- c(id1, if (isGridded) id1 + 1L)
      tmp <- seq_len(nDims)
      xv <- aperm(xv, perm = c(tmp[-idsDimDomain], idsDimDomain))
      dim_x[[kv]] <- dim(xv)
      tagVar <- paste(dim_x[[kv]], collapse = "x")
      if (length(paste0(tagDom, "$")) > 1) print(paste0(tagDom, "$"))
      ids <- gregexpr(pattern = paste0(tagDom, "$"), text = tagVar)[[1L]]
    }

    tmp <- gregexpr("x", text = substr(tagVar, 1L, ids), fixed = TRUE)[[1L]]
    id1 <- if (all(ids > 0L)) 1L + sum(tmp > 0L)
    idsDimDomain <- c(id1, if (isGridded) id1 + 1L)

    # Implemented only if domain dimensions are right-most dimensions
    stopifnot(idsDimDomain[[length(idsDimDomain)]] == nDims)

    res[[kv]] <- if (isGridded) {
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
    if (length(ds[[1L]]) == 2L && isTRUE(all.equal(ds[[1]], ds[[2L]]))) {
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

getVerticalNC <- function(nc, vars) {
  stopifnot(requireNamespace("RNetCDF"))

  res <- vector(mode = "list", length = length(vars))
  names(res) <- vars

  nDims <- RNetCDF::file.inq.nc(nc)[["ndims"]]
  dimInfo <- lapply(
    seq_len(nDims) - 1L, function(id) RNetCDF::dim.inq.nc(nc, id)
  )

  idDimVertical <- NULL
  nVertical <- NULL

  for (k in seq_len(nDims)) {
    if (isTRUE(dimInfo[[k]][["name"]] == "vertical")) {
      idDimVertical <- dimInfo[[k]][["id"]]
      nVertical <- dimInfo[[k]][["length"]]
      break
    }
  }

  for (k in seq_along(vars)) {
    vdimids <- RNetCDF::var.inq.nc(nc, variable = vars[[k]])[["dimids"]]

    isDimVertical <- which(vdimids == idDimVertical)
    hasDimVertical <- length(isDimVertical) > 0L

    res[[k]] <- list(
      idDimVertical = if (hasDimVertical) isDimVertical,
      nVertical = if (hasDimVertical) nVertical
    )
  }

  res
}

compareNC <- function(
  fn,
  path,
  vars_required,
  vars_other,
  checkValues = TRUE,
  idExampleSite = 1L,
  limitVerticalToRef = FALSE,
  tolerance = sqrt(.Machine[["double.eps"]])
) {
  stopifnot(requireNamespace("RNetCDF"))
  resMsg <- NULL

  ncref <- RNetCDF::open.nc(fn)
  on.exit(RNetCDF::close.nc(ncref), add = TRUE)
  # collapse = T: 1x1 -> 1
  xref <- RNetCDF::read.nc(ncref, collapse = TRUE, unpack = TRUE)

  nc2 <- RNetCDF::open.nc(file.path(path, basename(fn)))
  on.exit(RNetCDF::close.nc(nc2), add = TRUE)
  x2 <- RNetCDF::read.nc(nc2, collapse = FALSE, unpack = TRUE)

  vars_shared <- intersect(names(xref), names(x2))
  vars_test <- setdiff(vars_shared, vars_other)

  refVertical <- if (limitVerticalToRef) getVerticalNC(ncref, vars = vars_test)

  if (
    all(vars_required %in% vars_shared) && length(vars_test) > 0L
  ) {
    targetVals <- xref[vars_test]
    currentVals <- subsetNC(
      x2[vars_test],
      ref = xref[vars_test],
      xdom = x2[["domain"]],
      xid = findExampleSiteIndex(idExampleSite, x2[["domain"]]),
      limitVerticalToRef = limitVerticalToRef,
      refVertical = refVertical,
      xVertical = if (limitVerticalToRef) getVerticalNC(nc2, vars = vars_test)
    )

    msg <- if (isTRUE(checkValues)) {
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
  stopifnot(requireNamespace("RNetCDF"))
  stopifnot(requireNamespace("units"))

  resMsg <- NULL

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

    targetVals <- subsetNC(
      xin[input[["var"]]],
      ref = NULL,
      xdom = xin[["dom"]],
      xid = wIndex
    )[[input[["var"]]]]

    currentVals <- subsetNC(
      xout[output[["var"]]],
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

    # Hack: just take the first 365 days of the first year
    # fix at 365 days to work with all calendars (noleap, allleap, standard)
    # and any splits of data across multiple files
    # TODO: match up with actual dates from time dimension
    ntime <- min(365L, length(targetVals), length(currentVals))
    idsTime <- seq_len(ntime)

    msg <- all.equal(
      target = targetVals[idsTime],
      current = currentVals[idsTime],
      tolerance = tolerance
    )

    if (isTRUE(msg)) {
      resMsg <- if (length(idsTime) == 365L) {
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

  if (im == 0) TRUE else resMsg
}


#------ . ------
