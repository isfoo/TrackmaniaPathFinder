#pragma once
#include "utility.h"
#include "common.h"
#include <fstream>
#include <vector>
#include <string>
#include <charconv>

std::vector<std::vector<ConditionalCost>> splitLineToConditionalCosts(std::string_view str, std::string& errorMsg) {
	constexpr auto PossiblyFloatChars = "0123456789.";
	std::vector<std::vector<ConditionalCost>> result;

	while (true) {
		auto pos = str.find_first_of(PossiblyFloatChars);
		if (pos == std::string::npos)
			return result;
		str = str.substr(pos);

		std::vector<ConditionalCost> condCostList;
		while (str.size() > 0 && (std::isdigit(str[0]) || str[0] == '.')) {
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


std::pair<std::vector<std::vector<int>>, std::vector<std::vector<std::vector<int>>>> loadCsvData(const std::string& inputFileName, int ignoredValue, std::string& errorMsg) {
	std::ifstream inFile(inputFileName);
	if (!inFile) {
		errorMsg = "Couldn't open input file";
		return {};
	}
	std::vector<std::vector<int>> A;
	std::vector<std::vector<std::vector<int>>> B;
	A.emplace_back(); // first row to be filled later
	B.emplace_back();
	std::string line;
	while (std::getline(inFile, line)) {
		auto condCostMatrix = splitLineToConditionalCostsMatrix(line, ignoredValue, errorMsg);
		if (!errorMsg.empty()) {
			errorMsg += std::to_string(B.size()) + " row";
			return {};
		}
		B.push_back(condCostMatrix);
		std::vector<int> minimums(condCostMatrix.size());
		for (int i = 0; i < condCostMatrix.size(); ++i) {
			minimums[i] = *std::min_element(condCostMatrix[i].begin(), condCostMatrix[i].end());
		}
		A.push_back(minimums);
	}
	if (A.size() <= 1) {
		errorMsg = "Couldn't load data from file";
		return {};
	}
	B[0].resize(B[1].size(), std::vector<int>(B[1].size(), ignoredValue));
	A[0].resize(A[1].size(), ignoredValue);
	if (A[0].size() != A.size()) {
		errorMsg = "Found " + std::to_string(A.size() - 1) + " rows but there are " + std::to_string(A[0].size() - 1) + " columns in the first row";
		return {};
	}
	for (int i = 0; i < A.size(); ++i) {
		if (A[i].size() != A[0].size()) {
			errorMsg = "First row has " + std::to_string(A[0].size() - 1) + " values, but " + std::to_string(i) + " row has " + std::to_string(A[i].size() - 1);
			return {};
		}
	}
	return { A, B };
}

void writeSolutionFileProlog(const std::string& outputFileName, const std::string& inputFileName, int limit, bool isExactAlgorithm, bool allowRepeatNodes, const std::vector<int>& repeatNodesTurnedOff) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	solutionsFile << "\nSTART\n";
	solutionsFile << "Input file:            \"" << inputFileName << "\"\n";
	solutionsFile << "Algorithm:             " << (isExactAlgorithm ? "Exact (BnB Assignment Relaxation)" : "Heuristic (LKH)") << "\n";
	solutionsFile << "Max route time:        " << limit / 10.0 << "\n";
	solutionsFile << "allow repeat CPs:      " << (allowRepeatNodes ? "Yes" : "No") << "\n";
	solutionsFile << "turned off repeat CPs: [";
	if (!repeatNodesTurnedOff.empty()) {
		solutionsFile << repeatNodesTurnedOff[0];
		for (int i = 1; i < repeatNodesTurnedOff.size(); ++i) {
			solutionsFile << ", " << repeatNodesTurnedOff[i];
		}
	}
	solutionsFile << "]\n";

}

void writeSolutionFileEpilog(const std::string& outputFileName, bool taskWasCanceled, bool endedWithTimeout) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	solutionsFile << "END " << (endedWithTimeout ? "(Timeout)" : (taskWasCanceled ? "(Canceled)" : "(Completed)")) << "\n";
}

void writeSolutionToFile(std::ofstream& solutionsFile, const std::vector<int16_t>& solution, int time, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
	solutionsFile << std::fixed << std::setprecision(1);
	solutionsFile << std::setw(8) << time / 10.0 << " ";
	solutionsFile << createSolutionString(solution, repeatNodeMatrix, useRespawnMatrix) << '\n';
}
void writeSolutionToFile(const std::string& outputFileName, const std::vector<int16_t>& solution, int time, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
	if (!outputFileName.empty()) {
		std::ofstream solutionsFile(outputFileName, std::ios::app);
		writeSolutionToFile(solutionsFile, solution, time, repeatNodeMatrix, useRespawnMatrix);
	}
}

void overwriteFileWithSortedSolutions(const std::string& outputFileName, int maxSolutionCount, const ThreadSafeVec<std::pair<std::vector<int16_t>, int>>& solutionsView, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
	std::vector<std::pair<std::vector<int16_t>, int>> sortedSolutions;
	for (int i = 0; i < solutionsView.size(); ++i) {
		sortedSolutions.push_back(solutionsView[i]);
	}
	std::sort(sortedSolutions.begin(), sortedSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
	std::ofstream solutionsFile(outputFileName, std::ios::trunc);
	for (int i = 0; i < maxSolutionCount && i < sortedSolutions.size(); ++i) {
		auto& [solution, time] = sortedSolutions[i];
		writeSolutionToFile(solutionsFile, solution, time, repeatNodeMatrix, useRespawnMatrix);
	}
}

void clearFile(const std::string& outputFileName) {
	std::ofstream solutionsFile(outputFileName, std::ios::trunc);
}