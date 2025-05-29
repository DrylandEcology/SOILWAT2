#------ . ------
# Compare SOILWAT2 output among text-based, nc-based, mpi-based and rSOILWAT2 runs
#
# ```
#   # Run text-based SOILWAT2 on example simulation
#   rm -r tests/example/Output
#   make clean bin_run
#   cp -R tests/example/Output tests/example/Output_comps-txt
#
#   # Run nc-based SOILWAT2 on example simulation
#   rm -r tests/example/Output
#   CPPFLAGS='-DSWNC' make clean bin_run
#   mv tests/example/Input_nc/domain_template.nc tests/example/Input_nc/domain.nc
#   bin/SOILWAT2 -d ./tests/example -f files.in
#   cp -R tests/example/Output tests/example/Output_comps-nc
#   rm tests/example/Input_nc/domain.nc tests/example/Input_nc/progress.nc
# ```
#
# Compare output generated above
# ```
#   Rscript tools/rscripts/Rscript__SW2_output_txt_vs_r_vs_nc.R
# ```
#
# See also `tools/check_outputModes.sh`
#
#------ . ------
stopifnot(
  requireNamespace("rSOILWAT2", quietly = TRUE),
  requireNamespace("RNetCDF", quietly = TRUE),
  requireNamespace("units", quietly = TRUE)
)

#--- Settings ------
qprobs <- c(0, 0.05, 0.25, 0.5, 0.75, 0.95, 1)


#--- Paths ------
dir_example <- file.path("tests", "example")
dir_nc <- file.path(dir_example, "Output_comps-nc")
dir_txt <- file.path(dir_example, "Output_comps-txt")
dir_mpi <- file.path(dir_example, "Output_comps-mpi")


#--- SOILWAT2 metadata ------
outModes <- c("txt", "nc", "mpi", "r")

pds2 <- rSOILWAT2:::rSW2_glovars[["kSOILWAT2"]][["OutPeriods"]]
pds1 <- tolower(pds2)
pds3 <- c("daily", "weekly", "monthly", "yearly")

vegtypes <- c("tree", "shrub", "forbs", "grass")

outkeys <- rSOILWAT2:::rSW2_glovars[["kSOILWAT2"]][["OutKeys"]]

outvars <- read.delim(
  file.path(dir_example, "Input_nc", "SW2_netCDF_output_variables.tsv"),
  sep = "\t"
)



#--- . ------
#--- Run rSOILWAT2 ------
cat(
  "Run rSOILWAT2 v", getNamespaceVersion("rSOILWAT2"),
  " on example simulation ...\n",
  sep = ""
)

# Run rSOILWAT2 with inputs from SOILWAT2/tests/example
# (instead of rSOILWAT2::sw_exampleData) because
# rSOILWAT2 turns off "ESTABL" by default (SOILWAT2 default: on)
swr <- rSOILWAT2::sw_exec(dir = dir_example)


#--- . ------
#--- Function to read all output data ------
read_swdata <- function(outkey, pd, swr, dir_txt, dir_nc, dir_mpi) {

  #--- Load txt-based output ------
  ftxts <- list.files(dir_txt, pattern = ".csv") |>
    grep(pattern = pds3[[pd]], x = _, value = TRUE)

  stopifnot(length(ftxts) == 2L)

  swtxt <- lapply(file.path(dir_txt, ftxts), read.csv) |>
    do.call(cbind, args = _)


  #--- Subset txt and r data to output period and to outkey
  keyTag <- paste0("^", outkey, "_")
  idh <- if (identical(pds1[[pd]], "year")) 1 else 1:2

  x <- list(
    txt = swtxt[, grep(keyTag, colnames(swtxt)), drop = FALSE] |>
      data.matrix()
  )

  x[["r"]] = {
    tmp <- slot(slot(swr, outkey), pds2[[pd]])[, -idh, drop = FALSE]
    if (identical(dim(x[["txt"]]), dim(tmp))) {
      colnames(tmp) <- paste0(outkey, "_", colnames(tmp))
      tmp
    } else {
      array(dim = dim(x[["txt"]]), dimnames = dimnames(x[["txt"]]))
    }
  }

  dx <- dim(x[["txt"]])
  cnsx <- colnames(x[["txt"]])

  vegsTag <- paste0(vegtypes, collapse = "|")
  mamTag <- "_min|_avg|_max"

  has_veg <- any(grepl(vegsTag, cnsx))
  has_totalVeg <- any(grepl("_total", cnsx, fixed = TRUE))
  has_slyrs <- any(grepl("Lyr_[[:digit:]]+", cnsx))
  has_MinAvgMax <- any(grepl(mamTag, cnsx))

  if (has_slyrs || has_veg) {
    nmam <- if (has_MinAvgMax) 3L else 1L
    nvegs <- if (has_veg) length(vegtypes) else 1L
    ntv <- {
      # Number of groups of vegtypes that also include "total"
      tagtv <- paste0(c("total", vegsTag), collapse = "|")
      tmpcntv <-
        unique(gsub("Lyr_[[:digit:]]+", "", x = cnsx)) |>
        grep(tagtv, x = _, value = TRUE)
      tmputv <- gsub(tagtv, "", x = tmpcntv)
      sum(table(tmputv) == length(vegtypes) + 1L)
    }

    ucns1 <- unique(
      gsub(
        paste0(c("total", vegsTag, mamTag), collapse = "|"),
        "",
        x = cnsx
      )
    )
    ucns2 <- unique(gsub("Lyr_[[:digit:]]+", "", x = ucns1))

    nslyrs <- max(1L, grepl("Lyr_[[:digit:]]+", ucns1) |> sum())

    nvars <- nmam * length(ucns2) + ntv

  } else {
    nslyrs <- 1L
    nvars <- dx[[2L]]
  }


  #--- Load nc-based and mpi-based output ------
  for (mt in c("nc", "mpi")) {
    dir_mt <- switch(mt, nc = dir_nc, mpi = dir_mpi)

    tmp <- list.files(
      dir_mt,
      pattern = paste0(keyTag, "[[:print:]]*_", pds1[[pd]], ".nc")
    )
    fnc <- file.path(dir_mt, tmp) # sorted in alphabetical order

    keydesc <- outvars[outvars[[1L]] == outkey, , drop = FALSE]

    if (identical(outkey, "ESTABL")) {
      keydesc <- keydesc[rep(1L, times = dx[[2L]]), , drop = FALSE]
      keydesc[["SW2.txt.output"]] <- keydesc[["netCDF.variable.name"]] <-
        gsub(keyTag, "", cnsx)
    }

    matchers <- cnsx |>
      gsub(keyTag, "", x = _) |>
      gsub("Lyr_[[:digit:]]+", "Lyr_<slyr>", x = _) |>
      gsub(vegsTag, "<veg>", x = _) |>
      unique()

    ivars <- match(
      matchers,
      # change "-" into "." because `matchers` have passed through `make.names()`
      # e.g., "H_oh_MJm-2" -> "H_oh_MJm.2"
      # but retain "<veg>" and "<slyrs>"
      table = gsub("-", ".", keydesc[["SW2.txt.output"]]),
      nomatch = 0L
    )

    stopifnot(nvars == length(ivars))


    if (any(dx == 0L, length(fnc) == 0L, ivars == 0L)) {
      x[[mt]] <- array(dim = c(0L, 0L))

    } else {
      ncc <- lapply(fnc, RNetCDF::open.nc)

      x[[mt]] <- lapply(
        ivars,
        function(kv) {
          tmp <- lapply(
            ncc,
            function(nc) {
              tmp <- RNetCDF::var.get.nc(
                nc,
                variable = keydesc[kv, "netCDF.variable.name"]
              )
              nd <- length(dim(tmp))
              thisHas_slyrs <- grepl("Z", keydesc[kv, "XY.Dim"], fixed = TRUE)
              thisHas_veg <- grepl("V", keydesc[kv, "XY.Dim"], fixed = TRUE)
              thisHas_totalVeg <- all(
                has_totalVeg,
                grepl("_total", keydesc[kv, "SW2.txt.output"]),
                !grepl("V", keydesc[kv, "XY.Dim"])
              )
              res <- if (thisHas_slyrs && thisHas_veg && !thisHas_totalVeg) {
                tmp2 <- if (nd == 5L) {
                  tmp[, , , 1L, 1L, drop = TRUE] # grab first gridcell
                } else if (nd == 4L) {
                  tmp[, , , 1L, drop = TRUE] # grab first site
                } else {
                  tmp # just one site/gridcell
                }
                ndt <- dim(tmp2)
                array(
                  data = aperm(tmp2, perm = c(3L, 2L, 1L)),
                  dim = c(ndt[[3L]], prod(ndt[1:2]))
                )

              } else if (xor(thisHas_slyrs, thisHas_veg)) {
                if (nd == 4L) {
                  tmp[, , 1L, 1L, drop = TRUE] # grab first gridcell
                } else if (nd == 3L) {
                  tmp[, , 1L, drop = TRUE] # grab first site
                } else {
                  tmp # just one site/gridcell
                } |>
                  t()

              } else {
                if (nd == 3L) {
                  tmp[, 1L, 1L, drop = TRUE] # grab first gridcell
                } else if (nd == 2L) {
                  tmp[, 1L, drop = TRUE] # grab first site
                } else {
                  tmp # just one site/gridcell
                }
              }

              # Read units from nc
              units_has <- RNetCDF::att.get.nc(
                nc,
                variable = keydesc[kv, "netCDF.variable.name"],
                attribute = "units"
              )

              if (!identical(units_has, keydesc[kv, "netCDF.units"])) {
                stop(
                  "Found unit ", shQuote(units_has),
                  " but expected ", shQuote(keydesc[kv, "netCDF.units"]),
                  " for variable ", shQuote(keydesc[kv, "netCDF.variable.name"]),
                  " in nc-output."
                )
              }

              # Convert units to SOILWAT2 units
              if (identical(units_has, keydesc[kv, "SW2.units"])) {
                res
              } else {
                units::set_units(res, units_has, mode = "standard") |>
                  units::set_units(keydesc[kv, "SW2.units"], mode = "standard") |>
                  units::drop_units()
              }
            }
          )
          # concatenate time-series if split across multiple netCDFs
          tmp <- do.call(
            what = if (isTRUE(length(dim(tmp[[1L]])) > 0L)) rbind else c,
            args = tmp
          )
          # subset columns if soil evaporation
          if (identical(keydesc[kv, "SW2.output.group"], "EVAPSOIL")) {
            tmp[, seq_len(nslyrs), drop = FALSE]
          } else {
            tmp
          }
        }
      ) |>
        do.call(cbind, args = _)

      ndnc <- dim(x[[mt]])

      if (isTRUE(length(ndnc) >= 3L && ndnc[[3L]] == 1L)) {
        # Drop a 1-length 3rd dimension if it was created
        x[[mt]] <- array(
          x[[mt]][, , 1L, drop = TRUE],
          dim = ndnc[1:2]
        )
      }
    }
  }


  # columns are interleaved in txt-sw and r-sw but not nc-sw
  if (identical(outkey, "SOILTEMP")) {
    tmp <- nmam * (seq_len(nslyrs) - 1)
    ids <- c(1L + tmp, 2L + tmp, 3L + tmp)
    x[["txt"]] <- x[["txt"]][, ids, drop = FALSE]
    x[["r"]] <- x[["r"]][, ids, drop = FALSE]
  }

  x
}


#--- . ------
cat("Compare output among SOILWAT2 runs:\n")

#--- Compile and compare output data ------
diffs <- array(
  data = vector(mode = "list", length = length(outkeys) * length(pds1)),
  dim = c(length(outkeys), length(pds1)),
  dimnames = list(outkeys, pds1)
)

hasModes <- array(
  data = NA,
  dim = c(length(outkeys), length(pds1), length(outModes)),
  dimnames = list(outkeys, pds1, outModes)
)

for (pd in seq_along(pds1)) {

  #--- Loop over outkeys ------
  for (key in seq_along(outkeys)) {

    #--- Load output ------
    x <- try(
      read_swdata(outkey = outkeys[[key]], pd, swr, dir_txt, dir_nc, dir_mpi),
      silent = TRUE
    )

    if (inherits(x, "try-error")) next

    #--- Identify output modes ------
    hasModes[key, pd, outModes] <- vapply(
      outModes,
      function(om) isTRUE(all(dim(x[[om]]) > 0L)),
      FUN.VALUE = NA
    )

    #--- Compare data ------
    list_combinations <- combn(names(x), m = 2L) |> as.data.frame()

    xnds <- vapply(x, dim, FUN.VALUE = c(NA_integer_, NA_integer_))

    diffs[key, pd] <- lapply(
      stats::setNames(
        list_combinations,
        apply(list_combinations, 2L, paste, collapse = "-")
      ),
      function(e) {
        if (
          all(
            xnds[, e[[1L]]] > 0L,
            identical(xnds[, e[[1L]]], xnds[, e[[2L]]])
          )
        ) {
          tmp <- apply(
            x[[e[[1L]]]] - x[[e[[2L]]]],
            MARGIN = 2L,
            FUN = quantile,
            probs = qprobs,
            names = TRUE,
            na.rm = TRUE
          )
          data.frame(
            diff = paste(e, collapse = "-"),
            key = outkeys[[key]],
            var = gsub(paste0("^", outkeys[[key]], "_"), "", colnames(tmp)),
            t(tmp),
            row.names = NULL
          )
        }
      }
    ) |>
      do.call(rbind, args = _) |>
      list()
  }
}


#--- Identify available output modes ------
has_modes <- apply(hasModes, MARGIN = 3L, sum) > 0L

cat("* Modes with output:", toString(names(has_modes)[has_modes]), "\n")
cat("* Modes without output:", toString(names(has_modes)[!has_modes]), "\n")


#--- Identify outkeys without output ------
# Expect to contain no output: "WTHR", "ALLH2O", "ET", "ALLVEG"
has_out <- apply(
  diffs,
  MARGIN = 1L,
  function(x) all(!vapply(x, is.null, FUN.VALUE = NA))
)

has_out2 <- apply(
  diffs,
  MARGIN = 1L:2L,
  function(xt) {
    if (length(dim(xt[[1L]])) == 2L) {
      ids <- apply(xt[[1L]], 1L, anyNA)
      if (any(ids)) {
        unique(xt[[1L]][ids, c("diff", "key")])
      }
    }
  }
)

has_out3 <- if (!is.null(has_out2)) {
  tmp <- unique(do.call(rbind, args = has_out2))
  tapply(
    tmp[["diff"]], INDEX = tmp[["key"]], FUN = function(x) toString(x)
  )
}


cat("* Output keys with output:", toString(names(has_out)[has_out]), "\n")
cat("* Output keys without output:", toString(names(has_out)[!has_out]), "\n")
if (!is.null(has_out3)) {
  cat(
    "* Output keys without output for certain comparisons:",
    paste(paste(names(has_out3), has_out3, sep = ": "), collapse = "; "),
    "\n"
  )
}



#--- Identify meaningful differences in output ------
if (any(has_out)) {
  ndiffs <- 0L

  for (pd in seq_along(pds1)) {
    ddiffs <- do.call(rbind, args = diffs[, pd])

    tol <- 1e-6 # sqrt(.Machine[["double.eps"]])
    idsgt0 <- which(abs(ddiffs[, "X0."]) > tol | abs(ddiffs[, "X100."]) > tol)
    idsgt0 <- na.omit(idsgt0)

    nt <- table(
      ddiffs[idsgt0, "diff"],
      apply(ddiffs[idsgt0, c("key", "var")], 1L, paste, collapse = ": ")
    )

    if (all(dim(nt) > 0L)) {
      ndiffs <- ndiffs + 1L

      cat("* Relevant differences at time step", pds1[[pd]], ":")
      print(nt)
    }


    if (FALSE) {
      stopifnot(requireNamespace("ggplot2"))

      ggplot2::ggplot(
        data = ddiffs,
        mapping = ggplot2::aes(x = X0., y = X100.)
      ) +
        ggplot2::geom_point() +
        ggplot2::facet_wrap(ggplot2::vars(diff))


      ggplot2::ggplot(
        data = ddiffs,
        mapping = ggplot2::aes(x = X100.)
      ) +
        ggplot2::geom_histogram() +
        ggplot2::facet_wrap(ggplot2::vars(diff), scales = "free")
    }
  }

  if (ndiffs > 0L) {
    cat("* Failure: relevant differences in output.\n")
  } else {
    cat("* Success: no differences in available output.\n")
  }

} else {
  cat("* No output to compare.\n")
}
