#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "utility.h"

struct ConditionalCost {
    int cost;
    uint8_t srcNode;

    ConditionalCost(int cost, uint8_t srcNode = std::numeric_limits<uint8_t>::max()) : cost(cost), srcNode(srcNode) {}

    bool isRemainingClause() {
        return srcNode == std::numeric_limits<uint8_t>::max();
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