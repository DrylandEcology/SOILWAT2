
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
#       --path-to-referenceOutput=<...>
# ```
#------------------------------------------------------------------------------#

#------ Requirements ------

#------ . ------
#------ Grab command line arguments (if any)
args <- commandArgs(trailingOnly = TRUE)

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

frefOutput <- if (any(ids <- grepl("--path-to-referenceOutput", args))) {
  sub("--path-to-referenceOutput", "", args[ids]) |>
    sub("=", "", x = _) |>
    trimws()
} else {
  file.path("tests", "ncTestRuns", "results", "referenceRun", "Output")
}

dir_refRun <- dirname(file.path(dir_prj, "..", "..", frefOutput))
dir_refRunTemplate <- file.path(dir_prj, "..", "..", "tests", "example")
stopifnot(dir.exists(dir_refRunTemplate))

dir_R <- file.path(dir_prj, "R")
stopifnot(dir.exists(dir_R))


#------ . ------
#------ Load functions ------
copyDir <- NULL
toggleNCInputTSV <- NULL
runSW2 <- NULL

res <- lapply(
  list.files(path = dir_R, pattern = ".R$", full.names = TRUE),
  source
)


#------ . ------
#------ Create reference run ------

#--- * Create temporary run from reference template ------
unlink(dir_refRun, recursive = TRUE)
dir.create(dir_refRun, recursive = TRUE, showWarnings = FALSE)

stopifnot(
  copyDir(
    from = file.path(dir_refRunTemplate, "Input"),
    to = file.path(dir_refRun, "Input")
  ),
  copyDir(
    from = file.path(dir_refRunTemplate, "Input_nc"),
    to = file.path(dir_refRun, "Input_nc")
  ),
  file.copy(
    from = file.path(dir_refRunTemplate, "files.in"),
    to = dir_refRun
  )
)


#--- * Turn off nc-inputs ------
fname_ncintsv <- file.path(
  dir_refRun, "Input_nc", "SW2_netCDF_input_variables.tsv"
)

toggleNCInputTSV(
  filename = fname_ncintsv,
  inkeys = c("inTopo", "inClimate", "inSoil", "inVeg", "inWeather"),
  sw2vars = NULL,
  value = 0L
)


#--- * Execute refRun ------
res <- runSW2(
  sw2 = fname_sw2, path_inputs = dir_refRun, renameDomainTemplate = TRUE
)

fname_logfile <- file.path(dir_refRun, "Output", "logfile.log")
has_logfile <- file.exists(fname_logfile)

logfile <- if (has_logfile) readLines(fname_logfile)
has_logContent <- nchar(paste(logfile, collapse = " ")) > 0L


if (!is.null(res[["msg"]]) || has_logContent) {
  cat("Reference run failed.", fill = TRUE)
  quit(status = 1L)
}

#------ . ------
