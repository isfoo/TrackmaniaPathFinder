#pragma once
#include "utility.h"
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

constexpr float MaxLimit = 1e10;
constexpr int MaxMaxSolutionCount = 1e10;

struct ConstantData {
	float limit;
	std::vector<std::vector<float>> A;
	ThreadPool threadPool;
	std::mutex mutex;
	std::vector<std::vector<int>> adjList;
	std::vector<std::vector<int>> revAdjList;
	std::vector<std::pair<std::vector<int16_t>, float>> solutions;
	int maxSolutionCount;
	ThreadSafeVec<std::pair<std::vector<int16_t>, float>>* solutionsVec;
};

struct VariableData {
	std::vector<int16_t> B;
	DynamicBitset Bvisited;
	std::vector<int> inCounts; 
	std::vector<int> outCounts;
};

struct CopyData {
	int x = 0;
	int countR = 0;
	int adjM = 1;
	float timeA = 0;
	bool parallalize = true;
};

int dfs(int x, DynamicBitset& visited, std::vector<std::vector<int>>& adjList) {
	int visitedCount = 1;
	visited.set(x);
	for (auto i : adjList[x]) {
		if (!visited.test(i)) {
			visitedCount += dfs(i, visited, adjList);
		}
	}
	return visitedCount;
}
int countReachableNodes(int node, const DynamicBitset& Bvisited, std::vector<std::vector<int>>& adjList) {
	auto visited = Bvisited;
	return dfs(node, visited, adjList);
}

void findSolutions(ConstantData& constData, VariableData& variableData, const CopyData& copyData = CopyData()) {
	auto& [limit, A, threadPool, mutex, adjList, revAdjList, solutions, maxSolutionCount, solutionsVec] = constData;
	auto& [B, Bvisited, inCounts, outCounts] = variableData;
	auto& [x, countR, adjM, timeA, parallalize] = copyData;

	if (countR == A.size() - 1) {
		std::unique_lock l{ mutex };
		if (maxSolutionCount != MaxMaxSolutionCount && solutions.size() == maxSolutionCount) {
			if (timeA >= solutions.back().second) {
				return;
			}
			solutionsVec->push_back({ B, timeA });
			solutions.back() = { B, timeA };
			std::sort(solutions.begin(), solutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
			limit = solutions.back().second;
		} else {
			solutions.push_back({ B, timeA });
			solutionsVec->push_back({ B, timeA });
		}
		return;
	}
	int necessaryY = -1;
	for (int y : adjList[x]) {
		if (!Bvisited.test(y) && inCounts[y] == 1) {
			if (necessaryY != -1) {
				return;
			}
			necessaryY = y;
		}
	}
	for (int a : revAdjList[x]) {
		if (!Bvisited.test(a)) {
			if (outCounts[a] <= 1) {
				return;
			}
		}
	}
	float minIn = 0;
	float minOut = 0;
	for (int i = 0; i < adjList.size(); ++i) {
		if (!Bvisited.test(i)) {
			float minValue = 1e10;
			for (int j : revAdjList[i]) {
				if (!Bvisited.test(j) || j == x)
					minValue = std::min(minValue, A[i][j]);
			}
			minIn += minValue;
			if (i != adjList.size() - 1) {
				float minValue = 1e10;
				for (int j : adjList[i]) {
					if (!Bvisited.test(j))
						minValue = std::min(minValue, A[j][i]);
				}
				minOut += minValue;
			}
		}
	}
	if (timeA + std::max(minIn, minOut) > limit)
		return;
	for (int a : revAdjList[x]) {
		outCounts[a] -= 1;
	}
	for (int y : adjList[x]) {
		inCounts[y] -= 1;
	}
	for (int i = 0; i < adjList[x].size(); ++i) {
		int y = adjList[x][i];
		if (necessaryY != -1 && y != necessaryY)
			continue;
		if (Bvisited.test(y) || (countR % 10 == 9 && countReachableNodes(y, Bvisited, adjList) + countR < A.size() - 1))
			continue;
		B[countR] = y;
		Bvisited.set(y);
		if (parallalize && adjM >= 100) {
			threadPool.addTask([&constData, variableData, y, countR, timeA = timeA + A[y][x]]() mutable {
				findSolutions(constData, variableData, { y, countR + 1, 1, timeA, false });
			});
		} else {
			findSolutions(constData, variableData, { y, countR + 1, int(adjM * adjList[x].size()), timeA + A[y][x], parallalize });
		}
		B[countR] = -1;
		Bvisited.reset(y);
	}
	for (int y : adjList[x]) {
		inCounts[y] += 1;
	}
	for (int a : revAdjList[x]) {
		outCounts[a] += 1;
	}
}

std::vector<std::pair<std::vector<int16_t>, float>> runAlgorithm(const std::vector<std::vector<float>>& A_, float ignoredValue, float _limit, int _maxSolutionCount, ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& _solutionsVec) {
	ConstantData constData;
	VariableData variableData;
	auto& [limit, A, threadPool, mutex, adjList, revAdjList, solutions, maxSolutionCount, solutionsVec] = constData;
	auto& [B, Bvisited, inCounts, outCounts] = variableData;

	limit = _limit;
	maxSolutionCount = _maxSolutionCount;
	A = A_;
	solutionsVec = &_solutionsVec;

	// Create adjList
	adjList.resize(A.size());
	for (int i = 0; i < A.size(); ++i) {
		for (int j = 0; j < A[i].size(); ++j) {
			if (A[j][i] != ignoredValue) {
				adjList[i].push_back(j);
			}
		}
	}

	// remove unnecessary connections
	inCounts.resize(A.size(), 0);
	for (int i = 0; i < adjList.size(); ++i) {
		for (int j : adjList[i]) {
			inCounts[j] += 1;
		}
	}

	bool removedConnection = true;
	while (removedConnection) {
		removedConnection = false;
		for (int i = 0; i < adjList.size(); ++i) {
			for (int j : adjList[i]) {
				if (adjList[i].size() == 1)
					continue;
				if (inCounts[j] == 1) {
					for (int k : adjList[i]) {
						if (k != j)
							inCounts[k] -= 1;
					}
					adjList[i] = { j };
					removedConnection = true;
					break;
				}
			}
		}
	}

	outCounts.resize(A.size(), 0);
	for (int i = 0; i < adjList.size(); ++i) {
		outCounts[i] = adjList[i].size();
	}

	revAdjList.resize(A.size());
	for (int i = 0; i < adjList.size(); ++i) {
		for (int j : adjList[i]) {
			revAdjList[j].push_back(i);
		}
	}

	// run algorithm
	B.resize(A.size() - 1, -1);
	Bvisited = DynamicBitset(A.size());
	Bvisited.set(0);
	findSolutions(constData, variableData);
	threadPool.completeTasksAndStop();
	std::sort(solutions.begin(), solutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
	return solutions;
}
