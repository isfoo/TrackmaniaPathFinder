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

struct BestSolution {
    BestSolution() : solutionConnections(0) {}
    BestSolution(const std::vector<int16_t>& solution, const std::vector<int16_t>& sortedSolution, const std::vector<int16_t>& solutionWithRepeats, const FastSet2d& solutionConnections, int time) :
        solution(sortedSolution), solutionConnections(solutionConnections), time(time)
    {
        allVariations.push_back(solution);
        variations.push_back(sortedSolution);
        variationsWithRepeats.push_back(solutionWithRepeats);
    }
    std::vector<int16_t> solution;
    std::vector<std::vector<int16_t>> allVariations;
    std::vector<std::vector<int16_t>> variations;
    std::vector<std::vector<int16_t>> variationsWithRepeats;
    FastSet2d solutionConnections;
    int time;
};
struct SolutionConfig {
    std::vector<std::vector<int>> weights;
    std::vector<std::vector<std::vector<int>>> condWeights;
    int maxSolutionCount;
    int limit;
    int ignoredValue;
    bool useExtendedMatrix;
    ThreadSafeVec<BestSolution> solutionsVec;
    std::vector<BestSolution> bestSolutions;
    std::string appendFileName;
    std::string outputFileName;
    Vector3d<FastSmallVector<uint8_t>> repeatNodeMatrix;
    Vector3d<Bool> useRespawnMatrix;
    std::atomic<int64_t> partialSolutionCount;
    std::atomic<bool> stopWorking;
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
