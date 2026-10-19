#pragma once
#include <optional>
#include <string>
#include "plot_buffer.hpp"

// Loads a CSV with a header row. X = "pc_timestamp" column (or column 0),
// made relative to the first row. Other columns containing "timestamp" are skipped.
std::optional<DevicePlotBuffer> loadPlotFromCsv(const std::string& path,
                                                std::string& error);