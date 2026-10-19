#include "csv_loader.hpp"
#include "logger.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

static std::string trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> out;
    std::string field;
    std::istringstream ss(line);
    while (std::getline(ss, field, ','))
        out.push_back(trim(field));
    if (!line.empty() && line.back() == ',')
        out.emplace_back();
    return out;
}

static bool parseDouble(const std::string& s, double& out)
{
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end != s.c_str();
}

std::optional<DevicePlotBuffer> loadPlotFromCsv(const std::string& path,
                                                std::string& error)
{
    std::ifstream file(path);
    if (!file) {
        error = "Cannot open: " + path;
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(file, line)) {
        error = "File is empty";
        return std::nullopt;
    }
    const auto header = splitCsvLine(line);

    int timeCol = 0;
    for (int i = 0; i < (int)header.size(); ++i)
        if (header[i] == "pc_timestamp") { timeCol = i; break; }

    std::vector<int> dataCols;
    for (int i = 0; i < (int)header.size(); ++i)
        if (i != timeCol && header[i].find("timestamp") == std::string::npos)
            dataCols.push_back(i);

    const std::string name = std::filesystem::path(path).filename().string();
    DevicePlotBuffer buf("csv:" + name, "CSV");
    buf.setStatic(true);
    buf.requestFit();

    double t0 = 0.0;
    bool   first = true;
    size_t rows = 0, skipped = 0;

    while (std::getline(file, line)) {
        if (trim(line).empty()) continue;
        const auto fields = splitCsvLine(line);
        double t;
        if (fields.size() < header.size() || !parseDouble(fields[timeCol], t)) {
            ++skipped;
            continue;
        }
        if (first) { t0 = t; first = false; }

        for (int c : dataCols) {
            double v;
            if (parseDouble(fields[c], v))
                buf.pushSample(header[c], t - t0, static_cast<float>(v));
        }
        ++rows;
    }

    if (rows == 0) {
        error = "No numeric data rows in " + name;
        return std::nullopt;
    }

    LOG_INFO("Loaded {} rows from {} ({} skipped)", rows, name, skipped);
    return buf;
}