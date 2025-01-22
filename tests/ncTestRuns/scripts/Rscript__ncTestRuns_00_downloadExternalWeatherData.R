
#------------------------------------------------------------------------------#
# Download daily inputs from extern weather data sources
#
#   * If successful, this will allow test runs with daily inputs
#     from external data sources including Daymet, gridMET, and MACAv2METDATA
#
#
# Run this script as follows (command-line arguments are optional)
# ```
#   Rscript \
#       Rscript__ncTestRuns_00_downloadExternalWeatherData.R \
#       --path-to-inWeather=<...>
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

#------ Paths (possibly as command-line arguments) ------
dir_prj <- ".."

dir_inWeather <- if (any(ids <- grepl("--path-to-inWeather", args))) {
  sub("--path-to-inWeather", "", args[ids]) |>
    sub("=", "", x = _) |>
    trimws()
} else {
  file.path(dir_prj, "data-raw", "inWeather")
}

stopifnot(dir.exists(dir_inWeather))

#------ . ------

list_wget_scriptFilenames <- c(
  file.path(dir_inWeather, "Daymet", "get_daymet.sh"),
  file.path(dir_inWeather, "gridMET", "get_gridmet.sh"),
  file.path(dir_inWeather, "MACAv2METDATA", "get_macav2metdata.sh")
)

tmp <- lapply(
  list_wget_scriptFilenames,
  function(filename) {
    if (file.exists(filename)) {
      system2(
        filename, args = paste("--out", file.path(dirname(filename), "data"))
      )
    } else {
      cat(filename, "does not exist.")
    }
  }
)

#------ . ------
