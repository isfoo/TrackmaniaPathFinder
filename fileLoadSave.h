#pragma once
#include "utility.h"
#include "common.h"
#include <fstream>
#include <vector>
#include <string>
#include <charconv>

std::vector<std::vector<float>> loadCsvData(const std::string& inputFileName, float ignoredValue, std::string& errorMsg) {
	std::ifstream inFile(inputFileName);
	if (!inFile) {
		errorMsg = "Couldn't open input file";
		return {};
	}
	std::vector<std::vector<float>> A;
	A.emplace_back(); // first row to be filled later
	std::string line;
	while (std::getline(inFile, line)) {
		A.push_back(splitToFloats(line, ignoredValue));
		A.back().push_back(ignoredValue);
	}
	if (A.size() <= 1) {
		errorMsg = "Couldn't load data from file";
		return {};
	}
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
	return A;
}

void writeSolutionFileProlog(const std::string& outputFileName, const std::string& inputFileName, float limit, bool isExactAlgorithm, bool allowRepeatNodes, const std::vector<int>& repeatNodesTurnedOff) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	solutionsFile << "\nSTART\n";
	solutionsFile << "Input file:            \"" << inputFileName << "\"\n";
	solutionsFile << "Algorithm:             " << (isExactAlgorithm ? "Exact (BnB Assignment Relaxation)" : "Heuristic (LKH)") << "\n";
	solutionsFile << "Max route time:        " << limit << "\n";
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

void writeSolutionFileEpilog(const std::string& outputFileName, std::atomic<bool>& taskWasCanceled) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	solutionsFile << "END " << (taskWasCanceled ? "(Canceled)" : "(Completed)") << "\n";
}

void writeSolutionToFile(std::ofstream& solutionsFile, const std::vector<int16_t>& solution, float time, const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix) {
	solutionsFile << std::fixed << std::setprecision(1);
	solutionsFile << std::setw(8) << time << " ";
	solutionsFile << createSolutionString(solution, repeatNodeMatrix) << '\n';
}
void writeSolutionToFile(const std::string& outputFileName, const std::vector<int16_t>& solution, float time, const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	writeSolutionToFile(solutionsFile, solution, time, repeatNodeMatrix);
}

void overwriteFileWithSortedSolutions(const std::string& outputFileName, int maxSolutionCount, const ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& solutionsView, const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix) {
	std::vector<std::pair<std::vector<int16_t>, float>> sortedSolutions;
	for (int i = 0; i < solutionsView.size(); ++i) {
		sortedSolutions.push_back(solutionsView[i]);
	}
	std::sort(sortedSolutions.begin(), sortedSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
	std::ofstream solutionsFile(outputFileName, std::ios::trunc);
	for (int i = 0; i < maxSolutionCount && i < sortedSolutions.size(); ++i) {
		auto& [solution, time] = sortedSolutions[i];
		writeSolutionToFile(solutionsFile, solution, time, repeatNodeMatrix);
	}
}

void clearFile(const std::string& outputFileName) {
	std::ofstream solutionsFile(outputFileName, std::ios::trunc);
}