#pragma once
#include "branchAndBoundSolutionFinder.h"
#include "minSpanningArborescence.h"

struct AssignmentSolution : BranchAndBoundSolution<AssignmentSolution> {
    using Super = BranchAndBoundSolution<AssignmentSolution>;

    // loose variables - normal assignment copy
    EdgeCostType arboCost = 0;

    // initialized data (do full copy)
    Array<EdgeCostType> inReductions;
    Array<EdgeCostType> outReductions;
    ArrayWithSize<NodeType> unassignedDstNodes;
    Array<bool> unassignedDstNodesSet;
    ArrayWithSize<PartialRoute> partialRoutes;

    static int InitializedSectionSize(int problemSize, bool useExtendedMatrix) {
        int size = 0;
        size += (problemSize + 1) * sizeof(EdgeCostType); // inReductions
        size += (problemSize + 1) * sizeof(EdgeCostType); // outReductions
        size += (problemSize + 1) * sizeof(NodeType); // unassignedDstNodes
        size += (problemSize + 1) * sizeof(bool); // unassignedDstNodesSet
        size += (problemSize + 1) * sizeof(PartialRoute); // partialRoutes
        return size;
    }
    static int SpecialInitializedDataSize(int problemSize, int strideSize) {
        return 0;
    }
    
    void assignLooseVariables(const AssignmentSolution& other) {
        arboCost = other.arboCost;
    }
    void assignNonLooseVariables(const AssignmentSolution& other) {
        inReductions = other.inReductions;
        outReductions = other.outReductions;
        unassignedDstNodes = other.unassignedDstNodes;
        unassignedDstNodesSet = other.unassignedDstNodesSet;
        partialRoutes = other.partialRoutes;
    }
    void assignMemory() {
        Super::assignMemory(inReductions);
        Super::assignMemory(outReductions);
        Super::assignMemory(unassignedDstNodes);
        Super::assignMemory(unassignedDstNodesSet);
        Super::assignMemory(partialRoutes);
    }
    void copyInitializedDataSection(const AssignmentSolution& other) {
        unassignedDstNodes.size_ = other.unassignedDstNodes.size();
        partialRoutes.size_ = other.partialRoutes.size();
    }

    AssignmentSolution(AssignmentSolution&& other) : Super(std::forward<Super>(other)) { Super::moveInit(std::forward<Super>(other)); }
    AssignmentSolution(const AssignmentSolution& other) : Super(other) { Super::copyInit(other); }
    AssignmentSolution& operator=(AssignmentSolution&& other) {
        return static_cast<AssignmentSolution&>(static_cast<Super&>(*this) = std::forward<Super>(other));
    }
    AssignmentSolution& operator=(const AssignmentSolution&) = delete;

    AssignmentSolution(ArrayOfPoolAllocators& allocators, SolutionConfig& config) :
        Super(allocators, &config.weights, &config.condWeights, config.ignoredValue, config.useExtendedMatrix)
    {
        Super::init();
        std::fill(inReductions.data, inReductions.data + problemSize + 1, 0);
        std::fill(outReductions.data, outReductions.data + problemSize + 1, 0);
        std::fill(unassignedDstNodesSet.data, unassignedDstNodesSet.data + problemSize + 1, true);
        std::iota(unassignedDstNodes.data, unassignedDstNodes.data + problemSize, 0);
        unassignedDstNodes.size_ = problemSize;
        partialRoutes.size_ = 0;

        lockSingleEdges(Out, adjList, 0, size() - 2);
        lockSingleEdges(In, revAdjList, 1, size() - 1);
        shrinkToFit();
    }

    void addUnassignedDstNode(NodeType node) {
        if (!unassignedDstNodesSet[node]) {
            unassignedDstNodesSet[node] = true;
            unassignedDstNodes.push_back(node);
            solution[revSolution[node]] = NullNode;
        }
    }

    void removeTooExpensiveEdges(int costLimit) {
        auto maxDiff = costLimit - cost;
        for (int i = 0; i < adjList.size(); ++i) {
            for (int k = 0; k < adjList[i].size(); ++k) {
                int j = adjList[i][k];
                if (valueAt(i, j) > maxDiff) {
                    removeOutEdge({ i, j });
                }
            }
        }
    }

    int getCost() const {
        return cost + arboCost;
    }

    int valueAt(NodeType i, NodeType j) {
        return (*costMatrix)[j][i] - outReductions[i] - inReductions[j] + (useExtendedMatrix ? costIncreases[j * size() + i] : 0);
    }

    bool shouldLockOutEdges() const {
        return true;
    }
    bool customCanRemoveLastOutEdge(Edge edge) {
        return false;
    }
    void edgeCostIncreasedCallback(Edge edge) {
        if (solution[edge.first] == edge.second) {
            addUnassignedDstNode(edge.second);
        }
    }
    void customRemoveOutEdge(Edge edge) {
        if (solution[edge.first] == edge.second) {
            addUnassignedDstNode(edge.second);
        }
    }
    bool customLockOutEdge(Edge edge) {
        PartialRoute newPartialRoute = PartialRoute(edge.first, edge.second, 1);
        bool connectedRoute = false;
        int i = 0;
        for (; i < partialRoutes.size(); ++i) {
            if (partialRoutes[i].end == newPartialRoute.start) {
                partialRoutes[i].end = newPartialRoute.end;
                partialRoutes[i].size += newPartialRoute.size;
                newPartialRoute = partialRoutes[i];
                connectedRoute = true;
                break;
            }
        }
        if (!connectedRoute) {
            partialRoutes.push_back(newPartialRoute);
            i = partialRoutes.size() - 1;
        }
        for (int j = 0; j < partialRoutes.size(); ++j) {
            if (partialRoutes[j].start == newPartialRoute.end) {
                partialRoutes[j].start = newPartialRoute.start;
                partialRoutes[j].size += newPartialRoute.size;
                newPartialRoute = partialRoutes[j];
                partialRoutes.erase(i);
                break;
            }
        }
        if (!isComplete() && !removeOutEdgeIfExists({ newPartialRoute.end, newPartialRoute.start })) {
            return false;
        }
        return true;
    }

    bool isComplete() {
        return partialRoutes.size() == 1 && partialRoutes[0].size == size() - 1;
    }
    
    Edge findPivotEdge() {
        /*
            Find most promising edge - that is one which removal from solution 
            would at a first glance lead to the biggest increase in cost.
            We do this by summing 2nd lowest element in row and column of each edge
        */
        thread_local XorShift64 rng;
        auto findMin = [this](AdjList& adj, NodeType i, NodeType ignoreNode, bool rev) -> EdgeCostType {
            EdgeCostType min = Inf;
            for (int k = 0; k < adj[i].size(); ++k) {
                auto j = adj[i][k];
                if (j == ignoreNode)
                    continue;
                min = std::min(min, rev ? valueAt(j, i) : valueAt(i, j));
            }
            return min;
        };
        Edge pivot = NullEdge;
        double bestIncrease = -1;
        for (int i = 0; i < problemSize - 1; ++i) {
            auto j = solution[i];
            if (adjList[i].size() <= 1 || revAdjList[j].size() <= 1)
                continue;
            double increase = findMin(adjList, i, j, false) + findMin(revAdjList, j, i, true);
            increase *= 0.5 + (rng() % 100) / 100.0;
            if (increase > bestIncrease) {
                bestIncrease = increase;
                pivot = { i, j };
            }
        }

        return pivot;
    }

    struct HungarianMethodData {
        std::vector<EdgeCostType> minFrom;
        std::vector<NodeType> prv;
        std::vector<unsigned char> inZ;
        std::vector<NodeType> nodesInZ;

        void initWithSize(int size) {
            if (minFrom.size() < size + 1)  minFrom.resize(size);
            if (prv.size() < size + 1)      prv.resize(size);
            if (inZ.size() < size + 1)      inZ.resize(size);
            if (nodesInZ.size() < size + 1) nodesInZ.resize(size);
        }
    };
    void hungarianMethod() {
        const int N = size();

        thread_local HungarianMethodData data;
        data.initWithSize(problemSize + 1);
        auto minFrom = Array<int>{ data.minFrom.data() };
        auto prv = Array<NodeType>{ data.prv.data() };
        auto inZ = Array<bool>{ (bool*)data.inZ.data() };
        auto nodesInZ = ArrayWithSize<NodeType>{ data.nodesInZ.data(), 0 };
        solution[N] = NullNode;

        for (auto curDstNode : unassignedDstNodes) {
            NodeType curSrcNode = N;
            solution[curSrcNode] = curDstNode;
            revSolution[curDstNode] = curSrcNode;
        
            std::fill(minFrom.data, minFrom.data + (N + 1), Inf);
            std::fill(prv.data, prv.data + (N + 1), -1);
            std::fill(inZ.data, inZ.data + (N + 1), false);
            nodesInZ.clear();

            while (solution[curSrcNode] != NullNode) {
                inZ[curSrcNode] = true;
                nodesInZ.push_back(curSrcNode);
                auto dst = solution[curSrcNode];
                auto delta = Inf;
                auto nextSrc = NullNode;

                auto& revAdj = revAdjList;
                for (int k = 0; k < revAdj[dst].size(); ++k) {
                    auto src = revAdj[dst][k];
                    if (!inZ[src]) {
                        auto cost = valueAt(src, dst);
                        if (cost < minFrom[src]) {
                            minFrom[src] = cost;
                            prv[src] = curSrcNode;
                        }
                    }
                }
                for (int src = 0; src < N; ++src) {
                    if (!inZ[src]) {
                        if (minFrom[src] < delta) {
                            delta = minFrom[src];
                            nextSrc = src;
                        }
                    }
                }
                if (delta > ignoredValue) {
                    cost = Inf;
                    return;
                }
                for (int src = 0; src <= N; ++src) {
                    if (!inZ[src]) {
                        minFrom[src] -= delta;
                    }
                }
                for (auto src : nodesInZ) {
                    outReductions[src] -= delta;
                    inReductions[solution[src]] += delta;
                }
                cost += delta;
                curSrcNode = nextSrc;
            }
            for (NodeType src; curSrcNode != N; curSrcNode = src) {
                src = prv[curSrcNode];
                solution[curSrcNode] = solution[src];
                revSolution[solution[src]] = curSrcNode;
            }
        }
        unassignedDstNodes.clear();
        std::fill(unassignedDstNodesSet.data, unassignedDstNodesSet.data + size(), false);
    }

    bool solveRelaxationAndCheckIfStillViable(SolutionConfig& config) {
        removeTooExpensiveEdges(config.limit());
        shrinkToFit();
        hungarianMethod();

        if (cost > config.limit())
            return false;

        arboCost = minArborescence(*this);

        return true;
    }

    void saveSolution(SolutionConfig& config) {
        ::saveSolution(config, std::vector<NodeType>(solution.data, solution.data + size()));
    }
};

void findSolutionsAssignment(SolutionConfig& config) {
    findSolutionsBranchAndBound<AssignmentSolution>(config);
}
