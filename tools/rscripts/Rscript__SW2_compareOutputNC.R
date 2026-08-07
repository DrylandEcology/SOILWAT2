
#------------------------------------------------------------------------------#
# Compare nc-output from two simulation runs
#
# Note: this works best for single-site simulations;
# sites/gridcells are represented as multiple lines
#
# Run this script as follows
# ```
#   Rscript \
#       tools/rscripts/Rscript__SW2_compareOutputNC.R \
#       --pathToTools=<tools> \
#       --pathToOut1=<...> \
#       --pathToOut2=<...> \
#       --pathToFigures=<tools/figures> \
#       --skipEqualityCheck \
#       --skipFigures
# ```
#
# Examples
# ```
#   Rscript \
#       tools/rscripts/Rscript__SW2_compareOutputNC.R \
#       --pathToOut1=tests/example/Output \
#       --pathToOut2=tests/ncTestRuns/results/referenceRuns/example/Output
# ```
# ```
#   Rscript \
#       tools/rscripts/Rscript__SW2_compareOutputNC.R \
#       --pathToOut1=tests/ncTestRuns/results/referenceRuns/example/Output \
#       --pathToOut2=tests/ncTestRuns/results/referenceRuns/example-wGen/Output \
#       --pathToFigures=tools/figures/referenceRunsComparisons \
#       --skipEqualityCheck
# ```
#
# Output will be written to the console and as PDFs to `pathToFigure`
#
#------------------------------------------------------------------------------#

#------ Requirements ------
stopifnot(
  requireNamespace("RNetCDF"),
  requireNamespace("ggplot2"),
  requireNamespace("patchwork")
)


#------ . ------
#------ Grab command line arguments (if any)
args <- commandArgs(trailingOnly = TRUE)

skipEqualityCheck <- any(grepl("--skipEqualityCheck", args, fixed = TRUE))

skipFigures <- any(grepl("--skipFigures", args, fixed = TRUE))


#------ Paths (possibly as command-line arguments) ------
ids <- grepl("--pathToTools", args, fixed = TRUE)
dir_tools <- if (any(ids)) {
  sub("--pathToTools", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws()
} else {
  "tools"
}

stopifnot(dir.exists(dir_tools))

ids <- grepl("--pathToFigures", args, fixed = TRUE)
dir_figures <- if (any(ids)) {
  sub("--pathToFigures", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws()
} else {
  file.path("tools", "figures")
}

ids <- grepl("--pathToOut1", args, fixed = TRUE)
pathToOut1 <- if (any(ids)) {
  sub("--pathToOut1", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws()
} else {
 stop("--pathToOut1 is a required argument")
}

stopifnot(dir.exists(pathToOut1))


ids <- grepl("--pathToOut2", args, fixed = TRUE)
pathToOut2 <- if (any(ids)) {
  sub("--pathToOut2", "", args[ids], fixed = TRUE) |>
    sub("=", "", x = _, fixed = TRUE) |>
    trimws()
} else {
  stop("--pathToOut2 is a required argument")
}

stopifnot(dir.exists(pathToOut2))


#------ . ------
#--- Locate shared output files ------
tag1 <- basename(dirname(pathToOut1))
tag2 <- basename(dirname(pathToOut2))

if (identical(tag1, tag2)) {
  tag1 <- paste(basename(dirname(dirname(pathToOut1))), tag1, sep = "-")
  tag2 <- paste(basename(dirname(dirname(pathToOut2))), tag2, sep = "-")

  if (identical(tag1, tag2)) {
    tag1 <- dirname(pathToOut1)
    tag2 <- dirname(pathToOut2)

    if (identical(tag1, tag2)) {
      stop("Cannot distinguish pathToOut1 and pathToOut2", call. = FALSE)
    }
  }
}

cat("\n")
cat(
  sprintf("Comparison of nc-output file names (%s vs. %s): ", tag1, tag2),
  fill = TRUE
)
fnames1 <- list.files(path = pathToOut1, pattern = ".nc$", recursive = TRUE)
fnames2 <- list.files(path = pathToOut2, pattern = ".nc$", recursive = TRUE)

tmp <- setdiff(sort(basename(fnames1)), sort(basename(fnames2)))
cat("\tFiles in 1 but not 2:", toString(tmp), fill = TRUE)

tmp <- setdiff(sort(basename(fnames2)), sort(basename(fnames1)))
cat("\tFiles in 2 but not 1:", toString(tmp), fill = TRUE)

fnames <- intersect(fnames1, fnames2)
stopifnot(length(fnames) > 0L)
cat("\tShared files n =", length(fnames), fill = TRUE)


#------ . ------
#--- Equality of values ------
if (!skipEqualityCheck) {
  cat("\n")
  cat(
    sprintf("Comparison of values in nc-output (%s vs. %s): ", tag1, tag2),
    fill = TRUE
  )
  compareOut <- function(filename, path1, path2) {
    nc1 <- RNetCDF::open.nc(file.path(path1, filename))
    on.exit(RNetCDF::close.nc(nc1), add = TRUE)
    nc2 <- RNetCDF::open.nc(file.path(path2, filename))
    on.exit(RNetCDF::close.nc(nc2), add = TRUE)
    all.equal(RNetCDF::read.nc(nc1), RNetCDF::read.nc(nc2))
  }
  res <- vapply(
    fnames,
    function(fn) {
      isTRUE(try(compareOut(fn, pathToOut1, pathToOut2), silent = TRUE))
    },
    FUN.VALUE = NA
  )

  cat("\tFiles with equal values n =", sum(res), fill = TRUE)
  cat("\t", toString(names(res)[res]), sep = "", fill = TRUE)

  cat("\tFiles with differences in values n =", sum(!res), fill = TRUE)
  cat("\t", toString(names(res)[!res]), sep = "", fill = TRUE)
}


#------ . ------
#--- Visually compare output ------
if (!skipFigures) {
  cat("\n")
  cat(
    sprintf("Create figure of plots with nc-output (%s vs. %s): ", tag1, tag2),
    fill = TRUE
  )

  dir.create(dir_figures, recursive = TRUE, showWarnings = FALSE)

  tpout <- c("day", "week", "month", "year")

  fnameFigs <- file.path(
    dir_figures,
    sprintf("Fig__compareOutputNC__%s_vs_%s__%s.pdf", tag1, tag2, tpout)
  )

  hasOIC <- requireNamespace("ggokabeito", quietly = TRUE)

  pb <- utils::txtProgressBar(max = length(fnames), style = 3L)
  pbi <- 1L

  for (kt in seq_along(tpout)) {
    grDevices::pdf(file = fnameFigs[[kt]], height = 7L, width = 9L)

    usedFnamesIDs <- grep(sprintf("_%s.nc$", tpout[[kt]]), fnames)

    for (kf in usedFnamesIDs) {
      fn <- fnames[[kf]]

      nc1 <- RNetCDF::open.nc(file.path(pathToOut1, fn))
      x1 <- RNetCDF::read.nc(nc1)
      RNetCDF::close.nc(nc1)

      nc2 <- RNetCDF::open.nc(file.path(pathToOut2, fn))
      x2 <- RNetCDF::read.nc(nc2)
      RNetCDF::close.nc(nc2)

      vars <- setdiff(
        names(x1),
        c(
          "site",
          "lat", "lon", "latitude", "longitude", "x", "y",
          "lat_bnds", "lon_bnds",
          "latitude_bnds", "longitude_bnds",
          "y_bnds", "x_bnds",
          "domain",
          "crs_geogsc", "crs_projsc",
          "time", "time_bnds",
          "pft",
          "vertical", "vertical_bnds"
        )
      )

      nSizeDomain <- length(x1[["domain"]])
      nDimDomain <- length(dim(x1[["domain"]]))
      if (nDimDomain == 0L && nSizeDomain > 1L) {
        nDimDomain <- 1L
      }
      nSizeTime <- length(x1[["time"]])

      res <- lapply(
        vars,
        function(var) {
          vals <- as.vector(x1[[var]])
          nSizeVar <- length(vals)
          nDimVar <- length(dim(x1[[var]]))
          nDimVarNonDomain <- max(c(0, nDimVar - nDimDomain))
          hasVerticalVeg <- nDimVarNonDomain > 1L
          nSizeVerticalVeg <- nSizeVar / (nSizeTime * nSizeDomain)

          # SOILWAT2 <= v8.4.0: [pft, vertical, time, spatial]
          idsVerticalVeg <- rep_len(
            seq_len(nSizeVerticalVeg), length.out = nSizeVar
          ) |>
            factor()
          timeVals <- rep_len(
            rep(x1[["time"]], each = nSizeVerticalVeg), length.out = nSizeVar
          )
          suids <- rep_len(
            rep(seq_len(nSizeDomain), each = nSizeVerticalVeg * nSizeTime),
            length.out = nSizeVar
          )

          rbind(
            data.frame(
              id = tag1,
              var = var,
              suid = suids,
              time = timeVals,
              ind = idsVerticalVeg,
              value = vals
            ),
            data.frame(
              id = tag2,
              var = var,
              suid = suids,
              time = timeVals,
              ind = idsVerticalVeg,
              value = as.vector(x2[[var]])
            ),
            data.frame(
              id = "difference",
              var = paste0("diff(", var, ")"),
              suid = suids,
              time = timeVals,
              ind = idsVerticalVeg,
              value = as.vector(x2[[var]] - x1[[var]])
            )
          )
        }
      ) |>
        do.call(rbind, args = _)

      res[["identifier"]] <- paste(res[["id"]], res[["ind"]], sep = "/")

      nInds <- length(unique(res[["identifier"]]))

      tmpg <- list()

      tmpg[[1L]] <- ggplot2::ggplot(
        data = res[!res[["id"]] %in% "difference", ],
        mapping = ggplot2::aes(
          x = time,
          y = value,
          color = identifier,
          group = paste(suid, identifier)
        )
      ) +
        ggplot2::ggtitle(fn) +
        ggplot2::facet_wrap(ggplot2::vars(var), scales = "free_y") +
        ggplot2::geom_line(
          mapping = ggplot2::aes(linetype = id),
          show.legend = nInds < 9L
        )

      tmpg[[1L]] <- if (hasOIC && nInds < 9L) {
        tmpg[[1L]] + ggokabeito::scale_color_okabe_ito(name = "")
      } else {
        tmpg[[1L]] + ggplot2::scale_color_discrete(name = "")
      }

      tmpg[[1L]] <- tmpg[[1L]] +
        ggplot2::theme_bw() +
        ggplot2::theme(legend.position = "bottom")


      tmpg[[2L]] <- ggplot2::ggplot(
        data = res[res[["id"]] %in% "difference", ],
        mapping = ggplot2::aes(
          x = time,
          y = value,
          color = identifier,
          group = paste(suid, identifier)
        )
      ) +
        ggplot2::facet_wrap(ggplot2::vars(var), scales = "free_y") +
        ggplot2::geom_line(show.legend = FALSE) +
        ggplot2::geom_hline(yintercept = 0, linetype = "dashed")

      if (nInds == 1L) {
        tmpg[[2L]] <- tmpg[[2L]] + ggplot2::scale_color_manual(values = "black")
      } else if (hasOIC && nInds < 9L) {
        tmpg[[2L]] <- tmpg[[2L]] + ggokabeito::scale_color_okabe_ito()
      }

      tmpg[[2L]] <- tmpg[[2L]] +
        ggplot2::theme_bw()


      tmp <- patchwork::wrap_plots(
        tmpg,
        ncol = 1L,
        guides = "collect",
        axes = "collect",
        axis_titles = "collect"
      ) &
        ggplot2::theme(legend.position = "bottom")

      plot(tmp)

      utils::setTxtProgressBar(pb, pbi)
      pbi <- pbi + 1L
    }

    grDevices::dev.off()
  }

  close(pb)
  cat("\n")
}
#------ . ------
