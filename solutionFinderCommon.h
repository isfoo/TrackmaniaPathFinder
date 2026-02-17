#pragma once
#include "utility.h"
#include "fileLoadSave.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <future>
#include <mutex>
#include <cstdint>
#include <string>
#include <optional>
#include <string_view>
#include <iomanip>
#include <charconv>
#include <array>
#include <queue>
#include <stack>
#include <algorithm>
#include <filesystem>
#include <numeric>

void findSolutionsAssignment(SolutionConfig& config);
void findSolutionsLinKernighan(SolutionConfig& config);

enum Direction { Out, In };
constexpr NodeType NullNode = NodeType(-1);
constexpr Edge NullEdge = { NullNode, NullNode };
constexpr EdgeCostType Inf = 100'000'000;

std::vector<int16_t> solutionWithExplicitRepeats(const std::vector<int16_t>& solution, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix) {
    auto B = solution;
    B.insert(B.begin(), 0);
    B.insert(B.begin(), int16_t(repeatNodeMatrix.size() - 1));
    std::vector<int16_t> solutionWithRepeats;
    for (int i = 2; i < B.size(); ++i) {
        if (!repeatNodeMatrix.empty() && !repeatNodeMatrix[B[i]][B[i - 1]][B[i - 2]].empty()) {
            auto& repeatNodes = repeatNodeMatrix[B[i]][B[i - 1]][B[i - 2]];
            for (int i = 0; i < repeatNodes.size(); ++i) {
                solutionWithRepeats.push_back(repeatNodes[i]);
            }
        }
        solutionWithRepeats.push_back(B[i]);
    }
    return solutionWithRepeats;
}
std::vector<int16_t> solutionWithRepeatsAtEnd(const std::vector<int16_t>& solution, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix) {
    std::vector<int16_t> result;
    auto allCps = solutionWithExplicitRepeats(solution, repeatNodeMatrix);
    for (auto cp : allCps) {
        if (std::find(result.begin(), result.end(), cp) == result.end()) {
            result.push_back(cp);
        }
    }
    return result;
}
FastSet2d solutionConnectionsSet(const std::vector<int16_t>& solutionWithRepeats, int nodeCount) {
    FastSet2d result(nodeCount);
    auto B = solutionWithRepeats;
    B.insert(B.begin(), 0);
    for (int i = 2; i < B.size(); ++i) {
        result.set(B[i], B[i - 1]);
    }
    return result;
}
template<typename T, typename Pred> void insertSorted(std::vector<T>& vec, const T& val, Pred pred) {
    vec.insert(std::upper_bound(vec.begin(), vec.end(), val, pred), val);
}
std::vector<int16_t> getSortedSolutionIfPossible(const SolutionConfig& config, const std::vector<int16_t>& solution) {
    auto solutionWithSortedRepeats = solutionWithRepeatsAtEnd(solution, config.repeatNodeMatrix);
    auto a = solutionWithExplicitRepeats(solutionWithSortedRepeats, config.repeatNodeMatrix);
    auto b = solutionWithExplicitRepeats(solution, config.repeatNodeMatrix);
    if (a == b)
        return solutionWithSortedRepeats;
    return solution;
}
void saveSolutionAndUpdateLimit(SolutionConfig& config, std::pair<std::vector<int16_t>, int> solution) {
    if (solution.second > config.limit || (config.bestSolutions.size() >= config.maxSolutionCount && solution.second >= config.limit))
        return;

    auto solutionWithRepeats = solutionWithExplicitRepeats(solution.first, config.repeatNodeMatrix);
    auto solutionConnections = solutionConnectionsSet(solutionWithRepeats, config.repeatNodeMatrix.size());
    for (int i = 0; i < config.bestSolutions.size(); ++i) {
        if (config.bestSolutions[i].time == solution.second) {
            if (solutionConnections == config.bestSolutions[i].solutionConnections) {
                for (auto& variation : config.bestSolutions[i].allVariations) {
                    if (solution.first == variation) {
                        return;
                    }
                }
                config.bestSolutions[i].allVariations.push_back(solution.first);
                for (auto& variationWithRepeats : config.bestSolutions[i].variationsWithRepeats) {
                    if (solutionWithRepeats == variationWithRepeats) {
                        return;
                    }
                }
                config.bestSolutions[i].variations.push_back(getSortedSolutionIfPossible(config, solution.first));
                config.bestSolutions[i].variationsWithRepeats.push_back(solutionWithExplicitRepeats(solution.first, config.repeatNodeMatrix));
                return;
            }
        }
    }
    auto sortedSolution = getSortedSolutionIfPossible(config, solution.first);
    auto solutionString = createSolutionString(solution.first, config.repeatNodeMatrix, config.useRespawnMatrix);

    auto newSolution = BestSolution(solution.first, sortedSolution, solutionWithRepeats, solutionConnections, solutionString, config.addedConnection, solution.second);
    config.solutionsVec.push_back_not_thread_safe(newSolution);
    insertSorted(config.bestSolutions, newSolution, [](auto& a, auto& b) {
        if (a.time < b.time)
            return true;
        if (a.time > b.time)
            return false;
        return a.solution < b.solution;
    });
    if (config.bestSolutions.size() > config.maxSolutionCount) {
        config.limit = config.bestSolutions.back().time;
        config.bestSolutions.pop_back();
    }
}
void saveSolution(SolutionConfig& config, const std::vector<NodeType>& solution, std::mutex& solutionMutex) {
    std::vector<int16_t> solutionVec = { 0 };
    for (int i = 0; i < solution.size() - 1; ++i) {
        solutionVec.push_back(solution[solutionVec.back()]);
    }
    int realCost = 0;
    for (int i = 1; i < solutionVec.size(); ++i) {
        auto connectionCost = config.condWeights[solutionVec[i]][solutionVec[i - 1]][i > 1 ? solutionVec[i - 2] : 0];
        if (connectionCost >= config.ignoredValue)
            return;
        realCost += connectionCost;
    }
    solutionVec.erase(solutionVec.begin());
    std::scoped_lock l{ solutionMutex };
    saveSolutionAndUpdateLimit(config, std::make_pair(solutionVec, realCost));
}

struct RepeatEdgePath {
    RepeatEdgePath(int k, int j, int i) : k(k), j(j), i(i) {}
    int k, j, i;
    int time(const std::vector<std::vector<int>>& A) const {
        return A[k][j] + A[j][i];
    }
};
std::vector<RepeatEdgePath> getRepeatNodeEdges(const std::vector<std::vector<int>>& A, int ignoredValue, std::vector<int> turnedOffRepeatNodes) {
    std::vector<std::vector<int>> adjList(A.size());
    for (int i = 0; i < A.size(); ++i) {
        for (int j = 0; j < A[i].size(); ++j) {
            if (A[j][i] < ignoredValue) {
                adjList[i].push_back(j);
            }
        }
    }

    std::vector<RepeatEdgePath> additionalPaths;
    for (int i = 0; i < adjList.size(); ++i) {
        for (int j : adjList[i]) {
            for (int k : adjList[j]) {
                if (i == k)
                    continue;
                auto time = A[k][j] + A[j][i];
                if (time < A[k][i] && time < ignoredValue) {
                    if (std::find(turnedOffRepeatNodes.begin(), turnedOffRepeatNodes.end(), j) == turnedOffRepeatNodes.end())
                        additionalPaths.emplace_back(k, j, i);
                }
            }
        }
    }
    std::sort(additionalPaths.begin(), additionalPaths.end(), [&](auto& a, auto& b) { return a.time(A) < b.time(A); });
    return additionalPaths;
}

int addRepeatNodeEdges(std::vector<std::vector<int>>& A, std::vector<std::vector<std::vector<int>>>& B, Vector3d<FastSmallVector<uint8_t>>& repeatEdgeMatrix, const std::vector<RepeatEdgePath>& additionalPaths, int maxEdgesToAdd) {
    int addedEdgesCount = 0;
    for (int m = 0; m < additionalPaths.size() && addedEdgesCount < maxEdgesToAdd; ++m) {
        auto k = additionalPaths[m].k;
        auto j = additionalPaths[m].j;
        auto i = additionalPaths[m].i;
        addedEdgesCount += repeatEdgeMatrix[k][i].back().empty();
        for (int z = 0; z < B.size(); ++z) {
            auto newTime = B[k][j][i] + B[j][i][z];
            if (newTime < B[k][i][z]) {
                auto combined = FastSmallVector<uint8_t>::Combine(repeatEdgeMatrix[j][i][z], j, repeatEdgeMatrix[k][j][i]);
                if (combined) {
                    repeatEdgeMatrix[k][i][z] = *combined;
                    B[k][i][z] = newTime;
                    if (newTime < A[k][i]) {
                        A[k][i] = newTime;
                    }
                }
            }
        }
    }
    return addedEdgesCount;
}

Vector3d<FastSmallVector<uint8_t>> addRepeatNodeEdges(std::vector<std::vector<int>>& A, std::vector<std::vector<std::vector<int>>>& B, int ignoredValue, int maxEdgesToAdd, std::vector<int> turnedOffRepeatNodes) {
    auto repeatEdgeMatrix = Vector3d<FastSmallVector<uint8_t>>(int(B.size()));
    if (maxEdgesToAdd <= 0)
        return repeatEdgeMatrix;
    for (int i = 0; i < 2; ++i) {
        auto repeatEdges = getRepeatNodeEdges(A, ignoredValue, turnedOffRepeatNodes);
        auto edgesAdded = addRepeatNodeEdges(A, B, repeatEdgeMatrix, repeatEdges, maxEdgesToAdd);
        maxEdgesToAdd = std::max<int>(0, maxEdgesToAdd - edgesAdded);
    }
    return repeatEdgeMatrix;
}
void addRingCps(SolutionConfig& config, const std::vector<int>& ringCps) {
    for (auto ringCp : ringCps) {
        if (ringCp >= config.condWeights.size())
            continue;
        for (int i = 0; i < config.condWeights.size(); ++i) {
            if (std::find(ringCps.begin(), ringCps.end(), i) != ringCps.end())
                continue;
            for (int j = 0; j < config.condWeights.size(); ++j) {
                if (j == ringCp)
                    continue;
                if (config.weights[ringCp][i] < config.ignoredValue && config.condWeights[j][i].back() < config.condWeights[j][ringCp][i]) {
                    // faster to respawn from ringCp to i then go from i to j, then to directly go from ring to j
                    config.condWeights[j][ringCp][i] = config.condWeights[j][i].back();
                    config.useRespawnMatrix[j][ringCp][i] = true;
                    if (!config.repeatNodeMatrix.empty()) {
                        config.repeatNodeMatrix[j][ringCp][i] = config.repeatNodeMatrix[j][i].back();
                    }
                    config.weights[j][ringCp] = std::min(config.weights[j][ringCp], config.condWeights[j][i].back());
                }
            }
        }
    }
}

std::vector<std::vector<int>> createAtspMatrixFromInput(const std::vector<std::vector<int>>& weights) {
    auto copy = weights;
    copy[0].back() = 0;
    return copy;
}

bool isUsingExtendedMatrix(std::vector<std::vector<std::vector<int>>> B) {
    bool useExtendedMatrix = false;
    for (int i = 0; i < B.size(); ++i) {
        for (int j = 0; j < B.size(); ++j) {
            auto val = B[i][j][0];
            for (int k = 0; k < B.size(); ++k) {
                if (B[i][j][k] != val) {
                    return true;
                }
            }
        }
    }
    return false;
}


void runAlgorithm(Algorithm algorithm, SolutionConfig& config, const InputData& input, State& state) {
    state.taskWasCanceled = false;
    state.endedWithTimeout = false;

    config.limit = input.limitValue * 10;
    config.ignoredValue = input.ignoredValue * 10;
    config.maxSolutionCount = input.maxSolutionCount;
    config.outputFileName = input.outputDataFile;
    config.partialSolutionCount = 0;
    config.stopWorking = false;
    config.repeatNodeMatrix.clear();
    config.solutionsVec.clear();
    config.addedConnection = NullEdge;

    auto [A_, B_] = loadCsvData(input.inputDataFile, config.ignoredValue, state.errorMsg);
    if (!state.errorMsg.empty())
        return;
    config.weights = A_;
    config.condWeights = B_;
    config.useRespawnMatrix = Vector3d<Bool>(int(config.condWeights.size()));

    state.cpPositionsVis.clear();
    if (input.positionReplayFilePath[0] != '\0') {
        state.cpPositionsVis = readPositionsFile(input.positionReplayFilePath);
        if (state.cpPositionsVis.size() != config.weights.size()) {
            state.errorMsg = "incorrect CP position file - wrong number of CPs";
            return;
        }
    }

    auto ringCps = parseIntList(input.ringCps, 1, config.weights.size() - 2, "ring CPs list", state.errorMsg);
    auto repeatNodesTurnedOff = parseIntList(input.turnedOffRepeatNodes, 0, config.weights.size() - 2, "turned off repeat nodes", state.errorMsg);
    auto searchSourceNodes = parseIntList(input.searchSourceNodes, 0, config.weights.size() - 2, "search source CPs list", state.errorMsg);
    if (!state.errorMsg.empty())
        return;

    state.timer = Timer();
    state.timerThread = std::thread([&state, maxTime=input.maxTime, &config]() {
        while (!config.stopWorking && !state.taskWasCanceled && state.timer.getTime() < maxTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        state.endedWithTimeout = state.timer.getTime() >= maxTime;
        config.stopWorking = true;
    });
    if (input.isConnectionSearchAlgorithm) {
        auto connectionFinderSettings = input.connectionFinderSettings;
        connectionFinderSettings.minConnectionTime *= 10;
        connectionFinderSettings.maxConnectionTime *= 10;
        connectionFinderSettings.testedConnectionTime *= 10;
        config.useExtendedMatrix = isUsingExtendedMatrix(config.condWeights);
        config.bestSolutions.clear();
        state.connectionsToTest.clear();
        state.algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [algorithm, &input, &state, &config, ringCps, repeatNodesTurnedOff, searchSourceNodes, connectionFinderSettings]() mutable {
            auto privateConfig = config;
            privateConfig.maxSolutionCount = 1;

            for (int src = 0; src < config.weights.size() - 1; ++src) {
                if (!searchSourceNodes.empty() && std::find(searchSourceNodes.begin(), searchSourceNodes.end(), src) == searchSourceNodes.end())
                    continue;
                for (int dst = 1; dst < config.weights.size(); ++dst) {
                    if (src == dst)
                        continue;

                    if (config.weights[dst][src] > connectionFinderSettings.maxConnectionTime)
                        continue;
                    if (config.weights[dst][src] < connectionFinderSettings.minConnectionTime)
                        continue;

                    if (!state.cpPositionsVis.empty()) {
                        auto srcPos = state.cpPositionsVis[src];
                        auto dstPos = state.cpPositionsVis[dst];
                        if (dstPos.y - srcPos.y > connectionFinderSettings.maxHeightDiff)
                            continue;
                        if (dstPos.y - srcPos.y < connectionFinderSettings.minHeightDiff)
                            continue;
                        if (dist3d(srcPos, dstPos) > connectionFinderSettings.maxDistance)
                            continue;
                        if (dist3d(srcPos, dstPos) < connectionFinderSettings.minDistance)
                            continue;
                    }
                    state.connectionsToTest.emplace_back(src, dst);
                }
            }
            for (auto [src, dst] : state.connectionsToTest) {
                if (config.stopWorking)
                    break;

                privateConfig.addedConnection = Edge{ src, dst };
                privateConfig.weights[dst][src] = connectionFinderSettings.testedConnectionTime;
                privateConfig.bestSolutions.clear();
                privateConfig.solutionsVec.clear();
                privateConfig.limit = config.limit;
                std::fill(privateConfig.condWeights[dst][src].begin(), privateConfig.condWeights[dst][src].end(), privateConfig.weights[dst][src]);
                privateConfig.repeatNodeMatrix = addRepeatNodeEdges(privateConfig.weights, privateConfig.condWeights, privateConfig.ignoredValue, input.maxRepeatNodesToAdd, repeatNodesTurnedOff);
                addRingCps(config, ringCps);
                privateConfig.weights = createAtspMatrixFromInput(privateConfig.weights);
                std::fill(privateConfig.condWeights[0].back().begin(), privateConfig.condWeights[0].back().end(), 0);
                if (algorithm == Algorithm::Assignment) {
                    findSolutionsAssignment(privateConfig);
                } else {
                    findSolutionsLinKernighan(privateConfig);
                }

                if (!privateConfig.bestSolutions.empty()) {
                    auto& newSolution = privateConfig.bestSolutions[0];
                    config.solutionsVec.push_back_not_thread_safe(newSolution);
                    insertSorted(config.bestSolutions, newSolution, [](auto& a, auto& b) {
                        if (a.time < b.time)
                            return true;
                        if (a.time > b.time)
                            return false;
                        return a.solution < b.solution;
                    });
                    if (config.bestSolutions.size() >= config.maxSolutionCount) {
                        config.limit = config.bestSolutions.back().time;
                        config.bestSolutions.pop_back();
                    }
                }

                privateConfig.weights = config.weights;
                privateConfig.condWeights = config.condWeights;
                config.partialSolutionCount += 1;
            }
            config.stopWorking = true;
            state.timerThread.join();
            state.timer.stop();
        });
    } else {
        config.repeatNodeMatrix = addRepeatNodeEdges(config.weights, config.condWeights, config.ignoredValue, input.maxRepeatNodesToAdd, repeatNodesTurnedOff);
        addRingCps(config, ringCps);
        clearFile(config.outputFileName);
        state.algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [algorithm, &state, &config]() mutable {
            config.weights = createAtspMatrixFromInput(config.weights);
            std::fill(config.condWeights[0].back().begin(), config.condWeights[0].back().end(), 0);
            config.useExtendedMatrix = isUsingExtendedMatrix(config.condWeights);
            config.bestSolutions.clear();
            if (algorithm == Algorithm::Assignment) {
                findSolutionsAssignment(config);
            } else {
                findSolutionsLinKernighan(config);
            }
            config.stopWorking = true;
            state.timerThread.join();
            overwriteFileWithSortedSolutions(config.outputFileName, config.maxSolutionCount, config.solutionsVec, config.repeatNodeMatrix, config.useRespawnMatrix);
            state.timer.stop();
        });
    }
}
