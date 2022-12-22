#--- Create plots of horizontal and tilted
#    first sunrise and last sunset hour angles
#    for each latitude and a combination of slopes/aspects/day of year

# Run SOILWAT2 unit tests with appropriate flag
# ```
#   CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lats make test_run
# ```
#
# Produce plots based on output generated above
# ```
#   Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lats.R
# ```

#------
dir_out <- file.path("tests", "example", "Output")

tag_filename <- "SW2_SolarPosition_Test__hourangles_by_lats"

file_sw2_test_output <- list.files(
  dir_out,
  pattern = paste0("Table__", tag_filename, ".csv"),
  full.names = TRUE
)


do_plot <- TRUE


if (length(file_sw2_test_output) == 0) {
  stop("No ", shQuote(tag_filename), " output found.")
}

if (!requireNamespace("reshape2", quietly = TRUE)) {
  stop("Package 'reshape2' is required.")
}



if (do_plot) {
  data <- utils::read.csv(file_sw2_test_output, row.names = NULL)

  doys <- unique(data[, "DOY"])
  lats <- unique(data[, "Latitude"])
  slopes <- unique(data[, "Slope"])
  slopes_selected <- slopes[slopes > 0]
  aspects <- unique(data[, "Aspect"])
  dangle2 <- c(-10, -1, 0, 1, 10)

  var_sets <- list(
    horizontal = c("oH_sunrise", "oH_sunset"),
    primary = c("oT1_sunrise", "oT2_sunset"),
    secondary = c("oT1_sunset", "oT2_sunrise")
  )
  vars <- unlist(var_sets)


  # Convert data to array
  vals <- array(
    NA,
    dim = c(
      length(doys),
      length(lats),
      length(slopes),
      length(aspects),
      length(vars)
    ),
    dimnames = list(
      paste0("doy", doys),
      paste0("lat", lats),
      paste0("slope", slopes),
      paste0("asp", aspects),
      vars
    )
  )

  for (k in seq_along(vars)) {
    vals[, , , , vars[k]] <- reshape2::acast(
      data,
      DOY ~ Latitude ~ Slope ~ Aspect,
      drop = FALSE,
      value.var = vars[k]
    )
  }

  vals[vals == 999] <- NA


  #--- Helper functions
  make_panel <- function(vals, doys, slopes_selected, var1, var2,
    vars_oH = var_sets[["horizontal"]]
  ) {
    xlim <- c(-90, 90)
    ylim <- c(-pi, pi)
    legend_text <- dimnames(vals)[[4]]
    legend_colors <- grDevices::hcl.colors(n = length(legend_text))

    for (k1 in seq_along(doys)) {
      doy <- paste0("doy", doys[k1])

      for (k2 in seq_along(slopes_selected)) {
        slope <- paste0("slope", slopes_selected[k2])

        vars <- c(var1, var2)
        x <- vals[doy, , slope, , vars]
        xH <- vals[doy, , 2, 1, vars_oH]

        if (any(!is.na(x))) {

          graphics::plot(
            1,
            type = "n",
            xlab = "Latitude",
            ylab = "omega",
            xlim = xlim,
            ylim = ylim
          )

          id_good <- !apply(xH, 1, function(x) anyNA(x))
          graphics::polygon(
            x = c(lats[id_good], rev(lats[id_good])),
            y = c(xH[id_good, 1], rev(xH[id_good, 2])),
            col = "aliceblue",
            border = NA
          )

          graphics::matplot(
            x = lats,
            y = x[, , var1],
            col = legend_colors,
            type = "l",
            lty = 2,
            add = TRUE
          )
          graphics::matplot(
            x = lats,
            y = x[, , var2],
            col = legend_colors,
            type = "l",
            lty = 1,
            add = TRUE
          )

          graphics::abline(v = c(-1, 1) * (90 - 23.44), lty = 2, col = "gray")

          graphics::mtext(
            side = 3,
            text = paste(doy, slope),
            adj = 0.05
          )

        } else {
          graphics::plot.new()
        }

        if (k1 == 1 && k2 == 1) {
          graphics::legend(
            "top",
            ncol = 2,
            cex = 0.75,
            legend = legend_text,
            col = legend_colors,
            lwd = 2
          )
        }

      }
    }
  }



  #--- Make plots
  n_panels <- c(length(slopes_selected), length(doys))

  for (iasp in (-1):8) {
    if (iasp == -1) {
      aspect_used <- aspects
    } else {
      aspect_used <- (iasp - 4.) / 4. * 180 + dangle2
    }

    cn_aspu <- paste0("asp", aspect_used)
    tag_aspu <- paste0(
      "aspects", aspect_used[1],
      "to", aspect_used[length(aspect_used)]
    )

    for (iv in seq_along(var_sets)[-1]) {
      fname <- file.path(
        dirname(file_sw2_test_output),
        sub(
          "Table__",
          "Fig__",
          sub(
            ".csv",
            paste0(
              "__", names(var_sets)[iv],
              "_part", iasp + 1,
              "-", tag_aspu,
              ".pdf"
            ),
            basename(file_sw2_test_output)
          )
        )
      )

      grDevices::pdf(
        height = 2.75 * n_panels[1],
        width = 3 * n_panels[2],
        file = fname
      )

      par_prev <- graphics::par(
        mfcol = n_panels,
        mar = c(2.25, 2.25, 1.5, 0.5),
        mgp = c(1, 0, 0),
        tcl = 0.3,
        cex = 1
      )

      make_panel(
        vals = vals[, , , cn_aspu, ],
        doys = doys,
        slopes_selected = slopes_selected,
        var1 = var_sets[[iv]][1],
        var2 = var_sets[[iv]][2]
      )

      graphics::par(par_prev)
      grDevices::dev.off()
    }
  }
}

