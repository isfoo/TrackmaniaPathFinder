#pragma once
#include <vector>
#include <string>
#include <cstdint>

std::string createSolutionString(const std::vector<int16_t>& solution, const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix, bool useLegacyOutputFormat) {
	auto& B_ = solution;
	auto B = solution;
	if (!useLegacyOutputFormat) {
		B.insert(B.begin(), 0);
		for (auto& b : B)
			b += 1;
	}

	std::string solStr = "[";
	if (!useLegacyOutputFormat) {
		solStr += "Start";
	} else {
		solStr += std::to_string(B[0] - 1);
	}
	for (int i = 1; i < B.size(); ++i) {
		if (useLegacyOutputFormat && !repeatNodeMatrix.empty() && !repeatNodeMatrix[B[i]][B[i - 1]].empty()) {
			auto& repeatNodes = repeatNodeMatrix[B_[i]][B_[i - 1]];
			solStr += ",(";
			solStr += std::to_string(repeatNodes[0] - 1);
			for (int i = 1; i < repeatNodes.size(); ++i) {
				solStr += "," + std::to_string(repeatNodes[i] - 1);
			}
			solStr += "),";
		} else if (!useLegacyOutputFormat && i > 1 && !repeatNodeMatrix.empty() && !repeatNodeMatrix[B_[i - 1]][B_[i - 2]].empty()) {
			auto& repeatNodes = repeatNodeMatrix[B_[i - 1]][B_[i - 2]];
			solStr += ",(";
			solStr += std::to_string(repeatNodes[0]);
			for (int i = 1; i < repeatNodes.size(); ++i) {
				solStr += "," + std::to_string(repeatNodes[i]);
			}
			solStr += "),";
		} else if (B[i] == B[i - 1] + 1) {
			solStr += '-';
			i += 1;
			while (i < B.size() && B[i] == B[i - 1] + 1) {
				i += 1;
			}
			i -= 1;
		} else {
			solStr += ',';
		}
		if (i == B.size() - 1 && !useLegacyOutputFormat)
			solStr += "Finish";
		else
			solStr += std::to_string(B[i] - 1);
	}
	solStr += "]";
	return solStr;
}
