#--- Create plots of numbers of daylight hours and of sunrises/sunsets
#    for each latitude and day of year

# Run SOILWAT2 unit tests with appropriate flag
# ```
#   CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lat_and_doy make test test_run
# ```
#
# Produce plots based on output generated above
# ```
#   Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lat_and_doy.R
# ```

#------
dir_out <- file.path("testing", "Output")

tag_filename <- "SW2_SolarPosition_Test__hourangles_by_lat_and_doy"

file_sw2_test_outputs <- list.files(
  dir_out,
  pattern = paste0("Table__", tag_filename, "[[:print:]]*\\.csv"),
  full.names = TRUE
)


do_plot_maps <- TRUE
do_plot_expectations <- TRUE
do_plot_expectations2 <- TRUE
do_plot_expectations3 <- TRUE
do_plot_expectations4 <- TRUE
do_plot_expectations5 <- TRUE


if (length(file_sw2_test_outputs) == 0) {
  stop("No ", shQuote(tag_filename), " output found.")
}

if (!requireNamespace("reshape2", quietly = TRUE)) {
  stop("Package 'reshape2' is required.")
}



if (do_plot_maps || do_plot_expectations) {
  vars <- c(
    "Daylight_tilted_hours",
    "omega_indicator",
    "oT1_sunrise", "oT1_sunset", "oT2_sunrise", "oT2_sunset"
  )

  tmp <- seq.Date(
    from = as.Date("2019/1/1"),
    to = as.Date("2019/12/31"),
    by = "1 month"
  )
  xlab_doys <- c(as.POSIXlt(tmp)$yday + 1, 365)

  ylab_ats <- c(-90, -66.56, -45, 0, 45, 66.56, 90)

  if (do_plot_maps) {
    onelems <- 12
    nelems <- list(
      Daylight_tilted_hours = 24,
      omega_indicator = 5,
      oT1_sunrise = onelems,
      oT1_sunset = onelems,
      oT2_sunrise = onelems,
      oT2_sunset = onelems
    )

    ztol <- 1e-6
    olims <- c(-pi - ztol, pi + ztol)
    zlims <- list(
      Daylight_tilted_hours = c(0, 24),
      omega_indicator = c(-2, 2),
      oT1_sunrise = olims,
      oT1_sunset = olims,
      oT2_sunrise = olims,
      oT2_sunset = olims
    )

    ocols_srtoss <- c(
      grDevices::hcl.colors(n = onelems, palette = "viridis", rev = TRUE),
      "black",
      grDevices::hcl.colors(n = onelems, palette = "YlOrBr")
    )
    cols <- list(
      Daylight_tilted_hours = {
        cols24 <- grDevices::hcl.colors(n = 1 + nelems[[1]], palette = "YlOrRd")
        cols24[c(1, length(cols24))] <- c("black", "azure2")
        cols24
      },
      omega_indicator = c("blue", "gray", "azure2", "pink", "purple"),
      oT1_sunrise = ocols_srtoss,
      oT1_sunset = ocols_srtoss,
      oT2_sunrise = rev(ocols_srtoss),
      oT2_sunset = rev(ocols_srtoss)
    )

    olevels <- pretty(olims, n = onelems)
    levels <- list(
      Daylight_tilted_hours = 0:nelems[[1]],
      omega_indicator = (-2):2,
      oT1_sunrise = olevels,
      oT1_sunset = olevels,
      oT2_sunrise = olevels,
      oT2_sunset = olevels
    )

  }

  #--- Helper functions
  add_density_panel <- function(x) {

    has_spread <- diff(range(x, na.rm = TRUE)) > 1e-3

    if (has_spread) {
      plot(density(x), main = "")
    } else {
      plot.new()
    }

    br <- c(-24, -20, -5, -1, 1, 5, 20, 24)
    tt <- hist(x, breaks = br, plot = FALSE)[c("breaks", "counts")]
    tmpdf <- data.frame(
      Values = paste(br[-length(br)], "to", br[-1]),
      Count = tt[["counts"]]
    )

    tmpxy <- graphics::par("usr")[c(2, 4)]
    text(
      x = tmpxy[1],
      y = tmpxy[2],
      labels = paste(
        utils::capture.output(print(tmpdf, row.names = FALSE)),
        collapse = "\n"
      ),
      adj = c(1.05, 1.1),
      family = "mono"
    )
  }


  add_maxabs_panel <- function(x) {
    tmp <- apply(
      x,
      1,
      function(x) max(abs(x))
    )

    plot(
      tmp,
      type = "p",
      pch = 4,
      xlab = "Day of non-leap year",
      ylab = "Maximum absolute deviation"
    )
  }


  #--- Extract slope/aspect combinations
  ftmp <- strsplit(basename(file_sw2_test_outputs), split = "__|\\.")
  file_slopes <- as.integer(sapply(ftmp, function(x) sub("slope", "", x[[4]])))
  fig_slopes <- unique(file_slopes)


  #--- Plot all aspects for each slope value
  for (isl in seq_along(fig_slopes)) {
    ivars_used <- if (fig_slopes[isl] == 0) 1:2 else seq_along(vars)

    fids <- which(file_slopes == fig_slopes[isl])

    # Extract aspects for this slope value
    aspects <- as.integer(sapply(
      X = ftmp[fids],
      FUN = function(x) sub(".csv", "", sub("aspect", "", x[[5]]))
    ))

    order_by_aspect <- order(aspects)
    aspects <- aspects[order_by_aspect]
    fids <- fids[order_by_aspect]


    #--- Create lat x doy maps
    if (do_plot_maps) {

      n_panels <- c(length(fids), length(ivars_used))

      # Start plot
      fname <- file.path(
        dirname(file_sw2_test_outputs[fids[1]]),
        sub(
          "Table__",
          "Fig__",
          sub(
            "__csv",
            paste0("__slope", fig_slopes[isl], ".png"),
            paste0(ftmp[fids[1]][[1]][-c(4, 5)], collapse = "__")
          )
        )
      )

      grDevices::png(
        height = n_panels[1] * 3.5,
        width = n_panels[2] * 5,
        units = "in",
        res = 300,
        file = fname
      )

      par_prev <- graphics::par(
        mfrow = n_panels,
        mar = c(2.25, 2.25, 1.5, 0.75),
        mgp = c(1, 0, 0),
        tcl = 0.3,
        cex = 1
      )

      # Loop over aspects/rows for this slope
      for (k in fids) {
        # Read and prepare data
        x <- utils::read.csv(file = file_sw2_test_outputs[k])

        lats <- sort(unique(x[, "Latitude"]))
        doys <- sort(unique(x[, "DOY"]))

        for (iv in ivars_used) {
          x2 <- reshape2::acast(
            x,
            Latitude ~ DOY,
            drop = FALSE,
            value.var = vars[iv]
          )
          tx2 <- t(x2)

          graphics::image(
            doys,
            lats,
            tx2,
            zlim = zlims[[iv]],
            col = cols[[iv]],
            xlab = "Day of non-leap year",
            ylab = "Latitude",
            axes = FALSE
          )

          graphics::contour(
            doys,
            lats,
            tx2,
            levels = levels[[iv]],
            add = TRUE
          )

          graphics::box()

          graphics::axis(side = 1, at = xlab_doys)
          graphics::axis(side = 2, at = ylab_ats)

          title(
            main = paste0(
              gsub("_", " ", vars[iv]), ": ",
              "slope = ", round(x[1, "Slope"], 1), " / ",
              "aspect = ", round(x[1, "Aspect"], 1)
            )
          )
        }
      }


      graphics::par(par_prev)
      grDevices::dev.off()
    }


    #--- Create maps of expected symmetries
    if (do_plot_expectations) {
      tmp2 <- seq(-7, 7, by = 2)
      tmp1 <- seq(-(24 + 4), 24 + 4, by = 1)
      colsDiffs <- grDevices::hcl.colors(
        n = length(tmp1),
        palette = "Purple-Green"
      )[!(tmp1 %in% tmp2)]


      # Expectation 2: Daylength:
      #   symmetric in aspect reflected around South aspect
      if (do_plot_expectations2) {

        aspects_paired <- data.frame(
          aspect = tmp <- sort(unique(abs(aspects))),
          aspect_reflected = -tmp
        )

        ids_available <- apply(aspects_paired, 1, function(x) all(x %in% aspects))
        aspects_paired <- aspects_paired[ids_available, , drop = FALSE]

        n_panels <- c(nrow(aspects_paired), 3)

        # Start plot
        fname <- file.path(
          dirname(file_sw2_test_outputs[fids[1]]),
          sub(
            "Table__",
            "Fig__Symmetry_ReflectedAspect__",
            sub(
              "__csv",
              paste0("__slope", fig_slopes[isl], ".png"),
              paste0(ftmp[fids[1]][[1]][-c(4, 5)], collapse = "__")
            )
          )
        )

        grDevices::png(
          height = n_panels[1] * 3.5,
          width = n_panels[2] * 5,
          units = "in",
          res = 300,
          file = fname
        )

        par_prev <- graphics::par(
          mfrow = n_panels,
          mar = c(2.25, 2.25, 1.5, 0.75),
          mgp = c(1, 0, 0),
          tcl = 0.3,
          cex = 1
        )

        for (k in seq_len(n_panels[1])) {
          # Read and prepare data
          id1 <- fids[aspects == aspects_paired[k, "aspect"]]
          x1 <- utils::read.csv(file = file_sw2_test_outputs[id1])
          id2 <- fids[aspects == aspects_paired[k, "aspect_reflected"]]
          x2 <- utils::read.csv(file = file_sw2_test_outputs[id2])

          lats <- sort(unique(x1[, "Latitude"]))
          doys <- sort(unique(x1[, "DOY"]))

          x1m <- reshape2::acast(
            x1,
            Latitude ~ DOY,
            drop = FALSE,
            value.var = "Daylight_tilted_hours"
          )

          x2m <- reshape2::acast(
            x2,
            Latitude ~ DOY,
            drop = FALSE,
            value.var = "Daylight_tilted_hours"
          )


          txdiff <- t(x1m - x2m)

          zlim <- c(-1, 1) * max(abs(range(txdiff)))

          graphics::image(
            doys,
            lats,
            txdiff,
            zlim = zlim,
            col = colsDiffs,
            xlab = "Day of non-leap year",
            ylab = "Latitude",
            axes = FALSE
          )

          graphics::contour(
            doys,
            lats,
            txdiff,
            add = TRUE
          )

          graphics::box()

          graphics::axis(side = 1, at = xlab_doys)
          graphics::axis(side = 2, at = ylab_ats)

          title(
            main = paste0(
              "Slope = ", round(x1[1, "Slope"], 1), " / ",
              "aspect = ", round(x1[1, "Aspect"], 1),
              "|", round(x2[1, "Aspect"], 1)
            )
          )


          #--- Density panel
          add_density_panel(txdiff)

          #--- Max-abs margin panel
          add_maxabs_panel(txdiff)
        }


        graphics::par(par_prev)
        grDevices::dev.off()
      }


      # Expectation 3: Tilted sunrise/sunset:
      #   negatively symmetric in aspect reflected around South aspect
      if (do_plot_expectations3) {
        if (fig_slopes[isl] > 0) {
          ivars_used2 <- 3:6

          aspects_paired <- data.frame(
            aspect = tmp <- sort(unique(abs(aspects))),
            aspect_reflected = -tmp
          )

          ids_available <- apply(aspects_paired, 1, function(x) all(x %in% aspects))
          aspects_paired <- aspects_paired[ids_available, , drop = FALSE]

          n_panels <- c(nrow(aspects_paired), length(ivars_used2))

          # Start plot
          fname <- file.path(
            dirname(file_sw2_test_outputs[fids[1]]),
            sub(
              "Table__",
              "Fig__Symmetry_ReflectedAspect2__",
              sub(
                "__csv",
                paste0("__slope", fig_slopes[isl], ".png"),
                paste0(ftmp[fids[1]][[1]][-c(4, 5)], collapse = "__")
              )
            )
          )

          grDevices::png(
            height = n_panels[1] * 3.5,
            width = n_panels[2] * 5,
            units = "in",
            res = 300,
            file = fname
          )

          par_prev <- graphics::par(
            mfrow = n_panels,
            mar = c(2.25, 2.25, 1.5, 0.75),
            mgp = c(1, 0, 0),
            tcl = 0.3,
            cex = 1
          )

          for (k in seq_len(n_panels[1])) {
            # Read and prepare data
            id1 <- fids[aspects == aspects_paired[k, "aspect"]]
            x1 <- utils::read.csv(file = file_sw2_test_outputs[id1])
            id2 <- fids[aspects == aspects_paired[k, "aspect_reflected"]]
            x2 <- utils::read.csv(file = file_sw2_test_outputs[id2])

            lats <- sort(unique(x1[, "Latitude"]))
            doys <- sort(unique(x1[, "DOY"]))

            for (iv in seq_along(ivars_used2)) {
              var1 <- vars[ivars_used2[iv]]
              var2 <- vars[rev(ivars_used2)[iv]]

              x1m <- reshape2::acast(
                x1,
                Latitude ~ DOY,
                drop = FALSE,
                value.var = var1
              )
              x1m[x1m == 999] <- 0

              x2m <- reshape2::acast(
                x2,
                Latitude ~ DOY,
                drop = FALSE,
                value.var = var2
              )
              x2m[x2m == 999] <- 0

              txdiff <- t(x1m + x2m)

              zlim <- c(-1, 1) * max(abs(range(txdiff)))

              graphics::image(
                doys,
                lats,
                txdiff,
                zlim = zlim,
                col = colsDiffs,
                xlab = "Day of non-leap year",
                ylab = "Latitude",
                axes = FALSE
              )

              graphics::contour(
                doys,
                lats,
                txdiff,
                add = TRUE
              )

              graphics::box()

              graphics::axis(side = 1, at = xlab_doys)
              graphics::axis(side = 2, at = ylab_ats)

              title(
                main = paste0(
                  var1, " vs ", var2, ": ",
                  "slope = ", round(x1[1, "Slope"], 1), " / ",
                  "aspect = ", round(x1[1, "Aspect"], 1),
                  "|", round(x2[1, "Aspect"], 1)
                ),
                cex.main = 0.75
              )
            }
          }


          graphics::par(par_prev)
          grDevices::dev.off()
        }
      }


      # Expectation 4: Daylength:
      #   approximately symmetric in day of year reflected around
      #   June solstice
      if (do_plot_expectations4) {
        n_panels <- c(length(fids), 3)

        # Start plot
        fname <- file.path(
          dirname(file_sw2_test_outputs[fids[1]]),
          sub(
            "Table__",
            "Fig__Symmetry_ReflectedDOY__",
            sub(
              "__csv",
              paste0("__slope", fig_slopes[isl], ".png"),
              paste0(ftmp[fids[1]][[1]][-c(4, 5)], collapse = "__")
            )
          )
        )

        grDevices::png(
          height = n_panels[1] * 3.5,
          width = n_panels[2] * 5,
          units = "in",
          res = 300,
          file = fname
        )

        par_prev <- graphics::par(
          mfrow = n_panels,
          mar = c(2.25, 2.25, 1.5, 0.75),
          mgp = c(1, 0, 0),
          tcl = 0.3,
          cex = 1
        )

        # Loop over aspects/rows for this slope
        for (k in fids) {
          # Read and prepare data
          x <- utils::read.csv(file = file_sw2_test_outputs[k])

          lats <- sort(unique(x[, "Latitude"]))
          doys <- sort(unique(x[, "DOY"]))
          idoys2 <- (2 * 172 - doys - 1) %% 365 + 1

          x1m <- reshape2::acast(
            x,
            Latitude ~ DOY,
            drop = FALSE,
            value.var = "Daylight_tilted_hours"
          )

          x2m <- x1m[, idoys2]

          txdiff <- t(x1m - x2m)

          zlim <- c(-1, 1) * max(abs(range(txdiff)))

          graphics::image(
            doys,
            lats,
            txdiff,
            zlim = zlim,
            col = colsDiffs,
            xlab = "Day of non-leap year",
            ylab = "Latitude",
            axes = FALSE
          )

          graphics::contour(
            doys,
            lats,
            txdiff,
            add = TRUE
          )

          graphics::box()

          graphics::axis(side = 1, at = xlab_doys)
          graphics::axis(side = 2, at = ylab_ats)

          title(
            main = paste0(
              "slope = ", round(x[1, "Slope"], 1), " / ",
              "aspect = ", round(x[1, "Aspect"], 1)
            )
          )

          #--- Density panel
          add_density_panel(txdiff)

          #--- Max-abs margin panel
          add_maxabs_panel(txdiff)
        }


        graphics::par(par_prev)
        grDevices::dev.off()
      }


      # Expectation 5: Daylength:
      #   approximately symmetric in day of year shifted by half-year,
      #   reflected latitude, and flipped aspect
      if (do_plot_expectations5) {

        aspects_flipped <- data.frame(
          aspect = tmp <- sort(unique(-abs(aspects)), decreasing = TRUE),
          aspects_flipped = 180 + tmp
        )

        ids_available <- apply(aspects_flipped, 1, function(x) all(x %in% aspects))
        aspects_flipped <- aspects_flipped[ids_available, , drop = FALSE]

        n_panels <- c(nrow(aspects_flipped), 3)

        if (n_panels[1] > 0) {
          # Start plot
          fname <- file.path(
            dirname(file_sw2_test_outputs[fids[1]]),
            sub(
              "Table__",
              "Fig__Symmetry_ShiftedDOY-ReflectedLatitude-FlippedAspect__",
              sub(
                "__csv",
                paste0("__slope", fig_slopes[isl], ".png"),
                paste0(ftmp[fids[1]][[1]][-c(4, 5)], collapse = "__")
              )
            )
          )

          grDevices::png(
            height = n_panels[1] * 3.5,
            width = n_panels[2] * 5,
            units = "in",
            res = 300,
            file = fname
          )

          par_prev <- graphics::par(
            mfrow = n_panels,
            mar = c(2.25, 2.25, 1.5, 0.75),
            mgp = c(1, 0, 0),
            tcl = 0.3,
            cex = 1
          )

          # Loop over aspects/rows for this slope
          for (k in seq_len(n_panels[1])) {
            # Read and prepare data
            id1 <- fids[aspects == aspects_flipped[k, "aspect"]]
            x1 <- utils::read.csv(file = file_sw2_test_outputs[id1])
            id2 <- fids[aspects == aspects_flipped[k, "aspects_flipped"]]
            x2 <- utils::read.csv(file = file_sw2_test_outputs[id2])

            lats <- sort(unique(x1[, "Latitude"]))
            ilats2 <- rev(seq_along(lats))
            doys <- sort(unique(x1[, "DOY"]))
            idoys2 <- (doys - 1 + 183) %% 365 + 1

            x1m <- reshape2::acast(
              x1,
              Latitude ~ DOY,
              drop = FALSE,
              value.var = "Daylight_tilted_hours"
            )

            x2m <- reshape2::acast(
              x2,
              Latitude ~ DOY,
              drop = FALSE,
              value.var = "Daylight_tilted_hours"
            )

            txdiff <- t(x1m - x2m[ilats2, idoys2])

            zlim <- c(-1, 1) * max(abs(range(txdiff)))

            graphics::image(
              doys,
              lats,
              txdiff,
              zlim = zlim,
              col = colsDiffs,
              xlab = "Day of non-leap year",
              ylab = "Latitude",
              axes = FALSE
            )

            graphics::contour(
              doys,
              lats,
              txdiff,
              add = TRUE
            )

            graphics::box()

            graphics::axis(side = 1, at = xlab_doys)
            graphics::axis(side = 2, at = ylab_ats)

            title(
              main = paste0(
                "slope = ", round(x1[1, "Slope"], 1), " / ",
                "aspect = ", round(x1[1, "Aspect"], 1),
                "|", round(x2[1, "Aspect"], 1)
              )
            )

            #--- Density panel
            add_density_panel(txdiff)

            #--- Max-abs margin panel
            add_maxabs_panel(txdiff)
          }


          graphics::par(par_prev)
          grDevices::dev.off()
        }
      }
    }
  }
}

