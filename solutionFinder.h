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
#include "lkh.h"

constexpr float Inf = 1e10;
constexpr int RemovedEdgesSize = 4096;
constexpr float MaxLimit = 1e10;
constexpr int MaxMaxSolutionCount = 1e7;

using Edge = std::pair<int, int>;
constexpr Edge NullEdge = { -1, -1 };

enum Direction { Out, In };

struct AdjList {
	std::array<std::vector<int8_t>, 2> data;
	std::array<std::vector<int8_t>, 2> sizes;
	std::array<FreeListVector<float, 256>, 2> reductions;
	std::vector<std::vector<float>>* weights;
	int n_;
	AdjList(std::vector<std::vector<float>>* weights, int n=0) : weights(weights), n_(n) {
		data[Out].resize(n * n);
		data[In].resize(n * n);
		sizes[Out].resize(n);
		sizes[In].resize(n);
		reductions[Out].resize(n);
		reductions[In].resize(n);
		std::memset(reductions[Out].data, 0, n * sizeof(float));
		std::memset(reductions[In].data, 0, n * sizeof(float));
	}

	void add(int i, int j) {
		add(Out, i, j);
		add(In, j, i);
	}

	void setNecessaryEdge(int i, int j, FreeListVector<Edge, RemovedEdgesSize>& removedEdges) {
		for (int k = 0; k < sizes[Out][i]; ++k) {
			int x = data[Out][i * n() + k];
			removeEdge(i, x, &removedEdges);
			k -= 1;
		}

		for (int k = 0; k < sizes[In][j]; ++k) {
			int x = data[In][j * n() + k];
			removeEdge(x, j, &removedEdges);
			k -= 1;
		}
	}

	void removeEdge(int i, int j, FreeListVector<Edge, RemovedEdgesSize>* removedEdges) {
		if (removeEdge(Out, i, j)) {
			if (removedEdges) removedEdges->push_back({ i, j });
		}
		removeEdge(In, j, i);
	}

	void shrinkToSize() {
		std::array<std::vector<int8_t>, 2> newData;
		int newN = 0;
		for (int i = 0; i < sizes[0].size(); ++i) {
			newN = std::max<int>(newN, sizes[0][i]);
			newN = std::max<int>(newN, sizes[1][i]);
		}
		for (int d = 0; d < 2; ++d) {
			newData[d].resize(sizes[0].size() * newN);
			for (int i = 0; i < sizes[0].size(); ++i) {
				memcpy(&newData[d][i * newN], &data[d][i * n()], newN * sizeof(data[0][0]));
			}
		}
		data = newData;
		n_ = newN;
	}

	float getMin(Direction d, int i) {
		float min = Inf;
		auto* dataPtr = &data[d][i * n()];
		if (d == Out) {
			for (int k = 0; k < sizes[Out][i]; ++k) {
				auto j = dataPtr[k];
				min = std::min(min, (*weights)[j][i] - reductions[In][j]);
			}
		} else {
			auto& mPtr = (*weights)[i];
			for (int k = 0; k < sizes[In][i]; ++k) {
				auto j = dataPtr[k];
				min = std::min(min, mPtr[j] - reductions[Out][j]);
			}
		}
		return min - reductions[d][i];
	}

	float reduce(Direction d, int i) {
		auto min = getMin(d, i);
		reductions[d][i] += min;
		return min;
	}

	Edge getPivot() {
		float bestIncrease = 0;
		Edge bestPivot = NullEdge;

		for (int i = 0; i < sizes[0].size(); ++i) {
			for (int k = 0; k < sizes[Out][i]; ++k) {
				int j = data[Out][i * n() + k];
				if (std::abs(valueAt(Out, i, j)) <= 0.01) {
					auto oldValue = (*weights)[j][i];
					(*weights)[j][i] = Inf;
					auto outMin = getMin(Out, i);
					auto inMin = getMin(In, j);
					float increase = outMin + inMin;
					if (increase > bestIncrease) {
						bestIncrease = increase;
						bestPivot = Edge(i, j);
					}
					(*weights)[j][i] = oldValue;
				}
			}
		}

		return bestPivot;
	}


	float valueAt(Direction d, int i, int j) {
		if (d == Out) {
			return (*weights)[j][i] - reductions[Out][i] - reductions[In][j];
		} else {
			return (*weights)[i][j] - reductions[In][i] - reductions[Out][j];
		}
	}
	void add(Direction d, int i, int j) {
		data[d][i * n() + sizes[d][i]] = j;
		sizes[d][i] += 1;
	}
	bool removeEdge(Direction d, int i, int j) {
		for (int k = 0; k < sizes[d][i]; ++k) {
			if (data[d][i * n() + k] == j) {
				sizes[d][i] -= 1;
				std::swap(data[d][i * n() + k], data[d][i * n() + sizes[d][i]]);
				return true;
			}
		}
		return false;
	}
	int n() {
		return n_;
	}
	int size() {
		return sizes[Out].size();
	}
};

struct PartialSolution {
	bool operator>(const PartialSolution& other) const {
		return lowerBound > other.lowerBound;
	}

	struct SavedState {
		float time;
		float lowerBound;
		std::array<FreeListVector<float, 256>, 2> reductions;
		std::array<FreeListVector<int, 256>, 2> edges;
		FreeListVector<Edge, RemovedEdgesSize> removedEdges;
	};

	SavedState saveState() {
		return SavedState{ time, lowerBound, adjList.reductions, edges };
	}
	void restoreState(SavedState& state) {
		time = state.time;
		lowerBound = state.lowerBound;
		adjList.reductions = state.reductions;
		edges = state.edges;
		for (auto [i, j] : state.removedEdges) {
			adjList.add(i, j);
		}
	}

	SavedState addEdge(Edge pivot, const std::vector<std::vector<float>>& A) {
		auto savedState = saveState();

		auto i = pivot.first;
		auto j = pivot.second;
		time += A[j][i];
		adjList.setNecessaryEdge(i, j, savedState.removedEdges);
		edges[Out][i] = j;
		edges[In][j] = i;

		auto subpathTo = traverseSubPath(i, Out);
		auto subpathFrom = traverseSubPath(i, In);
		if (subpathTo.size() + subpathFrom.size() - 1 != n) {
			adjList.removeEdge(subpathTo.back(), subpathFrom.back(), &savedState.removedEdges);
		}

		for (auto [k, m] : savedState.removedEdges) {
			if (std::abs(adjList.valueAt(Out, k, m)) <= 0.01) {
				if (k != i) reduceMatrix(Out, k);
				if (m != j) reduceMatrix(In, m);
			}
		}

		return savedState;
	}

	SavedState removeEdge(Edge pivot) {
		auto savedState = saveState();

		auto i = pivot.first;
		auto j = pivot.second;

		adjList.removeEdge(i, j, &savedState.removedEdges);
		if (std::abs(adjList.valueAt(Out, i, j)) <= 0.01) {
			reduceMatrix(Out, i);
			reduceMatrix(In, j);
		}

		return savedState;
	}

	Edge choosePivotEdge() {
		return adjList.getPivot();
	}

	FreeListVector<int, 256> traverseSubPath(int cur, Direction edgeType) {
		FreeListVector<int, 256> subpath;
		subpath.push_back(cur);
		for (int k = 0; k < n; ++k) {
			auto next = edges[int(edgeType)][cur];
			if (next == -1) {
				break;
			}
			subpath.push_back(next);
			cur = next;
		}
		return subpath;
	}

	void reduceMatrix(Direction edgeType, int i) {
		lowerBound += adjList.reduce(edgeType, i);
	}

	void reduceMatrix() {
		for (int i = 0; i < n; ++i)
			reduceMatrix(Out, i);
		for (int j = 0; j < n; ++j)
			reduceMatrix(In, j);
	}

	bool isComplete() {
		auto path = traverseSubPath(0, Out);
		return path.size() == n + 1 && path[n - 1] == n - 1;
	}

	std::vector<int16_t> getPath() {
		std::vector<int16_t> result;
		auto path = traverseSubPath(0, Out);
		for (int i = 1; i < path.size() - 1; ++i) {
			result.push_back(path[i]);
		}
		return result;
	}

	int n;
	float time = Inf;
	float lowerBound = 0;
	AdjList adjList;
	std::array<FreeListVector<int, 256>, 2> edges;

	PartialSolution(const std::vector<std::vector<float>>& A, AdjList adjList, float ignoredValue) : n(A.size()), adjList(adjList) {
		time = 0;
		
		edges[0].resize(n);
		edges[1].resize(n);

		for (int i = 0; i < n; ++i) {
			edges[0][i] = -1;
			edges[1][i] = -1;
		}

		for (int i = 0; i < A.size(); ++i) {
			for (int j = 0; j < A[i].size(); ++j) {
				if (A[j][i] >= ignoredValue) {
					this->adjList.removeEdge(i, j, nullptr);
				}
			}
		}
		this->adjList.shrinkToSize();

		reduceMatrix();
	}
};

struct SolutionConfig {
	SolutionConfig(
		const std::vector<std::vector<float>>& weights, int maxSolutionCount, float& limit,
		ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& solutionsVec, 
		const std::string& appendFileName, const std::string& outputFileName,
		const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix, 
		std::atomic<int>& partialSolutionCount, std::atomic<bool>& taskWasCanceled)
		: 
		weights(weights), maxSolutionCount(maxSolutionCount), limit(limit), solutionsVec(solutionsVec),
		outputFileName(outputFileName), appendFileName(appendFileName), repeatNodeMatrix(repeatNodeMatrix),
		partialSolutionCount(partialSolutionCount), taskWasCanceled(taskWasCanceled)
	{}

	std::vector<std::vector<float>> weights;
	int maxSolutionCount;
	float& limit;
	ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& solutionsVec;
	const std::string& appendFileName;
	const std::string& outputFileName;
	const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix;
	std::atomic<int>& partialSolutionCount;
	std::atomic<bool>& taskWasCanceled;
};

void findSolutions(SolutionConfig& config, PartialSolution& currentSolution, std::vector<std::pair<std::vector<int16_t>, float>>& bestSolutions) {
	config.partialSolutionCount += 1;
	if (config.taskWasCanceled)
		return;
	if (currentSolution.isComplete()) {
		auto solution = std::make_pair(currentSolution.getPath(), currentSolution.time);
		
		config.solutionsVec.push_back_not_thread_safe(solution);
		writeSolutionToFile(config.appendFileName, solution.first, currentSolution.time, config.repeatNodeMatrix);
		writeSolutionToFile(config.outputFileName, solution.first, currentSolution.time, config.repeatNodeMatrix);

		if (bestSolutions.size() < config.maxSolutionCount) {
			bestSolutions.push_back(solution);
		} else if (currentSolution.time < config.limit) {
			bestSolutions.back() = solution;
		}
		if (bestSolutions.size() >= config.maxSolutionCount) {
			std::sort(bestSolutions.begin(), bestSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
			config.limit = bestSolutions.back().second;
		}
	} else if (currentSolution.lowerBound < config.limit) {
		auto pivot = currentSolution.choosePivotEdge();
		if (pivot != NullEdge) {
			auto savedState = currentSolution.addEdge(pivot, config.weights);
			if (currentSolution.lowerBound < config.limit) {
				findSolutions(config, currentSolution, bestSolutions);
			}
			currentSolution.restoreState(savedState);

			savedState = currentSolution.removeEdge(pivot);
			if (currentSolution.lowerBound < config.limit) {
				findSolutions(config, currentSolution, bestSolutions);
			}
			currentSolution.restoreState(savedState);
		}
	}
}


struct RepeatEdgePath {
	RepeatEdgePath(int k, int j, int i) : k(k), j(j), i(i) {}
	int k, j, i;
	float time(const std::vector<std::vector<float>>& A) const {
		return A[k][j] + A[j][i];
	}
};
std::vector<RepeatEdgePath> getRepeatNodeEdges(const std::vector<std::vector<float>>& A, float ignoredValue, std::vector<int> turnedOffRepeatNodes) {
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
				float time = A[k][j] + A[j][i];
				if (time < A[k][i]) {
					if (std::find(turnedOffRepeatNodes.begin(), turnedOffRepeatNodes.end(), j) == turnedOffRepeatNodes.end())
						additionalPaths.emplace_back(k, j, i);
				}
			}
		}
	}
	std::sort(additionalPaths.begin(), additionalPaths.end(), [&](auto& a, auto& b) { return a.time(A) < b.time(A); });
	return additionalPaths;
}

int addRepeatNodeEdges(std::vector<std::vector<float>>& A, std::vector<std::vector<std::vector<int>>>& repeatEdgeMatrix, const std::vector<RepeatEdgePath>& additionalPaths, int maxEdgesToAdd) {
	auto ACopy = A;
	int addedEdgesCount = 0;
	for (int m = 0; m < additionalPaths.size() && addedEdgesCount < maxEdgesToAdd; ++m) {
		auto k = additionalPaths[m].k;
		auto j = additionalPaths[m].j;
		auto i = additionalPaths[m].i;
		addedEdgesCount += repeatEdgeMatrix[k][i].empty();
		repeatEdgeMatrix[k][i] = repeatEdgeMatrix[j][i];
		repeatEdgeMatrix[k][i].push_back(j);
		repeatEdgeMatrix[k][i].insert(repeatEdgeMatrix[k][i].end(), repeatEdgeMatrix[k][j].begin(), repeatEdgeMatrix[k][j].end());
		ACopy[k][i] = additionalPaths[m].time(A);
	}
	A = ACopy;
	return addedEdgesCount;
}

std::vector<std::vector<std::vector<int>>> addRepeatNodeEdges(std::vector<std::vector<float>>& A, float ignoredValue, int maxEdgesToAdd, std::vector<int> turnedOffRepeatNodes) {
	std::vector<std::vector<std::vector<int>>> repeatEdgeMatrix(A.size(), std::vector<std::vector<int>>(A.size()));
	for (int i = 0; i < 5; ++i) {
		auto repeatEdges = getRepeatNodeEdges(A, ignoredValue, turnedOffRepeatNodes);
		auto edgesAdded = addRepeatNodeEdges(A, repeatEdgeMatrix, repeatEdges, maxEdgesToAdd);
		maxEdgesToAdd = std::max<int>(0, maxEdgesToAdd - edgesAdded);
	}
	return repeatEdgeMatrix;
}

int countRepeatNodeEdges(const std::vector<std::vector<float>>& A, float ignoredValue, std::vector<int> turnedOffRepeatNodes) {
	std::vector<std::vector<std::vector<int>>> repeatEdgeMatrix(A.size(), std::vector<std::vector<int>>(A.size()));
	auto A_ = A;
	for (int i = 0; i < 5; ++i) {
		auto repeatEdges = getRepeatNodeEdges(A_, ignoredValue, turnedOffRepeatNodes);
		addRepeatNodeEdges(A_, repeatEdgeMatrix, repeatEdges, std::numeric_limits<int>::max());
	}
	int repeatEdgesCount = 0;
	for (int i = 0; i < A.size(); ++i) {
		for (int j = 0; j < A.size(); ++j) {
			repeatEdgesCount += !repeatEdgeMatrix[i][j].empty();
		}
	}
	return repeatEdgesCount;
}


void runAlgorithm(
	const std::vector<std::vector<float>>& A_, int maxSolutionCount, float limit, float ignoredValue, 
	ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& solutionsVec,
	const std::string& appendFileName, const std::string& outputFileName,
	const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix,
	std::atomic<int>& partialSolutionCount, std::atomic<bool>& taskWasCanceled
) {
	auto A = A_;
	A[0].back() = 0;
	
	std::vector<std::pair<std::vector<int16_t>, float>> bestSolutions;
	AdjList adjList(&A, A.size());

	for (int i = 0; i < A.size(); ++i) {
		for (int j = 0; j < A[i].size(); ++j) {
			adjList.add(i, j);
		}
	}

	PartialSolution root = PartialSolution(A, adjList, ignoredValue);
	root.addEdge(Edge(A.size() - 1, 0), A);
	SolutionConfig config(A, maxSolutionCount, limit, solutionsVec, appendFileName, outputFileName, repeatNodeMatrix, partialSolutionCount, taskWasCanceled);
	findSolutions(config, root, bestSolutions);
}


std::pair<std::vector<int16_t>, float> runHlk(SolutionConfig& config, std::vector<std::vector<float>> weights, LkhSharedMemoryManager& sharedMemory, const char* programPath, std::mutex& fileWriteMutex) {
	auto solution = runLkhInChildProcess(sharedMemory, weights, config.taskWasCanceled);
	if (solution.empty()) {
		return { {}, 0 };
	}

	float time = 0;
	for (int i = 1; i < solution.size(); ++i) {
		time += weights[solution[i]][solution[i - 1]];
	}
	solution.erase(solution.begin());
	if (std::find(solution.begin(), solution.end(), 0) != solution.end()) {
		int x = 0;
	}

	if (time < config.limit) {
		std::scoped_lock l{ fileWriteMutex };
		for (int i = 0; i < config.solutionsVec.size(); ++i) {
			auto& vec = config.solutionsVec[i].first;
			if (solution == vec) {
				return { {}, time };
			}
		}

		config.solutionsVec.push_back({ solution, time });
		writeSolutionToFile(config.appendFileName, solution, time, config.repeatNodeMatrix);
		writeSolutionToFile(config.outputFileName, solution, time, config.repeatNodeMatrix);
	}
	return { solution, time };
}

void runHlkRecursive(SolutionConfig& config, std::vector<std::vector<float>> weights, ThreadPool& threadPool, std::vector<LkhSharedMemoryManager>& sharedMemory, const char* programPath, std::mutex& writeFileMutex, float ignoredValue, int maxDepth, int currentDepth=0) {
	if (config.taskWasCanceled)
		return;
	
	std::vector<int16_t> solution;
	float time = 0;
	int tryCount = 0;
	do {
		auto [s, t] = runHlk(config, weights, sharedMemory.back(), programPath, writeFileMutex);
		solution = s;
		time = t;
		if (!solution.empty() && time <= config.limit)
			break;
		tryCount += 1;
	} while ((solution.empty() || time > config.limit) && tryCount < 3);
	config.partialSolutionCount += 1;
	
	if (currentDepth >= maxDepth) {
		return;
	}
	if (solution.empty() || time > config.limit) {
		config.partialSolutionCount += std::pow(config.weights.size() - 1, maxDepth - currentDepth);
		return;
	}

	solution.insert(solution.begin(), 0);
	for (int i = 1; i < solution.size(); ++i) {
		if (config.taskWasCanceled)
			return;
		if (currentDepth >= maxDepth - 1) {
			threadPool.addTask([&config, &writeFileMutex, weights=weights, solution, i, programPath, ignoredValue, &sharedMemory](int id) mutable {
				if (config.taskWasCanceled)
					return;
				auto oldValue = weights[solution[i]][solution[i - 1]];
				weights[solution[i]][solution[i - 1]] = ignoredValue;
				runHlk(config, weights, sharedMemory[id], programPath, writeFileMutex);
				config.partialSolutionCount += 1;
				weights[solution[i]][solution[i - 1]] = oldValue;
			});
		} else {
			auto oldValue = weights[solution[i]][solution[i - 1]];
			weights[solution[i]][solution[i - 1]] = ignoredValue;
			runHlkRecursive(config, weights, threadPool, sharedMemory, programPath, writeFileMutex, ignoredValue, maxDepth, currentDepth + 1);
			weights[solution[i]][solution[i - 1]] = oldValue;
		}
	}
}

void runAlgorithmHlk(const std::vector<std::vector<float>>& A_, const char* programPath, int searchDepth,
	const std::string& appendFile, const std::string& outputFile, float ignoredValue, float limit, ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& solutionsVec,
	const std::vector<std::vector<std::vector<int>>>& repeatNodeMatrix, std::atomic<int>& partialSolutionCount, std::atomic<bool>& taskWasCanceled
) {
	auto A = A_;
	A[0].back() = 0;

	int threadCount = 16;
	auto config = SolutionConfig(A, 0, limit, solutionsVec, appendFile, outputFile, repeatNodeMatrix, partialSolutionCount, taskWasCanceled);
	ThreadPool threadPool(threadCount, 1000);
	std::mutex writeFileMutex;
	auto sharedMemInstances = createLkhSharedMemoryInstances(threadCount + 1, A.size()); // +1 for main thread
	auto processes = startChildProcesses(programPath, sharedMemInstances);
	runHlkRecursive(config, A, threadPool, sharedMemInstances, programPath, writeFileMutex, ignoredValue, searchDepth);
	threadPool.wait();
	stopChildProcesses(processes, sharedMemInstances);
}
