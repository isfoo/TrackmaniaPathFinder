#pragma once

#include <vector>
#include <array>
#include "branchAndBoundSolutionFinder.h"

struct DisjointSetUnion {
    std::array<int, 256> parent;
    std::array<int, 256> size;
    DisjointSetUnion(int n) {
        for (int i = 0; i < n; ++i) {
            parent[i] = i;
            size[i] = 1;
        }
    }
    int getRoot(int a) {
        while (parent[a] != a) {
            auto p = parent[a];
            parent[a] = parent[p];
            a = p;
        }
        return a;
    }
    int joinAndReturnRoot(int a, int b) {
        auto aRoot = getRoot(a);
        auto bRoot = getRoot(b);
        if (aRoot == bRoot)
            return -1;
        if (size[bRoot] > size[aRoot])
            std::swap(aRoot, bRoot);
        size[aRoot] += size[bRoot];
        parent[bRoot] = aRoot;
        return aRoot;
    }
};

struct MinSpanningArborescence {
    static constexpr int Inf = 1e9;

    #pragma pack(push, 1)
    struct MinEdge {
        NodeType src;
        NodeType dst;
    };
    struct Elem {
        int weight;
        MinEdge edge;
    };
    #pragma pack(pop)

    struct Data {
        std::vector<Elem> matrix;
        std::vector<Elem> adjInitialCopy;
        std::vector<int> lastProcessedId;
        std::vector<int> forest;
        std::vector<int> nodesToProcess;
        std::vector<MinEdge> addedEdges;
        std::vector<int> rootNodes;

        void initWithSize(int size) {
            if (lastProcessedId.size() < size)    lastProcessedId.resize(size);
            if (forest.size() < 2 * size)         forest.resize(2 * size);
            if (nodesToProcess.size() < 2 * size) nodesToProcess.resize(2 * size);
            if (addedEdges.size() < 2 * size)     addedEdges.resize(2 * size);
            if (rootNodes.size() < size)          rootNodes.resize(size);
            if (matrix.size() < size * size)      matrix.resize(size * size);
            if (adjInitialCopy.size() < size * size) {
                adjInitialCopy.resize(size * size);
                for (int i = 0; i < size; ++i) {
                    for (int j = 0; j < size; ++j) {
                        adjInitialCopy[i * size + j] = { Inf, {NodeType(j), NodeType(i)} };
                    }
                }
            }
        }
    };

    VectorView2d<Elem> matrix;
    Array<int> lastProcessedId;
    Array<int> forest;
    ArrayWithSize<int> nodesToProcess;
    ArrayWithSize<MinEdge> addedEdges;
    ArrayWithSize<int> rootNodes;

    DisjointSetUnion weaklyConnectedComponents;
    DisjointSetUnion mergedComponents;

    int root = 0;

    template<typename T> MinSpanningArborescence(BranchAndBoundSolution<T>& data, MinSpanningArborescence::Data& arboData, bool reverse) : weaklyConnectedComponents(data.size()), mergedComponents(data.size()) {
        arboData.initWithSize(data.size());
        matrix = VectorView2d(arboData.matrix.data(), data.size());
        lastProcessedId = Array(arboData.lastProcessedId.data());
        forest = Array(arboData.forest.data());
        nodesToProcess = ArrayWithSize(arboData.nodesToProcess.data(), 0);
        addedEdges = ArrayWithSize(arboData.addedEdges.data(), 0);
        rootNodes = ArrayWithSize(arboData.rootNodes.data(), 0);

        auto& adjList = reverse ? data.adjList : data.revAdjList;
        root = reverse ? matrix.size - 1 : 0;
        std::memcpy(matrix.data, arboData.adjInitialCopy.data(), matrix.size * matrix.size * sizeof(*matrix.data));
        for (int dst = 0; dst < adjList.size(); ++dst) {
            auto a = matrix[dst];
            for (int i = 0; i < adjList[dst].size(); ++i) {
                int src = adjList[dst][i];
                if (reverse) {
                    a[src].weight = data.valueAt(dst, src);
                } else {
                    a[src].weight = data.valueAt(src, dst);
                }
            }
        }
    }

    int nodeCount() {
        return matrix.size;
    }
    Elem getMinInEdge(int dstNode) {
        Elem minEdge = { Inf, {-1, -1} };
        auto dstNodeRow = matrix[dstNode];
        for (int i = 0; i < rootNodes.size(); ++i) {
            auto srcNode = rootNodes[i];
            if (srcNode != dstNode) {
                if (dstNodeRow[srcNode].weight < minEdge.weight) {
                    minEdge = dstNodeRow[srcNode];
                }
            }
        }
        return minEdge;
    }
    void updateInEdgeWeights(int dstNode, int w) {
        auto dstNodeRow = matrix[dstNode];
        for (int i = 0; i < rootNodes.size(); ++i) {
            auto srcNode = rootNodes[i];
            dstNodeRow[srcNode].weight -= w;
        }
    }

    void mergeEdges(int oldRoot, int newRoot) {
        auto oldRow = matrix[oldRoot];
        auto newRow = matrix[newRoot];
        for (int src = 0; src < nodeCount(); ++src) {
            if (oldRow[src].weight < newRow[src].weight) {
                newRow[src] = oldRow[src];
            }
            auto row = matrix[src];
            if (row[oldRoot].weight < row[newRoot].weight) {
                row[newRoot] = row[oldRoot];
            }
        }
    }

    int nextNodeInCycle(int curNode) {
        return mergedComponents.getRoot(addedEdges[lastProcessedId[curNode]].src);
    }

    int calculate() {
        for (int node = 0; node < nodeCount(); ++node) {
            if (node != root) {
                nodesToProcess.push_back(node);
            }
            rootNodes.push_back(node);
        }

        int result = 0;
        for (int i = 0; i < nodesToProcess.size(); ++i) {
            auto node = nodesToProcess[i];
            lastProcessedId[node] = i;

            const auto [minWeight, minEdge] = getMinInEdge(node);
            if (minWeight == Inf)
                return Inf;
            addedEdges.push_back(minEdge);
            forest[i] = i;
            result += minWeight;
            updateInEdgeWeights(node, minWeight);

            if (weaklyConnectedComponents.joinAndReturnRoot(minEdge.src, minEdge.dst) != -1)
                continue;

            int merged = node;
            for (int cur = nextNodeInCycle(node); cur != merged; cur = nextNodeInCycle(cur)) {
                auto root = mergedComponents.joinAndReturnRoot(cur, merged);
                auto nonRoot = merged + cur - root;
                std::swap(rootNodes[std::distance(rootNodes.data, std::find(rootNodes.data, rootNodes.data + rootNodes.size(), nonRoot))], rootNodes[rootNodes.size() - 1]);
                rootNodes.pop_back();
                mergeEdges(nonRoot, root);
                merged = root;
                forest[lastProcessedId[cur]] = nodesToProcess.size();
            }
            forest[i] = nodesToProcess.size();
            nodesToProcess.push_back(merged);
        }

        return result;
    }

    void getResultEdges(ArrayWithSize<MinSpanningArborescence::MinEdge>& outSolutionEdges) {
        FastStackBitset removed;
        for (int r = addedEdges.size() - 1; r >= 0; --r) {
            if (removed.test(r))
                continue;
            outSolutionEdges.push_back(addedEdges[r]);
            int leaf = addedEdges[r].dst;
            auto leafEdgePos = leaf - (root < leaf);
            removed.set(r);
            for (int i = leafEdgePos; i != r; i = forest[i])
                removed.set(i);
        }
    }
};

template<typename T> int minArborescence(BranchAndBoundSolution<T>& branchAndBouncSolution, bool reverse=false, ArrayWithSize<MinSpanningArborescence::MinEdge>* outSolutionEdges=nullptr) {
    thread_local MinSpanningArborescence::Data arboData;
    MinSpanningArborescence alg(branchAndBouncSolution, arboData, reverse);
    auto cost = alg.calculate();
    if (outSolutionEdges) {
        outSolutionEdges->clear();
        alg.getResultEdges(*outSolutionEdges);
    }
    return cost;
}
