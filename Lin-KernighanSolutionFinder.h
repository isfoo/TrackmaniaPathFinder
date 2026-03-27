#pragma once
#include "solutionFinderCommon.h"
#include "utility.h"
#include "common.h"

struct TreeNode {
    NodeType parent;
    bool isOnMainPath;
    bool isRingCp;
    NodeType mainChild;
    SmallVector<NodeType> otherChildren;
};
template<typename T> bool operator==(const SmallVector<T>& a, const SmallVector<T>& b) {
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}
bool operator==(const TreeNode& a, const TreeNode& b) {
    if ((a.parent != b.parent) || (a.isOnMainPath != b.isOnMainPath) || (a.mainChild != b.mainChild) || (a.otherChildren.size() != b.otherChildren.size()))
        return false;
    if (a.otherChildren.size() == 0)
        return true;
    if (a.otherChildren.size() == 1 && a.otherChildren[0] == b.otherChildren[0])
        return true;
    auto aOtherChildren = a.otherChildren;
    auto bOtherChildren = b.otherChildren;
    std::sort(aOtherChildren.begin(), aOtherChildren.end());
    std::sort(bOtherChildren.begin(), bOtherChildren.end());
    return aOtherChildren == bOtherChildren;
}
template<typename T> bool operator==(const VectorPoolAlloc<T>& a, const VectorPoolAlloc<T>& b) {
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (!(a[i] == b[i]))
            return false;
    }
    return true;
}

namespace std {
    template <> struct hash<VectorPoolAlloc<TreeNode>> {
        std::size_t operator()(const VectorPoolAlloc<TreeNode>& k) const {
            uint64_t hash = 5381;
            for (int i = 0; i < k.size(); ++i) {
                hash = hash * 33 + k[i].parent;
                hash = hash * 33 + k[i].mainChild;
            }
            return hash;
        }
    };
}

void saveSolution(SolutionConfig& config, VectorPoolAlloc<TreeNode>& nodes) {
    std::vector<std::vector<CompressedEdge>> allSolutions(1);
    NodeType cur = 0;
    while (cur != nodes.size() - 1) {
        auto mainPathNext = nodes[cur].mainChild;
        std::vector<std::vector<CompressedEdge>> subTrees;
        for (auto next : nodes[cur].otherChildren) {
            auto& subTree = subTrees.emplace_back();
            auto subTreeSrc = cur;
            auto subTreeDst = next;
            while (subTreeDst != NullNode) {
                subTree.push_back(CompressedEdge{ 0, subTreeSrc, subTreeDst });
                subTreeSrc = subTreeDst;
                subTreeDst = nodes[subTreeSrc].mainChild;
            }
        };
        if (subTrees.size() == 1) {
            for (auto& solution : allSolutions) {
                solution.insert(solution.end(), subTrees[0].begin(), subTrees[0].end());
            }
        } else if (subTrees.size() >= 2) {
            std::vector<std::vector<CompressedEdge>> newAllSolutions;
            for (int i = 0; i < subTrees.size(); ++i) {
                auto allSolutionsCopy = allSolutions;
                for (auto& solution : allSolutionsCopy) {
                    solution.insert(solution.end(), subTrees[i].begin(), subTrees[i].end());
                    for (int j = 0; j < subTrees.size(); ++j) {
                        if (i == j)
                            continue;
                        solution.insert(solution.end(), subTrees[j].begin(), subTrees[j].end());
                    }
                }
                newAllSolutions.insert(newAllSolutions.end(), allSolutionsCopy.begin(), allSolutionsCopy.end());
            }
            allSolutions = std::move(newAllSolutions);
        }
        for (auto& solution : allSolutions)
            solution.push_back(CompressedEdge{ 0, cur, mainPathNext });
        cur = mainPathNext;
    }
    for (auto& solution : allSolutions) {
        saveSolution(config, solution);
    }
}

std::pair<EdgeCostType, NodeType> calculateBranchingSubPathEdgeCosts(SolutionConfig& config, const VectorPoolAlloc<TreeNode>& nodes, NodeType cur, NodeType prev) {
    thread_local FixedStackVector<std::pair<EdgeCostType, NodeType>> nonRespawnDiff;
    if (nodes[cur].otherChildren.empty())
        return { 0, NullNode };
    nonRespawnDiff.clear();
    EdgeCostType respawnCostSum = 0;
    for (auto child : nodes[cur].otherChildren) {
        auto respawnCost = config.condWeights.withRespawn(child, cur);
        respawnCostSum += respawnCost;
        nonRespawnDiff.emplace_back(std::pair{ config.condWeights[child][cur][prev] - respawnCost, child });
    }
    auto minDiff = *std::min_element(nonRespawnDiff.begin(), nonRespawnDiff.end(), [](auto& a, auto& b) { return a.first < b.first; });
    return { respawnCostSum + minDiff.first, minDiff.second };
}
EdgeCostType calculateDifferentOutCostsWithNewPrevNode(SolutionConfig& config, const VectorPoolAlloc<TreeNode>& nodes, NodeType cur, NodeType prev) {
    if (nodes[cur].otherChildren.empty())
        return config.condWeights[nodes[cur].mainChild][cur][prev];
    return calculateBranchingSubPathEdgeCosts(config, nodes, cur, prev).first;
}
EdgeCostType calculateOutCosts(SolutionConfig& config, const VectorPoolAlloc<TreeNode>& nodes, NodeType cur) {
    auto branchPathCosts = calculateBranchingSubPathEdgeCosts(config, nodes, cur, nodes[cur].parent).first;
    if (nodes[cur].otherChildren.empty())
        return config.condWeights[nodes[cur].mainChild][cur][nodes[cur].parent] + branchPathCosts;
    else
        return config.condWeights.withRespawn(nodes[cur].mainChild, cur) + branchPathCosts;
}
bool moveRingCp(SolutionConfig& config, VectorPoolAlloc<TreeNode>& solution, FastThreadSafeishHashSet<VectorPoolAlloc<TreeNode>>& processedSolutions, int ringChainLength, int maxAllowedTimeDiff) {
    auto& costEx = config.condWeights;
    for (NodeType chainStart = 1; chainStart < solution.size() - 1; ++chainStart) {
        auto node = chainStart;
        if (config.stopWorking())
            return false;
        if (!solution[chainStart].isRingCp)
            continue;
        FixedStackVector<NodeType> ringChain;
        ringChain.emplace_back(chainStart);
        EdgeCostType costOfRemovingNode = 0;
        NodeType parent = NullNode;
        NodeType child = NullNode;
        int childIndex = -1;
        if (solution[chainStart].isOnMainPath) {
            bool couldExtractChain = true;
            for (int i = 1; i < ringChainLength; ++i) {
                if (!solution[solution[ringChain.back()].mainChild].isRingCp) {
                    couldExtractChain = false;
                    break;
                }
                ringChain.emplace_back(solution[ringChain.back()].mainChild);
            }
            if (!couldExtractChain)
                break;

            parent = solution[chainStart].parent;
            child = solution[ringChain.back()].mainChild;
            if (solution[parent].otherChildren.empty()) {
                costOfRemovingNode += costEx[child][parent][solution[parent].parent];
                costOfRemovingNode -= costEx[chainStart][parent][solution[parent].parent];
            } else {
                costOfRemovingNode += costEx.withRespawn(child, parent);
                costOfRemovingNode -= costEx.withRespawn(chainStart, parent);
            }
            if (ringChain.size() >= 2) {
                costOfRemovingNode -= costEx[ringChain[1]][ringChain[0]][parent];
                costOfRemovingNode -= costEx[child][ringChain.back()][ringChain[ringChain.size() - 2]];
            } else {
                costOfRemovingNode -= costEx[child][chainStart][parent];
            }
            costOfRemovingNode -= calculateDifferentOutCostsWithNewPrevNode(config, solution, child, ringChain.back());
            costOfRemovingNode += calculateDifferentOutCostsWithNewPrevNode(config, solution, child, parent);
            if (costOfRemovingNode > 0)
                continue;

            solution[parent].mainChild = child;
            solution[child].parent = parent;
        } else if (solution[solution[chainStart].parent].isOnMainPath) {
            parent = solution[chainStart].parent;
            costOfRemovingNode -= calculateOutCosts(config, solution, parent);
            for (int i = 0; i < solution[parent].otherChildren.size(); ++i) {
                if (chainStart == solution[parent].otherChildren[i]) {
                    childIndex = i;
                    break;
                }
            }
            child = solution[node].mainChild;
            if (ringChainLength >= 2) {
                if (child == NullNode)
                    continue;
                // TODO: for now I just always extract whole chain
                while (child != NullNode) {
                    ringChain.emplace_back(child);
                    child = solution[child].mainChild;
                }
                costOfRemovingNode -= costEx[ringChain[1]][ringChain[0]][parent];
            }
            if (child == NullNode) {
                std::swap(solution[parent].otherChildren[childIndex], solution[parent].otherChildren.back());
                solution[parent].otherChildren.pop_back();
            } else {
                solution[child].parent = parent;
                solution[parent].otherChildren[childIndex] = child;
            }
            costOfRemovingNode += calculateOutCosts(config, solution, parent);
            if (costOfRemovingNode > 0) {
                if (child == NullNode) {
                    solution[parent].otherChildren.emplace_back(chainStart);
                } else {
                    solution[child].parent = node;
                    solution[parent].otherChildren[childIndex] = node;
                }
                continue;
            }
        } else {
            parent = solution[node].parent;
            child = solution[node].mainChild;
            if (child != NullNode) {
                costOfRemovingNode -= costEx[child][node][parent];
                costOfRemovingNode += costEx[child][parent][solution[parent].parent];
            }
            costOfRemovingNode -= costEx[node][parent][solution[parent].parent];
            if (costOfRemovingNode > 0)
                continue;
            solution[parent].mainChild = child;
            if (child != NullNode) {
                solution[child].parent = parent;
            }
        }
        for (NodeType newParent = 0; newParent < solution.size() - 1; ++newParent) {
            if (newParent == node || (newParent == parent && !solution[parent].isOnMainPath))
                continue;
            if (ringChain.size() >= 2 && std::find(ringChain.begin(), ringChain.end(), newParent) != ringChain.end())
                continue;
            EdgeCostType costOfAddingNode = 0;
            auto tryAddNewSolutionCommon = [&solution, &processedSolutions, &ringChain, newParent](NodeType newChild, bool isOnMainPath) {
                auto oldParent = solution[ringChain[0]].parent;
                auto oldChild = solution[ringChain.back()].mainChild;
                solution[ringChain[0]].parent = newParent;
                solution[ringChain.back()].mainChild = newChild;
                auto oldIsOnMainPath = solution[ringChain.back()].isOnMainPath;
                for (int i = 0; i < ringChain.size(); ++i) {
                    solution[ringChain[i]].isOnMainPath = isOnMainPath;
                }
                if (!processedSolutions.find(solution)) {
                    processedSolutions.insert(solution);
                    return true;
                }
                solution[ringChain[0]].parent = oldParent;
                solution[ringChain.back()].mainChild = oldChild;
                for (int i = 0; i < ringChain.size(); ++i) {
                    solution[ringChain[i]].isOnMainPath = oldIsOnMainPath;
                }
                return false;
            };
            auto tryAddNewSolutionNewSubtree = [&tryAddNewSolutionCommon, maxAllowedTimeDiff, &solution, &ringChain, newParent, costOfRemovingNode, &costOfAddingNode]() {
                if (costOfAddingNode + costOfRemovingNode > maxAllowedTimeDiff)
                    return false;
                solution[newParent].otherChildren.emplace_back(ringChain[0]);
                if (tryAddNewSolutionCommon(NullNode, false))
                    return true;
                solution[newParent].otherChildren.pop_back();
                return false;
            };
            auto tryAddNewSolutionPath = [&tryAddNewSolutionCommon, maxAllowedTimeDiff, &solution, &ringChain, newParent, costOfRemovingNode, &costOfAddingNode](NodeType newChild, bool isOnMainPath) {
                if (costOfAddingNode + costOfRemovingNode > maxAllowedTimeDiff)
                    return false;
                solution[newParent].mainChild = ringChain[0];
                if (newChild != NullNode)
                    solution[newChild].parent = ringChain.back();
                if (tryAddNewSolutionCommon(newChild, isOnMainPath))
                    return true;
                solution[newParent].mainChild = newChild;
                if (newChild != NullNode)
                    solution[newChild].parent = newParent;
                return false;
            };
            if (!solution[newParent].isOnMainPath) {
                // adding ring in the middle of a subtree
                auto newChild = solution[newParent].mainChild;
                if (ringChain.size() >= 2) {
                    if (newChild != NullNode)
                        continue;
                    costOfAddingNode += costEx[ringChain[0]][newParent][solution[newParent].parent];
                    costOfAddingNode += costEx[ringChain[1]][ringChain[0]][newParent];
                    if (tryAddNewSolutionPath(NullNode, false))
                        return true;
                } else {
                    if (newChild != NullNode) {
                        costOfAddingNode -= costEx[newChild][newParent][solution[newParent].parent];
                        costOfAddingNode += costEx[newChild][node][newParent];
                    }
                    costOfAddingNode += costEx[node][newParent][solution[newParent].parent];
                    if (tryAddNewSolutionPath(newChild, false))
                        return true;
                }
            } else {
                auto newChild = solution[newParent].mainChild;
                auto [oldBranchCost, noRespawnNode] = calculateBranchingSubPathEdgeCosts(config, solution, newParent, solution[newParent].parent);

                if (!solution[newParent].isRingCp && solution[newParent].otherChildren.empty()) {
                    // Adding ring as first subtree
                    if (ringChain.size() >= 2) {
                        costOfAddingNode += costEx[ringChain[1]][ringChain[0]][newParent];
                    }
                    costOfAddingNode += costEx[chainStart][newParent][solution[newParent].parent];
                    costOfAddingNode += costEx.withRespawn(newChild, newParent);
                    costOfAddingNode -= costEx[newChild][newParent][solution[newParent].parent];
                    if (tryAddNewSolutionNewSubtree())
                        return true;
                } else if (!solution[newParent].isRingCp) {
                    if (ringChain.size() >= 2)
                        continue;
                    // Adding ring as another subtree
                    auto costIfNewNoRespawn = costEx[node][newParent][solution[newParent].parent] + costEx.withRespawn(noRespawnNode, newParent) - costEx[noRespawnNode][newParent][solution[newParent].parent];
                    costOfAddingNode += std::min(costEx.withRespawn(node, newParent), costIfNewNoRespawn);
                    if (tryAddNewSolutionNewSubtree())
                        return true;
                    
                    // Adding ring at the start of existing subtree
                    for (int i = 0; i < solution[newParent].otherChildren.size(); ++i) {
                        auto newChild = solution[newParent].otherChildren[i];
                        costOfAddingNode = 0;
                        // TODO: sometimes undervalues
                        if (newChild == noRespawnNode) {
                            costOfAddingNode -= costEx[newChild][newParent][solution[newParent].parent];
                            costOfAddingNode += costEx[node][newParent][solution[newParent].parent];
                        } else {
                            costOfAddingNode -= costEx.withRespawn(newChild, newParent); 
                            costOfAddingNode += costEx.withRespawn(node, newParent);
                        }
                        costOfAddingNode += costEx[newChild][node][newParent];
                        if (solution[newChild].mainChild != NullNode) {
                            costOfAddingNode -= costEx[solution[newChild].mainChild][newChild][newParent];
                            costOfAddingNode += costEx[solution[newChild].mainChild][newChild][node];
                        }
                        if (costOfAddingNode + costOfRemovingNode <= maxAllowedTimeDiff) {
                            solution[newParent].otherChildren[i] = node;
                            solution[newChild].parent = node;
                            if (tryAddNewSolutionCommon(newChild, false))
                                return true;
                            solution[newParent].otherChildren[i] = newChild;
                            solution[newChild].parent = newParent;
                        }
                    }
                }
                if (ringChain.size() >= 2)
                    continue;

                // Adding ring as part of main path
                costOfAddingNode = 0;
                // TODO: the logic like this if/else one appears in at least one more place, maybe more
                if (solution[newParent].otherChildren.empty()) {
                    costOfAddingNode -= costEx[newChild][newParent][solution[newParent].parent];
                    costOfAddingNode += costEx[node][newParent][solution[newParent].parent];
                } else {
                    costOfAddingNode -= costEx.withRespawn(newChild, newParent);
                    costOfAddingNode += costEx.withRespawn(node, newParent);
                }
                costOfAddingNode += costEx[newChild][node][newParent];
                costOfAddingNode -= calculateDifferentOutCostsWithNewPrevNode(config, solution, newChild, newParent);
                costOfAddingNode += calculateDifferentOutCostsWithNewPrevNode(config, solution, newChild, node);
                if (tryAddNewSolutionPath(newChild, true))
                    return true;
            }
        }
        if (solution[node].isOnMainPath || !solution[solution[node].parent].isOnMainPath) {
            solution[parent].mainChild = chainStart;
            if (child != NullNode) {
                solution[child].parent = ringChain.back();
            }
        } else {
            if (child == NullNode) {
                solution[parent].otherChildren.emplace_back(chainStart);
            } else {
                solution[child].parent = node;
                solution[parent].otherChildren[childIndex] = node;
            }
        }
    }
    return false;
}

bool doubleBridge(SolutionConfig& config, VectorPoolAlloc<TreeNode>& solution, FastThreadSafeishHashSet<VectorPoolAlloc<TreeNode>>& processedSolutions) {
    auto& cost = config.weights;
    auto& costEx = config.condWeights;

    std::vector<NodeType> solutionVec;
    solutionVec.push_back(0);
    while (solution[solutionVec.back()].mainChild != 0) {
        solutionVec.push_back(solution[solutionVec.back()].mainChild);
    }

    auto N = solutionVec.size();
    auto next = [N](int i) { return (i + 1 == N)  ? (0)     : (i + 1); };
    auto prev = [N](int i) { return (i - 1 == -1) ? (N - 1) : (i - 1); };

    auto newSolution = solution;
    for (int i_ = 0; i_ < N - 1; ++i_) {
        auto i = solutionVec[i_];
        auto iPrev = solution[i].otherChildren.empty() ? solution[i].parent : costEx[0][0].size() - 1;
        auto soli = solution[i].mainChild;
        for (int k_ = next(next(i_)); k_ != prev(i_); k_ = next(k_)) {
            auto k = solutionVec[k_];
            auto kPrev = solution[k].otherChildren.empty() ? solution[k].parent : costEx[0][0].size() - 1;
            auto solk = solution[k].mainChild;
            if (solk == 0)
                continue;
            EdgeCostType costChangeik = 0;

            newSolution[i].mainChild = solution[k].mainChild;
            newSolution[k].mainChild = solution[i].mainChild;
            newSolution[newSolution[i].mainChild].parent = i;
            newSolution[newSolution[k].mainChild].parent = k;
            if (config.useExtendedMatrix) {
                costChangeik -= costEx[soli][i][iPrev] + costEx[solk][k][kPrev];
                costChangeik += costEx[newSolution[i].mainChild][i][iPrev] + costEx[newSolution[k].mainChild][k][kPrev];
            } else {
                costChangeik -= cost[soli][i] + cost[solk][k];
                costChangeik += cost[solk][i] + cost[soli][k];
            }
            
            if (costChangeik <= 0) {
                for (int j_ = next(i_); j_ != prev(k_); j_ = next(j_)) {
                    auto j = solutionVec[j_];
                    auto jPrev = solution[j].otherChildren.empty() ? solution[j].parent : costEx[0][0].size() - 1;
                    auto solj = solution[j].mainChild;
                    if (solj == 0)
                        continue;
                    for (int m_ = next(k_); m_ != prev(i_); m_ = next(m_)) {
                        auto m = solutionVec[m_];
                        auto mPrev = solution[m].otherChildren.empty() ? solution[m].parent : costEx[0][0].size() - 1;
                        if (solution[m].mainChild == 0)
                            continue;

                        newSolution[m].mainChild = solution[j].mainChild;
                        newSolution[j].mainChild = solution[m].mainChild;
                        newSolution[newSolution[j].mainChild].parent = j;
                        newSolution[newSolution[m].mainChild].parent = m;

                        auto newjPrev = newSolution[j].otherChildren.empty() ? newSolution[j].parent : costEx[0][0].size() - 1;
                        auto newmPrev = newSolution[m].otherChildren.empty() ? newSolution[m].parent : costEx[0][0].size() - 1;

                        EdgeCostType costChange = costChangeik;
                        if (config.useExtendedMatrix) {
                            costChange -= costEx[solj][j][jPrev] + costEx[solution[m].mainChild][m][mPrev];
                            costChange += costEx[newSolution[j].mainChild][j][newjPrev] + costEx[newSolution[m].mainChild][m][newmPrev];
                            if (solution[i].mainChild != j) {
                                costChange -= calculateDifferentOutCostsWithNewPrevNode(config, solution, solution[i].mainChild, i);
                                costChange += calculateDifferentOutCostsWithNewPrevNode(config, newSolution, solution[i].mainChild, k);
                            }
                            if (solution[j].mainChild != k) {
                                costChange -= calculateDifferentOutCostsWithNewPrevNode(config, solution, solution[j].mainChild, j);
                                costChange += calculateDifferentOutCostsWithNewPrevNode(config, newSolution, solution[j].mainChild, m);
                            }
                            if (solution[k].mainChild != m) {
                                costChange -= calculateDifferentOutCostsWithNewPrevNode(config, solution, solution[k].mainChild, k);
                                costChange += calculateDifferentOutCostsWithNewPrevNode(config, newSolution, solution[k].mainChild, i);
                            }
                            if (solution[m].mainChild != i) {
                                costChange -= calculateDifferentOutCostsWithNewPrevNode(config, solution, solution[m].mainChild, m);
                                costChange += calculateDifferentOutCostsWithNewPrevNode(config, newSolution, solution[m].mainChild, j);
                            }
                        } else {
                            costChange -= cost[solj][j] + cost[solution[m].mainChild][m];
                            costChange += cost[solj][m] + cost[solution[m].mainChild][j];
                        }
                        if (costChange < 0 && !processedSolutions.find(newSolution)) {
                            solution = newSolution;
                            processedSolutions.insert(solution);
                            return true;
                        }
                        newSolution[newSolution[j].mainChild].parent = m;
                        newSolution[newSolution[m].mainChild].parent = j;
                        newSolution[j].mainChild = solution[j].mainChild;
                        newSolution[m].mainChild = solution[m].mainChild;
                    }
                }
            }
            newSolution[newSolution[i].mainChild].parent = k;
            newSolution[newSolution[k].mainChild].parent = i;
            newSolution[i].mainChild = solution[i].mainChild;
            newSolution[k].mainChild = solution[k].mainChild;
        }
    }
    return false;
}
bool linKernighanRec(SolutionConfig& config, const std::vector<std::vector<NodeType>>& adjList, VectorPoolAlloc<TreeNode>& solution, EdgeCostType costChange, NodeType curNode, NodeType endNode, int addedEdgesCount, FastStackBitset& bannedDstNodes, const std::vector<int>& maxSearchWidths, FastThreadSafeishHashSet<VectorPoolAlloc<TreeNode>>& processedSolutions, std::atomic<bool>& stopWorking) {
    if (config.stopWorking() || stopWorking)
        return false;
    
    auto& cost = config.weights;
    auto& costEx = config.condWeights;
    auto N = solution.size();

    auto calculateCostChange = [&](NodeType newDstNode, NodeType curNode, NodeType oldSrcNode) {
        EdgeCostType result = 0;
        if (config.useExtendedMatrix) {
            if (solution[curNode].otherChildren.empty()) {
                result += costEx[newDstNode][curNode][solution[curNode].parent];
            } else {
                result += costEx.withRespawn(newDstNode, curNode);
            }
            if (solution[oldSrcNode].otherChildren.empty()) {
                result -= costEx[newDstNode][oldSrcNode][solution[oldSrcNode].parent];
            } else {
                result -= costEx.withRespawn(newDstNode, oldSrcNode);
            }
            result += calculateDifferentOutCostsWithNewPrevNode(config, solution, newDstNode, curNode);
            result -= calculateDifferentOutCostsWithNewPrevNode(config, solution, newDstNode, oldSrcNode);
            if (newDstNode == solution[endNode].parent) {
                if (solution[newDstNode].otherChildren.empty()) {
                    result += costEx[endNode][newDstNode][curNode];
                    result -= costEx[endNode][newDstNode][oldSrcNode];
                }
            }
        } else {
            result += cost[newDstNode][curNode];
            result -= cost[newDstNode][oldSrcNode];
        }
        return result;
    };

    if (maxSearchWidths[addedEdgesCount] > 0) {
        auto curBannedDstNodes = bannedDstNodes;
        curBannedDstNodes.set(solution[curNode].mainChild);
        if (addedEdgesCount % 2 == 1) {
            // when adding the next even connection (2th, 4th, etc) it has to break a cycle that was made in the previous move
            NodeType bannedNode = endNode;
            while (bannedNode != curNode) {
                curBannedDstNodes.set(bannedNode);
                bannedNode = solution[bannedNode].mainChild;
            }
        }

        FixedStackVector<std::pair<NodeType, EdgeCostType>> newDstNodesWithNegativeCostChange;
        for (int k = 0; k < adjList[curNode].size(); ++k) {
            auto newDstNode = adjList[curNode][k];
            if (curBannedDstNodes.test(newDstNode))
                continue;
            auto oldSrcNode = solution[newDstNode].parent;
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
            auto oldSrcNode = solution[newDstNode].parent;
            auto oldDstNode = solution[curNode].mainChild;

            solution[curNode].mainChild = newDstNode;
            solution[newDstNode].parent = curNode;
            if (linKernighanRec(config, adjList, solution, newCostChange, oldSrcNode, endNode, addedEdgesCount + 1, bannedDstNodes, maxSearchWidths, processedSolutions, stopWorking)) {
                bannedDstNodes.reset(newDstNode);
                return true;
            }
            if (config.stopWorking())
                return false;
            solution[curNode].mainChild = oldDstNode;
            solution[newDstNode].parent = oldSrcNode;
            bannedDstNodes.reset(newDstNode);
        }
    }

    if (addedEdgesCount % 2 == 0) {
        auto endCostGain = costChange + calculateCostChange(endNode, curNode, solution[endNode].parent);
        if (endCostGain < 0) {
            auto oldDstNode = solution[curNode].mainChild;
            auto oldParent = solution[endNode].parent;
            solution[curNode].mainChild = endNode;
            solution[endNode].parent = curNode;
            if (!processedSolutions.find(solution)) {
                processedSolutions.insert(solution);
                return true;
            }
            solution[curNode].mainChild = oldDstNode;
            solution[endNode].parent = oldParent;
        }
    }

    return false;
}
bool linKernighan(SolutionConfig& config, const std::vector<std::vector<NodeType>>& adjList, VectorPoolAlloc<TreeNode>& solution, const std::vector<int>& maxSearchWidths, FastThreadSafeishHashSet<VectorPoolAlloc<TreeNode>>& processedSolutions, std::atomic<bool>& stopWorking) {
    NodeType startNode = 0;
    for (NodeType startNode = 0; startNode != solution.size() - 1; startNode = solution[startNode].mainChild) {
        if (config.stopWorking())
            return false;
        NodeType endNode = solution[startNode].mainChild;
        FastStackBitset bannedDstNodes;
        bannedDstNodes.set(0);
        bannedDstNodes.set(endNode);
        for (NodeType n = 0; n < config.nodeCount(); ++n) {
            if (!solution[n].isOnMainPath) {
                bannedDstNodes.set(n);
            }
        }
        if (linKernighanRec(config, adjList, solution, 0, startNode, endNode, 0, bannedDstNodes, maxSearchWidths, processedSolutions, stopWorking)) {
            return true;
        }
    }
    return false;
}
void generateRandomSolution(int tryId, VectorPoolAlloc<TreeNode>& nodes, XorShift64& rng, EdgeCostType ignoredValue, const ConditionalMatrix<int>& costEx) {
    auto N = costEx.data.size();
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
    for (int i = 0; i < N; ++i) {
        nodes[i].isOnMainPath = true;
        nodes[i].mainChild = solution[i];
        nodes[solution[i]].parent = i;
    }
}

void findSolutionsLinKernighan(SolutionConfig& config, LinKernighanSettings settings) {
    auto N = int(config.nodeCount());
    auto& cost = config.weights;
    auto& costEx = config.condWeights;

    FastStackBitset isRingCp;
    for (auto ringCp : config.ringCps)
        isRingCp.set(ringCp);

    ArrayOfPoolAllocators allocators(16384, 16, 8'000'000'000, sizeof(TreeNode) * N);

    VectorPoolAlloc<TreeNode> nodesBase(&allocators, N);
    for (int i = 0; i < N; ++i) {
        nodesBase[i].parent = NullNode;
        nodesBase[i].mainChild = NullNode;
        nodesBase[i].isOnMainPath = !isRingCp.test(i);
        nodesBase[i].isRingCp = isRingCp.test(i);
    }

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

    const int ThreadCount = (std::thread::hardware_concurrency() > 0) ? std::thread::hardware_concurrency() : 1;
    ThreadPool threadPool(ThreadCount);

    for (int maxSequenceLength = 4; maxSequenceLength < N; maxSequenceLength += 2) {
        if (config.stopWorking())
            break;
        if (maxSequenceLength >= settings.maxSequenceLengthLimit)
            break;
        std::vector<int> maxSearchWidths(N, 0);
        for (int i = 0; i < maxSequenceLength; ++i) {
            maxSearchWidths[i] = N;
        }
        XorShift64 rng;
        FastThreadSafeishHashSet<VectorPoolAlloc<TreeNode>> processedSolutions(3'000'000'000 / (N * sizeof(TreeNode)), maxSequenceLength < 8 ? 24 : 26);
        std::atomic<bool> stopWorking = false;

        config.partialSolutionCount.store(int128_t(maxSequenceLength + 1, 0));
        const int TryCount = settings.tryCount + ThreadCount * 2;
        for (int tryId = 0; tryId < TryCount; ++tryId) {
            auto solution = nodesBase;
            if (tryId < settings.initialSolutions.size()) {
                auto s = settings.initialSolutions[tryId].solution();
                std::stable_sort(s.begin(), s.end(), [](auto& a, auto& b) {
                    return a.src > b.src;
                });
                NodeType lastSrc = NullNode;
                for (int i = s.size() - 1; i >= 0; --i) {
                    auto& edge = s[i];
                    solution[edge.dst].parent = edge.src;
                    if (edge.src == lastSrc) {
                        solution[edge.src].otherChildren.emplace_back(edge.dst);
                    } else {
                        solution[edge.src].mainChild = edge.dst;
                    }
                    lastSrc = edge.src;
                }
                solution[0].parent = N - 1;
                solution[N - 1].mainChild = 0;
                auto n = NodeType(N - 1);
                do {
                    solution[n].isOnMainPath = true;
                    n = solution[n].parent;
                } while (n != 0);
            } else {
                generateRandomSolution(tryId, solution, rng, config.ignoredValue, costEx);
            }

            if (processedSolutions.find(solution))
                continue;
            processedSolutions.insert(solution);
            saveSolution(config, solution);

            threadPool.addTask([&config, &adjList, &processedSolutions, &maxSearchWidths, &stopWorking, &settings, solution](int) mutable {
                int count = 0;
                while (true) {
                    if (config.stopWorking() || stopWorking)
                        return;
                    while (linKernighan(config, adjList, solution, maxSearchWidths, processedSolutions, stopWorking)) {
                        saveSolution(config, solution);
                        if (config.stopWorking() || stopWorking)
                            return;
                    }
                    if (config.stopWorking() || stopWorking)
                        return;

                    if (settings.fullRingCpMode) {
                        bool madeChange = false;
                        if (moveRingCp(config, solution, processedSolutions, 1, 0) || moveRingCp(config, solution, processedSolutions, 1, 30)) {
                            saveSolution(config, solution);
                            madeChange = true;
                        }
                        if (config.stopWorking() || stopWorking)
                            return;

                        if (moveRingCp(config, solution, processedSolutions, 2, 0) || moveRingCp(config, solution, processedSolutions, 2, 30)) {
                            saveSolution(config, solution);
                            madeChange = true;
                        }
                        if (config.stopWorking() || stopWorking)
                            return;

                        if (!madeChange && doubleBridge(config, solution, processedSolutions)) {
                            saveSolution(config, solution);
                            madeChange = true;
                        }
                        if (config.stopWorking() || stopWorking)
                            return;
                            
                        if (!madeChange)
                            break;
                    } else {
                        if (!doubleBridge(config, solution, processedSolutions))
                            break;
                        saveSolution(config, solution);
                    }
                }
                config.incrementPartialSolutionCount();
            });
        }
        while (threadPool.remainingTasksInQueueCount() >= ThreadCount) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        stopWorking = true;
        threadPool.wait();
    }
}
