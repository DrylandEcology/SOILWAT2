#--- Create a figure to visualize effects of
#    temperature, relative humidity, wind speed, and cloud cover
#    on annual PET

# Run SOILWAT2 unit tests with appropriate flag
# ```
#   CPPFLAGS=-DSW2_PET_Test__petfunc_by_temps make test
#   bin/sw_test --gtest_filter=*PETPetfuncByTemps*
# ```
#
# Produce plots based on output generated above
# ```
#   Rscript tools/rscripts/Rscript__SW2_PET_Test__petfunc_by_temps.R
# ```

#------
dir_out <- file.path("tests", "example", "Output")
dir_fig <- file.path("tools", "figures")
dir.create(dir_fig, recursive = TRUE, showWarnings = FALSE)

tag_filename <- "SW2_PET_Test__petfunc_by_temps"

file_sw2_test_output <- list.files(
  dir_out,
  pattern = paste0("Table__", tag_filename, ".csv"),
  full.names = TRUE
)


do_plot <- TRUE


if (length(file_sw2_test_output) == 0) {
  stop("No ", shQuote(tag_filename), " output found.")
}


if (!requireNamespace("ggplot2", quietly = TRUE)) {
  stop("Package 'ggplot2' is required.")
}


if (do_plot) {
  data_pet <- utils::read.csv(file_sw2_test_output, row.names = NULL)

  fH <- unique(data_pet[, "fH_gt"])
  ws <- unique(data_pet[, "windspeed_m_per_s"])
  cc <- unique(data_pet[, "cloudcover_pct"])
  rh <- unique(data_pet[, "RH_pct"])

  all_labeller <- ggplot2::labeller(
    windspeed_m_per_s = stats::setNames(
      paste0("Wind speed: ", ws, " m/s"),
      nm = ws
    ),
    cloudcover_pct = stats::setNames(
      paste0("Cloud cover: ", round(cc), "%"),
      nm = cc
    ),
    RH_pct = stats::setNames(
      paste0("Rel. humidity: ", round(rh), "%"),
      nm = rh
    ),
    fH_gt = stats::setNames(paste0("RSDS scaler: ", fH * 100, "%"), nm = fH)
  )


  #--- Fixed radiation
  ids_fH <- data_pet[, "fH_gt"] == 1


  #--- Layout: x = cc, y = ws, by = rh
  n_panels <- c(length(ws), length(cc))

  pdf(
    file = file.path(dir_fig, paste0("Fig__", tag_filename, "__by_RH", ".pdf")),
    height = 2 * n_panels[1],
    width = 2 * n_panels[2]
  )

  tmp <- ggplot2::ggplot(
    data_pet[ids_fH, ],
    ggplot2::aes(Temperature_C, PET_mm, group = RH_pct, color = RH_pct)
  ) +
    ggplot2::xlim(-10, 40) +
    ggplot2::labs(x = "Temperature (C)", y = "Annual PET (mm)", color = "RH (%)") +
    ggplot2::geom_line(linewidth = 0.75) +
    ggplot2::scale_color_viridis_c(direction = -1) +
    ggplot2::facet_grid(
      windspeed_m_per_s ~ cloudcover_pct,
      labeller = all_labeller
    )

  if (requireNamespace("egg", quietly = TRUE)) {
    tmp <- tmp +
      egg::theme_article()
  } else {
    tmp <- tmp +
      ggplot2::theme_classic()
  }

  tmp <- tmp +
    ggplot2::theme(
      legend.position.inside = c(
        0.4 / (n_panels[2] + 1),
        1 - 0.4 / (n_panels[1] + 1)
      ),
      legend.key.size = ggplot2::unit(0.015, units = "npc"),
      legend.title = ggplot2::element_text(size = 10)
    )

  plot(tmp)
  #egg::tag_facet()


  dev.off()


  #--- Layout: x = ws, y = rh, by = cc
  n_panels <- c(length(rh), length(ws))

  pdf(
    file = file.path(dir_fig, paste0("Fig__", tag_filename, "__by_CloudCover", ".pdf")),
    height = 2 * n_panels[1],
    width = 2 * n_panels[2]
  )

  tmp <- ggplot2::ggplot(
    data_pet[ids_fH, ],
    ggplot2::aes(Temperature_C, PET_mm, group = cloudcover_pct, color = cloudcover_pct)
  ) +
    ggplot2::xlim(-10, 40) +
    ggplot2::labs(x = "Temperature (C)", y = "Annual PET (mm)", color = "Clouds (%)") +
    ggplot2::geom_line(linewidth = 0.75) +
    ggplot2::scale_color_viridis_c(direction = -1) +
    ggplot2::facet_grid(
      RH_pct ~ windspeed_m_per_s,
      labeller = all_labeller
    )

  if (requireNamespace("egg", quietly = TRUE)) {
    tmp <- tmp +
      egg::theme_article()
  } else {
    tmp <- tmp +
      ggplot2::theme_classic()
  }

  tmp <- tmp +
    ggplot2::theme(
      legend.position.inside = c(
        0.4 / (n_panels[2] + 1),
        1 - 0.4 / (n_panels[1] + 1)
      ),
      legend.key.size = ggplot2::unit(0.015, units = "npc"),
      legend.title = ggplot2::element_text(size = 10)
    )

  plot(tmp)
  #egg::tag_facet()

  dev.off()



  #--- Layout: x = cc, y = rh, by = ws
  n_panels <- c(length(rh), length(cc))

  pdf(
    file = file.path(dir_fig, paste0("Fig__", tag_filename, "__by_WindSpeed", ".pdf")),
    height = 2 * n_panels[1],
    width = 2 * n_panels[2]
  )

  tmp <- ggplot2::ggplot(
    data_pet[ids_fH, ],
    ggplot2::aes(Temperature_C, PET_mm, group = windspeed_m_per_s, color = windspeed_m_per_s)
  ) +
    ggplot2::xlim(-10, 40) +
    ggplot2::labs(x = "Temperature (C)", y = "Annual PET (mm)", color = "Wind speed (m/s)") +
    ggplot2::geom_line(linewidth = 0.75) +
    ggplot2::scale_color_viridis_c(direction = -1) +
    ggplot2::facet_grid(
      RH_pct ~ cloudcover_pct,
      labeller = all_labeller
    )

  if (requireNamespace("egg", quietly = TRUE)) {
    tmp <- tmp +
      egg::theme_article()
  } else {
    tmp <- tmp +
      ggplot2::theme_classic()
  }

  tmp <- tmp +
    ggplot2::theme(
      legend.position.inside = c(
        0.4 / (n_panels[2] + 1),
        1 - 0.4 / (n_panels[1] + 1)
      ),
      legend.key.size = ggplot2::unit(0.015, units = "npc"),
      legend.title = ggplot2::element_text(size = 10)
    )

  plot(tmp)
  #egg::tag_facet()

  dev.off()



  #--- Fixed cloud cover
  ids_cc <- data_pet[, "cloudcover_pct"] == cc[3]

  #--- Layout: x = ws, y = rh, by = fH
  n_panels <- c(length(rh), length(ws))

  pdf(
    file = file.path(dir_fig, paste0("Fig__", tag_filename, "__by_fRSDS", ".pdf")),
    height = 2 * n_panels[1],
    width = 2 * n_panels[2]
  )

  tmp <- ggplot2::ggplot(
    data_pet[ids_cc, ],
    ggplot2::aes(Temperature_C, PET_mm, group = factor(fH_gt), color = factor(fH_gt))
  ) +
    ggplot2::xlim(-10, 40) +
    ggplot2::labs(x = "Temperature (C)", y = "Annual PET (mm)", color = "fRSDS (%)") +
    ggplot2::geom_line(linewidth = 0.75) +
    ggplot2::scale_color_viridis_d(direction = -1) +
    ggplot2::facet_grid(
      RH_pct ~ windspeed_m_per_s,
      labeller = all_labeller
    )

  if (requireNamespace("egg", quietly = TRUE)) {
    tmp <- tmp +
      egg::theme_article()
  } else {
    tmp <- tmp +
      ggplot2::theme_classic()
  }

  tmp <- tmp +
    ggplot2::theme(
      legend.position.inside = c(
        0.4 / (n_panels[2] + 1),
        1 - 0.4 / (n_panels[1] + 1)
      ),
      legend.key.size = ggplot2::unit(0.015, units = "npc"),
      legend.title = ggplot2::element_text(size = 10)
    )

  plot(tmp)
  #egg::tag_facet()

  dev.off()

}

