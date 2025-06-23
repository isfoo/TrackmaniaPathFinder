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

    auto newSolution = BestSolution(solution.first, sortedSolution, solutionWithRepeats, solutionConnections, solution.second);
    config.solutionsVec.push_back_not_thread_safe(newSolution);
    writeSolutionToFile(config.appendFileName, solution.first, solution.second, config.repeatNodeMatrix, config.useRespawnMatrix);
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

std::vector<std::vector<int>> createAtspMatrixFromInput(const std::vector<std::vector<int>>& weights) {
    auto copy = weights;
    copy[0].back() = 0;
    return copy;
}


enum Direction { Out, In };
constexpr NodeType NullNode = NodeType(-1);
constexpr Edge NullEdge = { NullNode, NullNode };
constexpr EdgeCostType Inf = 100'000'000;

template<typename T> struct Array {
    T* data;
    T& operator[](int i) { return data[i]; }
};

template<typename T> struct ArrayWithSize {
    T* data;
    int size_ = 0;
    void push_back(const T& val) { data[size_++] = val; }
    void erase(int i)            { data[i] = data[--size_]; }
    T& operator[](int i) { return data[i]; }
    void clear()     { size_ = 0; }
    int size() const { return size_; }
    T* begin()       { return data; }
    T* end()         { return data + size_; }
};

struct AdjList2 {
    #pragma pack(push, 1)
    struct ArrayViewData {
        NodeType size_;
        NodeType data_[1];
    };
    #pragma pack(pop)

    struct ArrayView {
        ArrayViewData* ptr_;

        ArrayView(ArrayViewData* ptr_) : ptr_(ptr_) {}
        int size() {
            return ptr_->size_;
        }
        void setSize(int newSize) {
            ptr_->size_ = newSize;
        }
        NodeType& operator[](int i) {
            return ptr_->data_[i];
        }
        void push_back(NodeType node) {
            ptr_->data_[ptr_->size_++] = node;
        }
        void erase(int i) {
            ptr_->size_ -= 1;
            ptr_->data_[i] = ptr_->data_[ptr_->size_];
        }
    };
    
    ArrayWithSize<NodeType> data_;
    int stride_ = 0;

    int calculateMinimumStride() {
        int minStride = 0;
        for (int i = 0; i < size(); ++i) {
            minStride = std::max<int>(minStride, (*this)[i].ptr_->size_);
        }
        return minStride + 1; // +1 for size
    }

    void shrinkToFit() {
        auto newStride = calculateMinimumStride();
        if (newStride < stride_ - 5) { // only if significant change in stride
            for (int i = 1; i < size(); ++i) {
                ArrayView newRow((ArrayViewData*)(data_.data + newStride * i));
                std::memcpy(newRow.ptr_->data_ - 1, (*this)[i].ptr_->data_ - 1, ((*this)[i].ptr_->size_ + 1) * sizeof(NodeType));
            }
            stride_ = newStride;
        }
    }

    void init(const std::vector<std::vector<EdgeCostType>>& costMatrix, EdgeCostType ignoredValue, bool transpose = false) {
        data_.size_ = int(costMatrix.size());
        stride_ = int(costMatrix.size()) + 1;
        for (int i = 0; i < costMatrix.size(); ++i) {
            (*this)[i].ptr_->size_ = 0;
        }
        for (int i = 0; i < costMatrix.size(); ++i) {
            for (int j = 0; j < costMatrix.size(); ++j) {
                if (i == j)
                    continue;
                if (costMatrix[j][i] < ignoredValue) {
                    if (transpose) {
                        (*this)[j].push_back(i);
                    } else {
                        (*this)[i].push_back(j);
                    }
                }
            }
        }
    }

    ArrayView operator[](int i) {
        return ArrayView((ArrayViewData*)(data_.data + stride_ * i));
    }
    int size() const {
        return int(data_.size());
    }
};


struct ArrayOfPoolAllocators {
    struct AllocatorWithSize {
        PoolAllocator allocator;
        int size;
        AllocatorWithSize(int elementSize, int elementCount) : allocator(elementSize, elementCount), size(elementSize) {}
    };
    std::vector<AllocatorWithSize> allocators;

    ArrayOfPoolAllocators(int elementsPerAllocation, std::vector<int> sizes) {
        std::sort(sizes.begin(), sizes.end());
        for (auto size : sizes) {
            allocators.emplace_back(size, elementsPerAllocation);
        }
    }
    void* allocate(int size) {
        return getAllocator(size).allocate();
    }
    void deallocate(void* ptr, int size) {
        getAllocator(size).deallocate(ptr);
    }
private:
    PoolAllocator& getAllocator(int size) {
        int i = 0;
        while (allocators[i].size < size)
            i += 1;
        return allocators[i].allocator;
    }
};

struct MemoryPool {
    uint8_t* curPtr = nullptr;
    uint8_t* memory = nullptr;
    int memorySize = 0;
    ArrayOfPoolAllocators& allocators;

    MemoryPool(ArrayOfPoolAllocators& allocators) : allocators(allocators) {}
    ~MemoryPool() {
        deallocate();
    }
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&& other) : allocators(other.allocators) {
        std::swap(curPtr, other.curPtr);
        std::swap(memory, other.memory);
        std::swap(memorySize, other.memorySize);
    }
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

    void allocate(int size) {
        deallocate();
        curPtr = memory = (uint8_t*)allocators.allocate(size);
        memorySize = size;
    }
    void deallocate() {
        if (memory)
            allocators.deallocate(memory, memorySize);
        curPtr = nullptr;
        memory = nullptr;
        memorySize = 0;
    }

    template<typename T> T* assignNextMemory(int size) {
        auto result = curPtr;
        curPtr += size * sizeof(T);
        return (T*)result;
    }
};

struct AssignmentSolution {
    struct PartialRoute {
        PartialRoute(NodeType start, NodeType end, int size) : start(start), end(end), size(size) {}
        NodeType start;
        NodeType end;
        int size;
    };

    /*
        This struct holds all the memory needed to work with given possible candidate solution.
        One of the bottlenecks is allocation and copying of this data, so the below layout was made
        to optimize that.
        Allocations are done using PoolAllocators with pre-selecated range of sizes and whole struct uses
        single allocated memory area, so allocation is almost as free as if it was stack allocation.
        Layout is divided for initialized and unitialized data, so only the required data is copied
        and on top of that all is done with small number of calls to memcpy to initialize everything which is very fast.
        It's kind of ugly in that you have to manually remember to change multiple places in code to add/remove
        member of this struct. Maybe could make some nice layer of abstraction for that.
    */

    // memory pool
    MemoryPool memoryPool;

    // loose variables - normal assignment copy
    const std::vector<std::vector<EdgeCostType>>* costMatrix;
    const std::vector<std::vector<std::vector<EdgeCostType>>>* costMatrixEx;
    int problemSize = 0;
    EdgeCostType ignoredValue;
    EdgeCostType cost = 0;
    bool useExtendedMatrix;

    // initialized data (do full copy)
    Array<EdgeCostType> inReductions;
    Array<EdgeCostType> outReductions;
    Array<NodeType> solution;
    Array<NodeType> revSolution;
    Array<NodeType> lockedOutEdges;
    Array<NodeType> lockedInEdges;
    ArrayWithSize<NodeType> unassignedDstNodes;
    Array<bool> unassignedDstNodesSet;
    ArrayWithSize<PartialRoute> partialRoutes;
    Array<uint16_t> costIncreases;

    // uninitialized data (don't copy, just assign memory)
    Array<EdgeCostType> minFrom;
    Array<NodeType> prv;
    Array<bool> inZ;
    ArrayWithSize<NodeType> nodesInZ;

    // special initialized data (do full copy, but the size is not constant)
    AdjList2 adjList;
    AdjList2 revAdjList;

    static int InitializedSectionSize(int problemSize, bool useExtendedMatrix) {
        int size = 0;
        size += (problemSize + 1) * sizeof(EdgeCostType); // inReductions
        size += (problemSize + 1) * sizeof(EdgeCostType); // outReductions
        size += (problemSize + 1) * sizeof(NodeType); // solution
        size += (problemSize + 1) * sizeof(NodeType); // revSolution
        size += (problemSize + 1) * sizeof(NodeType); // lockedOutEdges
        size += (problemSize + 1) * sizeof(NodeType); // lockedInEdges
        size += (problemSize + 1) * sizeof(NodeType); // unassignedDstNodes
        size += (problemSize + 1) * sizeof(bool); // unassignedDstNodesSet
        size += (problemSize + 1) * sizeof(PartialRoute); // partialRoutes
        if (useExtendedMatrix) {
            size += problemSize * problemSize * sizeof(uint16_t); // costIncreases
        }
        return size;
    }
    static int UninitializedSectionSize(int problemSize) {
        int size = 0;
        size += (problemSize + 1) * sizeof(EdgeCostType); // minFrom
        size += (problemSize + 1) * sizeof(NodeType); // prv
        size += (problemSize + 1) * sizeof(bool); // inZ
        size += (problemSize + 1) * sizeof(NodeType); // nodesInZ
        return size;
    }
    static int RequiredAllocationSize(int problemSize, int strideSize, bool useExtendedMatrix) {
        int requiredSize = InitializedSectionSize(problemSize, useExtendedMatrix) + UninitializedSectionSize(problemSize);
        requiredSize += strideSize * problemSize * sizeof(NodeType); // adjList
        requiredSize += strideSize * problemSize * sizeof(NodeType); // revAdjList
        return requiredSize;
    }
    static int RequiredAllocationSize(int problemSize, bool useExtendedMatrix) {
        return RequiredAllocationSize(problemSize, problemSize + 1, useExtendedMatrix);
    }
    
    int minimumAllocationSize() const {
        int size = InitializedSectionSize(problemSize, useExtendedMatrix) + UninitializedSectionSize(problemSize);
        size += adjList.stride_* adjList.size() * sizeof(NodeType);
        size += revAdjList.stride_* revAdjList.size() * sizeof(NodeType);
        return size;
    }
    
    void assignLooseVariables(const AssignmentSolution& other) {
        costMatrix = other.costMatrix;
        costMatrixEx = other.costMatrixEx;
        problemSize = other.problemSize;
        ignoredValue = other.ignoredValue;
        useExtendedMatrix = other.useExtendedMatrix;
        cost = other.cost;
    }
    void assignNonLooseVariables(const AssignmentSolution& other) {
        inReductions = other.inReductions;
        outReductions = other.outReductions;
        solution = other.solution;
        revSolution = other.revSolution;
        lockedOutEdges = other.lockedOutEdges;
        lockedInEdges = other.lockedInEdges;
        unassignedDstNodes = other.unassignedDstNodes;
        unassignedDstNodesSet = other.unassignedDstNodesSet;
        partialRoutes = other.partialRoutes;
        costIncreases = other.costIncreases;
        minFrom = other.minFrom;
        prv = other.prv;
        inZ = other.inZ;
        nodesInZ = other.nodesInZ;
        adjList = other.adjList;
        revAdjList = other.revAdjList;
    }
    template<typename ArrayWithData> void assignMemory(ArrayWithData& array) {
        array.data = memoryPool.assignNextMemory<std::remove_reference_t<decltype(array.data[0])>>(problemSize + 1);
    }
    void assignMemory(int adjListStride, int revAdjListStride) {
        assignMemory(inReductions);
        assignMemory(outReductions);
        assignMemory(solution);
        assignMemory(revSolution);
        assignMemory(lockedOutEdges);
        assignMemory(lockedInEdges);
        assignMemory(unassignedDstNodes);
        assignMemory(unassignedDstNodesSet);
        assignMemory(partialRoutes);
        if (useExtendedMatrix) {
            costIncreases.data = memoryPool.assignNextMemory<uint16_t>(problemSize * problemSize);
        }
        assignMemory(minFrom);
        assignMemory(prv);
        assignMemory(inZ);
        assignMemory(nodesInZ);
        adjList.data_.data = memoryPool.assignNextMemory<NodeType>(adjListStride * problemSize);
        revAdjList.data_.data = memoryPool.assignNextMemory<NodeType>(revAdjListStride * problemSize);
    }
    void copyInitializedDataSection(const AssignmentSolution& other) {
        std::memcpy(memoryPool.memory, other.memoryPool.memory, InitializedSectionSize(other.problemSize, other.useExtendedMatrix));
        unassignedDstNodes.size_ = other.unassignedDstNodes.size();
        partialRoutes.size_ = other.partialRoutes.size();
    }
    static void copyAdjList(AdjList2& dst, const AdjList2& src) {
        dst.data_.size_ = src.data_.size_;
        dst.stride_ = src.stride_;
        std::memcpy(dst.data_.data, src.data_.data, src.stride_ * src.data_.size_ * sizeof(NodeType));
    }

    AssignmentSolution(AssignmentSolution&& other) : memoryPool(std::move(other.memoryPool)) {
        assignLooseVariables(other);
        assignNonLooseVariables(other);
    }
    AssignmentSolution(const AssignmentSolution& other) : memoryPool(other.memoryPool.allocators) {
        memoryPool.allocate(other.minimumAllocationSize());
        assignLooseVariables(other);
        assignMemory(other.adjList.stride_, other.revAdjList.stride_);
        copyInitializedDataSection(other);
        copyAdjList(adjList, other.adjList);
        copyAdjList(revAdjList, other.revAdjList);
    }
    AssignmentSolution& operator=(AssignmentSolution&& other) {
        std::swap(memoryPool.curPtr, other.memoryPool.curPtr);
        std::swap(memoryPool.memory, other.memoryPool.memory);
        std::swap(memoryPool.memorySize, other.memoryPool.memorySize);
        assignLooseVariables(other);
        assignNonLooseVariables(other);
        return *this;
    }
    AssignmentSolution& operator=(const AssignmentSolution&) = delete;

    AssignmentSolution(ArrayOfPoolAllocators& allocators, const std::vector<std::vector<EdgeCostType>>* costMatrix, 
        const std::vector<std::vector<std::vector<EdgeCostType>>>* costMatrixEx, EdgeCostType ignoredValue, bool useExtendedMatrix) :
        memoryPool(allocators), costMatrix(costMatrix), costMatrixEx(costMatrixEx), ignoredValue(ignoredValue), useExtendedMatrix(useExtendedMatrix)
    {
        problemSize = int(costMatrix->size());
        memoryPool.allocate(RequiredAllocationSize(problemSize, useExtendedMatrix));
        assignMemory(problemSize + 1, problemSize + 1);

        std::fill(inReductions.data, inReductions.data + problemSize + 1, 0);
        std::fill(outReductions.data, outReductions.data + problemSize + 1, 0);
        std::fill(solution.data, solution.data + problemSize + 1, -1);
        std::fill(revSolution.data, revSolution.data + problemSize + 1, -1);
        std::fill(lockedOutEdges.data, lockedOutEdges.data + problemSize + 1, -1);
        std::fill(lockedInEdges.data, lockedInEdges.data + problemSize + 1, -1);
        std::fill(unassignedDstNodesSet.data, unassignedDstNodesSet.data + problemSize + 1, true);
        std::iota(unassignedDstNodes.data, unassignedDstNodes.data + problemSize, 0);
        if (useExtendedMatrix) {
            std::fill(costIncreases.data, costIncreases.data + problemSize * problemSize, 0);
        }
        unassignedDstNodes.size_ = problemSize;
        partialRoutes.size_ = 0;

        adjList.init(*costMatrix, ignoredValue);
        revAdjList.init(*costMatrix, ignoredValue, true);

        auto lockSingleEdges = [this](Direction d, AdjList2& adj, NodeType startNode, NodeType endNode) {
            for (int i = startNode; i <= endNode; ++i) {
                if (adj[i].size() == 1) {
                    lockEdge(d, { i, adj[i][0] });
                }
            }
        };
        lockSingleEdges(Out, adjList, 0, size() - 2);
        lockSingleEdges(In, revAdjList, 1, size() - 1);

        shrinkToFit();
    }

    void shrinkToFit() {
        adjList.shrinkToFit();
        revAdjList.shrinkToFit();
    }

    bool isComplete() {
        return partialRoutes.size() == 1 && partialRoutes[0].size == size() - 1;
    }

    void addUnassignedDstNode(NodeType node) {
        if (!unassignedDstNodesSet[node]) {
            unassignedDstNodesSet[node] = true;
            unassignedDstNodes.push_back(node);
            solution[revSolution[node]] = NullNode;
        }
    }

    bool lockOutEdge(Edge edge) {
        /*
            Locking edge A -> B means:
            - removing all other output edges (A -> X, where X != B)
            - removing all other input edges (X -> B, where X != A)
            - checking if this edge connects to some previous locked chain of connections
              and if so extending that connection. Say the final connection is from node C to node D
            - if there is connection from D to C, remove it - it would form a cycle, which would be 
              valid for Assignment problem, but not for ATSP, so better to deal with it as soon as possible
            Note that locking edge and removing edge can cascade. Could be the case that locking edge
            means removing edge such that some node has only 1 IN or OUT node so it's locked and while locking that edge
            we might remove edge such that some node has only 1 IN or OUT node...
        */
        if (lockedOutEdges[edge.first] == edge.second && lockedInEdges[edge.second] == edge.first)
            return true; // edge is either already locked or is the process of getting locked
        if (lockedOutEdges[edge.first] != NullNode || lockedInEdges[edge.second] != NullNode)
            return false; // different edge with the same OUT or IN node is already locked or in the process of getting locked
        lockedOutEdges[edge.first] = edge.second;
        lockedInEdges[edge.second] = edge.first;

        auto removeAllOtherEdges = [this](Direction d, AdjList2& adj, Edge edge) -> bool {
            for (int i = 0; i < adj[edge.first].size(); ++i) {
                if (adj[edge.first][i] == edge.second) {
                    std::swap(adj[edge.first][0], adj[edge.first][i]);
                }
            }
            while (adj[edge.first].size() > 1) {
                if (!removeEdge(d, { edge.first, adj[edge.first][1] })) {
                    return false;
                }
            }
            return true;
        };
        if (!removeAllOtherEdges(Out, adjList, edge))
            return false;
        if (!removeAllOtherEdges(In, revAdjList, { edge.second, edge.first }))
            return false;

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

        if (useExtendedMatrix) {
            for (int m = 0; m < adjList[edge.second].size(); ++m) {
                int i = adjList[edge.second][m];
                auto newCostIncrease = (*costMatrixEx)[i][edge.second][edge.first] - (*costMatrix)[i][edge.second];
                if (newCostIncrease > costIncreases[i * size() + edge.second]) {
                    costIncreases[i * size() + edge.second] = newCostIncrease;
                    addUnassignedDstNode(i);
                }
            }
        }

        return true;
    }
    bool removeOutEdge(Edge edge) {
        addUnassignedDstNode(edge.second);

        if (adjList[edge.first].size() <= 1)
            return false;
        if (revAdjList[edge.second].size() <= 1)
            return false;

        auto eraseEdge = [](AdjList2& adj, Edge edge) -> Edge {
            for (int i = 0; i < adj[edge.first].size(); ++i) {
                if (adj[edge.first][i] == edge.second) {
                    adj[edge.first].erase(i);
                    if (adj[edge.first].size() == 1) {
                        return { edge.first, adj[edge.first][0] };
                    }
                    break;
                }
            }
            return NullEdge;
        };
        auto outEdgeToLock = eraseEdge(adjList, edge);
        auto inEdgeToLock = eraseEdge(revAdjList, { edge.second, edge.first });

        if (outEdgeToLock != NullEdge && !lockEdge(Out, outEdgeToLock))
            return false;
        if (inEdgeToLock != NullEdge && !lockEdge(In, inEdgeToLock))
            return false;

        if (useExtendedMatrix) {
            for (int m = 0; m < adjList[edge.second].size(); ++m) {
                int i = adjList[edge.second][m];
                int minRemainingValue = Inf;
                for (int k = 0; k < revAdjList[edge.second].size(); ++k) {
                    auto j = revAdjList[edge.second][k];
                    minRemainingValue = std::min(minRemainingValue, (*costMatrixEx)[i][edge.second][j]);
                }
                auto newCostIncrease = minRemainingValue - (*costMatrix)[i][edge.second];
                if (newCostIncrease > costIncreases[i * size() + edge.second]) {
                    costIncreases[i * size() + edge.second] = newCostIncrease;
                    addUnassignedDstNode(i);
                }
            }
        }

        return true;
    }
    bool removeOutEdgeIfExists(Edge edge) {
        for (int i = 0; i < adjList[edge.first].size(); ++i) {
            if (adjList[edge.first][i] == edge.second) {
                return removeOutEdge(edge);
            }
        }
        return true;
    }

    bool lockEdge(Direction d, Edge edge) {
        if (d == Out) return lockOutEdge(edge);
        else return lockOutEdge({ edge.second, edge.first });
    }
    bool removeEdge(Direction d, Edge edge) {
        if (d == Out) return removeOutEdge(edge);
        else return removeOutEdge({ edge.second, edge.first });
    }
    
    Edge findPivotEdge() {
        /*
            Find most promising edge - that is one which removal from solution 
            would at a first glance lead to the biggest increase in cost.
            We do this by summing 2nd lowest element in row and column of each edge
        */
        auto findMin = [this](AdjList2& adj, NodeType i, NodeType ignoreNode, bool rev) -> EdgeCostType {
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
        EdgeCostType bestIncrease = -1;
        for (int i = 0; i < problemSize - 1; ++i) {
            auto j = solution[i];
            if (adjList[i].size() <= 1 || revAdjList[j].size() <= 1)
                continue;
            auto increase = findMin(adjList, i, j, false) + findMin(revAdjList, j, i, true);
            if (increase > bestIncrease) {
                bestIncrease = increase;
                pivot = { i, j };
            }
        }

        return pivot;
    }
    int size() {
        return adjList.size();
    }
    int valueAt(NodeType i, NodeType j) {
        return (*costMatrix)[j][i] - outReductions[i] - inReductions[j] + (useExtendedMatrix ? costIncreases[j * size() + i] : 0);
    }
    int operator()(NodeType i, NodeType j) {
        return valueAt(i, j);
    }
};
void hungarianMethod(AssignmentSolution& assignmentSolution) {
    const int N = assignmentSolution.size();

    auto& minFrom = assignmentSolution.minFrom;
    auto& prv = assignmentSolution.prv;
    auto& inZ = assignmentSolution.inZ;
    auto& nodesInZ = assignmentSolution.nodesInZ;

    auto& solution = assignmentSolution.solution;
    auto& revSolution = assignmentSolution.revSolution;
    solution[N] = NullNode;

    for (auto curDstNode : assignmentSolution.unassignedDstNodes) {
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

            auto& revAdj = assignmentSolution.revAdjList;
            for (int k = 0; k < revAdj[dst].size(); ++k) {
                auto src = revAdj[dst][k];
                if (!inZ[src]) {
                    auto cost = assignmentSolution(src, dst);
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
            if (delta > assignmentSolution.ignoredValue) {
                assignmentSolution.cost = Inf;
                return;
            }
            for (int src = 0; src <= N; ++src) {
                if (!inZ[src]) {
                    minFrom[src] -= delta;
                }
            }
            for (auto src : nodesInZ) {
                assignmentSolution.outReductions[src] -= delta;
                assignmentSolution.inReductions[solution[src]] += delta;
            }
            assignmentSolution.cost += delta;
            curSrcNode = nextSrc;
        }
        for (NodeType src; curSrcNode != N; curSrcNode = src) {
            src = prv[curSrcNode];
            solution[curSrcNode] = solution[src];
            revSolution[solution[src]] = curSrcNode;
        }
    }
    assignmentSolution.unassignedDstNodes.clear();
    std::fill(assignmentSolution.unassignedDstNodesSet.data, assignmentSolution.unassignedDstNodesSet.data + assignmentSolution.size(), false);
}

template<typename T> struct PriorityQueue {
    std::vector<T> heap;
    std::function<bool(const T&, const T&)> comparator;
    std::mutex m;

    PriorityQueue(std::function<bool(const T&, const T&)> comparator) : comparator(comparator) {}
    void push(T&& value) {
        std::scoped_lock l{ m };
        heap.emplace_back(std::move(value));
        std::push_heap(heap.begin(), heap.end(), comparator);
    }
    std::optional<T> pop() {
        std::scoped_lock l{ m };
        if (size() <= 0)
            return std::nullopt;
        std::pop_heap(heap.begin(), heap.end(), comparator);
        auto value = std::move(heap.back());
        heap.pop_back();
        return value;
    }
    bool empty() {
        return heap.empty();
    }
    int size() {
        return int(heap.size());
    }
    template<typename Pred> void removeAll(Pred predicate) {
        heap.erase(std::remove_if(heap.begin(), heap.end(), predicate), heap.end());
        std::make_heap(heap.begin(), heap.end(), comparator);
    }
};

void findSolutions(SolutionConfig& config, AssignmentSolution& assignmentSolution, PriorityQueue<AssignmentSolution>& assignmentQueue, std::mutex& solutionMutex, int maxQueueSize) {
    config.partialSolutionCount += 1;
    if (config.stopWorking)
        return;

    if (assignmentSolution.cost > config.limit)
        return;

    assignmentSolution.shrinkToFit();
    hungarianMethod(assignmentSolution);

    if (assignmentSolution.cost > config.limit)
        return;

    if (assignmentSolution.isComplete()) {
        saveSolution(config, std::vector<NodeType>(assignmentSolution.solution.data, assignmentSolution.solution.data + assignmentSolution.size()), solutionMutex);
    } else {
        auto pivotEdge = assignmentSolution.findPivotEdge();
        if (pivotEdge == NullEdge)
            return;

        auto assignmentSolutionCopy = assignmentSolution;
        if (assignmentQueue.size() >= maxQueueSize) {
            if (assignmentSolution.lockEdge(Out, pivotEdge))
                findSolutions(config, assignmentSolution, assignmentQueue, solutionMutex, maxQueueSize);
            if (assignmentSolutionCopy.removeEdge(Out, pivotEdge))
                findSolutions(config, assignmentSolutionCopy, assignmentQueue, solutionMutex, maxQueueSize);
        } else {
            if (assignmentSolution.lockEdge(Out, pivotEdge))
                assignmentQueue.push(std::move(assignmentSolution));
            if (assignmentSolutionCopy.removeEdge(Out, pivotEdge))
                assignmentQueue.push(std::move(assignmentSolutionCopy));
        }
    }
}

void findSolutionsPriority(SolutionConfig& config) {
    ArrayOfPoolAllocators freeLists(1024, { 
        AssignmentSolution::RequiredAllocationSize(int(config.weights.size()), config.useExtendedMatrix),
        AssignmentSolution::RequiredAllocationSize(int(config.weights.size()), int(config.weights.size() / 2), config.useExtendedMatrix),
        AssignmentSolution::RequiredAllocationSize(int(config.weights.size()), int(config.weights.size() / 4), config.useExtendedMatrix),
        AssignmentSolution::RequiredAllocationSize(int(config.weights.size()), int(config.weights.size() / 8), config.useExtendedMatrix),
        AssignmentSolution::RequiredAllocationSize(int(config.weights.size()), int(config.weights.size() / 16), config.useExtendedMatrix),
    });
    AssignmentSolution initialSolution(freeLists, &config.weights, &config.condWeights, config.ignoredValue, config.useExtendedMatrix);

    initialSolution.shrinkToFit();
    hungarianMethod(initialSolution);

    // no more than 2 GB of data in queue
    const int MaxQueueSize = 2'000'000'000 / initialSolution.minimumAllocationSize();
    const int ThreadCount = (std::thread::hardware_concurrency() > 0) ? std::thread::hardware_concurrency() : 1;

    PriorityQueue<AssignmentSolution> assignmentQueue([](auto& a, auto& b) { return a.cost > b.cost; });

    if (config.weights.size() >= 30 || (config.useExtendedMatrix && config.weights.size() >= 15))
        assignmentQueue.heap.reserve(MaxQueueSize); // reserve only if not trivial problem

    assignmentQueue.push(std::move(initialSolution));
    std::atomic<int> lastCleanupCount = 0;
    ThreadPool threadPool(ThreadCount);
    std::mutex waitMutex;
    std::mutex solutionMutex;
    std::atomic<bool>* threadIsWaiting = new std::atomic<bool>[ThreadCount];
    for (int i = 0; i < ThreadCount; ++i) {
        threadIsWaiting[i] = false;
    }
    for (int i = 0; i < ThreadCount; ++i) {
        threadPool.addTask([&config, &assignmentQueue, &threadIsWaiting, &waitMutex, &solutionMutex, &lastCleanupCount, ThreadCount, MaxQueueSize](int id) {
            while (true) {
                while (assignmentQueue.empty()) {
                    // I'm sure there is more elegant way to do this. Basically I want all worker threads to wait
                    // until all work is finished and until that happens periodically check queue for new tasks possibly
                    // added by still working threads.
                    threadIsWaiting[id] = true;
                    {
                        std::scoped_lock l{ waitMutex };
                        bool stillWorking = false;
                        for (int i = 0; i < ThreadCount; ++i) {
                            if (!threadIsWaiting[i])
                                stillWorking = true;
                        }
                        if (!stillWorking)
                            return;
                    }
                    if (config.stopWorking)
                        return;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                threadIsWaiting[id] = false;
                auto assignmentSolution = assignmentQueue.pop();
                if (!assignmentSolution)
                    continue;

                {
                    // Could be the case that old possible solutions can be discarded early, because they
                    // cost more than updated max solution time limit. This will free up some space for queue.
                    std::scoped_lock l{ assignmentQueue.m };
                    if (config.partialSolutionCount > lastCleanupCount + MaxQueueSize) {
                        lastCleanupCount.store(int(config.partialSolutionCount));
                        assignmentQueue.removeAll([&config](auto& a) { return a.cost > config.limit; });
                    }
                }

                findSolutions(config, *assignmentSolution, assignmentQueue, solutionMutex, MaxQueueSize);
            }
        });
    }
    threadPool.wait();
    delete[] threadIsWaiting;
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

    const int ThreadCount = (std::thread::hardware_concurrency() > 0) ? std::thread::hardware_concurrency() : 1;
    ThreadPool threadPool(ThreadCount);
    std::mutex solutionMutex;

    for (int maxSequenceLength = 4; maxSequenceLength < N; maxSequenceLength += 2) {
        if (config.stopWorking)
            break;
        std::vector<int> maxSearchWidths(N, 0);
        for (int i = 0; i < maxSequenceLength; ++i) {
            maxSearchWidths[i] = N;
        }
        XorShift64 rng;
        FastThreadSafeishHashSet<std::vector<NodeType>> processedSolutions(24);

        config.partialSolutionCount = ((uint64_t(maxSequenceLength) + 1) << 32);
        constexpr int TryCount = 1000;
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
