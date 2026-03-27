#pragma once
#include "solutionFinderCommon.h"

struct AdjList {
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

struct MemoryPool {
    uint8_t* curPtr = nullptr;
    uint8_t* memory = nullptr;
    int memorySize = 0;
    ArrayOfPoolAllocators& allocators;

    static inline ArrayOfPoolAllocators DummyEmptyAllocators = ArrayOfPoolAllocators();

    MemoryPool() : allocators(DummyEmptyAllocators) {}
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
        curPtr = memory = (uint8_t*)allocators.allocate();
        memorySize = size;
    }
    void deallocate() {
        if (memory)
            allocators.deallocate(memory);
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

template<typename SolutionType> struct BranchAndBoundSolution {
    /*
        This struct holds all the memory needed to work with given possible candidate solution.
        One of the bottlenecks is allocation and copying of this data, so the below layout was made
        to optimize that.
        Allocations are done using PoolAllocators with pre-selecated range of sizes and whole struct uses
        single allocated memory area, so allocation is almost as free as if it was stack allocation.
        All initializations are done with small number of calls to memcpy which is very fast.
        It's kind of ugly in that you have to manually remember to change multiple places in code to add/remove
        member of this struct. Maybe could make some nice layer of abstraction for that.
    */
    using Self = BranchAndBoundSolution<SolutionType>;

    struct PartialRoute {
        PartialRoute(NodeType start, NodeType end, int size) : start(start), end(end), size(size) {}
        NodeType start;
        NodeType end;
        int size;
    };

    // memory pool
    MemoryPool memoryPool;

    // loose variables - normal assignment copy
    const std::vector<std::vector<EdgeCostType>>* costMatrix;
    const ConditionalMatrix<EdgeCostType>* costMatrixEx;
    int problemSize = 0;
    EdgeCostType ignoredValue;
    EdgeCostType cost = 0;
    bool useExtendedMatrix;

    // initialized data (do full copy)
    Array<NodeType> solution;
    Array<NodeType> revSolution;
    Array<NodeType> lockedInEdges;
    Array<uint16_t> costIncreases;

    // special initialized data (do full copy, but the size is not constant)
    AdjList adjList;
    AdjList revAdjList;

    SolutionType& derived()                                { return static_cast<SolutionType&>(*this); }
    const SolutionType& derived() const                    { return static_cast<const SolutionType&>(*this); }
    const SolutionType& asDerived(const Self& other) const { return static_cast<const SolutionType&>(other); }

    static int InitializedSectionSize(int problemSize, bool useExtendedMatrix) {
        int size = 0;
        size += (problemSize + 1) * sizeof(NodeType); // solution
        size += (problemSize + 1) * sizeof(NodeType); // revSolution
        size += (problemSize + 1) * sizeof(NodeType); // lockedInEdges
        if (useExtendedMatrix) {
            size += problemSize * problemSize * sizeof(uint16_t); // costIncreases
        }
        return size + SolutionType::InitializedSectionSize(problemSize, useExtendedMatrix);
    }
    static int SpecialInitializedDataSize(int problemSize, int strideSize) {
        int size = 0;
        size += strideSize * problemSize * sizeof(NodeType); // adjList
        size += strideSize * problemSize * sizeof(NodeType); // revAdjList
        return size + SolutionType::SpecialInitializedDataSize(problemSize, strideSize);
    }
    static int RequiredAllocationSize(int problemSize, int strideSize, bool useExtendedMatrix) {
        return InitializedSectionSize(problemSize, useExtendedMatrix) + SpecialInitializedDataSize(problemSize, strideSize);
    }
    static int RequiredAllocationSize(int problemSize, bool useExtendedMatrix) {
        return RequiredAllocationSize(problemSize, problemSize + 1, useExtendedMatrix);
    }
    int minimumAllocationSize() const {
        int size = InitializedSectionSize(problemSize, useExtendedMatrix);
        size += adjList.stride_ * adjList.size() * sizeof(NodeType);
        size += revAdjList.stride_ * revAdjList.size() * sizeof(NodeType);
        return size;
    }

    void assignLooseVariables(const Self& other) {
        costMatrix = other.costMatrix;
        costMatrixEx = other.costMatrixEx;
        problemSize = other.problemSize;
        ignoredValue = other.ignoredValue;
        useExtendedMatrix = other.useExtendedMatrix;
        cost = other.cost;
        derived().assignLooseVariables(asDerived(other));
    }
    void assignNonLooseVariables(const Self& other) {
        solution = other.solution;
        revSolution = other.revSolution;
        lockedInEdges = other.lockedInEdges;
        costIncreases = other.costIncreases;
        adjList = other.adjList;
        revAdjList = other.revAdjList;
        derived().assignNonLooseVariables(asDerived(other));
    }
    template<typename ArrayWithData> void assignMemory(ArrayWithData& array) {
        array.data = memoryPool.assignNextMemory<std::remove_reference_t<decltype(array.data[0])>>(problemSize + 1);
    }
    void assignMemory(int adjListStride, int revAdjListStride) {
        assignMemory(solution);
        assignMemory(revSolution);
        assignMemory(lockedInEdges);
        if (useExtendedMatrix) {
            costIncreases.data = memoryPool.assignNextMemory<uint16_t>(problemSize * problemSize);
        }
        derived().assignMemory();
        adjList.data_.data = memoryPool.assignNextMemory<NodeType>(adjListStride * problemSize);
        revAdjList.data_.data = memoryPool.assignNextMemory<NodeType>(revAdjListStride * problemSize);
    }
    void copyInitializedDataSection(const Self& other) {
        std::memcpy(memoryPool.memory, other.memoryPool.memory, InitializedSectionSize(other.problemSize, other.useExtendedMatrix));
        derived().copyInitializedDataSection(asDerived(other));
    }
    static void CopyAdjList(AdjList& dst, const AdjList& src) {
        dst.data_.size_ = src.data_.size_;
        dst.stride_ = src.stride_;
        std::memcpy(dst.data_.data, src.data_.data, src.stride_ * src.data_.size_ * sizeof(NodeType));
    }

    void moveInit(Self&& other) {
        assignLooseVariables(other);
        assignNonLooseVariables(other);
    }
    void copyInit(const Self& other) {
        memoryPool.allocate(other.minimumAllocationSize());
        assignLooseVariables(other);
        assignMemory(other.adjList.stride_, other.revAdjList.stride_);
        copyInitializedDataSection(other);
        CopyAdjList(adjList, other.adjList);
        CopyAdjList(revAdjList, other.revAdjList);
    }
    BranchAndBoundSolution(Self&& other) : memoryPool(std::move(other.memoryPool)) {}
    BranchAndBoundSolution(const Self& other) : memoryPool(other.memoryPool.allocators) {}
    BranchAndBoundSolution& operator=(Self&& other) {
        std::swap(memoryPool.curPtr, other.memoryPool.curPtr);
        std::swap(memoryPool.memory, other.memoryPool.memory);
        std::swap(memoryPool.memorySize, other.memoryPool.memorySize);
        assignLooseVariables(other);
        assignNonLooseVariables(other);
        return *this;
    }
    BranchAndBoundSolution& operator=(const Self&) = delete;

    void init() {
        problemSize = int(costMatrix->size());
        memoryPool.allocate(RequiredAllocationSize(problemSize, useExtendedMatrix));
        assignMemory(problemSize + 1, problemSize + 1);

        std::fill(solution.data, solution.data + problemSize + 1, NullNode);
        std::fill(revSolution.data, revSolution.data + problemSize + 1, NullNode);
        std::fill(lockedInEdges.data, lockedInEdges.data + problemSize + 1, NullNode);
        if (useExtendedMatrix) {
            std::fill(costIncreases.data, costIncreases.data + problemSize * problemSize, 0);
        }

        adjList.init(*costMatrix, ignoredValue);
        revAdjList.init(*costMatrix, ignoredValue, true);
    }
    BranchAndBoundSolution(ArrayOfPoolAllocators& allocators, const std::vector<std::vector<EdgeCostType>>* costMatrix,
        const ConditionalMatrix<EdgeCostType>* costMatrixEx, EdgeCostType ignoredValue, bool useExtendedMatrix) :
        memoryPool(allocators), costMatrix(costMatrix), costMatrixEx(costMatrixEx), ignoredValue(ignoredValue), useExtendedMatrix(useExtendedMatrix)
    {}

    void shrinkToFit() {
        adjList.shrinkToFit();
        revAdjList.shrinkToFit();
    }

    bool isComplete() {
        return derived().isComplete();
    }

    bool removeAllOtherEdges(Direction d, AdjList& adj, Edge edge) {
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

    bool lockOutEdge(Edge edge) {
        /*
            Locking edge A -> B means:
            - removing all other output edges (A -> X, where X != B) [Unless it's Ring-CP compliant solution]
            - removing all other input edges (X -> B, where X != A)
            - checking if this edge connects to some previous locked chain of connections
              and if so extending that connection. Say the final connection is from node C to node D
            - if there is connection from D to C, remove it - it would form a cycle, which would be 
              valid for Assignment problem, but not for ATSP, so better to deal with it as soon as possible
            Note that locking edge and removing edge can cascade. Could be the case that locking edge
            means removing edge such that some node has only 1 IN or OUT node so it's locked and while locking that edge
            we might remove edge such that some node has only 1 IN or OUT node...
        */
        if (lockedInEdges[edge.second] == edge.first)
            return true; // edge is either already locked or is the process of getting locked
        if (lockedInEdges[edge.second] != NullNode)
            return false; // different edge with the same IN node is already locked or in the process of getting locked
        lockedInEdges[edge.second] = edge.first;
        
        if (derived().shouldLockOutEdges()) {
            if (!removeAllOtherEdges(Out, adjList, edge))
                return false;
        }
        if (!removeAllOtherEdges(In, revAdjList, { edge.second, edge.first }))
            return false;
        if (!derived().customLockOutEdge(edge))
            return false;

        if (useExtendedMatrix) {
            for (int m = 0; m < adjList[edge.second].size(); ++m) {
                int i = adjList[edge.second][m];
                auto newCostIncrease = (*costMatrixEx)[i][edge.second][edge.first] - (*costMatrix)[i][edge.second];
                if (newCostIncrease > costIncreases[i * size() + edge.second]) {
                    costIncreases[i * size() + edge.second] = newCostIncrease;
                    derived().edgeCostIncreasedCallback(Edge{ edge.second, i });
                }
            }
        }

        return true;
    }
    bool removeOutEdge(Edge edge) {
        derived().customRemoveOutEdge(edge);
        if (adjList[edge.first].size() <= 1) {
            if (adjList[edge.first].size() <= 0 || !derived().customCanRemoveLastOutEdge(edge)) {
                return false;
            }
        }
        if (revAdjList[edge.second].size() <= 1)
            return false;

        auto eraseEdge = [](AdjList& adj, Edge edge) -> Edge {
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

        if (derived().shouldLockOutEdges() && outEdgeToLock != NullEdge && !lockEdge(Out, outEdgeToLock))
            return false;
        if (inEdgeToLock != NullEdge && !lockEdge(In, inEdgeToLock))
            return false;

        if (useExtendedMatrix) {
            for (int m = 0; m < adjList[edge.second].size(); ++m) {
                int i = adjList[edge.second][m];
                auto costDataRow = (*costMatrixEx)[i][edge.second].data();
                int minRemainingValue = Inf;
                for (int k = 0; k < revAdjList[edge.second].size(); ++k) {
                    auto j = revAdjList[edge.second][k];
                    minRemainingValue = std::min(minRemainingValue, costDataRow[j]);
                }
                auto newCostIncrease = minRemainingValue - (*costMatrix)[i][edge.second];
                if (newCostIncrease > costIncreases[i * size() + edge.second]) {
                    costIncreases[i * size() + edge.second] = newCostIncrease;
                    derived().edgeCostIncreasedCallback(Edge{ edge.second, i });
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

    void lockSingleEdges(Direction d, AdjList& adj, NodeType startNode, NodeType endNode) {
        for (int i = startNode; i <= endNode; ++i) {
            if (adj[i].size() == 1) {
                lockEdge(d, { i, adj[i][0] });
            }
        }
    }

    int getCost() const {
        return derived().getCost();
    }
    int size() {
        return adjList.size();
    }
    int valueAt(NodeType i, NodeType j) {
        return derived().valueAt(i, j);
    }
    int operator()(NodeType i, NodeType j) {
        return valueAt(i, j);
    }
};

template<typename SolutionType> void findSolutions(SolutionConfig& config, SolutionType& branchAndBoundSolution, PriorityMultiQueue<SolutionType>& assignmentQueue, PreallocatedVector<std::pair<SolutionType, Edge>>& backlog) {
    bool updatedSolution = false;
    do {
        if (!updatedSolution && !backlog.empty()) {
            while (true) {
                if (config.stopWorking() || backlog.empty())
                    return;
                branchAndBoundSolution = std::move(backlog.back().first);
                auto pivotEdge = backlog.back().second;
                backlog.pop_back();
                if (branchAndBoundSolution.getCost() > config.limit())
                    continue;
                if (branchAndBoundSolution.removeEdge(Out, pivotEdge))
                    break;
            }
        }
        updatedSolution = false;

        config.lazyIncrementPartialSolutionCount();
        if (config.stopWorking())
            return;

        if (branchAndBoundSolution.getCost() > config.limit())
            continue;

        if (!branchAndBoundSolution.solveRelaxationAndCheckIfStillViable(config))
            continue;

        if (branchAndBoundSolution.getCost() > config.limit())
            continue;

        if (branchAndBoundSolution.isComplete()) {
            branchAndBoundSolution.saveSolution(config);
            continue;
        }
        if (config.stopWorking())
            return;

        auto pivotEdge = branchAndBoundSolution.findPivotEdge();
        if (pivotEdge == NullEdge)
            continue;

        if (config.stopWorking())
            return;
        auto assignmentSolutionCopy = branchAndBoundSolution;
        if (assignmentQueue.isAlmostFull()) {
            if (branchAndBoundSolution.lockEdge(Out, pivotEdge))
                updatedSolution = true;
            backlog.emplace_back({ std::move(assignmentSolutionCopy), pivotEdge });
        } else {
            if (branchAndBoundSolution.lockEdge(Out, pivotEdge))
                assignmentQueue.push(std::move(branchAndBoundSolution));
            if (config.stopWorking())
                return;
            if (assignmentSolutionCopy.removeEdge(Out, pivotEdge))
                assignmentQueue.push(std::move(assignmentSolutionCopy));
        }
    } while (updatedSolution || !backlog.empty());
}

template<typename BacklogType, typename SolutionType, typename FunctionType> void findSolutionsBfs(SolutionConfig& config, SolutionType& initialSolution, int queueElementSize, FunctionType function) {
    const int MaxQueueSize = 2'000'000'000 / queueElementSize;
#ifdef DEBUG
    const int ThreadCount = 1;
#else
    const int ThreadCount = (std::thread::hardware_concurrency() > 0) ? std::thread::hardware_concurrency() : 1;
#endif
    const int QueueCount = ThreadCount * 2;
    PriorityMultiQueue<SolutionType> assignmentQueue(QueueCount, MaxQueueSize / QueueCount, [](auto& a, auto& b) { return a.getCost() > b.getCost(); });

    auto dummySolution = initialSolution;
    assignmentQueue.push(std::move(initialSolution));
    ThreadPool threadPool(ThreadCount);
    std::mutex waitMutex;
    std::atomic<bool>* threadIsWaiting = new std::atomic<bool>[ThreadCount];
    for (int i = 0; i < ThreadCount; ++i) {
        threadIsWaiting[i] = false;
    }
    for (int i = 0; i < ThreadCount; ++i) {
        threadPool.addTask([&config, &function, &assignmentQueue, &threadIsWaiting, &waitMutex, &dummySolution, ThreadCount](int id) {
            PreallocatedVector<BacklogType> backlog(config.nodeCount() * config.nodeCount());
            SolutionType solution = dummySolution;
            while (true) {
                if (config.stopWorking())
                    break;
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
                            goto End;
                    }
                    if (config.stopWorking())
                        goto End;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                threadIsWaiting[id] = false;

                if (!assignmentQueue.pop(solution))
                    continue;

                function(config, solution, assignmentQueue, backlog);
            }
        End:
            config.flushPartialSolutionCount();
        });
    }
    threadPool.wait();
    delete[] threadIsWaiting;
}

template<typename SolutionType> void findSolutionsBranchAndBound(SolutionConfig& config) {
    ArrayOfPoolAllocators freeLists(1024, 16, 3'000'000'000, SolutionType::RequiredAllocationSize(int(config.weights.size()), config.useExtendedMatrix));
    SolutionType initialSolution(freeLists, config);
    initialSolution.shrinkToFit();
    findSolutionsBfs<std::pair<SolutionType, Edge>>(config, initialSolution, initialSolution.minimumAllocationSize(), findSolutions<SolutionType>);
}
