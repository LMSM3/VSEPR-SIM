#!/usr/bin/env Rscript

args <- commandArgs(trailingOnly = TRUE)
output_dir <- if (length(args) >= 1) args[[1]] else file.path("out", "dual_backend_3d")
scenes_path <- file.path(output_dir, "scenes.csv")
particles_path <- file.path(output_dir, "particles.csv")
manifest_path <- file.path(output_dir, "manifest.json")

for (path in c(scenes_path, particles_path, manifest_path)) {
  if (!file.exists(path)) stop("Required demo artifact is missing: ", path, call. = FALSE)
}

scenes <- read.csv(scenes_path, check.names = FALSE)
particles <- read.csv(particles_path, check.names = FALSE)
manifest <- if (requireNamespace("jsonlite", quietly = TRUE)) {
  jsonlite::fromJSON(manifest_path)
} else {
  list(schema = "vsepr.dual_backend_3d.v1", manifest_path = manifest_path)
}

plot_proxy_summary <- function(device) {
  device()
  on.exit(dev.off(), add = TRUE)
  metrics <- t(as.matrix(scenes[, c("cohesion_proxy", "texture_proxy", "stabilization_proxy")]))
  colors <- c("#3366CC", "#DC3912", "#109618")
  barplot(metrics,
		  beside = TRUE,
		  col = colors,
		  names.arg = paste0("Scene ", scenes$scene_id),
		  ylim = c(0, max(1, metrics, na.rm = TRUE)),
		  ylab = "Proxy value",
		  main = "VSEPR-SIM dual-backend 3D demonstration")
  legend("topright",
		 legend = c("Cohesion", "Texture", "Stabilization"),
		 fill = colors,
		 bty = "n")
}

plot_proxy_summary(function() png(file.path(output_dir, "proxy_summary.png"), width = 1400, height = 900, res = 140))
plot_proxy_summary(function() pdf(file.path(output_dir, "proxy_summary.pdf"), width = 10, height = 6.5))

manifest_table <- data.frame(
  key = names(manifest),
  value = vapply(manifest, function(value) paste(value, collapse = ","), character(1)),
  stringsAsFactors = FALSE
)
xlsx_path <- file.path(output_dir, "proxy_summary.xlsx")

if (requireNamespace("writexl", quietly = TRUE)) {
  writexl::write_xlsx(list(Scenes = scenes, Particles = particles, Manifest = manifest_table), xlsx_path)
} else if (requireNamespace("openxlsx", quietly = TRUE)) {
  workbook <- openxlsx::createWorkbook()
  for (sheet in c("Scenes", "Particles", "Manifest")) openxlsx::addWorksheet(workbook, sheet)
  openxlsx::writeData(workbook, "Scenes", scenes)
  openxlsx::writeData(workbook, "Particles", particles)
  openxlsx::writeData(workbook, "Manifest", manifest_table)
  openxlsx::saveWorkbook(workbook, xlsx_path, overwrite = TRUE)
} else {
  stop(
	"XLSX output requires an R package. Install one with: install.packages('writexl')",
	call. = FALSE
  )
}

cat("R visualization and XLSX postprocessing complete:\n")
cat("  ", normalizePath(file.path(output_dir, "proxy_summary.png")), "\n", sep = "")
cat("  ", normalizePath(file.path(output_dir, "proxy_summary.pdf")), "\n", sep = "")
cat("  ", normalizePath(xlsx_path), "\n", sep = "")
