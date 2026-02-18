#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include "utility.h"

namespace fs = std::filesystem;

enum class Algorithm { None, Assignment, LinKernighan };

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

    InputData() {
        strcpy(outputPositionsFile, "CP_positions.txt");
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
            }
        }
    }
};

struct BestSolution {
    BestSolution() : solutionConnections(0) {}
    BestSolution(const std::vector<int16_t>& solution, const std::vector<int16_t>& sortedSolution, const std::vector<int16_t>& solutionWithRepeats, const FastSet2d& solutionConnections, const std::string& solutionString, Edge addedConnection, int time) :
        solution(sortedSolution), solutionConnections(solutionConnections), solutionString(solutionString), addedConnection(addedConnection), time(time)
    {
        allVariations.push_back(solution);
        variations.push_back(sortedSolution);
        variationsWithRepeats.push_back(solutionWithRepeats);
    }
    std::vector<int16_t> solution;
    std::vector<std::vector<int16_t>> allVariations;
    std::vector<std::vector<int16_t>> variations;
    std::vector<std::vector<int16_t>> variationsWithRepeats;
    std::string solutionString;
    FastSet2d solutionConnections;
    Edge addedConnection;
    int time;
};

struct FilterConnection {
    enum Status {
        Required, Banned, Optional, AutoBanned
    };
    FilterConnection() {}
    FilterConnection(std::pair<int, int> connection, Status status) : connection(connection), status(status) {}

    std::pair<int, int> connection;
    Status status;
};

struct State {
    fs::path workingDir;

    std::string errorMsg;
    std::vector<BestSolution> bestFoundSolutions;

    std::future<void> algorithmRunTask;
    std::atomic<bool> taskWasCanceled = false;

    std::vector<int> calculatedCpOrder;
    double outputRouteCalcTime = 0;

    Timer timer;

    std::vector<Position> calculatedCpPositions;
    std::vector<Position> pathToVisualize;

    Algorithm currentAlgorithm = Algorithm::None;
    bool endedWithTimeout = false;
    std::thread timerThread;

    bool isGraphWindowOpen = false;
    std::vector<int16_t> solutionToShowInGraphWindow;
    std::vector<int16_t> solutionToCompareToInGraphWindow;
    int solutionToShowId = -1;
    int solutionToCompareId = -1;
    std::vector<Position> cpPositionsVis;
    bool realCpPositionView = false;

    bool isVariationsWindowOpen = false;
    BestSolution solutionToShowVariation;
    int solutionToShowVariationId = -1;
    bool showRepeatNodeVariations = true;

    std::pair<NodeType, NodeType> hoveredConnection = { 0, 0 };

    bool isOnPathFinderTab = true;

    bool copiedBestSolutionsAfterAlgorithmDone = false;

    std::vector<Edge> connectionsToTest;

    std::vector<FilterConnection> resultRequiredConnections;
    std::vector<FilterConnection> resultOptionalConnections;

    State() {
        timer = Timer();
        timer.stop();
    }
};

struct ConditionalCost {
    int cost;
    uint8_t srcNode;

    ConditionalCost(int cost, uint8_t srcNode = std::numeric_limits<uint8_t>::max()) : cost(cost), srcNode(srcNode) {}

    bool isRemainingClause() {
        return srcNode == std::numeric_limits<uint8_t>::max();
    }
};


struct SolutionConfig {
    std::vector<std::vector<int>> weights;
    std::vector<std::vector<std::vector<int>>> condWeights;
    int maxSolutionCount;
    int limit;
    int ignoredValue;
    bool useExtendedMatrix;
    ThreadSafeVec<BestSolution> solutionsVec;
    std::vector<BestSolution> bestSolutions;
    std::string outputFileName;
    Vector3d<FastSmallVector<uint8_t>> repeatNodeMatrix;
    Vector3d<Bool> useRespawnMatrix;
    std::atomic<int64_t> partialSolutionCount;
    std::atomic<bool>& stopWorking;
    Edge addedConnection;

    SolutionConfig(std::atomic<bool>& stopWorking) : stopWorking(stopWorking) {}
    SolutionConfig(const SolutionConfig& other) : stopWorking(other.stopWorking) {
        weights = other.weights;
        condWeights = other.condWeights;
        maxSolutionCount = other.maxSolutionCount;
        limit = other.limit;
        ignoredValue = other.ignoredValue;
        useExtendedMatrix = other.useExtendedMatrix;
        bestSolutions = other.bestSolutions;
        outputFileName = other.outputFileName;
        repeatNodeMatrix = other.repeatNodeMatrix;
        useRespawnMatrix = other.useRespawnMatrix;
        partialSolutionCount = other.partialSolutionCount.load();
        addedConnection = other.addedConnection;
    }
};

std::string createSolutionString(const std::vector<int16_t>& solution, const Vector3d<FastSmallVector<uint8_t>>& repeatNodeMatrix, const Vector3d<Bool>& useRespawnMatrix) {
    auto B = solution;
    B.insert(B.begin(), 0); // Explicit start node
    B.insert(B.begin(), int16_t(useRespawnMatrix.size() - 1)); // Added for convenience for useRespawnMatrix and repeatNodeMatrix

    std::string solStr = "[";
    solStr += "Start";
    for (int i = 2; i < B.size(); ++i) {
        if (useRespawnMatrix[B[i]][B[i - 1]][B[i - 2]]) {
            solStr += ",R";
        }
        if (!repeatNodeMatrix.empty() && !repeatNodeMatrix[B[i]][B[i - 1]][B[i - 2]].empty()) {
            auto& repeatNodes = repeatNodeMatrix[B[i]][B[i - 1]][B[i - 2]];
            solStr += ",(";
            solStr += std::to_string(repeatNodes[0]);
            for (int i = 1; i < repeatNodes.size(); ++i) {
                solStr += "," + std::to_string(repeatNodes[i]);
            }
            solStr += "),";
        } else if (B[i] == B[i - 1] + 1 && !useRespawnMatrix[B[i]][B[i - 1]][B[i - 2]]) {
            solStr += '-';
            i += 1;
            while (i < B.size() && B[i] == B[i - 1] + 1 && !useRespawnMatrix[B[i]][B[i - 1]][B[i - 2]]) {
                i += 1;
            }
            i -= 1;
        } else {
            solStr += ',';
        }
        if (i == B.size() - 1)
            solStr += "Finish";
        else
            solStr += std::to_string(B[i]);
    }
    solStr += "]";
    return solStr;
}


struct Token {
    Token(int type, int value=0) : type(type), value(value) {}
    int value;
    int type;
    enum Type { Number, RepeatNumber, Dash, Comma };
};

int readNumber(const std::string& str, int& i) {
    int start = i;
    while (std::isdigit(str[i])) {
        i += 1;
    }
    int number = std::stoi(str.substr(start, i - start));
    i -= 1;
    return number;
}
std::pair<std::vector<int16_t>, double> createSolutionFromString(std::string solStr, const SolutionConfig& config) {
    std::vector<Token> tokens;
    auto ErrorValue = std::pair<std::vector<int16_t>, double>{ {}, -1.0 };

    try {
        if (solStr.substr(0, 6) != "[Start")
            return ErrorValue;
        if (solStr.substr(solStr.size() - 7) != "Finish]")
            return ErrorValue;
        solStr = solStr.substr(5);
        solStr[0] = '0';

        int i = 0;
        while (i < solStr.size()) {
            char c = solStr[i];
            if (c == '-') {
                tokens.emplace_back(Token::Dash);
            } else if (c == ',') {
                if (solStr[i + 1] == '(') {
                    i += 2;
                    while (true) {
                        tokens.emplace_back(Token::RepeatNumber, readNumber(solStr, i));
                        i += 1;
                        if (solStr[i] == ')')
                            break;
                        i += 1;
                    }
                } else {
                    tokens.emplace_back(Token::Comma);
                }
            } else if (c == 'R') {
                i += 1;
            } else if (c == 'F') {
                tokens.emplace_back(Token::Number, config.weights.size() - 1);
                break;
            } else {
                tokens.emplace_back(Token::Number, readNumber(solStr, i));
            }
            i += 1;
        }

        std::vector<int16_t> solution;
        i = 0;
        while (i < tokens.size()) {
            if (tokens[i].type == Token::Number) {
                solution.push_back(tokens[i].value);
            } else if (tokens[i].type == Token::Dash) {
                i += 1;
                while (solution.back() != tokens[i].value) {
                    solution.push_back(solution.back() + 1);
                }
            }
            i += 1;
        }

        int realCost = 0;
        for (int i = 1; i < solution.size(); ++i) {
            auto connectionCost = config.condWeights[solution[i]][solution[i - 1]][i > 1 ? solution[i - 2] : 0];
            if (connectionCost >= config.ignoredValue)
                return ErrorValue;
            realCost += connectionCost;
        }

        solution.erase(solution.begin());
        return { solution, realCost / 10.0 };
    } catch (std::exception _) {
        return ErrorValue;
    }
}