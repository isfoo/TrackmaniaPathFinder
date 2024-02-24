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
		errorMsg = "Couldn't load data from file\n";
		return {};
	}
	A[0].resize(A[1].size(), ignoredValue);
	return A;
}

void writeSolutionFileProlog(const std::string& outputFileName, const std::string& inputFileName, float limit, bool isExactAlgorithm, bool allowRepeatNodes, const std::vector<int>& repeatNodesTurnedOff) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	solutionsFile << "\nSTART\n";
	solutionsFile << "Input file:              \"" << inputFileName << "\"\n";
	solutionsFile << "Algorithm:               " << (isExactAlgorithm ? "Exact (BnB Assignment Relaxation)" : "Heuristic (LKH)") << "\n";
	solutionsFile << "Limit:                   " << limit << "\n";
	solutionsFile << "Allow repeat nodes:      " << (allowRepeatNodes ? "Yes" : "No") << "\n";
	solutionsFile << "Repeat nodes turned off: [";
	if (!repeatNodesTurnedOff.empty()) {
		solutionsFile << repeatNodesTurnedOff[0];
		for (int i = 1; i < repeatNodesTurnedOff.size(); ++i) {
			solutionsFile << ", " << repeatNodesTurnedOff[i];
		}
	}
	solutionsFile << "]\n";

}

void writeSolutionFileEpilog(const std::string& outputFileName) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	solutionsFile << "END\n";
}

void writeSolutionToFile(const std::string& outputFileName, const std::vector<int16_t>& solution, float time, const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix) {
	std::ofstream solutionsFile(outputFileName, std::ios::app);
	solutionsFile << std::fixed << std::setprecision(1);
	solutionsFile << std::setw(8) << time << " ";
	solutionsFile << createSolutionString(solution, repeatNodeMatrix) << '\n';
}
