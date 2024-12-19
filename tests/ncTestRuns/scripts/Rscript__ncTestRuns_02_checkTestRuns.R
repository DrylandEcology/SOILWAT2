
#------------------------------------------------------------------------------#
# Execute and check a set of nc-based SOILWAT2 test runs
#
#   * Comprehensive set of spatial configurations of simulation domains and
#     input netCDFs (see tests/ncTestRuns/data-raw/metadata_testRuns.csv)
#   * (if available) Runs with daily inputs from external data sources
#     including Daymet, gridMET, and MACAv2METDATA
#   * One site/grid cell in the simulation domain is set up to correspond
#     to the reference run (which is equal to the "example" at tests/example);
#     the output of that site/grid cell is compared against the reference output
#
#   * This script will carry out the following checks
#     * Does simulation run succeed or fail as expected?
#     * Do the outputs for the site/grid cell that correspond to the
#       "reference" run match the values and/or structure of the outputs
#       of the "reference" run? If the test run differs in weather source
#       or weather calendar, then only structure but not values are compared.
#     * Do daily minimum and maximum air temperature and daily precipitation
#       amount match between inputs and outputs? Only the first 365 days of the
#       simulation runs are compared.
#
#
# Run this script as follows (command-line arguments are optional)
# ```
#   Rscript \
#       Rscript__ncTestRuns_02_checkTestRuns.R \
#       --path-to-ncTestRuns=<...> \
#       --path-to-sw2=<...> \
#       --path-to-referenceOutput=<...> \
#       --testRuns=<...>
# ```
#------------------------------------------------------------------------------#

#------ . ------
#------ Settings ------

treatWarningsAsErrors <- FALSE
reportWarnings <- TRUE
comparableInputCalendars <- c(NA, "standard", "allleap")


#------ Requirements ------
stopifnot(
  requireNamespace("RNetCDF", quietly = TRUE),
  requireNamespace("sf", quietly = TRUE),
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

frefOutput <- if (any(ids <- grepl("--path-to-referenceOutput", args))) {
  sub("--path-to-referenceOutput", "", args[ids]) |>
    sub("=", "", x = _) |>
    trimws()
} else {
  file.path("tests", "ncTestRuns", "results", "referenceRun", "Output")
}

dir_refOutput <- file.path(dir_prj, "..", "..", frefOutput)
fnames_ref <- list.files(
  path = dir_refOutput, pattern = ".nc$", full.names = TRUE
)

dir_dataraw <- file.path(dir_prj, "data-raw")
stopifnot(dir.exists(dir_dataraw))

dir_R <- file.path(dir_prj, "R")
stopifnot(dir.exists(dir_R))

dir_results <- file.path(dir_prj, "results")
stopifnot(dir.exists(dir_results))

dir_testRunsTemplates <- file.path(dir_results, "testRunsTemplates")
stopifnot(dir.exists(dir_testRunsTemplates))

dir_testRuns <- file.path(dir_results, "testRuns")
dir.create(dir_testRuns, recursive = TRUE, showWarnings = FALSE)


#------ . ------
#------ Load functions ------
copyDir <- NULL
runSW2 <- NULL
getSitesFromNC <- NULL
locateExampleSite <- NULL
appendToMessage <- NULL
compareEqualityNCs <- NULL
compareNC <- NULL
compareNCWeather <- NULL
listInputWeather <- NULL
listOutputWeather <- NULL
colorTestReport <- NULL
printColoredDF <- NULL

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

listTestRuns <- listTestRuns[order(listTestRuns[["testrun"]]), , drop = FALSE]

if (reqTestRuns > 0L) {
  stopifnot(reqTestRuns <= nrow(listTestRuns))
  ids <- which(listTestRuns[["testrun"]] %in% reqTestRuns)
  listTestRuns <- listTestRuns[ids, , drop = FALSE]
}

testRunTags <- paste0(
  "testRun-", formatC(listTestRuns[["testrun"]], width = 2L, flag = 0L),
  "__",
  listTestRuns[["tag"]]
)

testRunsTemplates <- list.dirs(
  path = dir_testRunsTemplates,
  full.names = TRUE,
  recursive = FALSE
)


vars_required <- c("lat", "lon", "domain", "time", "time_bnds")
vars_other <- c(
  vars_required,
  "x", "y", "site", "crs_geogsc", "crs_projsc", "lat_bnds", "lon_bnds",
  "vertical", "vertical_bnds", "pfts", "time_bnds"
)

#--- List of excluded warning
fep <- file.path(dir_dataraw, "logfileExclusionPatterns.txt")
listExclusionPatterns <- if (file.exists(fep)) {
  readLines(fep)
}

#------ . ------
#------ Execute nc-testRuns ------
resTestRuns <- data.frame(
  TestName = testRunTags,
  Expectation = NA,
  CheckRun = "not run",
  MessageRun = "",
  CompToRef = "missing",
  CompToWeather = "missing"
)

#--- Loop over testRuns ------
pb <- utils::txtProgressBar(max = nrow(listTestRuns), style = 3L)

for (k0 in seq_len(nrow(listTestRuns))) {

  kt <- which(testRunTags[[k0]] == basename(testRunsTemplates))

  if (length(kt) != 1L) {
    utils::setTxtProgressBar(pb, value = k0)
    next
  }

  resTestRuns[k0, "Expectation"] <- tolower(listTestRuns[k0, "expectation"])

  expectFailure <- identical(resTestRuns[k0, "Expectation"], "error")


  #--- * Create testRun from template ------
  dir_testRun <- file.path(dir_testRuns, testRunTags[[k0]])

  unlink(dir_testRun, recursive = TRUE)

  stopifnot(
    copyDir(
      from = testRunsTemplates[[kt]],
      to = dir_testRun
    )
  )


  #--- * Execute testRun ------
  res <- runSW2(sw2 = fname_sw2, path_inputs = dir_testRun)

  hasSW2Error <- !is.null(res[["msg"]])


  #--- * Check output ------
  dir_testRunOutput <- file.path(dir_testRun, "Output")

  fname_logfile <- file.path(dir_testRunOutput, "logfile.log")
  has_logfile <- file.exists(fname_logfile)

  logfile <- if (has_logfile) readLines(fname_logfile)

  if (length(listExclusionPatterns) > 0L) {
    idsRemove <- lapply(listExclusionPatterns, grep, x = logfile) |>
      unlist() |>
      unique()
    if (length(idsRemove) > 0L) {
      logfile <- logfile[-idsRemove]
    }
  }

  resTestRuns[k0, "MessageRun"] <- appendToMessage(
    hasMsg = res[["msg"]], newMsg = paste(logfile, collapse = " ")
  )


  if (expectFailure) {
    #--- ..** Expected error ------
    # Expect: logfile with error message
    if (
      has_logfile && any(grepl("ERROR:", x = logfile, fixed = TRUE))
    ) {
      resTestRuns[k0, "CheckRun"] <- "ok"
    } else {
      resTestRuns[k0, "CheckRun"] <- "failed"
      resTestRuns[k0, "MessageRun"] <- appendToMessage(
        hasMsg = res[["msg"]], newMsg = "Failed to produce expected error."
      )
    }

    resTestRuns[k0, "CompToRef"] <- "skipped"
    resTestRuns[k0, "CompToWeather"] <- "skipped"

  } else {
    #--- ..** Expected success ------
    res <- if (treatWarningsAsErrors) {
      # Expect: no logfile or logfile without any message
      is.null(logfile) || length(logfile) == 0L
    } else {
      # Expect: no logfile or logfile without error message
      !any(grepl("ERROR:", x = logfile, fixed = TRUE))
    }


    fnames_output <- list.files(path = dir_testRunOutput, pattern = ".nc$")

    if (isTRUE(res) && !hasSW2Error) {
      if (length(fnames_output) > 0L) {
        resTestRuns[k0, "CheckRun"] <- "ok"
      } else {
        resTestRuns[k0, "MessageRun"] <- appendToMessage(
          hasMsg = resTestRuns[k0, "MessageRun"], newMsg = "No output."
        )
        resTestRuns[k0, "CheckRun"] <- "failed"
      }

    } else {
      resTestRuns[k0, "CheckRun"] <- "failed"
    }


    #--- ....*** Check index lookup netCDFs ------
    ilnc <- list.files(
      file.path(dir_testRun, "Input_nc"),
      pattern = "index_[[:print:]]+.nc$",
      recursive = TRUE
    )

    if (
      !identical(length(ilnc) > 0L, listTestRuns[k0, "expectIndexLookup"] > 0L)
    ) {
      resTestRuns[k0, "CheckRun"] <- "failed"
      resTestRuns[k0, "MessageRun"] <- appendToMessage(
        hasMsg = resTestRuns[k0, "MessageRun"],
        newMsg = paste0(
          "Expected ",
          if (listTestRuns[k0, "expectIndexLookup"] == 0L) "no ",
          "index lookup netCDFs but found n = ", length(ilnc), "."
        )
      )
    }


    if (identical(resTestRuns[k0, "CheckRun"], "ok")) {

      testTolerance <- switch(
        EXPR = tolower(listTestRuns[k0, "inputVarType"]),
        float = sqrt(
          (.Machine[["double.base"]] ^ (.Machine[["double.ulp.digits"]] / 2)) / 2
        ),
        double = sqrt(.Machine[["double.eps"]])
      )

      #--- Identify simulation run corresponding to example site
      idSimExampleSite <-
        file.path(dir_testRunOutput, basename(fnames_ref[[1L]])) |>
        getSitesFromNC() |>
        locateExampleSite()

      stopifnot(length(idSimExampleSite) == 1L)


      #--- ....*** Comparisons to reference simulation output ------
      if (length(fnames_ref) > 0L) {
        ok <- TRUE
        outkeysWithMsg <- NULL

        checkValues <-
          identical(listTestRuns[k0, "inWeather"], "sw2") &&
          isTRUE(
            listTestRuns[k0, "calendarWeather"] %in% comparableInputCalendars
          )

        if (
          listTestRuns[k0, "simStartYear"] != 1980 ||
            listTestRuns[k0, "simStartYear"] != 2010
        ) {
          # Only check common output files if simulation years differ
          ftmp <- list.files(
            path = dir_testRunOutput, pattern = ".nc$", full.names = TRUE
          )
          ids <- basename(fnames_ref) %in% basename(ftmp)
          fnames_ref <- fnames_ref[ids]
        }


        #--- ....**** Compare example simulation subset to reference simulation output ------
        for (kr in seq_along(fnames_ref)) {
          msg <- try(
            compareNC(
              fn = fnames_ref[[kr]],
              path = dir_testRunOutput,
              vars_required = vars_required,
              vars_other = vars_other,
              idExampleSite = idSimExampleSite,
              checkValues = checkValues,
              limitVerticalToRef = !identical(
                listTestRuns[k0, "inputSoilProfile"], "standard"
              ),
              tolerance = testTolerance
            ),
            silent = TRUE
          )

          thisok <- !inherits(msg, "try-error") && nchar(msg) == 0L
          ok <- ok && thisok

          if (!thisok) {
            if (nchar(msg) > 0L) {
              tmp <- strsplit(
                basename(fnames_ref[[kr]]), split = "_"
              )[[1L]][[1L]]

              if (!tmp %in% outkeysWithMsg) {
                outkeysWithMsg <- c(outkeysWithMsg, tmp)
                resTestRuns[k0, "MessageRun"] <- appendToMessage(
                  hasMsg = resTestRuns[k0, "MessageRun"], newMsg = msg
                )
              }
            }
          }
        }


        #--- ....**** Compare testRun01 to reference simulation output ------
        # testRun01 is supposed to be exactly equal to the reference
        if (testRunTags[[k0]] == "testRun-01__dom-s-1-geog_in-s-geog-1") {
          msg <- compareEqualityNCs(
            dir1 = dir_refOutput,
            dir2 = dir_testRunOutput,
            tolerance = testTolerance
          )

          if (!isTRUE(msg)) {
            resTestRuns[k0, "MessageRun"] <- appendToMessage(
              hasMsg = resTestRuns[k0, "MessageRun"],
              newMsg = toString(unlist(msg))
            )
            ok <- FALSE
          }
        }


        #--- Update test results
        resTestRuns[k0, "CompToRef"] <- paste(
          if (checkValues) "values:" else "structure:",
          if (ok) "ok" else "failed"
        )

      } else {
        resTestRuns[k0, "CompToRef"] <- "no reference"
      }


      #--- ....*** Compare daily weather between input/output ------
      intsv <- utils::read.delim(
        file.path(dir_testRun, "Input_nc", "SW2_netCDF_input_variables.tsv"),
        check.names = FALSE
      )

      listCompToWeather <- list(
        tasmax = list(
          input = listInputWeather("temp_max", intsv, dir_testRun),
          output = listOutputWeather("tasmax", "TEMP", dir_testRunOutput)
        ),
        tasmin = list(
          input = listInputWeather("temp_min", intsv, dir_testRun),
          output = listOutputWeather("tasmin", "TEMP", dir_testRunOutput)
        ),
        pr = list(
          input = listInputWeather("ppt", intsv, dir_testRun),
          output = listOutputWeather("pr", "PRECIP", dir_testRunOutput)
        )
      )

      ok <- TRUE

      for (kr in seq_along(listCompToWeather)) {
        msg <- try(
          compareNCWeather(
            input = listCompToWeather[[kr]][["input"]],
            output = listCompToWeather[[kr]][["output"]],
            idExampleSite = idSimExampleSite,
            tolerance = sqrt(.Machine[["double.eps"]])
          ),
          silent = TRUE
        )

        thisok <- !inherits(msg, "try-error") && nchar(msg) == 0L
        ok <- ok && thisok

        if (!thisok && nchar(msg) > 0L) {
          resTestRuns[k0, "MessageRun"] <- appendToMessage(
            hasMsg = resTestRuns[k0, "MessageRun"], newMsg = msg
          )
        }
      }

      resTestRuns[k0, "CompToWeather"] <- if (ok) "ok" else "failed"
    }
  }

  utils::setTxtProgressBar(pb, value = k0)
}

close(pb)

tmp <- summary(warnings())
if (length(tmp) > 0L) {
  print(tmp)
}


#------ . ------
#------ Report test outcomes ------
has_cli <- isTRUE(requireNamespace("cli", quietly = TRUE))

#--- * Overall report ------
vars_report <- c(
  "TestNameShort", "Expectation", "CheckRun", "CompToRef", "CompToWeather"
)
res <- resTestRuns
rownames(res) <- NULL

res[["TestNameShort"]] <- vapply(
  strsplit(res[["TestName"]], split = "__", fixed = TRUE),
  function(x) x[[1L]],
  FUN.VALUE = NA_character_
)

msg <- "\nSummary of test outcomes:"
if (has_cli) {
  cat(cli::style_bold(msg), fill = TRUE)
  printColoredDF(res, vars = vars_report)

} else {
  cat(msg, fill = TRUE)
  print(res[, vars_report, drop = FALSE])
}
cat("\n")

ids_failed <- c(
  grep("failed", res[["CheckRun"]], fixed = TRUE),
  grep("failed", res[["CompToRef"]], fixed = TRUE),
  grep("failed", res[["CompToWeather"]], fixed = TRUE),
  which(
    res[["Expectation"]] == "success" & res[["CompToRef"]] == "missing"
  )
) |>
  unique() |> sort()


#--- * Warning details ------
if (reportWarnings) {
  tmp <- seq_len(nrow(res))
  if (length(ids_failed) > 0L) tmp <- tmp[-ids_failed]
  ids <- which(res[["Expectation"]] == "error")
  if (length(ids) > 0L) tmp <- tmp[-ids]
  ids_okWithMsg <- intersect(tmp, which(nchar(res[["MessageRun"]]) > 0L))

  if (length(ids_okWithMsg) > 0L) {
    msg <- "Messages produced by successful tests:"
    cat(if (has_cli) cli::style_bold(msg) else msg, fill = TRUE)

    for (kr in ids_okWithMsg) {
      tmp <- paste0(
        "* ", res[kr, "TestName"],
        paste0("\n\t** Messages: ", res[kr, "MessageRun"]),
        "\n"
      )

      cat(tmp, fill = TRUE)
    }
  }
}


#--- * Failures details ------
if (length(ids_failed) > 0L) {
  msg <- "Failed tests:"
  cat(if (has_cli) cli::style_bold(msg) else msg, fill = TRUE)

  for (kr in ids_failed) {
    tmp <- paste0(
      "* ", res[kr, "TestName"],
      "\n\t** Check: ", res[kr, "CheckRun"],
      " -- expected outcome (", res[kr, "Expectation"], ")",
      "\n\t** Comparison to reference: ", res[kr, "CompToRef"],
      "\n\t** Comparison to weather inputs: ", res[kr, "CompToWeather"]
    )

    if (has_cli) {
      tmp <- colorTestReport(tmp)
    }

    tmp <- paste0(
      tmp,
      if (nchar(res[kr, "MessageRun"]) > 0L) {
        paste0("\n\t** Messages: ", res[kr, "MessageRun"])
      },
      "\n"
    )

    cat(tmp, fill = TRUE)
  }
}

#------ . ------
