#pragma once

extern "C" {
#include "LKH/LKH.h"
#include "LKH/Genetic.h"
}

#define Fixed(a, b) ((a)->FixedTo1 == (b) || (a)->FixedTo2 == (b))
#define Link(a, b) { ((a)->Suc = (b))->Pred = (a); }

template<typename T> struct ArrayView {
    int32_t size;
    T data[1];

    ArrayView() = delete;
    ArrayView(const ArrayView&) = delete;
    ArrayView(ArrayView&&) = delete;
    ArrayView& operator=(const ArrayView&) = delete;
    ArrayView& operator=(ArrayView&&) = delete;

    T& operator[](int i) {
        return data[i];
    }
};
template<typename T> struct Array2dView {
    int32_t size;
    T data[1];

    Array2dView() = delete;
    Array2dView(const Array2dView&) = delete;
    Array2dView(Array2dView&&) = delete;
    Array2dView& operator=(const Array2dView&) = delete;
    Array2dView& operator=(Array2dView&&) = delete;

    T* operator[](int i) {
        return &data[i * size];
    }
};

namespace LKH {
    void lkhSetParameters() {
        ProblemFileName = PiFileName = InputTourFileName = OutputTourFileName = TourFileName = 0;
        CandidateFiles = MergeTourFiles = 0;
        AscentCandidates = 50;
        BackboneTrials = 0;
        Backtracking = 0;
        CandidateSetSymmetric = 0;
        CandidateSetType = ALPHA;
        Crossover = ERXT;
        DelaunayPartitioning = 0;
        DelaunayPure = 0;
        Excess = -1;
        ExtraCandidates = 0;
        ExtraCandidateSetSymmetric = 0;
        ExtraCandidateSetType = QUADRANT;
        Gain23Used = 1;
        GainCriterionUsed = 1;
        GridSize = 1000000.0;
        InitialPeriod = -1;
        InitialStepSize = 0;
        InitialTourAlgorithm = WALK;
        InitialTourFraction = 1.0;
        KarpPartitioning = 0;
        KCenterPartitioning = 0;
        KMeansPartitioning = 0;
        Kicks = 1;
        KickType = 0;
        MaxBreadth = INT_MAX;
        MaxCandidates = 5;
        MaxPopulationSize = 0;
        MoorePartitioning = 0;
        MoveType = 5;
        NonsequentialMoveType = -1;
        Optimum = MINUS_INFINITY;
        PatchingA = 1;
        PatchingC = 0;
        PatchingAExtended = 0;
        PatchingARestricted = 0;
        PatchingCExtended = 0;
        PatchingCRestricted = 0;
        Precision = 100;
        POPMUSIC_InitialTour = 0;
        POPMUSIC_MaxNeighbors = 5;
        POPMUSIC_SampleSize = 10;
        POPMUSIC_Solutions = 50;
        POPMUSIC_Trials = 1;
        Recombination = IPT;
        RestrictedSearch = 1;
        RohePartitioning = 0;
        SierpinskiPartitioning = 0;
        StopAtOptimum = 1;
        Subgradient = 1;
        SubproblemBorders = 0;
        SubproblemsCompressed = 0;
        SubproblemSize = 0;
        SubsequentMoveType = 0;
        SubsequentPatching = 1;
        TimeLimit = DBL_MAX;
        TotalTimeLimit = DBL_MAX;
        TraceLevel = 0;

        Runs = 1;
        Seed = 0;
        MaxSwaps = 10;
        MaxTrials = 10;
    }


    static void HeapMake(int Size) {
        Heap = (Node**)malloc((Size + 1) * sizeof(Node*));
    }
    static void CreateNodes() {
        Node* Prev = 0, * N = 0;
        int i;

        if (Dimension <= 0)
            eprintf("DIMENSION is not positive (or not specified)");
        Dimension *= 2;
        NodeSet = (Node*)calloc(Dimension + 1, sizeof(Node));
        for (i = 1; i <= Dimension; i++, Prev = N) {
            N = &NodeSet[i];
            if (i == 1)
                FirstNode = N;
            else
                Link(Prev, N);
            N->Id = i;
        }
        Link(N, FirstNode);
    }
    static int FixEdge(Node* Na, Node* Nb) {
        if (!Na->FixedTo1 || Na->FixedTo1 == Nb)
            Na->FixedTo1 = Nb;
        else if (!Na->FixedTo2 || Na->FixedTo2 == Nb)
            Na->FixedTo2 = Nb;
        else
            return 0;
        if (!Nb->FixedTo1 || Nb->FixedTo1 == Na)
            Nb->FixedTo1 = Na;
        else if (!Nb->FixedTo2 || Nb->FixedTo1 == Na)
            Nb->FixedTo2 = Na;
        else
            return 0;
        return 1;
    }
    static void Read_EDGE_WEIGHT_SECTION(Array2dView<int32_t>& inputMatrix) {
        Node* Ni, * Nj;
        int i, j, n, W;

        CreateNodes();
        n = Dimension / 2;
        CostMatrix = (int*)calloc((size_t)n * n, sizeof(int));
        for (Ni = FirstNode; Ni->Id <= n; Ni = Ni->Suc)
            Ni->C = &CostMatrix[(size_t)(Ni->Id - 1) * n] - 1;

        n = Dimension / 2;
        for (i = 1; i <= n; i++) {
            Ni = &NodeSet[i];
            for (j = 1; j <= n; j++) {
                W = inputMatrix[j - 1][i - 1];
                Ni->C[j] = W;
                if (i != j && W > M)
                    M = W;
            }
            Nj = &NodeSet[i + n];
            FixEdge(Ni, Nj);
        }
        Distance = Distance_ATSP;
        WeightType = -1;
    }
    void ReadProblem(Array2dView<int32_t>& inputMatrix) {
        int i, K;

        FreeStructures();
        FirstNode = 0;
        WeightType = WeightFormat = ProblemType = -1;
        CoordType = NO_COORDS;
        Type = EdgeWeightType = EdgeWeightFormat = 0;
        EdgeDataFormat = NodeCoordType = DisplayDataType = 0;
        GridSize = 1000000.0;
        C = 0;
        c = 0;
        DimensionSaved = Dimension = inputMatrix.size;
        ProblemType = ATSP;
        Name = nullptr;
        WeightFormat = FULL_MATRIX;
        WeightType = EXPLICIT;
        Distance = Distance_EXPLICIT;
        Read_EDGE_WEIGHT_SECTION(inputMatrix);

        Swaps = 0;

        /* Adjust parameters */
        if (Seed == 0)
            Seed = (unsigned)(time(0) * (size_t)(&Seed));
        if (Precision == 0)
            Precision = 100;
        if (InitialStepSize == 0)
            InitialStepSize = 1;
        if (MaxSwaps < 0)
            MaxSwaps = Dimension;
        if (KickType > Dimension / 2)
            KickType = Dimension / 2;
        if (Runs == 0)
            Runs = 10;
        if (MaxCandidates > Dimension - 1)
            MaxCandidates = Dimension - 1;
        if (ExtraCandidates > Dimension - 1)
            ExtraCandidates = Dimension - 1;
        if (SubproblemSize >= Dimension)
            SubproblemSize = Dimension;
        else if (SubproblemSize == 0) {
            if (AscentCandidates > Dimension - 1)
                AscentCandidates = Dimension - 1;
            if (InitialPeriod < 0) {
                InitialPeriod = Dimension / 2;
                if (InitialPeriod < 100)
                    InitialPeriod = 100;
            }
            if (Excess < 0)
                Excess = 1.0 / Dimension;
            if (MaxTrials == -1)
                MaxTrials = Dimension;
            HeapMake(Dimension);
        }
        if (POPMUSIC_MaxNeighbors > Dimension - 1)
            POPMUSIC_MaxNeighbors = Dimension - 1;
        if (POPMUSIC_SampleSize > Dimension)
            POPMUSIC_SampleSize = Dimension;
        if (CostMatrix == 0 && Dimension <= MaxMatrixDimension &&
            Distance != 0 && Distance != Distance_1 && Distance != Distance_LARGE &&
            Distance != Distance_ATSP && Distance != Distance_SPECIAL)
        {
            Node* Ni, * Nj;
            CostMatrix = (int*)calloc((size_t)Dimension * (Dimension - 1) / 2,
                sizeof(int));
            Ni = FirstNode->Suc;
            do {
                Ni->C = &CostMatrix[(size_t)(Ni->Id - 1) * (Ni->Id - 2) / 2] - 1;
                if (ProblemType != HPP || Ni->Id < Dimension)
                    for (Nj = FirstNode; Nj != Ni; Nj = Nj->Suc)
                        Ni->C[Nj->Id] = Fixed(Ni, Nj) ? 0 : Distance(Ni, Nj);
                else
                    for (Nj = FirstNode; Nj != Ni; Nj = Nj->Suc)
                        Ni->C[Nj->Id] = 0;
            } while ((Ni = Ni->Suc) != FirstNode);
            WeightType = EXPLICIT;
            c = 0;
        }
        if (Precision > 1 && (WeightType == EXPLICIT || ProblemType == ATSP)) {
            int j, n = ProblemType == ATSP ? Dimension / 2 : Dimension;
            for (i = 2; i <= n; i++) {
                Node* N = &NodeSet[i];
                for (j = 1; j < i; j++)
                    if (N->C[j] * Precision / Precision != N->C[j])
                        eprintf("PRECISION (= %d) is too large", Precision);
            }
        }
        C = WeightType == EXPLICIT ? C_EXPLICIT : C_FUNCTION;
        D = WeightType == EXPLICIT ? D_EXPLICIT : D_FUNCTION;
        if (SubsequentMoveType == 0)
            SubsequentMoveType = MoveType;
        K = MoveType >= SubsequentMoveType
            || !SubsequentPatching ? MoveType : SubsequentMoveType;
        if (PatchingC > K)
            PatchingC = K;
        if (PatchingA > 1 && PatchingA >= PatchingC)
            PatchingA = PatchingC > 2 ? PatchingC - 1 : 1;
        if (NonsequentialMoveType == -1 ||
            NonsequentialMoveType > K + PatchingC + PatchingA - 1)
            NonsequentialMoveType = K + PatchingC + PatchingA - 1;
        if (PatchingC >= 1) {
            BestMove = BestSubsequentMove = BestKOptMove;
            if (!SubsequentPatching && SubsequentMoveType <= 5) {
                MoveFunction BestOptMove[] =
                { 0, 0, Best2OptMove, Best3OptMove,
                Best4OptMove, Best5OptMove
                };
                BestSubsequentMove = BestOptMove[SubsequentMoveType];
            }
        } else {
            MoveFunction BestOptMove[] = { 0, 0, Best2OptMove, Best3OptMove, Best4OptMove, Best5OptMove };
            BestMove = MoveType <= 5 ? BestOptMove[MoveType] : BestKOptMove;
            BestSubsequentMove = SubsequentMoveType <= 5 ? BestOptMove[SubsequentMoveType] : BestKOptMove;
        }
        if (ProblemType == HCP || ProblemType == HPP)
            MaxCandidates = 0;

        free(LastLine);
        LastLine = 0;
    }


    void run(Array2dView<int32_t>& inputMatrix, ArrayView<int16_t>& outSolution) {
        // Dirty fix to even dirtier code
        Gain23NeedToReset = 1;
        lkhSetParameters();

        MaxMatrixDimension = 20000;
        MergeWithTour =
            Recombination == GPX2 ? MergeWithTourGPX2 :
            Recombination == CLARIST ? MergeWithTourCLARIST :
            MergeWithTourIPT;
        ReadProblem(inputMatrix);

        AllocateStructures();
        CreateCandidateSet();

        if (Norm != 0)
            BestCost = PLUS_INFINITY;
        else {
            Optimum = BestCost = (GainType)LowerBound;
            RecordBetterTour();
            RecordBestTour();
            Runs = 0;
        }

        for (Run = 1; Run <= Runs; Run++) {
            auto Cost = FindTour(); /* using the Lin-Kernighan heuristic */
            if (MaxPopulationSize > 1) {
                /* Genetic algorithm */
                int i;
                for (i = 0; i < PopulationSize; i++) {
                    Cost = MergeTourWithIndividual(i);
                }
                if (!HasFitness(Cost)) {
                    if (PopulationSize < MaxPopulationSize) {
                        AddToPopulation(Cost);
                        if (TraceLevel >= 1)
                            PrintPopulation();
                    } else if (Cost < Fitness[PopulationSize - 1]) {
                        i = ReplacementIndividual(Cost);
                        ReplaceIndividualWithTour(i, Cost);
                        if (TraceLevel >= 1)
                            PrintPopulation();
                    }
                }
            } else if (Run > 1)
                Cost = MergeTourWithBestTour();
            if (Cost < BestCost) {
                BestCost = Cost;
                RecordBetterTour();
                RecordBestTour();
            }
            if (Cost < Optimum) {
                if (FirstNode->InputSuc) {
                    Node* N = FirstNode;
                    while ((N = N->InputSuc = N->Suc) != FirstNode);
                }
                Optimum = Cost;
            }
            if (PopulationSize >= 2 && (PopulationSize == MaxPopulationSize || Run >= 2 * MaxPopulationSize) && Run < Runs) {
                Node* N;
                int Parent1, Parent2;
                Parent1 = LinearSelection(PopulationSize, 1.25);
                do
                    Parent2 = LinearSelection(PopulationSize, 1.25);
                while (Parent2 == Parent1);
                ApplyCrossover(Parent1, Parent2);
                N = FirstNode;
                do {
                    if (ProblemType != HCP && ProblemType != HPP) {
                        int d = C(N, N->Suc);
                        AddCandidate(N, N->Suc, d, INT_MAX);
                        AddCandidate(N->Suc, N, d, INT_MAX);
                    }
                    N = N->InitialSuc = N->Suc;
                } while (N != FirstNode);
            }
            SRandom(++Seed);
        }

        int startIndex = 0;
        for (int i = 0; i < DimensionSaved; ++i) {
            if (BestTour[i + 1] == 1)
                startIndex = i;
            outSolution[i] = BestTour[i + 1] - 1;
        }
        std::rotate(outSolution.data, outSolution.data + startIndex, outSolution.data + outSolution.size);
    }
}

#undef Fixed
#undef Link


class LkhSharedMemory {
    int32_t processEnded = 0;
    int32_t state = 0;
    int32_t offsetToInputMatrix;
    int32_t offsetToSolutionArray;
    char data[1];

    LkhSharedMemory() {}
    LkhSharedMemory(const LkhSharedMemory&) = delete;
    LkhSharedMemory(LkhSharedMemory&&) = delete;
    LkhSharedMemory& operator=(LkhSharedMemory&) = delete;
public:
    friend class LkhSharedMemoryManager;

    enum State : int32_t {
        NotReady = 0,
        WeightsReady = 1,
        SolutionReady = 2,
        StopProcess = 3,
    };

    Array2dView<int32_t>& inputMatrix() {
        return *(Array2dView<int32_t>*) & data[offsetToInputMatrix];
    }
    ArrayView<int16_t>& solution() {
        return *(ArrayView<int16_t>*) & data[offsetToSolutionArray];
    }
    int32_t getState() {
        return state;
    }
    void setState(int32_t s) {
        state = s;
    }
    bool shouldStop() {
        return processEnded;
    }
    void stopProcess() {
        processEnded = true;
    }
};

class LkhSharedMemoryManager {
    HANDLE hMapFile = nullptr;
    LkhSharedMemory* memory_ = nullptr;
    std::string name_;
    int size_ = 0;

    LkhSharedMemoryManager() {}
    LkhSharedMemoryManager(const LkhSharedMemoryManager&) = delete;
    LkhSharedMemoryManager& operator=(const LkhSharedMemoryManager&) = delete;

    bool createMapView() {
        if (hMapFile == nullptr) {
            return false;
        }

        memory_ = (LkhSharedMemory*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size_);
        if (memory_ == nullptr) {
            return false;
        }

        return true;
    }

public:
    LkhSharedMemoryManager(LkhSharedMemoryManager&& other) {
        std::swap_ranges((char*)this, ((char*)this) + sizeof(LkhSharedMemoryManager), (char*)&other);
    }
    ~LkhSharedMemoryManager() {
        if (memory_)
            UnmapViewOfFile(memory_);
        if (hMapFile)
            CloseHandle(hMapFile);
    }

    static std::optional<LkhSharedMemoryManager> Create(const std::string& name, int n) {
        LkhSharedMemoryManager manager;
        manager.name_ = name;
        int headerSize = sizeof(int32_t) * 4;
        int inputMatrixSize = sizeof(int32_t) + n * n * sizeof(int32_t);
        int solutionArraySize = sizeof(int32_t) + n * sizeof(int16_t);
        manager.size_ = headerSize + inputMatrixSize + solutionArraySize;

        manager.hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, manager.size_, manager.name_.c_str());
        if (!manager.createMapView())
            return std::nullopt;

        manager.memory_->processEnded = false;
        manager.memory_->state = LkhSharedMemory::NotReady;
        manager.memory_->offsetToInputMatrix = headerSize;
        manager.memory_->offsetToSolutionArray = headerSize + inputMatrixSize;
        manager.memory_->inputMatrix().size = n;
        manager.memory_->solution().size = n;

        return manager;
    }

    static std::optional<LkhSharedMemoryManager> Open(const std::string& name, int memorySize) {
        LkhSharedMemoryManager manager;

        manager.name_ = name;
        manager.size_ = memorySize;

        manager.hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, manager.name_.c_str());
        if (!manager.createMapView())
            return std::nullopt;

        return manager;
    }

    const std::string& name() const {
        return name_;
    }
    LkhSharedMemory* memory() {
        return memory_;
    }
    int size() {
        return size_;
    }
};


void lkhChildWorkerProcess(int argc, char** argv) {
    if (argc <= 1)
        return;

    auto sharedMemSize = parseInt(argv[1]);
    if (!sharedMemSize)
        return;

    auto memMgr = LkhSharedMemoryManager::Open(argv[0], int(*sharedMemSize));
    if (!memMgr)
        return;
    auto mem = memMgr->memory();

    while (true) {
        if (mem->shouldStop()) {
            ExitProcess(0);
        }
        if (mem->getState() != LkhSharedMemory::WeightsReady) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        LKH::run(mem->inputMatrix(), mem->solution());
        mem->setState(LkhSharedMemory::SolutionReady);
    }
}

std::vector<LkhSharedMemoryManager> createLkhSharedMemoryInstances(int count, int n) {
    std::vector<LkhSharedMemoryManager> result;
    for (int i = 0; i < count; ++i) {
        std::string sharedMemName = "TmPathFinderLKH_" + std::to_string(i);
        auto memMgr = LkhSharedMemoryManager::Create(sharedMemName, n);
        if (!memMgr)
            return {};
        result.emplace_back(std::move(*memMgr));
    }
    return result;
}


std::vector<int16_t> runLkhInChildProcess(LkhSharedMemoryManager& sharedMem, const std::vector<std::vector<int>>& weights, std::atomic<bool>& taskWasCanceled) {
    auto& inputMatrix = sharedMem.memory()->inputMatrix();
    for (int i = 0; i < weights.size(); ++i) {
        for (int j = 0; j < weights.size(); ++j) {
            inputMatrix[i][j] = weights[i][j];
        }
    }

    sharedMem.memory()->setState(LkhSharedMemory::WeightsReady);
    while (sharedMem.memory()->getState() != LkhSharedMemory::SolutionReady) {
        if (taskWasCanceled) {
            sharedMem.memory()->stopProcess();
            return {};
        }
        if (sharedMem.memory()->shouldStop())
            return {};
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::vector<int16_t> solution;
    auto& sol = sharedMem.memory()->solution();
    for (int i = 0; i < sol.size; ++i) {
        solution.push_back(sol[i]);
    }
    return solution;
}

std::vector<PROCESS_INFORMATION> startChildProcesses(const char* programPath, std::vector<LkhSharedMemoryManager>& sharedMemoryInstances) {
    std::vector<PROCESS_INFORMATION> processHandles;

    for (auto& sharedMem : sharedMemoryInstances) {
        std::string inputArgs = "LKH " + sharedMem.name() + " " + std::to_string(sharedMem.size());
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        if (!CreateProcessA(programPath, (char*)inputArgs.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            return {};
        }
        processHandles.push_back(pi);
    }
    return processHandles;
}
void stopChildProcesses(std::vector<PROCESS_INFORMATION>& processHandles, std::vector<LkhSharedMemoryManager>& sharedMemoryInstances) {
    for (auto& sharedMem : sharedMemoryInstances) {
        sharedMem.memory()->stopProcess();
    }
    for (auto& pi : processHandles) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
