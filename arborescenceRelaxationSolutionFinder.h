#pragma once
#include "branchAndBoundSolutionFinder.h"
#include "minSpanningArborescence.h"

struct ArborescenceSolution : BranchAndBoundSolution<ArborescenceSolution> {
    using Super = BranchAndBoundSolution<ArborescenceSolution>;

    struct TreeNode {
        NodeType parent = NullNode;
        NodeType firstChild = NullNode;
        NodeType nextSibling = NullNode;
        bool isOnMainPath;
    };

    // loose variables - normal assignment copy
    FastStackBitset isRingCp;

    // initialized data (do full copy)
    ArrayWithSize<MinSpanningArborescence::MinEdge> minArboSolutionEdges;
    Array<TreeNode> treeNodes;

    static int InitializedSectionSize(int problemSize, bool useExtendedMatrix) {
        int size = 0;
        size += (problemSize + 1) * sizeof(MinSpanningArborescence::MinEdge); // minArboSolutionEdges
        size += (problemSize + 1) * sizeof(TreeNode); // treeNodes
        return size;
    }
    static int SpecialInitializedDataSize(int problemSize, int strideSize) {
        return 0;
    }
    
    void assignLooseVariables(const ArborescenceSolution& other) {
        isRingCp = other.isRingCp;
    }
    void assignNonLooseVariables(const ArborescenceSolution& other) {
        minArboSolutionEdges = other.minArboSolutionEdges;
        treeNodes = other.treeNodes;
    }
    void assignMemory() {
        Super::assignMemory(minArboSolutionEdges);
        Super::assignMemory(treeNodes);
    }
    void copyInitializedDataSection(const ArborescenceSolution& other) {
        minArboSolutionEdges.size_ = other.minArboSolutionEdges.size();
    }

    ArborescenceSolution(ArborescenceSolution&& other) : Super(std::forward<Super>(other)) { Super::moveInit(std::forward<Super>(other)); }
    ArborescenceSolution(const ArborescenceSolution& other) : Super(other) { Super::copyInit(other); }
    ArborescenceSolution& operator=(ArborescenceSolution&& other) {
        return static_cast<ArborescenceSolution&>(static_cast<Super&>(*this) = std::forward<Super>(other));
    }
    ArborescenceSolution& operator=(const ArborescenceSolution&) = delete;

    ArborescenceSolution(ArrayOfPoolAllocators& allocators, SolutionConfig& config) :
        Super(allocators, &config.weights, &config.condWeights, config.ignoredValue, config.useExtendedMatrix)
    {
        Super::init();

        for (auto ringCp : config.ringCps) {
            isRingCp.set(ringCp);
        }

        for (int i = 0; i < size(); ++i) {
            treeNodes[i].parent = NullNode;
            treeNodes[i].firstChild = NullNode;
            treeNodes[i].nextSibling = i;
            treeNodes[i].isOnMainPath = !isRingCp.test(i);
        }

        lockSingleEdges(In, revAdjList, 1, size() - 1);
        shrinkToFit();
    }

    void addUnassignedDstNode(NodeType node) {}

    int valueAt(NodeType i, NodeType j) {
        return (*costMatrix)[j][i] + (useExtendedMatrix ? costIncreases[j * size() + i] : 0);
    }

    int getCost() const {
        return cost;
    }

    bool shouldLockOutEdges() const {
        return false;
    }

    TreeNode& node(int i) {
        return treeNodes[i];
    }

    bool removeAllOtherMainPathDstEdges(NodeType src) {
        auto mainChild = node(src).firstChild;
        for (int i = 0; i < adjList[src].size(); ++i) {
            auto dst = adjList[src][i];
            if (dst == mainChild)
                continue;
            if (node(dst).isOnMainPath) {
                if (!removeEdge(Out, { src, dst })) {
                    return false;
                }
                i = -1;
            }
        }
        return true;
    }

    bool setNodeAsMainPath(NodeType n) {
        if (node(n).isOnMainPath)
            return true;
        node(n).isOnMainPath = true;
        auto parent = node(n).parent;
        if (parent == NullNode)
            return true;
        auto parentFirstChild = node(parent).firstChild;
        if (n != parentFirstChild && node(parentFirstChild).isOnMainPath)
            return false;
        node(parent).firstChild = n;
        if (isRingCp.test(parent)) {
            return setNodeAsMainPath(parent);
        }
        return removeAllOtherMainPathDstEdges(parent);
    }

    bool customLockOutEdge(Edge edge) {
        node(edge.second).parent = edge.first;
        if (node(edge.first).firstChild == NullNode) {
            node(edge.first).firstChild = edge.second;
            if (node(edge.second).isOnMainPath) {
                if (!setNodeAsMainPath(edge.first))
                    return false;
            }
            if (isRingCp.test(edge.first) && !removeAllOtherEdges(Out, adjList, edge))
                return false;
        } else {
            if (isRingCp.test(edge.first))
                return false;
            auto firstChild = node(edge.first).firstChild;
            node(edge.second).nextSibling = node(firstChild).nextSibling;
            node(firstChild).nextSibling = edge.second;
            if (node(edge.second).isOnMainPath) {
                if (node(firstChild).isOnMainPath)
                    return false;
                node(edge.first).firstChild = edge.second;
                if (!removeAllOtherMainPathDstEdges(edge.first))
                    return false;
            }
        }
        return true;
    }

    bool customCanRemoveLastOutEdge(Edge edge) {
        return !node(edge.first).isOnMainPath;
    }

    bool isComplete() {
        for (int i = 1; i < size(); ++i) {
            if (lockedInEdges[i] == NullNode) {
                return false;
            }
        }
        return true;
    }
    
    Edge findPivotEdge() {
        auto& edges = minArboSolutionEdges;
        std::sort(edges.begin(), edges.end(), [&](MinSpanningArborescence::MinEdge a, MinSpanningArborescence::MinEdge b) {
            auto aIsLocked = lockedInEdges[a.dst] == a.src;
            auto bIsLocked = lockedInEdges[b.dst] == b.src;
            if (aIsLocked == bIsLocked) {
                return valueAt(a.src, a.dst) < valueAt(b.src, b.dst);
            } else {
                return aIsLocked < bIsLocked;
            }
        });
        if (lockedInEdges[edges[0].dst] == edges[0].src)
            return NullEdge; // Should never happen
        return { edges[0].src, edges[0].dst };
    }

    void calculateSolutionAndRevSolution() {
        auto& edges = minArboSolutionEdges;
        std::fill(solution.data, solution.data + problemSize + 1, NullNode);
        std::fill(revSolution.data, revSolution.data + problemSize + 1, NullNode);
        for (int i = 0; i < edges.size(); ++i) {
            solution[edges[i].src] = edges[i].dst;
            revSolution[edges[i].dst] = edges[i].src;
        }
    }

    bool solveRelaxationAndCheckIfStillViable(SolutionConfig& config) {
        shrinkToFit();
        cost = minArborescence(*this, false, &minArboSolutionEdges);

        if (cost > config.limit)
            return false;

        calculateSolutionAndRevSolution();

        return true;
    }

    void saveSolution(SolutionConfig& config, std::mutex& solutionMutex) {
        std::vector<std::vector<CompressedEdge>> allSolutions(1);
        NodeType cur = 0;
        while (cur != size() - 1) {
            auto mainPathNext = node(cur).firstChild;
            std::vector<std::vector<CompressedEdge>> subTrees;
            for (auto next = node(mainPathNext).nextSibling; next != mainPathNext; next = node(next).nextSibling) {
                auto& subTree = subTrees.emplace_back();
                auto subTreeSrc = cur;
                auto subTreeDst = next;
                while (subTreeDst != NullNode) {
                    subTree.push_back(CompressedEdge{ 0, subTreeSrc, subTreeDst });
                    subTreeSrc = subTreeDst;
                    subTreeDst = node(subTreeSrc).firstChild;
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
            ::saveSolution(config, solution, solutionMutex);
        }
    }
};

void findSolutionsArborescence(SolutionConfig& config) {
    findSolutionsBranchAndBound<ArborescenceSolution>(config);
}
