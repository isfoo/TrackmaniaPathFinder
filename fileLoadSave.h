#pragma once
#include "utility.h"
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

void saveSolutionsToFile(const std::string& outputFileName, const std::vector<std::pair<std::vector<int16_t>, float>>& solutions) {
	std::ofstream outFile(outputFileName);
	if (solutions.empty())
		return;

	for (auto& [B, timeA] : solutions) {
		outFile << std::fixed << std::setprecision(1);
		outFile << std::setw(8) << timeA << " [";
		outFile << B[0] - 1;
		for (int i = 1; i < B.size(); ++i) {
			if (B[i] == B[i - 1] + 1) {
				outFile << '-';
				i += 1;
				while (i < B.size() && B[i] == B[i - 1] + 1) {
					i += 1;
				}
				i -= 1;
			} else {
				outFile << ',';
			}
			outFile << B[i] - 1;
		}
		outFile << "]\n";
	}
}