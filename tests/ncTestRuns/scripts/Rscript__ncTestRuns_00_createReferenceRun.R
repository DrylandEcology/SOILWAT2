
#------------------------------------------------------------------------------#
# Create a nc-based SOILWAT2 reference run
#
# This is based on the "example" run (tests/example):
#   inputs (other than the domain) are all from text files;
#   the domain uses the netCDF "domain.nc"
#
# Run this script as follows (command-line arguments are optional)
# ```
#   Rscript \
#       Rscript__ncTestRuns_00_createReferenceRun.R \
#       --path-to-ncTestRuns=<...> \
#       --path-to-sw2=<...> \
#       --swMode=<...> \
#       --ntasks=<...> \
#       --path-to-references=<...>
# ```
#------------------------------------------------------------------------------#

#------ Requirements ------

#------ . ------
#------ Grab command line arguments (if any)
args <- commandArgs(trailingOnly = TRUE)

ids <- grepl("--swMode", args, fixed = TRUE)
swMode <- if (any(ids)) {
  sub("--swMode", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws() |>
    tolower()
} else {
  "nc"
}

stopifnot(swMode %in% c("nc", "mpi"))

ids <- grepl("--ntasks", args, fixed = TRUE)
nTasks <- if (any(ids)) {
  sub("--ntasks", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws() |>
    tolower()
}


#------ Paths (possibly as command-line arguments) ------
ids <- grepl("--path-to-ncTestRuns", args, fixed = TRUE)
dir_prj <- if (any(ids)) {
  sub("--path-to-ncTestRuns", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws()
} else {
  ".."
}

ids <- grepl("--path-to-sw2", args, fixed = TRUE)
fname_sw2 <- if (any(ids)) {
  sub("--path-to-sw2", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws()
} else {
  file.path(dir_prj, "..", "..", "bin", "SOILWAT2")
}

stopifnot(file.exists(fname_sw2))

ids <- grepl("--path-to-references", args, fixed = TRUE)
path_refFolder <- if (any(ids)) {
  sub("--path-to-references", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws()
} else {
  file.path("tests", "ncTestRuns", "results", "referenceRuns")
}

dir_refRunTemplate <- file.path(dir_prj, "..", "..", "tests", "example")
stopifnot(dir.exists(dir_refRunTemplate))

dir_dataraw <- file.path(dir_prj, "data-raw")
stopifnot(dir.exists(dir_dataraw))

dir_R <- file.path(dir_prj, "R")
stopifnot(dir.exists(dir_R))


#------ . ------
#------ Load functions ------
copyDir <- NULL
toggleNCInputTSV <- NULL
setTxtInput <- NULL
runSW2 <- NULL

res <- lapply(
  list.files(path = dir_R, pattern = ".R$", full.names = TRUE),
  source
)


#------ . ------
#------ Create reference runs ------

implementedReferences <- c(
  "example",
  "example-wGen",
  "example-spinup",
  "example-spinup-slowDyn"
)


#--- * Specifications of test runs ------
listTestRuns <- utils::read.csv(
  file = file.path(dir_dataraw, "metadata_testRuns.csv")
)

refRuns <- unique(listTestRuns[["reference"]])
stopifnot(refRuns %in% implementedReferences)

dir_refRuns <- file.path(dir_prj, "..", "..", path_refFolder, refRuns)


for (k0 in seq_along(dir_refRuns)) {

  #--- * Create temporary run from reference template ------
  unlink(dir_refRuns[[k0]], recursive = TRUE)
  dir.create(dir_refRuns[[k0]], recursive = TRUE, showWarnings = FALSE)

  stopifnot(
    copyDir(
      from = file.path(dir_refRunTemplate, "Input"),
      to = file.path(dir_refRuns[[k0]], "Input")
    ),
    copyDir(
      from = file.path(dir_refRunTemplate, "Input_nc"),
      to = file.path(dir_refRuns[[k0]], "Input_nc")
    ),
    file.copy(
      from = file.path(dir_refRunTemplate, "files.in"),
      to = dir_refRuns[[k0]]
    )
  )


  #--- * Turn off nc-inputs (all except domain) ------
  fname_ncintsv <- file.path(
    dir_refRuns[[k0]], "Input_nc", "SW2_netCDF_input_variables.tsv"
  )

  toggleNCInputTSV(
    filename = fname_ncintsv,
    inkeys = "all",
    sw2vars = NULL,
    value = 0L
  )

  toggleNCInputTSV(
    filename = fname_ncintsv,
    inkeys = c("inDomain", "inSpatial"),
    sw2vars = NULL,
    value = 1L
  )


  #--- * Turn on weather generator (wGen) ------
  if (grepl("wGen", basename(dir_refRuns[[k0]]), fixed = TRUE)) {
    fname <- file.path(dir_refRuns[[k0]], "Input", "weathsetup.in")
    setTxtInput(
      filename = fname,
      tag = "# 0 = use historical data only$",
      value = 2L,
      classic = TRUE
    )
  }

  #--- * Turn on spinup ------
  if (grepl("spinup", basename(dir_refRuns[[k0]]), fixed = TRUE)) {
    fname <- file.path(dir_refRuns[[k0]], "Input", "domain.in")
    setTxtInput(
      filename = fname,
      tag = "SpinupDuration",
      value = 2L,
      classic = FALSE
    )
  }

  #--- * Turn on slow dynamics (vegetation, soil temperature boundary) ------
  if (grepl("slowDyn", basename(dir_refRuns[[k0]]), fixed = TRUE)) {
    fname <- file.path(dir_refRuns[[k0]], "Input", "siteparam.in")
    setTxtInput(
      filename = fname,
      tag = "# Method for soil temperature at maximum depth:$",
      value = 1L,
      classic = TRUE
    )
    setTxtInput(
      filename = fname,
      tag = "# constant soil temperature \\(Celsius\\) at the lower boundary",
      value = 999, # junk value
      classic = TRUE
    )

    fname <- file.path(dir_refRuns[[k0]], "Input", "veg.in")
    setTxtInput(
      filename = fname,
      tag = "# 0 - Use composition and biomass inputs from veg.in or veg.nc$",
      value = 2L,
      classic = TRUE
    )
  }


  #--- * Execute refRun ------
  res <- runSW2(
    sw2 = fname_sw2,
    path_inputs = dir_refRuns[[k0]],
    mode = swMode,
    nTasks = nTasks,
    renameDomainTemplate = TRUE
  )

  fname_logfile <- file.path(dir_refRuns[[k0]], "logs", "rank_0_logfile.log")
  has_logfile <- file.exists(fname_logfile)

  logfile <- if (has_logfile) readLines(fname_logfile)
  has_logContent <- nzchar(paste(logfile, collapse = " "))


  if (!is.null(res[["msg"]]) || has_logContent) {
    cat(
      "Reference run", shQuote(basename(dir_refRuns[[k0]])), "failed.",
      fill = TRUE
    )
    quit(status = 1L)
  }
}

#------ . ------
