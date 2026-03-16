#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <boost/int128.hpp>
#include "utility.h"

namespace fs = std::filesystem;
using int128_t = boost::int128::int128_t;

struct ConnectionFinderSettings {
    int testedConnectionTime = 0;
    int minConnectionTime = 600;
    int maxConnectionTime = 100'000;
    int minDistance = 0;
    int maxDistance = 100'000;
    int minHeightDiff = -100'000;
    int maxHeightDiff = 100'000;
    char searchSourceNodes[1024] = { 0 };
};

struct InputData {
    // general
    int fontSize = 16;
    bool showAdvancedSettings = false;

    // Path Finder tab
    int ignoredValue = 600;
    int limitValue = 100'000;
    int maxSolutionCount = 100;
    int maxTime = 0;
    int maxRepeatNodesToAdd = 100'000;
    char turnedOffRepeatNodes[1024] = { 0 };

    char outputDataFile[1024] = { 0 };
    char ringCps[1024] = { 0 };
    char routeString[1024] = { 0 };
    char positionReplayFilePath[1024] = { 0 };
    char inputDataFile[1024] = { 0 };
    char inputDataLink[1024] = { 0 };
    bool downloadSpreadsheet = true;

    bool isConnectionSearchAlgorithm = false;
    ConnectionFinderSettings connectionFinderSettings;
    bool sortConnectionSearchResultsByConnection = false;

    bool showResultsFilter = false; // Don't save to file

    // CP positions creator / replay visualizer tabs
    char positionReplayFile[1024] = { 0 };
    char cpOrder[1024] = { 0 };
    char outputPositionsFile[1024] = { 0 };

    // Distance matrix creator
    char outputDistanceMatrixFile[1024] = { 0 };

    InputData() {
        strcpy(outputPositionsFile, "CP_positions.txt");
        strcpy(outputDistanceMatrixFile, "DistanceMatrix.csv");
    }

    void saveToFile(const std::string& filePath) {
        std::ofstream file(filePath);
        if (!file)
            return;
        file << "fontSize " << fontSize << '\n';
        file << "showAdvancedSettings " << showAdvancedSettings << '\n';

        file << "ignoredValue " << ignoredValue << '\n';
        file << "limitValue " << limitValue << '\n';
        file << "maxSolutionCount " << maxSolutionCount << '\n';
        file << "maxTime " << maxTime << '\n';
        file << "maxRepeatNodesToAdd " << maxRepeatNodesToAdd << '\n';
        file << "turnedOffRepeatNodes " << turnedOffRepeatNodes << '\n';

        file << "outputDataFile " << outputDataFile << '\n';
        file << "ringCps " << ringCps << '\n';
        file << "routeString " << routeString << '\n';
        file << "positionReplayFilePath " << positionReplayFilePath << '\n';
        file << "inputDataFile " << inputDataFile << '\n';
        file << "inputDataLink " << inputDataLink << '\n';
        file << "downloadSpreadsheet " << downloadSpreadsheet << '\n';

        file << "isConnectionSearchAlgorithm " << isConnectionSearchAlgorithm << '\n';
        file << "connectionFinderSettings.testedConnectionTime " << connectionFinderSettings.testedConnectionTime << '\n';
        file << "connectionFinderSettings.minConnectionTime " << connectionFinderSettings.minConnectionTime << '\n';
        file << "connectionFinderSettings.maxConnectionTime " << connectionFinderSettings.maxConnectionTime << '\n';
        file << "connectionFinderSettings.minDistance " << connectionFinderSettings.minDistance << '\n';
        file << "connectionFinderSettings.maxDistance " << connectionFinderSettings.maxDistance << '\n';
        file << "connectionFinderSettings.minHeightDiff " << connectionFinderSettings.minHeightDiff << '\n';
        file << "connectionFinderSettings.maxHeightDiff " << connectionFinderSettings.maxHeightDiff << '\n';
        file << "connectionFinderSettings.searchSourceNodes " << connectionFinderSettings.searchSourceNodes << '\n';
        file << "sortConnectionSearchResultsByConnection " << sortConnectionSearchResultsByConnection << '\n';

        file << "positionReplayFile " << positionReplayFile << '\n';
        file << "cpOrder " << cpOrder << '\n';
        file << "outputPositionsFile " << outputPositionsFile << '\n';

        file << "outputDistanceMatrixFile " << outputDistanceMatrixFile << '\n';
    }
    void loadFromFile(const std::string& filePath) {
        std::ifstream file(filePath);
        std::string key;
        while (file >> key) {
            std::string value;
            std::getline(file, value);
            if (key == "fontSize") {
                fontSize = stoi(value);
            } else if (key == "showAdvancedSettings") {
                showAdvancedSettings = stoi(value);
            } else if (key == "ignoredValue") {
                ignoredValue = stoi(value);
            } else if (key == "limitValue") {
                limitValue = stoi(value);
            } else if (key == "maxSolutionCount") {
                maxSolutionCount = stoi(value);
            } else if (key == "maxTime") {
                maxTime = stoi(value);
            } else if (key == "maxRepeatNodesToAdd") {
                maxRepeatNodesToAdd = stoi(value);
            } else if (key == "turnedOffRepeatNodes") {
                strcpy(turnedOffRepeatNodes, value.c_str() + 1);
            } else if (key == "outputDataFile") {
                strcpy(outputDataFile, value.c_str() + 1);
            } else if (key == "ringCps") {
                strcpy(ringCps, value.c_str() + 1);
            } else if (key == "routeString") {
                strcpy(routeString, value.c_str() + 1);
            } else if (key == "positionReplayFilePath") {
                strcpy(positionReplayFilePath, value.c_str() + 1);
            } else if (key == "inputDataFile") {
                strcpy(inputDataFile, value.c_str() + 1);
            } else if (key == "inputDataLink") {
                strcpy(inputDataLink, value.c_str() + 1);
            } else if (key == "downloadSpreadsheet") {
                downloadSpreadsheet = stoi(value);
            } else if (key == "isConnectionSearchAlgorithm") {
                isConnectionSearchAlgorithm = stoi(value);
            } else if (key == "connectionFinderSettings.testedConnectionTime") {
                connectionFinderSettings.testedConnectionTime = stoi(value);
            } else if (key == "connectionFinderSettings.minConnectionTime") {
                connectionFinderSettings.minConnectionTime = stoi(value);
            } else if (key == "connectionFinderSettings.maxConnectionTime") {
                connectionFinderSettings.maxConnectionTime = stoi(value);
            } else if (key == "connectionFinderSettings.minDistance") {
                connectionFinderSettings.minDistance = stoi(value);
            } else if (key == "connectionFinderSettings.maxDistance") {
                connectionFinderSettings.maxDistance = stoi(value);
            } else if (key == "connectionFinderSettings.minHeightDiff") {
                connectionFinderSettings.minHeightDiff = stoi(value);
            } else if (key == "connectionFinderSettings.maxHeightDiff") {
                connectionFinderSettings.maxHeightDiff = stoi(value);
            } else if (key == "connectionFinderSettings.searchSourceNodes") {
                strcpy(connectionFinderSettings.searchSourceNodes, value.c_str() + 1);
            } else if (key == "sortConnectionSearchResultsByConnection") {
                sortConnectionSearchResultsByConnection = stoi(value);
            } else if (key == "positionReplayFile") {
                strcpy(positionReplayFile, value.c_str() + 1);
            } else if (key == "cpOrder") {
                strcpy(cpOrder, value.c_str() + 1);
            } else if (key == "outputPositionsFile") {
                strcpy(outputPositionsFile, value.c_str() + 1);
            } else if (key == "outputDistanceMatrixFile") {
                strcpy(outputDistanceMatrixFile, value.c_str() + 1);
            }
        }
    }
};

struct ConditionalCost {
    int cost;
    uint8_t srcNode;

    static constexpr auto IsRemainingClauseConstant = std::numeric_limits<uint8_t>::max();
    static constexpr auto IsRespawnClauseConstant = std::numeric_limits<uint8_t>::max() - 1;

    ConditionalCost(int cost, uint8_t srcNode = IsRemainingClauseConstant) : cost(cost), srcNode(srcNode) {}

    bool isRemainingClause() { return srcNode == IsRemainingClauseConstant; }
    bool isRespawnClause()   { return srcNode == IsRespawnClauseConstant; }
};

enum Direction { Out, In };
constexpr NodeType NullNode = NodeType(-1);
constexpr Edge NullEdge = { NullNode, NullNode };
constexpr EdgeCostType Inf = 100'000'000;

enum class Algorithm { None, Assignment, Arborescence, BruteForce, LinKernighan };

using RepeatNodesVector = FastSmallVector<NodeType, 5>;

template<typename T> struct ConditionalMatrix {
    std::vector<std::vector<std::vector<T>>> data;

    std::vector<std::vector<T>>& operator[](int i)             { return data[i]; }
    const std::vector<std::vector<T>>& operator[](int i) const { return data[i]; }
    T& withRespawn(int dst, int src)             { return data[dst][src].back(); }
    const T& withRespawn(int dst, int src) const { return data[dst][src].back(); }
};

struct InputAlgorithmData {
    std::vector<std::vector<int>> weights;
    ConditionalMatrix<int> condWeights;
    ConditionalMatrix<bool> isVerifiedConnection;
};

struct RepeatNodeMatrix {
    Vector3d<RepeatNodesVector> data;

    static std::optional<RepeatNodesVector> Combine(const RepeatNodesVector& a, NodeType val, const RepeatNodesVector& b) {
        if (a.size() + b.size() + 1 >= 5)
            return std::nullopt;
        RepeatNodesVector result;
        std::copy(&a.data[0], &a.data[a.size_], &result.data[0]);
        result.data[a.size()] = val;
        std::copy(&b.data[0], &b.data[b.size_], &result.data[a.size() + 1]);
        result.size_ = a.size() + b.size() + 1;
        return result;
    }

    RepeatNodeMatrix() {}
    RepeatNodeMatrix(int nodeCount) : data(nodeCount + 1) {} // +1 for respawn
    int sizeInlcudingRespawn() const      { return data.size(); }
    auto operator[](int i)                { return data[i]; }
    auto operator[](int i) const          { return data[i]; }
    auto& withRespawn(int dst, int src)   { return data[dst][src].back(); }
    auto& unconditional(int dst, int src) { return data[dst][src].back(); }
    bool empty() const                    { return data.empty(); }
    void clear()                          { data.clear(); }
};

enum CompressedEdgeType {
    NonRepeat             = 0b00000000,
    Repeat                = 0b00000001,
    NonSequenceDependent  = 0b00000010,
    SequenceDependentIsh  = 0b00000100,
    SequenceDependent     = 0b00001000,
    SequenceDependentMask = NonSequenceDependent | SequenceDependentIsh | SequenceDependent,
    NonSorted             = 0b00000000,
    Sorted                = 0b00010000,
};
#pragma pack(push, 1)
struct CompressedEdge {
    NodeType prev;
    NodeType src;
    NodeType dst;
};
struct CompressedEdgeNoPrev {
    NodeType src;
    NodeType dst;
};
#pragma pack(pop)

bool operator==(const std::vector<CompressedEdge>& a, const std::vector<CompressedEdge>& b) {
    if (a.size() != b.size())
        return false;
    return std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])) == 0;
}

struct SolutionEdge {
    NodeType prev;
    NodeType src;
    NodeType dst;
    RepeatNodesVector repeatNodesBeforeEdge;
};

struct SolutionVariation {
    std::vector<SolutionEdge> solution;
    std::vector<CompressedEdge> compressedSolution;
};

bool operator<(const std::vector<SolutionEdge>& a, const std::vector<SolutionEdge>& b) {
    if (a.size() != b.size())
        return a.size() < b.size();
    for (int i = 0; i < a.size(); ++i) {
        if (a[i].dst != b[i].dst) {
            return a[i].dst < b[i].dst;
        }
    }
    return false;
}

struct BestSolution {
    BestSolution() : solutionConnections(0) {}
    BestSolution(const SolutionVariation& solution, const SolutionVariation& sortedSolution, const std::vector<CompressedEdge>& compressedVariation, const FastSet2d& solutionConnections, const std::vector<std::array<int16_t, 3>>& unverifiedConnections, const std::string& solutionString, Edge addedConnection, int time) :
        compressedVariation(compressedVariation), solutionConnections(solutionConnections), unverifiedConnections(unverifiedConnections), solutionString(solutionString), addedConnection(addedConnection), time(time)
    {
        if (!sortedSolution.solution.empty()) {
            variations.push_back(sortedSolution);
        }
        variations.push_back(solution);
    }
    const std::vector<SolutionEdge>& solution() const {
        if (empty())
            return {};
        return variations[0].solution;
    }
    const std::vector<CompressedEdge>& compressedSolution() const {
        if (empty())
            return {};
        return variations[0].compressedSolution;
    }
    void clear() {
        compressedVariation.clear();
        variations.clear();
        unverifiedConnections.clear();
        solutionString.clear();
    }
    bool empty() const {
        return variations.empty();
    }
    std::vector<CompressedEdge> compressedVariation;
    std::vector<SolutionVariation> variations;
    std::vector<std::array<int16_t, 3>> unverifiedConnections;
    std::string solutionString;
    FastSet2d solutionConnections;
    Edge addedConnection;
    int time;
};
bool operator==(const BestSolution& a, const BestSolution& b) {
    return !a.empty() && !b.empty() && a.variations[0].compressedSolution == b.variations[0].compressedSolution;
}
bool operator!=(const BestSolution& a, const BestSolution& b) {
    return !(a == b);
}

struct SolutionConfig {
    std::vector<std::vector<int>> weights;
    ConditionalMatrix<int> condWeights;
    ConditionalMatrix<bool> isVerifiedConnection;
    int maxSolutionCount;
    std::atomic<int>* limit_;
    int ignoredValue;
    bool useExtendedMatrix;
    ThreadSafeVec<BestSolution> solutionsVec;
    std::vector<BestSolution> bestSolutions;
    std::string outputFileName;
    RepeatNodeMatrix repeatNodeMatrix;
    Vector3d<Bool> useRespawnMatrix;
    std::vector<int> ringCps;
    std::atomic<int128_t> partialSolutionCount;
    std::atomic<bool>& stopWorking;
    Edge addedConnection;

    std::mutex solutionUpdateMutex;
    std::mutex partialSolutionCountMutex;

    static thread_local inline int LocalPartialSolutionCount = 0;

    SolutionConfig(std::atomic<bool>& stopWorking) : stopWorking(stopWorking) {}
    SolutionConfig(const SolutionConfig& other) : stopWorking(other.stopWorking) {
        weights = other.weights;
        condWeights = other.condWeights;
        isVerifiedConnection = other.isVerifiedConnection;
        maxSolutionCount = other.maxSolutionCount;
        limit_ = other.limit_;
        ignoredValue = other.ignoredValue;
        useExtendedMatrix = other.useExtendedMatrix;
        bestSolutions = other.bestSolutions;
        outputFileName = other.outputFileName;
        repeatNodeMatrix = other.repeatNodeMatrix;
        useRespawnMatrix = other.useRespawnMatrix;
        partialSolutionCount = other.partialSolutionCount.load();
        ringCps = other.ringCps;
        addedConnection = other.addedConnection;
    }
    int nodeCount() const {
        return weights.size();
    }
    void updateLimit(int newValue) {
        limit_->store(newValue);
    }
    int limit() const {
        return limit_->load();
    }

    std::string partialSolutionCountString() {
        return boost::int128::to_string(partialSolutionCount.load());
    }
    void flushPartialSolutionCount() {
        std::scoped_lock l{ partialSolutionCountMutex };
        partialSolutionCount.store(partialSolutionCount.load() + LocalPartialSolutionCount);
        LocalPartialSolutionCount = 0;
    }
    void incrementPartialSolutionCount() {
        LocalPartialSolutionCount += 1;
        flushPartialSolutionCount();
    }
    void lazyIncrementPartialSolutionCount(int countBeforeFlush = 13) {
        LocalPartialSolutionCount += 1;
        if (LocalPartialSolutionCount == countBeforeFlush) {
            flushPartialSolutionCount();
        }
    }
};

NodeType getRespawnPrev(const SolutionConfig& config) {
    return config.nodeCount();
}
NodeType getUnconditionalPrev(const SolutionConfig& config) {
    return config.nodeCount() - 1;
}
bool isRespawn(const SolutionConfig& config, const SolutionEdge& edge) {
    return edge.prev == getRespawnPrev(config);
}

std::vector<int> parseIntList(std::string_view str, int allowedRangeMin, int allowedRangeMax, std::string listName, std::string& errorMsg) {
    std::vector<int> result;
    auto tokens = tokenize(str, { {0, "0123456789"} });
    while (!tokens.empty()) {
        result.push_back(strToInt(tokens.eat().value));
        if (result.back() < allowedRangeMin || result.back() > allowedRangeMax) {
            errorMsg = "Found value outside of allowed range [" + std::to_string(allowedRangeMin) + ", " + std::to_string(allowedRangeMax) + "] while parsing " + listName;
            return {};
        }
    }
    return result;
}
