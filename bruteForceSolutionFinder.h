#pragma once
#include "branchAndBoundSolutionFinder.h"

struct MinInNode {
    NodeType prev;
    NodeType src;
    uint16_t cost;
};

template<int Size> struct alignas(64) BruteForceSolutionData {
    std::vector<std::vector<NodeType>>* adjList;
    std::vector<std::vector<NodeType>>* revAdjList;
    int currentNode;
    int previousLastNonRingNode;
    int lastNonRingNode;
    int cost;
    int minInSum;
    FastStackBitset visited;
    FastStackBitset srcNotToConsider;
    FastStackBitset prevNotToConsider;
    FastStackBitset isRingCp;
    FixedStackVector<MinInNode, Size> minInNodes;
    FixedStackVector<CompressedEdgeNoPrev, Size> edges;
    FixedStackVector<NodeType, Size> prevToConsider;
    FixedStackVector<NodeType, Size> notVisitedNodes;

    int getCost() const {
        return cost + minInSum;
    }
};
template<int Size> struct RawBruteForceSolutionData {
    alignas(BruteForceSolutionData<Size>) char data[sizeof(BruteForceSolutionData<Size>)];
    BruteForceSolutionData<Size>& asData() {
        return *(BruteForceSolutionData<Size>*)data;
    }
    const BruteForceSolutionData<Size>& asData() const {
        return *(BruteForceSolutionData<Size>*)data;
    }
    int getCost() const {
        return asData().getCost();
    }
};

template<int Size> void findSolutionsBruteForceLoop(SolutionConfig& config, RawBruteForceSolutionData<Size>& rawSolutionData, PriorityMultiQueue<RawBruteForceSolutionData<Size>>& dataQueue, PreallocatedVector<RawBruteForceSolutionData<Size>>& backlog) {
    do {
        if (!backlog.empty()) {
            rawSolutionData = std::move(backlog.back());
            backlog.pop_back();
        }
        auto& solutionData = rawSolutionData.asData();

        config.lazyIncrementPartialSolutionCount(1217);
        if (config.stopWorking)
            return;

        auto& [adjListPtr, revAdjListPtr, curNode, previousLastNonRingNode, lastNonRingNode, cost, minInSum, 
            visited, srcNotToConsider, prevNotToConsider, isRingCp, minInNodes, edges, prevToConsider, notVisitedNodes] = solutionData;
        auto& adjList = *adjListPtr;
        auto& revAdjList = *revAdjListPtr;

        minInSum -= minInNodes[curNode].cost;

        if (solutionData.getCost() > config.limit())
            continue;

        if (edges.size() == config.weights.size() - 1) {
            std::vector<CompressedEdge> edgesVec(edges.size());
            for (int i = 0; i < edges.size(); ++i) {
                edgesVec[i].src = edges[i].src;
                edgesVec[i].dst = edges[i].dst;
            }
            saveSolution(config, edgesVec);
            continue;
        }

        auto removeNodeIfPossible = [](FixedStackVector<NodeType, Size>& vec, NodeType node) {
            auto it = std::find(vec.begin(), vec.end(), node);
            if (it != vec.end()) {
                *it = vec.back();
                vec.pop_back();
            }
        };

        visited.set(curNode);
        removeNodeIfPossible(notVisitedNodes, curNode);

        if (previousLastNonRingNode != lastNonRingNode) {
            srcNotToConsider.set(previousLastNonRingNode);
        }
        if (edges.size() > 0 && isRingCp.test(edges.back().src)) {
            srcNotToConsider.set(edges.back().src);
        } else if (edges.size() >= 2 && edges.back().src != edges[edges.size() - 2].dst) {
            srcNotToConsider.set(edges[edges.size() - 2].dst);
        }

        
        if (previousLastNonRingNode != lastNonRingNode) {
            if (edges.size() <= 0 || edges.back().src != previousLastNonRingNode) {
                prevNotToConsider.set(previousLastNonRingNode);
                removeNodeIfPossible(prevToConsider, previousLastNonRingNode);
            }
        }
        if (edges.size() >= 2 && edges[edges.size() - 2].src != lastNonRingNode) {
            prevNotToConsider.set(edges[edges.size() - 2].src);
            removeNodeIfPossible(prevToConsider, edges[edges.size() - 2].src);
        }
        if (edges.size() >= 2 && edges.back().src != edges[edges.size() - 2].dst) {
            prevNotToConsider.set(edges[edges.size() - 2].dst);
            removeNodeIfPossible(prevToConsider, edges[edges.size() - 2].dst);
        }

        for (auto dst : notVisitedNodes) {
            auto& min = minInNodes[dst];
            bool needToRecalculate = prevNotToConsider.test(min.prev) || srcNotToConsider.test(min.src);
            if (!needToRecalculate)
                continue;
            minInSum -= min.cost;
            min.src = -1;
            min.cost = config.limit();
            auto& condWeightsDst = config.condWeights[dst];
            for (int src : revAdjList[dst]) {
                auto& condWeightsDstSrc = condWeightsDst[src];
                if (srcNotToConsider.test(src))
                    continue;
                for (auto prev : prevToConsider) {
                    auto value = condWeightsDstSrc[prev];
                    if (value < min.cost) {
                        min.cost = value;
                        min.src = src;
                        min.prev = prev;
                    }
                }
            }
            minInSum += min.cost;
            if (cost + minInSum > config.limit())
                break;
        }

        if (solutionData.getCost() > config.limit())
            continue;

        FixedStackVector<std::pair<NodeType, bool>, 256> nextNodes;
        for (int i = 0; i < adjList[curNode].size(); ++i) {
            int nextNode = adjList[curNode][i];
            if (visited.test(nextNode) || config.condWeights[nextNode][curNode][edges.size() >= 1 ? edges[edges.size() - 1].src : 0] >= config.ignoredValue)
                continue;
            nextNodes.emplace_back(std::pair{ nextNode, false });
        }
        if (curNode != lastNonRingNode) {
            for (int i = 0; i < adjList[lastNonRingNode].size(); ++i) {
                int nextNode = adjList[lastNonRingNode][i];
                if (visited.test(nextNode) || config.condWeights.withRespawn(nextNode, lastNonRingNode) >= config.ignoredValue)
                    continue;
                nextNodes.emplace_back(std::pair{ nextNode, true });
            }
        }
        std::reverse(nextNodes.begin(), nextNodes.end());
        for (auto [nextNode, isRespawn] : nextNodes) {
            auto srcNode = isRespawn ? lastNonRingNode : curNode;
            edges.emplace_back(CompressedEdgeNoPrev{ NodeType(srcNode), NodeType(nextNode) });
            auto diff = isRespawn ? config.condWeights.withRespawn(nextNode, srcNode) : config.condWeights[nextNode][srcNode][edges.size() >= 2 ? edges[edges.size() - 2].src : 0];
            int newLastNonRingNode = isRingCp.test(nextNode) ? lastNonRingNode : nextNode;
            auto queueIsAlmostFull = dataQueue.isAlmostFull();
            if (dataQueue.isAlmostFull()) {
                auto& newData = backlog.emplace_back(std::move(rawSolutionData)).asData();
                newData.currentNode = nextNode;
                newData.previousLastNonRingNode = lastNonRingNode;
                newData.lastNonRingNode = newLastNonRingNode;
                newData.cost = cost + diff;
            } else {
                auto newRawData = rawSolutionData;
                auto& newData = newRawData.asData();
                newData.currentNode = nextNode;
                newData.previousLastNonRingNode = lastNonRingNode;
                newData.lastNonRingNode = newLastNonRingNode;
                newData.cost = cost + diff;
                dataQueue.push(std::move(newRawData));
            }
            edges.pop_back();
        }
    } while (!backlog.empty());
}

template<int Size> void findSolutionsBruteForce(SolutionConfig& config) {
    RawBruteForceSolutionData<Size> rawSolutionData;
    BruteForceSolutionData<Size>& solutionData = rawSolutionData.asData();

    solutionData.currentNode = 0;
    solutionData.previousLastNonRingNode = 0;
    solutionData.lastNonRingNode = 0;
    solutionData.cost = 0;
    solutionData.minInSum = 0;
    solutionData.visited = FastStackBitset();
    solutionData.srcNotToConsider = FastStackBitset();
    solutionData.prevNotToConsider = FastStackBitset();
    solutionData.isRingCp = FastStackBitset();
    solutionData.minInNodes = FixedStackVector<MinInNode, Size>();
    solutionData.edges = FixedStackVector<CompressedEdgeNoPrev, Size>();
    solutionData.prevToConsider = FixedStackVector<NodeType, Size>();
    solutionData.notVisitedNodes = FixedStackVector<NodeType, Size>();
    auto& edges = solutionData.edges;
    auto& visited = solutionData.visited;

    std::vector<std::vector<NodeType>> adjList(config.weights.size());
    for (int i = 0; i < adjList.size(); ++i) {
        for (int j = 0; j < adjList.size(); ++j) {
            if (config.weights[j][i] < config.ignoredValue) {
                adjList[i].push_back(j);
            }
        }
        std::sort(adjList[i].begin(), adjList[i].end(), [&](int a, int b) { return config.weights[a][i] < config.weights[b][i]; });
    }
    std::vector<std::vector<NodeType>> revAdjList(config.weights.size());
    for (int i = 0; i < adjList.size(); ++i) {
        for (int j : adjList[i]) {
            revAdjList[j].push_back(i);
        }
    }
    solutionData.adjList = &adjList;
    solutionData.revAdjList = &revAdjList;

    auto& minInNodes = solutionData.minInNodes;
    minInNodes.size_ = config.weights.size();
    for (int dst = 0; dst < revAdjList.size(); ++dst) {
        minInNodes[dst].src = -1;
        minInNodes[dst].cost = config.limit();
        for (int src : revAdjList[dst]) {
            for (int prev = 0; prev < config.condWeights[dst][src].size(); ++prev) {
                auto value = config.condWeights[dst][src][prev];
                if (value < minInNodes[dst].cost) {
                    minInNodes[dst].cost = value;
                    minInNodes[dst].src = src;
                    minInNodes[dst].prev = prev;
                }
            }
        }
    }
    solutionData.minInSum = 0;
    for (auto& min : minInNodes) {
        solutionData.minInSum += min.cost;
    }

    for (int i = 0; i < config.condWeights[0][0].size(); ++i) {
        solutionData.prevToConsider.emplace_back(i);
    }
    for (int i = 1; i < config.weights.size(); ++i) {
        solutionData.notVisitedNodes.emplace_back(i);
    }
    visited.set(0);
    for (auto ringCp : config.ringCps) {
        solutionData.isRingCp.set(ringCp);
    }

    findSolutionsBfs<RawBruteForceSolutionData<Size>>(config, rawSolutionData, sizeof(RawBruteForceSolutionData<Size>), findSolutionsBruteForceLoop<Size>);
}

void findSolutionsBruteForce(SolutionConfig& config) {
    if (config.nodeCount() <= 38) {
        findSolutionsBruteForce<40>(config);
    } else if (config.nodeCount() <= 102) {
        findSolutionsBruteForce<104>(config);
    } else {
        findSolutionsBruteForce<256>(config);
    }
}

