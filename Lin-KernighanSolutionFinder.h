#pragma once
#include "solutionFinderCommon.h"

bool doubleBridge(SolutionConfig& config, std::vector<NodeType>& solution, std::vector<NodeType>& revSolution, FastThreadSafeishHashSet<std::vector<NodeType>>& processedSolutions) {
    auto& cost = config.weights;
    auto& costEx = config.condWeights;
    auto N = solution.size();

    std::vector<NodeType> solutionVec;
    solutionVec.push_back(0);
    for (int i = 0; i < solution.size() - 1; ++i)
        solutionVec.push_back(solution[solutionVec.back()]);
    
    auto next = [N](int i) { return (i + 1 == N)  ? (0)     : (i + 1); };
    auto prev = [N](int i) { return (i - 1 == -1) ? (N - 1) : (i - 1); };

    auto newSolution = solution;
    auto newRevSolution = revSolution;
    for (int i_ = 0; i_ < N - 1; ++i_) {
        auto i = solutionVec[i_];
        auto soli = solution[i];
        int k_ = next(next(i_));
        for (; k_ != prev(i_); k_ = next(k_)) {
            auto k = solutionVec[k_];
            auto solk = solution[k];
            if (solk == 0)
                continue;
            EdgeCostType costChangeik = 0;

            newSolution[i] = solution[k];
            newSolution[k] = solution[i];
            newRevSolution[newSolution[i]] = i;
            newRevSolution[newSolution[k]] = k;
            if (config.useExtendedMatrix) {
                costChangeik -= costEx[soli][i][revSolution[i]] + costEx[solk][k][revSolution[k]];
                costChangeik += costEx[newSolution[i]][i][newRevSolution[i]] + costEx[newSolution[k]][k][newRevSolution[k]];
            } else {
                costChangeik -= cost[soli][i] + cost[solk][k];
                costChangeik += cost[solk][i] + cost[soli][k];
            }
            
            if (costChangeik <= 0) {
                for (int j_ = next(i_); j_ != prev(k_); j_ = next(j_)) {
                    auto j = solutionVec[j_];
                    auto solj = solution[j];
                    if (solj == 0)
                        continue;
                    for (int m_ = next(k_); m_ != prev(i_); m_ = next(m_)) {
                        auto m = solutionVec[m_];
                        if (solution[m] == 0)
                            continue;

                        newSolution[m] = solution[j];
                        newSolution[j] = solution[m];
                        newRevSolution[newSolution[j]] = j;
                        newRevSolution[newSolution[m]] = m;

                        EdgeCostType costChange = costChangeik;
                        if (config.useExtendedMatrix) {
                            costChange -= costEx[solj][j][revSolution[j]] + costEx[solution[m]][m][revSolution[m]];
                            costChange += costEx[newSolution[j]][j][newRevSolution[j]] + costEx[newSolution[m]][m][newRevSolution[m]];
                            if (solution[i] != j) {
                                costChange -= costEx[solution[solution[i]]][solution[i]][i];
                                costChange += costEx[newSolution[newSolution[k]]][newSolution[k]][k];
                            }
                            if (solution[j] != k) {
                                costChange -= costEx[solution[solution[j]]][solution[j]][j];
                                costChange += costEx[newSolution[newSolution[m]]][newSolution[m]][m];
                            }
                            if (solution[k] != m) {
                                costChange -= costEx[solution[solution[k]]][solution[k]][k];
                                costChange += costEx[newSolution[newSolution[i]]][newSolution[i]][i];
                            }
                            if (solution[m] != i) {
                                costChange -= costEx[solution[solution[m]]][solution[m]][m];
                                costChange += costEx[newSolution[newSolution[j]]][newSolution[j]][j];
                            }
                        } else {
                            costChange -= cost[solj][j] + cost[solution[m]][m];
                            costChange += cost[solj][m] + cost[solution[m]][j];
                        }
                        if (costChange < 0 && !processedSolutions.find(newSolution)) {
                            processedSolutions.insert(newSolution);
                            solution = newSolution;
                            revSolution = newRevSolution;
                            return true;
                        }
                        newRevSolution[newSolution[j]] = m;
                        newRevSolution[newSolution[m]] = j;
                        newSolution[j] = solution[j];
                        newSolution[m] = solution[m];
                    }
                }
            }
            newRevSolution[newSolution[i]] = k;
            newRevSolution[newSolution[k]] = i;
            newSolution[i] = solution[i];
            newSolution[k] = solution[k];
        }
    }
    return false;
}
bool linKernighanRec(SolutionConfig& config, const std::vector<std::vector<NodeType>>& adjList, std::vector<NodeType>& solution, EdgeCostType costChange, NodeType curNode, NodeType endNode, int addedEdgesCount, FastStackBitset& bannedDstNodes, std::vector<NodeType>& revSolution, const std::vector<int>& maxSearchWidths, FastThreadSafeishHashSet<std::vector<NodeType>>& processedSolutions) {
    auto& cost = config.weights;
    auto& costEx = config.condWeights;
    auto N = solution.size();

    auto calculateCostChange = [&](NodeType newDstNode, NodeType curNode, NodeType oldSrcNode) {
        EdgeCostType result = 0;
        if (config.useExtendedMatrix) {
            result += costEx[newDstNode][curNode][revSolution[curNode]];
            result -= costEx[newDstNode][oldSrcNode][revSolution[oldSrcNode]];
            result += costEx[solution[newDstNode]][newDstNode][curNode];
            result -= costEx[solution[newDstNode]][newDstNode][oldSrcNode];
            if (newDstNode == revSolution[endNode]) {
                result += costEx[endNode][newDstNode][curNode];
                result -= costEx[endNode][newDstNode][oldSrcNode];
            }
        } else {
            result += cost[newDstNode][curNode];
            result -= cost[newDstNode][oldSrcNode];
        }
        return result;
    };

    if (maxSearchWidths[addedEdgesCount] > 0) {
        auto curBannedDstNodes = bannedDstNodes;
        curBannedDstNodes.set(solution[curNode]);
        if (addedEdgesCount % 2 == 1) {
            // when adding the next even connection (2th, 4th, etc) it has to break a cycle that was made in the previous move
            NodeType bannedNode = endNode;
            while (bannedNode != curNode) {
                curBannedDstNodes.set(bannedNode);
                bannedNode = solution[bannedNode];
            }
        }

        FixedStackVector<std::pair<NodeType, EdgeCostType>> newDstNodesWithNegativeCostChange;
        for (int k = 0; k < adjList[curNode].size(); ++k) {
            auto newDstNode = adjList[curNode][k];
            if (curBannedDstNodes.test(newDstNode))
                continue;
            auto oldSrcNode = revSolution[newDstNode];
            auto newCostChange = costChange + calculateCostChange(newDstNode, curNode, oldSrcNode);
            if (newCostChange < 0) {
                newDstNodesWithNegativeCostChange.emplace_back(newDstNode, newCostChange);
                if (newDstNodesWithNegativeCostChange.size() >= maxSearchWidths[addedEdgesCount])
                    break;
            }
        }
        std::sort(newDstNodesWithNegativeCostChange.begin(), newDstNodesWithNegativeCostChange.end(), [](auto& a, auto& b) { return a.second < b.second; });
        for (auto [newDstNode, newCostChange] : newDstNodesWithNegativeCostChange) {
            bannedDstNodes.set(newDstNode);
            auto oldSrcNode = revSolution[newDstNode];
            auto oldDstNode = solution[curNode];
            solution[curNode] = newDstNode;
            revSolution[newDstNode] = curNode;
            if (linKernighanRec(config, adjList, solution, newCostChange, oldSrcNode, endNode, addedEdgesCount + 1, bannedDstNodes, revSolution, maxSearchWidths, processedSolutions)) {
                bannedDstNodes.reset(newDstNode);
                return true;
            }
            if (config.stopWorking)
                return false;
            solution[curNode] = oldDstNode;
            revSolution[newDstNode] = oldSrcNode;
            bannedDstNodes.reset(newDstNode);
        }
    }

    if (addedEdgesCount % 2 == 0) {
        auto endCostGain = costChange + calculateCostChange(endNode, curNode, revSolution[endNode]);
        if (endCostGain < 0) {
            auto oldDstNode = solution[curNode];
            solution[curNode] = endNode;
            if (!processedSolutions.find(solution)) {
                processedSolutions.insert(solution);
                revSolution[endNode] = curNode;
                return true;
            }
            solution[curNode] = oldDstNode;
        }
    }

    return false;
}
bool linKernighan(SolutionConfig& config, const std::vector<std::vector<NodeType>>& adjList, std::vector<NodeType>& solution, std::vector<NodeType>& revSolution, const std::vector<int>& maxSearchWidths, FastThreadSafeishHashSet<std::vector<NodeType>>& processedSolutions) {
    NodeType startNode = 0;
    for (NodeType startNode = 0; startNode != solution.size() - 1; startNode = solution[startNode]) {
        if (config.stopWorking)
            return false;
        NodeType endNode = solution[startNode];
        FastStackBitset bannedDstNodes;
        bannedDstNodes.set(0);
        bannedDstNodes.set(endNode);
        if (linKernighanRec(config, adjList, solution, 0, startNode, endNode, 0, bannedDstNodes, revSolution, maxSearchWidths, processedSolutions)) {
            return true;
        }
    }
    return false;
}
std::vector<NodeType> generateRandomSolution(int tryId, XorShift64& rng, EdgeCostType ignoredValue, const std::vector<std::vector<std::vector<int>>>& costEx) {
    auto N = costEx.size();
    std::vector<int> solutionVec(N);
    std::iota(solutionVec.begin(), solutionVec.end(), 0);
    if (tryId != 0) {
        std::shuffle(solutionVec.begin() + 1, solutionVec.end() - 1, rng);
    }
    std::vector<NodeType> solution(N);
    for (int i = 1; i < solution.size(); ++i) {
        solution[solutionVec[i - 1]] = solutionVec[i];
    }
    solution.back() = 0;
    return solution;
}
void findSolutionsHeuristic(SolutionConfig& config) {
    auto N = int(config.weights.size());
    auto& cost = config.weights;
    auto& costEx = config.condWeights;

    std::vector<std::vector<NodeType>> adjList(N);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (i == j)
                continue;
            if (cost[j][i] < config.ignoredValue) {
                adjList[i].push_back(j);
            }
        }
    }

    bool isFastRun = config.addedConnection != NullEdge;

    const int ThreadCount = (std::thread::hardware_concurrency() > 0) ? std::thread::hardware_concurrency() : 1;
    ThreadPool threadPool(ThreadCount);
    std::mutex solutionMutex;

    for (int maxSequenceLength = 4; maxSequenceLength < N; maxSequenceLength += 2) {
        if (config.stopWorking)
            break;
        if (isFastRun && maxSequenceLength == 8)
            break;
        std::vector<int> maxSearchWidths(N, 0);
        for (int i = 0; i < maxSequenceLength; ++i) {
            maxSearchWidths[i] = N;
        }
        XorShift64 rng;
        FastThreadSafeishHashSet<std::vector<NodeType>> processedSolutions(24);

        config.partialSolutionCount = ((uint64_t(maxSequenceLength) + 1) << 32);
        const int TryCount = isFastRun ? 5 : 1000;
        for (int tryId = 0; tryId < TryCount; ++tryId) {
            auto solution = generateRandomSolution(tryId, rng, config.ignoredValue, costEx);
            std::vector<NodeType> revSolution(N);
            for (int i = 0; i < N; ++i)
                revSolution[solution[i]] = i;

            if (processedSolutions.find(solution))
                continue;
            processedSolutions.insert(solution);
            saveSolution(config, solution, solutionMutex);

            threadPool.addTask([&config, &adjList, &processedSolutions, &maxSearchWidths, &solutionMutex, solution, revSolution](int) mutable {
                while (true) {
                    while (linKernighan(config, adjList, solution, revSolution, maxSearchWidths, processedSolutions))
                        saveSolution(config, solution, solutionMutex);
                    if (config.stopWorking)
                        return;
                    if (!doubleBridge(config, solution, revSolution, processedSolutions))
                        break;
                    saveSolution(config, solution, solutionMutex);
                }
                config.partialSolutionCount += 1;
            });
        }
        threadPool.wait();
    }
}
