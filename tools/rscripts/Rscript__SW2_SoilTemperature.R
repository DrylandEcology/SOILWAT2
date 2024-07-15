
# Run script from within SOILWAT2/ with
# ```
#    Rscript tools/rscripts/Rscript__SW2_SoilTemperature.R
# ```
# after running SOILWAT2 on "tests/example", e.g.,
# ```
#    make bin_run
# ```

# Note: do not commit output (figures) from this script

dir_testing <- file.path("tests", "example")
dir_sw2_input <- file.path(dir_testing, "Input")
dir_sw2_output <- file.path(dir_testing, "Output")

dir_fig <- file.path("tools", "figures")
dir.create(dir_fig, recursive = TRUE, showWarnings = FALSE)


#--- Read data
soils <- utils::read.table(file.path(dir_sw2_input, "soils.in"), header = FALSE)

xw <- utils::read.csv(file.path(dir_sw2_output, "sw2_daily.csv"))

x <- utils::read.csv(file.path(dir_sw2_output, "sw2_daily_slyrs.csv"))

tag_soil_temp <- "SOILTEMP_Lyr"
tag_surface_temp <- "SOILTEMP_surfaceTemp_C"
if (any(grepl(tag_surface_temp, colnames(x)))) {
  xsurf <- x
} else {
  tag_surface_temp <- "TEMP_surfaceTemp"
  xsurf <- xw
}


#--- Convert to long format

# Surface soil temperature
tmp_vars1 <- grep(tag_surface_temp, colnames(xsurf), value = TRUE)
stopifnot(length(tmp_vars1) > 0)
tmp1 <- as.data.frame(tidyr::pivot_longer(
  xsurf[, c("Year", "Day", tmp_vars1)],
  cols = tidyr::all_of(tmp_vars1),
  names_to = "Fun",
  names_pattern = paste0(tag_surface_temp, "_(.*)_C"),
  values_to = "Temp_C"
))
tmp1[, "Layer"] <- 0

# Soil temperature
tmp_vars2 <- grep(tag_soil_temp, colnames(x), value = TRUE)
tmp2 <- as.data.frame(tidyr::pivot_longer(
  x[, c("Year", "Day", tmp_vars2)],
  cols = tidyr::all_of(tmp_vars2),
  names_to = c("Layer", "Fun"),
  names_pattern = paste0(tag_soil_temp, "_(.)_(.*)_C"),
  values_to = "Temp_C"
))

# Air temperature
tmp_vars0 <- grep("TEMP_[[:alpha:]]{3}_C", colnames(xw), value = TRUE)
tmp0 <- as.data.frame(tidyr::pivot_longer(
  xw[, c("Year", "Day", tmp_vars0)],
  cols = tidyr::all_of(tmp_vars0),
  names_to = "Fun",
  names_pattern = "TEMP_(.*)_C",
  values_to = "Temp_C"
))
tmp0[, "Layer"] <- -1

xtsoil <- rbind(tmp0[, colnames(tmp2)], tmp1[, colnames(tmp2)], tmp2)

xtsoil[, "depth_cm"] <- c(-10, 0, soils[, 1])[2 + as.integer(xtsoil[, "Layer"])]

n_funs <- length(unique(xtsoil[, "Fun"]))

# Temperature range
tmpr <-
  dplyr::filter(xtsoil, Fun == "max")$Temp_C -
  dplyr::filter(xtsoil, Fun == "min")$Temp_C
xtsoil[, "SoilTempRange"] <- rep(tmpr, each = n_funs)

# Temperature deviation
tmp_max_delta <-
  dplyr::filter(xtsoil, Fun == "max")$Temp_C -
  dplyr::filter(xtsoil, Fun == "avg")$Temp_C
xtsoil[, "max_delta"] <- rep(tmp_max_delta, each = n_funs)

tmp_min_delta <-
  dplyr::filter(xtsoil, Fun == "min")$Temp_C -
  dplyr::filter(xtsoil, Fun == "avg")$Temp_C
xtsoil[, "min_delta"] <- rep(tmp_min_delta, each = n_funs)



#--- Calculate deviation from mean
tmp <- dplyr::filter(xtsoil, Fun == "avg")
xtsoil_delta <- tidyr::pivot_longer(
  tmp[, c("Year", "Day", "depth_cm", "max_delta", "min_delta")],
  cols = c("max_delta", "min_delta"),
  names_to = "Fun",
  values_to = "delta_Temp_C"
)


#--- VWC vs soil temperature
tmp_vars3 <- grep("VWCBULK_Lyr", colnames(x), value = TRUE)
xvwc <- as.data.frame(tidyr::pivot_longer(
  x[, c("Year", "Day", tmp_vars3)],
  cols = tidyr::all_of(tmp_vars3),
  names_to = "Layer",
  names_pattern = "VWCBULK_Lyr_(.*)",
  values_to = "VWC"
))

xvwc2 <- merge(
  xvwc,
  dplyr::filter(xtsoil, Fun == "avg"),
  by = c("Year", "Day", "Layer"),
  all.x = TRUE,
  all.y = FALSE
)


#--- Panels by layer showing trends over one year
grDevices::pdf(file = file.path(dir_fig, "Fig_SoilTemperature_by_Layer.pdf"))

ggplot2::ggplot(
  data = dplyr::filter(xtsoil, Year == 1980)
) +
  ggplot2::geom_line(
    ggplot2::aes(x = Day, y = Temp_C, color = Fun)
  ) +
  ggplot2::facet_wrap(
    dplyr::vars(depth_cm),
    scales = "fixed"
  ) +
  ggplot2::theme_bw()

grDevices::dev.off()


grDevices::pdf(file = file.path(dir_fig, "Fig_SoilTemperatureDelta_by_Layer.pdf"))

ggplot2::ggplot(
  data = dplyr::filter(xtsoil_delta, Year == 2010)
) +
  ggplot2::geom_line(
    ggplot2::aes(x = Day, y = delta_Temp_C, color = Fun)
  ) +
  ggplot2::facet_wrap(
    dplyr::vars(depth_cm),
    scales = "free_y"
  ) +
  ggplot2::theme_bw()

grDevices::dev.off()



#--- Soil temperature range vs soil moisture
grDevices::pdf(
  file = file.path(dir_fig, "Fig_SoilTemperatureRange_vs_SoilMoisture.pdf")
)

ggplot2::ggplot(
  data = xvwc2
) +
  ggplot2::geom_hex(
    ggplot2::aes(x = VWC, y = SoilTempRange)
  ) +
  ggplot2::facet_wrap(dplyr::vars(depth_cm)) +
  ggplot2::theme_bw()

grDevices::dev.off()



#--- Panels of climate norms vs. soil depth
tmp1 <- xtsoil |>
  dplyr::group_by(depth_cm, Fun) |>
  dplyr::summarize(
    Temp_C = mean(Temp_C)
  )
tmp1$type = "Mean across: all days"

# remove air temperature on days when surface temperatur range = 0
tmp_days <- paste0(xtsoil[, "Year"], "-", xtsoil[, "Day"])
rm_days <- unique(
  tmp_days[xtsoil[, "Layer"] == 0 & xtsoil[, "SoilTempRange"] <= 0]
)
rm_ids <- tmp_days %in% rm_days & xtsoil[, "Layer"] == -1

tmp2 <- xtsoil[!rm_ids, , drop = FALSE] |>
  dplyr::filter(SoilTempRange > 0) |>
  dplyr::group_by(depth_cm, Fun) |>
  dplyr::summarize(
    Temp_C = mean(Temp_C)
  )
tmp2$type = "Mean across: range > 0 C"

xtsoilClim <- rbind(tmp1, tmp2)


grDevices::pdf(file = file.path(dir_fig, "Fig_SoilTemperatureClim_by_Layer.pdf"))

ggplot2::ggplot(
  data = xtsoilClim
) +
  ggplot2::geom_path(
    ggplot2::aes(x = Temp_C, y = depth_cm, group = Fun)
  ) +
  ggplot2::facet_grid(rows = dplyr::vars(type)) +
  ggplot2::scale_y_reverse() +
  ggplot2::geom_hline(yintercept = 0, color = "orange", linetype = "dashed") +
  ggplot2::theme_bw()

grDevices::dev.off()
