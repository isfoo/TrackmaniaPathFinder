#pragma once
#include "common.h"
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

std::vector<SolutionEdge> createSolution(const SolutionConfig& config, const std::vector<CompressedEdge>& edges) {
    std::vector<SolutionEdge> result;
    auto prev = getUnconditionalPrev(config);
    for (int i = 0; i < edges.size(); ++i) {
        bool isRespawn = (i > 0 && edges[i].src != edges[i - 1].dst) || config.useRespawnMatrix[edges[i].dst][edges[i].src][prev];
        auto solutionEdge = SolutionEdge{ isRespawn ? getRespawnPrev(config) : prev, edges[i].src, edges[i].dst };
        if (config.useRespawnMatrix[edges[i].dst][edges[i].src][prev]) {
            solutionEdge.src = prev;
        }
        if (!config.repeatNodeMatrix.empty())
            solutionEdge.repeatNodesBeforeEdge = config.repeatNodeMatrix[edges[i].dst][edges[i].src][prev];
        result.push_back(solutionEdge);
        prev = solutionEdge.src;
    }
    return result;
}

int calculateSolutionTime(const SolutionConfig& config, const std::vector<SolutionEdge>& solution) {
    int time = 0;
    for (auto& edge : solution)
        time += config.condWeights[edge.dst][edge.src][edge.prev];
    return time;
}

bool isUnconditional(const SolutionConfig& config, CompressedEdge edge) {
    if (!config.useExtendedMatrix)
        return true;
    auto time = config.weights[edge.dst][edge.src];
    auto& allPrevNodeTimes = config.condWeights[edge.dst][edge.src];
    for (auto t : allPrevNodeTimes) {
        if (t != time) {
            return false;
        }
    }
    return true;
}
FastSmallVector<CompressedEdge, 6> getCompressedEdges(const SolutionConfig& config, const SolutionEdge& edge, CompressedEdgeType edgeType) {
    FastSmallVector<CompressedEdge, 6> result;
    if (edgeType & Repeat && !edge.repeatNodesBeforeEdge.empty()) {
        auto e = CompressedEdge{ edge.prev, edge.src, 0 };
        for (int i = 0; i < edge.repeatNodesBeforeEdge.size(); ++i) {
            e.dst = edge.repeatNodesBeforeEdge[i];
            result.push_back(e);
            e.prev = e.src;
            e.src = e.dst;
        };
        e.dst = edge.dst;
        result.push_back(e);
    } else {
        result.push_back({ edge.prev, edge.src, edge.dst });
    }
    if ((edgeType & SequenceDependentMask) == NonSequenceDependent) {
        for (int i = 0; i < result.size(); ++i) {
            result[i].prev = getUnconditionalPrev(config);
        }
    } else if ((edgeType & SequenceDependentMask) == SequenceDependentIsh) {
        for (int i = 0; i < result.size(); ++i) {
            if (isUnconditional(config, result[i])) {
                result[i].prev = getUnconditionalPrev(config);
            }
        }
    }
    return result;
}
std::vector<CompressedEdge> getCompressedSolution(const SolutionConfig& config, const std::vector<SolutionEdge>& solution, CompressedEdgeType edgeType) {
    std::vector<CompressedEdge> result;
    for (auto& edge : solution) {
        auto compressedEdges = getCompressedEdges(config, edge, edgeType);
        result.insert(result.end(), compressedEdges.data.data(), compressedEdges.data.data() + compressedEdges.size());
    }
    if (edgeType & Sorted) {
        std::sort(result.begin(), result.end(), [](auto& a, auto& b) {
            if (a.prev != b.prev)
                return a.prev < b.prev;
            if (a.src != b.src)
                return a.src < b.src;
            return a.dst < b.dst;
        });
    }
    return result;
}

std::vector<SolutionEdge> getSortedSolutionIfPossible(const SolutionConfig& config, const std::vector<SolutionEdge>& solution, CompressedEdgeType variationCompressionType) {
    auto solutionWithRepeats = getCompressedSolution(config, solution, CompressedEdgeType(Repeat | NonSequenceDependent | NonSorted));
    std::vector<CompressedEdge> compressedSortedSolution;
    FastStackBitset visitedNodes;
    auto src = solutionWithRepeats[0].src;
    for (int i = 0; i < solutionWithRepeats.size(); ++i) {
        auto dst = solutionWithRepeats[i].dst;
        if (!visitedNodes.test(dst)) {
            visitedNodes.set(dst);
            compressedSortedSolution.push_back({ getUnconditionalPrev(config), src, dst });
            src = dst;
        }
    }
    auto sortedSolution = createSolution(config, compressedSortedSolution);
    auto sortedSolutionVariationCompression = getCompressedSolution(config, sortedSolution, variationCompressionType);
    auto solutionVariationCompression = getCompressedSolution(config, solution, variationCompressionType);
    if (solutionVariationCompression == sortedSolutionVariationCompression) {
        if (calculateSolutionTime(config, solution) != calculateSolutionTime(config, sortedSolution)) {
            return {}; // This should be impossible, but will leave it for now
        }
        auto sortedSolutionUniqueCompression = getCompressedSolution(config, sortedSolution, CompressedEdgeType(NonRepeat | NonSequenceDependent | NonSorted));
        auto solutionUniqueCompression = getCompressedSolution(config, solution, CompressedEdgeType(NonRepeat | NonSequenceDependent | NonSorted));
        if (sortedSolutionUniqueCompression == solutionUniqueCompression) {
            return {};
        }
        return sortedSolution;
    }
    return {};
}

std::string createSolutionString(const SolutionConfig& config, const std::vector<SolutionEdge>& solution) {
    std::string solStr = "[";
    solStr += "Start";
    for (int i = 0; i < solution.size(); ++i) {
        auto& edge = solution[i];
        if (isRespawn(config, edge)) {
            solStr += ",R(" + std::to_string(edge.src) + ")";
        }
        if (!edge.repeatNodesBeforeEdge.empty()) {
            auto& repeatNodes = edge.repeatNodesBeforeEdge;
            solStr += ",(";
            solStr += std::to_string(repeatNodes[0]);
            for (int i = 1; i < repeatNodes.size(); ++i) {
                solStr += "," + std::to_string(repeatNodes[i]);
            }
            solStr += "),";
        } else if (edge.dst == edge.src + 1 && !isRespawn(config, edge)) {
            solStr += '-';
            i += 1;
            while (i < solution.size() && solution[i].dst == solution[i].src + 1 && !isRespawn(config, solution[i]) && solution[i].repeatNodesBeforeEdge.empty()) {
                i += 1;
            }
            i -= 1;
        } else {
            solStr += ',';
        }
        if (solution[i].dst == config.nodeCount() - 1)
            solStr += "Finish";
        else
            solStr += std::to_string(solution[i].dst);
    }
    solStr += "]";
    return solStr;
}
FastSet2d solutionConnectionsSet(const std::vector<CompressedEdge>& compressedEdgesWithRepeats, int nodeCount) {
    FastSet2d result(nodeCount);
    for (auto edge : compressedEdgesWithRepeats)
        result.set(edge.dst, edge.src);
    return result;
}
std::vector<std::array<int16_t, 3>> solutionUnverifiedConnectionsList(const SolutionConfig& config, const std::vector<SolutionEdge>& solution) {
    std::vector<std::array<int16_t, 3>> result;
    for (auto& edge : solution) {
        if (!config.isVerifiedConnection[edge.dst][edge.src][edge.prev]) {
            result.push_back({ edge.prev, edge.src, edge.dst });
        }
    }
    return result;
}

void insertSortedByTimeAndConnectionOrder(std::vector<BestSolution>& vec, const BestSolution& val) {
    vec.insert(std::upper_bound(vec.begin(), vec.end(), val, [](auto& a, auto& b) {
        if (a.time != b.time)
            return a.time < b.time;
        return a.solution() < b.solution();
    }), val);
}
void saveSolutionAndUpdateLimit(SolutionConfig& config, const std::vector<CompressedEdge>& edges) {
    auto solution = createSolution(config, edges);
    auto time = calculateSolutionTime(config, solution);
    std::scoped_lock l{ config.solutionUpdateMutex };
    if (time > config.limit() || (config.bestSolutions.size() >= config.maxSolutionCount && time >= config.limit()))
        return;

    auto edgeTypeStrictVariation = CompressedEdgeType(NonRepeat | SequenceDependent | Sorted);
    auto edgeTypePermissiveVariation = CompressedEdgeType(Repeat | SequenceDependentIsh | Sorted);
    auto edgeTypeUnique = CompressedEdgeType(NonRepeat | NonSequenceDependent | NonSorted);

    auto compressedSolution = getCompressedSolution(config, solution, edgeTypeUnique);
    auto compressedSolutionForStrictVariations = getCompressedSolution(config, solution, edgeTypeStrictVariation);
    auto compressedSolutionForPermissiveVariations = getCompressedSolution(config, solution, edgeTypePermissiveVariation);

    auto& compressedVariation = compressedSolutionForPermissiveVariations; // TODO: Make this dependent on some new flag in .config
    auto solutionConnections = solutionConnectionsSet(compressedSolutionForPermissiveVariations, config.nodeCount());
    for (int i = 0; i < config.bestSolutions.size(); ++i) {
        if (config.bestSolutions[i].time == time) {
            if (compressedVariation == config.bestSolutions[i].compressedVariation) {
                for (auto& variation : config.bestSolutions[i].variations) {
                    if (compressedSolution == variation.compressedSolution) {
                        return;
                    }
                }
                config.bestSolutions[i].variations.push_back({ solution, compressedSolution });
                return;
            }
        }
    }
    auto sortedSolution = getSortedSolutionIfPossible(config, solution, edgeTypePermissiveVariation);
    auto compressedSortedSolution = getCompressedSolution(config, sortedSolution, edgeTypeUnique);
    auto solutionString = createSolutionString(config, sortedSolution.empty() ? solution : sortedSolution);
    auto unverifiedConnections = solutionUnverifiedConnectionsList(config, solution);

    auto newSolution = BestSolution({ solution, compressedSolution }, { sortedSolution, compressedSortedSolution }, compressedVariation, solutionConnections, unverifiedConnections, solutionString, config.addedConnection, time);
    config.solutionsVec.push_back_not_thread_safe(newSolution);
    insertSortedByTimeAndConnectionOrder(config.bestSolutions, newSolution);
    if (config.bestSolutions.size() > config.maxSolutionCount) {
        config.updateLimit(config.bestSolutions.back().time);
        config.bestSolutions.pop_back();
    }
}

void saveSolution(SolutionConfig& config, const std::vector<CompressedEdge>& edges) {
    saveSolutionAndUpdateLimit(config, edges);
}
void saveSolution(SolutionConfig& config, const std::vector<NodeType>& solution) {
    std::vector<CompressedEdge> edges = { {0, 0, solution[0]} };
    for (int i = 1; i < solution.size() - 1; ++i) {
        edges.push_back({ 0, edges.back().dst, solution[edges.back().dst]});
    }
    saveSolution(config, edges);
}

struct RepeatEdgePath {
    RepeatEdgePath(int k, int j, int i) : k(k), j(j), i(i) {}
    int k, j, i;
    int time(const std::vector<std::vector<int>>& A) const {
        return A[k][j] + A[j][i];
    }
};
std::vector<RepeatEdgePath> getRepeatNodeEdges(const std::vector<std::vector<int>>& A, int ignoredValue, std::vector<int> turnedOffRepeatNodes, bool allowRepeatCpsForFilledConnections) {
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
                if (!allowRepeatCpsForFilledConnections && A[k][i] < ignoredValue)
                    continue;
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

int addRepeatNodeEdges(std::vector<std::vector<int>>& A, ConditionalMatrix<int>& B, RepeatNodeMatrix& repeatEdgeMatrix, const std::vector<RepeatEdgePath>& additionalPaths, int maxEdgesToAdd) {
    int addedEdgesCount = 0;
    for (int m = 0; m < additionalPaths.size() && addedEdgesCount < maxEdgesToAdd; ++m) {
        auto k = additionalPaths[m].k;
        auto j = additionalPaths[m].j;
        auto i = additionalPaths[m].i;
        addedEdgesCount += repeatEdgeMatrix.unconditional(k, i).empty();
        for (int z = 0; z < repeatEdgeMatrix.sizeInlcudingRespawn(); ++z) {
            auto newTime = B[k][j][i] + B[j][i][z];
            if (newTime < B[k][i][z]) {
                auto combined = RepeatNodeMatrix::Combine(repeatEdgeMatrix[j][i][z], j, repeatEdgeMatrix[k][j][i]);
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

RepeatNodeMatrix addRepeatNodeEdges(std::vector<std::vector<int>>& A, ConditionalMatrix<int>& B, int ignoredValue, int maxEdgesToAdd, std::vector<int> turnedOffRepeatNodes, bool allowRepeatCpsForFilledConnections) {
    auto repeatEdgeMatrix = RepeatNodeMatrix(int(B.data.size()));
    if (maxEdgesToAdd <= 0)
        return repeatEdgeMatrix;
    for (int i = 0; i < 2; ++i) {
        auto repeatEdges = getRepeatNodeEdges(A, ignoredValue, turnedOffRepeatNodes, allowRepeatCpsForFilledConnections);
        auto edgesAdded = addRepeatNodeEdges(A, B, repeatEdgeMatrix, repeatEdges, maxEdgesToAdd);
        maxEdgesToAdd = std::max<int>(0, maxEdgesToAdd - edgesAdded);
    }
    return repeatEdgeMatrix;
}
void addRingCps(SolutionConfig& config, const std::vector<int>& ringCps) {
    for (auto ringCp : ringCps) {
        if (ringCp >= config.nodeCount())
            continue;
        for (int i = 0; i < config.nodeCount(); ++i) {
            if (std::find(ringCps.begin(), ringCps.end(), i) != ringCps.end())
                continue;
            for (int j = 0; j < config.nodeCount(); ++j) {
                if (j == ringCp)
                    continue;
                if (config.weights[ringCp][i] < config.ignoredValue && config.condWeights.withRespawn(j, i) < config.condWeights[j][ringCp][i]) {
                    // faster to respawn from ringCp to i then go from i to j, then to directly go from ring to j
                    config.condWeights[j][ringCp][i] = config.condWeights.withRespawn(j, i);
                    config.useRespawnMatrix[j][ringCp][i] = true;
                    if (!config.repeatNodeMatrix.empty()) {
                        config.repeatNodeMatrix[j][ringCp][i] = config.repeatNodeMatrix.withRespawn(j, i);
                    }
                    config.weights[j][ringCp] = std::min(config.weights[j][ringCp], config.condWeights.withRespawn(j, i));
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

bool isUsingExtendedMatrix(ConditionalMatrix<int>& B) {
    bool useExtendedMatrix = false;
    for (int i = 0; i < B.data.size(); ++i) {
        for (int j = 0; j < B[i].size(); ++j) {
            auto val = B[i][j][0];
            for (int k = 0; k < B[i][j].size(); ++k) {
                if (B[i][j][k] != val) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::pair<std::vector<CompressedEdge>, double> createSolutionFromString(std::string solStr, const SolutionConfig& config) {
    std::vector<CompressedEdge> solution;
    auto ErrorValue = std::pair<std::vector<CompressedEdge>, double>{ {}, -1.0 };

    enum { Start, Finish, Integer, Comma, Dash, Respawn, OpenParen, CloseParen };
    auto tokens = tokenize(solStr, { 
        {Start, "start", true}, {Finish, "finish", true}, {Integer, "0123456789"}, {Comma, ",", true},  {Dash, "-", true}, 
        {Respawn, "r", true}, {OpenParen, "(", true}, {CloseParen, ")", true}
    });

    bool endedInComma = true;
    NodeType src = NullNode;
    while (true) {
        if (tokens.empty())
            return ErrorValue;
        auto expectedNodeToken = tokens.eat();
        NodeType dst = NullNode;

        if (expectedNodeToken.typeId == Respawn) {
            if (!endedInComma || tokens.empty() || tokens.eat().typeId != OpenParen || tokens.empty() || tokens.peak().typeId != Integer)
                return ErrorValue;
            auto newSrc = strToInt(tokens.eat().value);
            if (newSrc >= config.weights.size())
                return ErrorValue;
            src = newSrc;
            if (tokens.empty() || tokens.eat().typeId != CloseParen || tokens.empty() || tokens.eat().typeId != Comma || tokens.empty())
                return ErrorValue;
            expectedNodeToken = tokens.eat();
        }
        if (expectedNodeToken.typeId == Start) {
            if (src != NullNode)
                return ErrorValue;
            dst = 0;
        } else if (expectedNodeToken.typeId == Finish) {
            dst = config.weights.size() - 1;
        } else if (expectedNodeToken.typeId == Integer) {
            auto newDst = strToInt(expectedNodeToken.value);
            if (newDst >= config.weights.size())
                return ErrorValue;
            dst = newDst;
        } else {
            return ErrorValue;
        }
        if (endedInComma) {
            if (src != NullNode) {
                solution.push_back({ 0, src, dst });
            }
        } else {
            if (src >= dst)
                return ErrorValue;
            while (src != dst) {
                solution.push_back({ 0, src, NodeType(src + 1) });
                src += 1;
            }
        }
        if (dst == config.weights.size() - 1) {
            if (!tokens.empty())
                return ErrorValue;
            break;
        }
        src = dst;

        if (tokens.empty())
            break;
        auto expectedCommaOrDash = tokens.eat();
        if (expectedCommaOrDash.typeId == Dash) {
            endedInComma = false;
        } else if (expectedCommaOrDash.typeId == Comma) {
            endedInComma = true;
            if (tokens.empty())
                return ErrorValue;
            if (tokens.peak().typeId == OpenParen) {
                tokens.eat();
                while (true) {
                    if (tokens.empty() || tokens.eat().typeId != Integer)
                        return ErrorValue;
                    if (tokens.empty())
                        return ErrorValue;
                    auto token = tokens.eat();
                    if (token.typeId == CloseParen) {
                        break;
                    } else if (token.typeId != Comma) {
                        return ErrorValue;
                    }
                }
                if (tokens.empty() || tokens.eat().typeId != Comma)
                    return ErrorValue;
            }
        } else {
            return ErrorValue;
        }
    }
    
    int realCost = calculateSolutionTime(config, createSolution(config, solution));
    return { solution, realCost / 10.0 };
}

void overwriteFileWithSortedSolutions(const std::string& outputFileName, int maxSolutionCount, const ThreadSafeVec<BestSolution>& solutionsView, SolutionConfig& config) {
    if (outputFileName.empty())
        return;
    std::vector<BestSolution> sortedSolutions;
    for (int i = 0; i < solutionsView.size(); ++i) {
        sortedSolutions.push_back(solutionsView[i]);
    }
    std::sort(sortedSolutions.begin(), sortedSolutions.end(), [](auto& a, auto& b) { return a.time < b.time; });
    std::vector<std::pair<std::string, int>> solutions;
    for (int i = 0; i < maxSolutionCount && i < sortedSolutions.size(); ++i) {
        solutions.push_back({ createSolutionString(config, sortedSolutions[i].solution()), sortedSolutions[i].time});
    }
    overwriteFileWithSortedSolutions(outputFileName, solutions);
}


struct FilterConnection {
    enum Status {
        Required, Banned, Optional, AutoBanned
    };
    FilterConnection() {}
    FilterConnection(std::pair<int, int> connection, Status status) : connection(connection), status(status) {}

    std::pair<int, int> connection;
    Status status;
};

struct State {
    fs::path workingDir;

    std::string errorMsg;
    std::vector<BestSolution> bestFoundSolutions;

    std::future<void> algorithmRunTask;
    std::atomic<bool> taskWasCanceled = false;

    std::vector<int> calculatedCpOrder;
    double outputRouteCalcTime = 0;

    Timer timer;

    std::vector<Position> calculatedCpPositions;
    std::vector<Position> pathToVisualize;

    Algorithm currentAlgorithm = Algorithm::None;
    bool endedWithTimeout = false;
    std::thread timerThread;

    bool isGraphWindowOpen = false;
    BestSolution solutionToShowInGraphWindow;
    BestSolution solutionToCompareToInGraphWindow;
    int solutionToShowId = -1;
    int solutionToCompareId = -1;
    std::vector<Position> cpPositionsVis;
    bool realCpPositionView = false;

    bool isVariationsWindowOpen = false;
    BestSolution solutionToShowVariation;
    int solutionToShowVariationId = -1;

    bool isUnverifiedConnectionsWindowOpen = false;
    BestSolution solutionToShowUnverifiedConnections;
    int solutionToShowUnverifiedConnectionsId = -1;

    std::pair<NodeType, NodeType> hoveredConnection = { 0, 0 };

    bool isOnPathFinderTab = true;

    bool copiedBestSolutionsAfterAlgorithmDone = false;

    std::vector<Edge> connectionsToTest;

    std::vector<FilterConnection> resultRequiredConnections;
    std::vector<FilterConnection> resultOptionalConnections;

    bool createdDistanceMatrix = false;

    int isDisabledStackCount = 0;

    State() {
        timer = Timer();
        timer.stop();
    }
};

struct LinKernighanSettings {
    int maxSequenceLengthLimit = 1'000;
    int tryCount = 1'000;
    bool fullRingCpMode = false;
    std::vector<BestSolution> initialSolutions = {};
};

void findSolutionsAssignment(SolutionConfig& config);
void findSolutionsArborescence(SolutionConfig& config);
void findSolutionsLinKernighan(SolutionConfig& config, LinKernighanSettings settings);
void findSolutionsBruteForce(SolutionConfig& config);

void findSolutionsLinKernighan(SolutionConfig& config, State& state, bool isFastMode) {
    LinKernighanSettings settings;
    if (!config.ringCps.empty()) {
        auto lkhConfig = config;
        lkhConfig.maxSolutionCount = 500;
        addRingCps(lkhConfig, config.ringCps);
        settings.fullRingCpMode = false;
        if (isFastMode) {
            settings.maxSequenceLengthLimit = 8;
            settings.tryCount = 5;
        } else {
            settings.maxSequenceLengthLimit = 20;
            settings.tryCount = 1'000;
        }
        config.partialSolutionCount = std::numeric_limits<uint64_t>::max();
        int maxTime = 1 + (config.nodeCount() >= 40) + (config.nodeCount() >= 75) + (config.nodeCount() >= 100);
        auto timerThread = std::thread([&state, maxTime = maxTime, &lkhConfig, &config]() {
            int copiedSolutionCount = 0;
            auto timer = Timer();
            while (!lkhConfig.stopWorking() && !config.stopWorking() && !state.taskWasCanceled && (maxTime == 0 || timer.getTime() < maxTime)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                for (; copiedSolutionCount < lkhConfig.solutionsVec.size(); ++copiedSolutionCount) {
                    saveSolution(config, lkhConfig.solutionsVec[copiedSolutionCount].compressedSolution());
                }
            }
            lkhConfig.localStopWorking = true;
        });
        findSolutionsLinKernighan(lkhConfig, settings);
        lkhConfig.localStopWorking = true;
        timerThread.join();
        settings.initialSolutions = lkhConfig.bestSolutions;
    }
    settings.fullRingCpMode = !config.ringCps.empty();
    settings.maxSequenceLengthLimit = isFastMode ? 5 : 1'000;
    config.partialSolutionCount = 0;
    findSolutionsLinKernighan(config, settings);
}

void runAlgorithm(Algorithm algorithm, SolutionConfig& config, InputData& input, State& state) {
    state.taskWasCanceled = false;
    state.endedWithTimeout = false;

    std::string inputDataFile = input.inputDataFile;
    if (input.inputDataLink[0] != '\0') {
        auto [spreadsheetId, gid] = getSpreadsheetIdAndGidFromLink(input.inputDataLink);
        if (spreadsheetId.empty() || gid.empty()) {
            state.errorMsg = "Failed to read spreadsheet ID or the sheet/tab ID (gid)";
            return;
        }
        auto dataFilePath = (state.workingDir / (spreadsheetId + "#" + gid + ".csv"));
        if (input.downloadSpreadsheet) {
            if (!downloadGoogleSpreadsheet(spreadsheetId, gid, dataFilePath.string())) {
                state.errorMsg = "Failed to download spreadsheet";
                return;
            }
        } else if (!fs::exists(dataFilePath)) {
            state.errorMsg = "Download checkbox turned off, but couldn't find any previously downloaded version of that sheet";
            return;
        }
        input.downloadSpreadsheet = false;
        inputDataFile = dataFilePath.string();
    }

    bool allowRepeatCpsForFilledConnections = input.allowRepeatCpsForFilledConnections;
    config.updateLimit(input.limitValue * 10);
    config.ignoredValue = input.ignoredValue * 10;
    config.maxSolutionCount = input.maxSolutionCount;
    config.outputFileName = input.outputDataFile;
    config.partialSolutionCount = 0;
    config.globalStopWorking = false;
    config.repeatNodeMatrix.clear();
    config.solutionsVec.clear();
    config.addedConnection = NullEdge;

    auto algorithmData = loadCsvData(inputDataFile, config.ignoredValue, state.errorMsg);
    if (!state.errorMsg.empty())
        return;
    config.weights = std::move(algorithmData.weights);
    config.condWeights = std::move(algorithmData.condWeights);
    config.isVerifiedConnection = std::move(algorithmData.isVerifiedConnection);
    config.useRespawnMatrix = Vector3d<Bool>(int(config.nodeCount()));

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
    auto searchSourceNodes = parseIntList(input.connectionFinderSettings.searchSourceNodes, 0, config.weights.size() - 2, "search source CPs list", state.errorMsg);
    if (!state.errorMsg.empty())
        return;

    state.timer = Timer();
    state.timerThread = std::thread([&state, maxTime=0, &config]() {
        while (!config.stopWorking() && !state.taskWasCanceled && (maxTime == 0 || state.timer.getTime() < maxTime)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        state.endedWithTimeout = maxTime != 0 && state.timer.getTime() >= maxTime;
        config.globalStopWorking = true;
    });
    if (input.isConnectionSearchAlgorithm) {
        auto connectionFinderSettings = input.connectionFinderSettings;
        connectionFinderSettings.minConnectionTime *= 10;
        connectionFinderSettings.maxConnectionTime *= 10;
        connectionFinderSettings.testedConnectionTime *= 10;
        config.useExtendedMatrix = isUsingExtendedMatrix(config.condWeights);
        config.bestSolutions.clear();
        state.connectionsToTest.clear();
        state.algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [algorithm, &input, &state, &config, ringCps, repeatNodesTurnedOff, allowRepeatCpsForFilledConnections, searchSourceNodes, connectionFinderSettings]() mutable {
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
                if (config.stopWorking())
                    break;

                privateConfig.addedConnection = Edge{ src, dst };
                privateConfig.weights[dst][src] = connectionFinderSettings.testedConnectionTime;
                privateConfig.bestSolutions.clear();
                privateConfig.solutionsVec.clear();
                std::atomic<int> limit = config.limit();
                privateConfig.limit_ = &limit;
                std::fill(privateConfig.condWeights[dst][src].begin(), privateConfig.condWeights[dst][src].end(), privateConfig.weights[dst][src]);
                privateConfig.repeatNodeMatrix = addRepeatNodeEdges(privateConfig.weights, privateConfig.condWeights, privateConfig.ignoredValue, input.maxRepeatNodesToAdd, repeatNodesTurnedOff, allowRepeatCpsForFilledConnections);
                if (algorithm == Algorithm::Assignment) {
                    addRingCps(config, ringCps);
                }
                privateConfig.weights = createAtspMatrixFromInput(privateConfig.weights);
                std::fill(privateConfig.condWeights[0].back().begin(), privateConfig.condWeights[0].back().end(), 0);
                if (algorithm == Algorithm::Assignment) {
                    findSolutionsAssignment(privateConfig);
                } else {
                    findSolutionsLinKernighan(privateConfig, state, true);
                }

                if (!privateConfig.bestSolutions.empty()) {
                    auto& newSolution = privateConfig.bestSolutions[0];
                    config.solutionsVec.push_back_not_thread_safe(newSolution);
                    insertSortedByTimeAndConnectionOrder(config.bestSolutions, newSolution);
                    if (config.bestSolutions.size() >= config.maxSolutionCount) {
                        config.updateLimit(config.bestSolutions.back().time);
                        config.bestSolutions.pop_back();
                    }
                }

                privateConfig.weights = config.weights;
                privateConfig.condWeights = config.condWeights;
                config.incrementPartialSolutionCount();
            }
            config.globalStopWorking = true;
            state.timerThread.join();
            state.timer.stop();
        });
    } else {
        config.repeatNodeMatrix = addRepeatNodeEdges(config.weights, config.condWeights, config.ignoredValue, input.maxRepeatNodesToAdd, repeatNodesTurnedOff, allowRepeatCpsForFilledConnections);
        config.ringCps = ringCps;
        if (algorithm == Algorithm::Assignment) {
            addRingCps(config, ringCps);
        }
        clearFile(config.outputFileName);
        state.algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [algorithm, &state, &config]() mutable {
            config.weights = createAtspMatrixFromInput(config.weights);
            std::fill(config.condWeights[0].back().begin(), config.condWeights[0].back().end(), 0);
            config.useExtendedMatrix = isUsingExtendedMatrix(config.condWeights);
            config.bestSolutions.clear();
            if (algorithm == Algorithm::Assignment) {
                findSolutionsAssignment(config);
            } else if (algorithm == Algorithm::Arborescence) {
                findSolutionsArborescence(config);
            } else if (algorithm == Algorithm::BruteForce) {
                findSolutionsBruteForce(config);
            } else {
                findSolutionsLinKernighan(config, state, false);
            }
            config.globalStopWorking = true;
            state.timerThread.join();
            overwriteFileWithSortedSolutions(config.outputFileName, config.maxSolutionCount, config.solutionsVec, config);
            state.timer.stop();
        });
    }
}
