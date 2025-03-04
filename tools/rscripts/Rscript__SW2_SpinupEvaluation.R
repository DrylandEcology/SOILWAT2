# Run SOILWAT2 unit tests with appropriate flag
# ```
#   CPPFLAGS=-DSW2_SpinupEvaluation make test
#   bin/sw_test --gtest_filter=*SpinupEvaluation*
# ```
#
# Produce plots based on output generated above
# ```
#   Rscript tools/rscripts/Rscript__SW2_SpinupEvaluation.R
# ```

#------

stopifnot(
  requireNamespace("ggplot2"),
  requireNamespace("patchwork"),
  requireNamespace("dplyr")
)

dir_out <- file.path("tests", "example", "Output")
dir_fig <- file.path("tools", "figures")
dir.create(dir_fig, recursive = TRUE, showWarnings = FALSE)

tag_filename <- "SW2_SpinupEvaluation"

file_sw2_test_outputs <- list.files(
  dir_out,
  pattern = paste0("Table__", tag_filename, "[[:print:]]*\\.csv"),
  full.names = TRUE
)

if (length(file_sw2_test_outputs) == 0L) {
  stop("No ", shQuote(tag_filename), " output found.")
}


for (k in seq_along(file_sw2_test_outputs)) {

  x <- utils::read.csv(file_sw2_test_outputs[[k]])

  ids <-
    !(x[["spinup_duration"]] == 0 & x[["stage"]] == "spinup") &
    (x[["spinup_duration"]] <= 3 | x[["spinup_duration"]] == 20)
  x <- x[ids, , drop = FALSE]

  xp <- list(
    swc = dplyr::rename(
      x[x[["variable"]] == "swc", , drop = FALSE],
      SWC = value
    ),
    ts = dplyr::rename(
      x[x[["variable"]] == "ts", , drop = FALSE],
      soilTemperature = value
    )
  )

  xp[["swc"]][["ts_init"]] <- as.factor(xp[["swc"]][["ts_init"]])
  xp[["ts"]][["swc_init"]] <- as.factor(xp[["ts"]][["swc_init"]])


  tmpg <- list()

  tmpg[[1L]] <- ggplot2::ggplot(
    data = xp[["swc"]],
    mapping = ggplot2::aes(
      y = SWC,
      x = soil_layer,
      col = stage,
      linetype = ts_init
    )
  ) +
    ggplot2::geom_line() +
    ggplot2::facet_grid(
      cols = ggplot2::vars(swc_init),
      rows = ggplot2::vars(spinup_duration)
    ) +
    ggplot2::theme_bw()


  tmpg[[2L]] <- ggplot2::ggplot(
    data = xp[["ts"]],
    mapping = ggplot2::aes(
      y = soilTemperature,
      x = soil_layer,
      col = stage,
      linetype = swc_init
    )
  ) +
    ggplot2::geom_line() +
    ggplot2::facet_grid(
      cols = ggplot2::vars(ts_init),
      rows = ggplot2::vars(spinup_duration)
    ) +
    ggplot2::theme_bw()


  grDevices::pdf(
    file = file.path(
      dir_fig,
      sub(
        "Table__", "Fig__",
        sub(".csv", ".pdf", basename(file_sw2_test_outputs[[k]]))
      )
    )
  )
  plot(
    patchwork::wrap_plots(tmpg, nrow = 2L, guides = "collect")
  )
  grDevices::dev.off()
}
