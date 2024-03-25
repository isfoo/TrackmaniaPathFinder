#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "utility.h"

struct ConditionalCost {
	int cost;
	uint8_t srcNode;

	ConditionalCost(int cost, uint8_t srcNode = std::numeric_limits<uint8_t>::max()) : cost(cost), srcNode(srcNode) {}

	bool isRemainingClause() {
		return srcNode == std::numeric_limits<uint8_t>::max();
	}
};

std::string createSolutionString(const std::vector<int16_t>& solution, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
	auto B = solution;
	B.insert(B.begin(), 0); // Explicit start node
	B.insert(B.begin(), int16_t(useRespawnMatrix.size() - 1)); // Added for convenience for useRespawnMatrix and repeatNodeMatrix

	std::string solStr = "[";
	solStr += "Start";
	for (int i = 2; i < B.size(); ++i) {
		if (useRespawnMatrix[B[i]][B[i - 1]][B[i - 2]]) {
			solStr += ",R";
		}
		if (!repeatNodeMatrix.empty() && !repeatNodeMatrix[B[i]][B[i - 1]][B[i - 2]].empty()) {
			auto& repeatNodes = repeatNodeMatrix[B[i]][B[i - 1]][B[i - 2]];
			solStr += ",(";
			solStr += std::to_string(repeatNodes[0]);
			for (int i = 1; i < repeatNodes.size(); ++i) {
				solStr += "," + std::to_string(repeatNodes[i]);
			}
			solStr += "),";
		} else if (B[i] == B[i - 1] + 1 && !useRespawnMatrix[B[i]][B[i - 1]][B[i - 2]]) {
			solStr += '-';
			i += 1;
			while (i < B.size() && B[i] == B[i - 1] + 1 && !useRespawnMatrix[B[i]][B[i - 1]][B[i - 2]]) {
				i += 1;
			}
			i -= 1;
		} else {
			solStr += ',';
		}
		if (i == B.size() - 1)
			solStr += "Finish";
		else
			solStr += std::to_string(B[i]);
	}
	solStr += "]";
	return solStr;
}
