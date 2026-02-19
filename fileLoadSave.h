#pragma once
#include "utility.h"
#include "common.h"
#include <fstream>
#include <vector>
#include <string>
#include <charconv>

namespace fs = std::filesystem;

std::vector<std::vector<ConditionalCost>> splitLineToConditionalCosts(std::string_view str, std::string& errorMsg) {
    constexpr auto PossiblyFloatChars = "0123456789.";
    std::vector<std::vector<ConditionalCost>> result;

    while (true) {
        auto pos = str.find_first_of(PossiblyFloatChars);
        if (pos == std::string::npos)
            return result;
        str = str.substr(pos);

        std::vector<ConditionalCost> condCostList;
        while (str.size() > 0 && (std::isdigit(uint8_t(str[0])) || str[0] == '.')) {
            auto costEnd = str.find_first_not_of(PossiblyFloatChars);
            auto cost = parseFloatAsInt(std::string(str.substr(0, costEnd)), 10);
            if (!cost) {
                errorMsg = "Failed to parse cost value at " + std::to_string(result.size() + 1) + " column ";
                return {};
            }
            auto condCost = ConditionalCost(*cost);
            str = str.substr(costEnd == std::string::npos ? str.size() : costEnd);
            if (!str.empty() && str[0] == '(') {
                size_t pos;
                do {
                    str = str.substr(1);
                    pos = str.find_first_of(",)");
                    if (pos == std::string::npos) {
                        errorMsg = "Missing closing bracket at " + std::to_string(result.size() + 1) + " column ";
                        return {};
                    }
                    auto node = parseInt(std::string(str.substr(0, pos)));
                    if (!node) {
                        errorMsg = "Failed to parse CP number in bracket at " + std::to_string(result.size() + 1) + " column ";
                        return {};
                    }
                    str = str.substr(pos);
                    condCost.srcNode = *node;
                    condCostList.push_back(condCost);
                } while (str[0] != ')');
                str = str.substr(1);
            } else {
                condCostList.push_back(condCost);
            }
        }
        result.push_back(condCostList);
    }
    return result;
}
std::vector<std::vector<int>> splitLineToConditionalCostsMatrix(std::string_view line, int ignoredValue, std::string& errorMsg) {
    auto condCosts = splitLineToConditionalCosts(line, errorMsg);
    if (!errorMsg.empty())
        return {};
    for (int i = 0; i < condCosts.size(); ++i) {
        for (int j = 0; j < condCosts[i].size(); ++j) {
            if (!condCosts[i][j].isRemainingClause() && condCosts[i][j].srcNode >= condCosts.size()) {
                errorMsg = "Conditional cost CP number is too big at " + std::to_string(i + 1) + " column ";
                return {};
            }
        }
    }
    std::vector<std::vector<int>> costs(condCosts.size() + 1, std::vector<int>(condCosts.size() + 1, ignoredValue));
    for (int i = 0; i < condCosts.size(); ++i) {
        for (int j = int(condCosts[i].size() - 1); j >= 0; --j) {
            if (condCosts[i][j].isRemainingClause()) {
                std::fill(costs[i].begin(), costs[i].end(), condCosts[i][j].cost);
            } else {
                costs[i][condCosts[i][j].srcNode] = condCosts[i][j].cost;
            }
        }
    }
    return costs;
}

struct InputAlgorithmData {
    std::vector<std::vector<int>> weights;
    std::vector<std::vector<std::vector<int>>> condWeights;
    std::vector<std::vector<std::vector<bool>>> isVerifiedConnection;
};

template<typename Func> auto parseNext(Func func, const char* chars, std::string& str) -> std::optional<decltype(func(""))> {
    auto start = str.find_first_of(chars);
    if (start == std::string::npos)
        return std::nullopt;
    str = str.substr(start);

    auto end = str.find_first_not_of(chars);
    if (end == std::string::npos)
        return std::nullopt;
    auto result = func(str.substr(0, end).c_str());

    str = str.substr(end);
    return result;
}
std::optional<int> parseNextInt(std::string& str) {
    return parseNext(std::atoi, "0123456789", str);
}
std::optional<double> parseNextDouble(std::string& str) {
    return parseNext(std::atof, "-0123456789.", str);
}
bool loadVerifiedConnection(InputAlgorithmData& data, std::string line) {
    std::array<int, 3> nodes; // prev, src, dst
    int i = 0;
    if (line[0] == 'X' || line[0] == 'x') {
        nodes[i++] = -1;
    }
    for (; i < nodes.size(); ++i) {
        auto res = parseNextInt(line);
        if (!res || *res < 0 || *res >= data.weights.size())
            return false;
        nodes[i] = *res;
    }
    bool isDelta = true;
    if (auto pos = line.find("Set"); pos != std::string::npos) {
        line = line.substr(pos + 3);
        isDelta = false;
    } else if (auto pos = line.find("set"); pos != std::string::npos) {
        line = line.substr(pos + 3);
        isDelta = false;
    }
    auto parsedTime = parseNextDouble(line);
    if (!parsedTime)
        return false;
    int time = std::round(*parsedTime * 10);

    auto updateWeights = [&data, isDelta, time](int dst, int src, int prev) {
        int newTime = 0;
        if (isDelta) {
            newTime = data.condWeights[dst][src][prev] += time;
        } else {
            newTime = data.condWeights[dst][src][prev] = time;
        }
        data.weights[dst][src] = std::min(data.weights[dst][src], newTime);
        data.isVerifiedConnection[dst][src][prev] = true;
    };
    if (nodes[0] == -1) {
        for (int i = 0; i < data.condWeights[nodes[2]][nodes[1]].size(); ++i) {
            updateWeights(nodes[2], nodes[1], i);
        }
    } else {
        updateWeights(nodes[2], nodes[1], nodes[0]);
    }

    return true;
}

InputAlgorithmData loadCsvData(const std::string& inputFileName, int ignoredValue, std::string& errorMsg) {
    std::ifstream inFile(inputFileName);
    if (!inFile) {
        errorMsg = "Couldn't open input file";
        return {};
    }
    InputAlgorithmData data;
    data.weights.emplace_back(); // first row to be filled later
    data.condWeights.emplace_back();
    std::string line;
    while (std::getline(inFile, line)) {
        auto firstNonSpacePos = line.find_first_not_of(' \t');
        if (firstNonSpacePos != std::string::npos && line[firstNonSpacePos] == '#')
            break;
        auto condCostMatrix = splitLineToConditionalCostsMatrix(line, ignoredValue, errorMsg);
        if (!errorMsg.empty()) {
            errorMsg += std::to_string(data.condWeights.size()) + " row";
            return {};
        }
        data.condWeights.push_back(condCostMatrix);
        std::vector<int> minimums(condCostMatrix.size());
        for (int i = 0; i < condCostMatrix.size(); ++i) {
            minimums[i] = *std::min_element(condCostMatrix[i].begin(), condCostMatrix[i].end());
        }
        data.weights.push_back(minimums);
    }
    if (data.weights.size() <= 1) {
        errorMsg = "Couldn't load data from file";
        return {};
    }
    data.condWeights[0].resize(data.condWeights[1].size(), std::vector<int>(data.condWeights[1].size(), ignoredValue));
    data.weights[0].resize(data.weights[1].size(), ignoredValue);
    if (data.weights[0].size() != data.weights.size()) {
        errorMsg = "Found " + std::to_string(data.weights.size() - 1) + " rows but there are " + std::to_string(data.weights[0].size() - 1) + " columns in the first row";
        return {};
    }
    for (int i = 0; i < data.weights.size(); ++i) {
        if (data.weights[i].size() != data.weights[0].size()) {
            errorMsg = "First row has " + std::to_string(data.weights[0].size() - 1) + " values, but " + std::to_string(i) + " row has " + std::to_string(data.weights[i].size() - 1);
            return {};
        }
    }

    data.isVerifiedConnection.resize(data.condWeights.size(), std::vector<std::vector<bool>>(data.condWeights[0].size(), std::vector<bool>(data.condWeights[0][0].size(), false)));
    int verifiedLineNr = 1;
    while (std::getline(inFile, line)) {
        auto firstRealChar = line.find_first_of("#Xx-0123456789.");
        if (firstRealChar == std::string::npos || line[firstRealChar] == '#')
            continue;
        line = line.substr(firstRealChar);
        if (!loadVerifiedConnection(data, line)) {
            errorMsg = "Failed to parse line " + std::to_string(verifiedLineNr) + " in verified connections list";
            return {};
        }
        verifiedLineNr += 1;
    }
    return data;
}

void writeSolutionToFile(std::ofstream& solutionsFile, const std::vector<int16_t>& solution, int time, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
    solutionsFile << std::fixed << std::setprecision(1);
    solutionsFile << std::setw(8) << time / 10.0 << " ";
    solutionsFile << createSolutionString(solution, repeatNodeMatrix, useRespawnMatrix) << '\n';
}
void writeSolutionToFile(const std::string& outputFileName, const std::vector<int16_t>& solution, int time, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
    if (outputFileName.empty())
        return;
    std::ofstream solutionsFile(outputFileName, std::ios::app);
    writeSolutionToFile(solutionsFile, solution, time, repeatNodeMatrix, useRespawnMatrix);
}

void overwriteFileWithSortedSolutions(const std::string& outputFileName, int maxSolutionCount, const ThreadSafeVec<BestSolution>& solutionsView, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
    if (outputFileName.empty())
        return;
    std::vector<BestSolution> sortedSolutions;
    for (int i = 0; i < solutionsView.size(); ++i) {
        sortedSolutions.push_back(solutionsView[i]);
    }
    std::sort(sortedSolutions.begin(), sortedSolutions.end(), [](auto& a, auto& b) { return a.time < b.time; });
    std::ofstream solutionsFile(outputFileName, std::ios::trunc);
    for (int i = 0; i < maxSolutionCount && i < sortedSolutions.size(); ++i) {
        writeSolutionToFile(solutionsFile, sortedSolutions[i].solution, sortedSolutions[i].time, repeatNodeMatrix, useRespawnMatrix);
    }
}

void clearFile(const std::string& outputFileName) {
    if (outputFileName.empty())
        return;
    std::ofstream solutionsFile(outputFileName, std::ios::trunc);
}

std::vector<Position> readPositionsFile(const std::string& positionsFilePath) {
    std::ifstream positionsFile(positionsFilePath);
    std::vector<Position> cpPositions;
    Position pos;
    while (positionsFile >> pos.x >> pos.y >> pos.z) {
        cpPositions.push_back(pos);
    }
    return cpPositions;
}

std::optional<fs::path> getLocalAppDataProgramDirectory() {
    PWSTR windowsPath;
    auto result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &windowsPath);
    if (result != S_OK) {
        CoTaskMemFree(windowsPath);
        return std::nullopt;
    }
    fs::path path = windowsPath;
    CoTaskMemFree(windowsPath);
    path /= "TrackmaniaPathFinder";
    fs::create_directory(path);
    return path;
}

std::pair<std::string, std::string> getSpreadsheetIdAndGidFromLink(const std::string& link) {
    std::string strBeforeSpreadsheetId = "docs.google.com/spreadsheets/d/";
    auto ind = link.find(strBeforeSpreadsheetId);
    if (ind == std::string::npos)
        return {};
    auto linkStr = link.substr(ind + strBeforeSpreadsheetId.size());

    auto spreadSheetIdEndIndex = linkStr.find_first_of('/');
    if (spreadSheetIdEndIndex == std::string::npos)
        return {};
    auto spreadsheetId = linkStr.substr(0, spreadSheetIdEndIndex);
    linkStr = linkStr.substr(spreadSheetIdEndIndex);

    ind = linkStr.find("gid=");
    if (ind == std::string::npos)
        return {};
    linkStr = linkStr.substr(ind + 4);
    auto gid = linkStr.substr(0, linkStr.find_first_not_of("0123456789"));

    return { spreadsheetId, gid };
}
bool downloadGoogleSpreadsheet(const std::string& spreadsheetId, const std::string& gid, const std::string& outputFilePath) {
    httplib::Client cli("https://docs.google.com");
    cli.set_follow_location(true);
    if (auto res = cli.Get("/spreadsheets/d/" + spreadsheetId + "/export?gid=" + gid + "&format=csv&range=B2:ZZ300")) {
        if (res->status == httplib::OK_200) {
            std::ofstream outFile(outputFilePath);
            outFile << res->body;
            return true;
        }
    }
    return false;
}
