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
#include <queue>
#include <stack>
#include <algorithm>

constexpr float MaxLimit = 1e10;
constexpr int MaxMaxSolutionCount = 1e7;

using Edge = std::pair<int, int>;
constexpr Edge NullEdge = { -1, -1 };

enum Direction {
	Out,
	In
};

constexpr float Inf = 1e10;
std::vector<std::vector<float>> M;

struct AdjList {
	std::array<std::vector<int8_t>, 2> data;
	std::array<std::vector<int8_t>, 2> sizes;
	std::array<std::vector<float>, 2> reductions;
	int n_;
	AdjList(int n=0) : n_(n) {
		data[Out].resize(n * n);
		data[In].resize(n * n);
		sizes[Out].resize(n);
		sizes[In].resize(n);
		reductions[Out].resize(n);
		reductions[In].resize(n);
	}

	void add(int i, int j) {
		add(Out, i, j);
		add(In, j, i);
	}

	void setNecessaryEdge(int i, int j) {
		for (int k = 0; k < sizes[Out][i]; ++k) {
			int x = data[Out][i * n() + k];
			removeEdge(i, x);
			k -= 1;
		}

		for (int k = 0; k < sizes[In][j]; ++k) {
			int x = data[In][j * n() + k];
			removeEdge(x, j);
			k -= 1;
		}
	}

	void removeEdge(int i, int j) {
		removeEdge(Out, i, j);
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
		for (int k = 0; k < sizes[d][i]; ++k) {
			auto j = data[d][i * n() + k];
			min = std::min(min, valueAt(d, i, j));
		}
		return min;
	}

	float reduce(Direction d, int i) {
		auto min = getMin(d, i);
		if (min != Inf) {
			reductions[d][i] += min;
		}
		return min;
	}

	Edge getPivot() {
		float bestIncrease = 0;
		Edge bestPivot = NullEdge;

		for (int i = 0; i < sizes[0].size(); ++i) {
			for (int k = 0; k < sizes[Out][i]; ++k) {
				int j = data[Out][i * n() + k];
				if (std::abs(valueAt(Out, i, j)) <= 0.01) {
					auto oldValue = M[j][i];
					M[j][i] = Inf;
					auto outMin = getMin(Out, i);
					auto inMin = getMin(In, j);
					float increase = outMin + inMin;
					if (increase > bestIncrease) {
						bestIncrease = increase;
						bestPivot = Edge(i, j);
					}
					M[j][i] = oldValue;
				}
			}
		}

		return bestPivot;
	}


	float valueAt(Direction d, int i, int j) {
		if (d == Out) {
			return M[j][i] - reductions[Out][i] - reductions[In][j];
		} else {
			return M[i][j] - reductions[In][i] - reductions[Out][j];
		}
	}
	void add(Direction d, int i, int j) {
		data[d][i * n() + sizes[d][i]] = j;
		sizes[d][i] += 1;
	}
	void removeEdge(Direction d, int i, int j) {
		for (int k = 0; k < sizes[d][i]; ++k) {
			if (data[d][i * n() + k] == j) {
				sizes[d][i] -= 1;
				std::swap(data[d][i * n() + k], data[d][i * n() + sizes[d][i]]);
				break;
			}
		}
	}
	int n() {
		return n_;
	}
};

struct PartialSolution {
	bool operator>(const PartialSolution& other) const {
		return lowerBound > other.lowerBound;
	}

	PartialSolution withEdge(Edge pivot, const std::vector<std::vector<float>>& A) {
		auto i = pivot.first;
		auto j = pivot.second;

		PartialSolution child = *this;
		child.time += A[j][i];

		child.adjList.setNecessaryEdge(i, j);
		child.edges[Out][i] = j;
		child.edges[In][j] = i;

		auto subpathTo = child.traverseSubPath(i, Out);
		auto subpathFrom = child.traverseSubPath(i, In);
		if (subpathTo.size() + subpathFrom.size() - 1 != n) {
			child.adjList.removeEdge(subpathTo.back(), subpathFrom.back());
		}

		child.reduceMatrix();

		return child;
	}

	PartialSolution withoutEdge(Edge pivot) {
		auto i = pivot.first;
		auto j = pivot.second;

		PartialSolution child = *this;
		child.adjList.removeEdge(i, j);
		child.reduceMatrix(Out, i);
		child.reduceMatrix(In, j);

		return child;
	}

	Edge choosePivotEdge() {
		return adjList.getPivot();
	}

	std::vector<int> traverseSubPath(int cur, Direction edgeType) {
		std::vector<int> subpath{ cur };
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
		auto min = adjList.reduce(edgeType, i);
		if (min != Inf)
			lowerBound += min;
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

	int IK(int i, int k, int kStride) {
		return (n + 1 - kStride) * i + kStride * k;
	}

	int n;
	float time = Inf;
	float lowerBound = 0;
	AdjList adjList;
	std::array<std::vector<int>, 2> edges;

	PartialSolution() {}
	PartialSolution(const std::vector<std::vector<float>>& A, AdjList adjList) : n(A.size()), adjList(adjList) {
		time = 0;
		
		edges[0].resize(n);
		edges[1].resize(n);

		for (int i = 0; i < n; ++i) {
			edges[0][i] = -1;
			edges[1][i] = -1;
		}

		for (int i = 0; i < A.size(); ++i) {
			for (int j = 0; j < A[i].size(); ++j) {
				if (A[j][i] == 9000) {
					adjList.removeEdge(i, j);
				}
			}
		}
		adjList.shrinkToSize();

		reduceMatrix();
	}
};

void findSolutions(
	const PartialSolution& root, 
	const std::vector<std::vector<float>>& A, 
	int maxSolutionCount, float& limit, 
	std::vector<std::pair<std::vector<int16_t>, float>>& bestSolutions, ThreadSafeVec<std::pair<std::vector<int16_t>, float>>* solutionsVec
) {
	std::priority_queue<PartialSolution, std::vector<PartialSolution>, std::greater<PartialSolution> > right;
	std::optional<PartialSolution> left = root;
	int taskCount = 0;

	while (left || !right.empty()) {
		auto currentSolution = left ? *left : right.top();
		if (left)
			left = std::nullopt;
		else
			right.pop();
		if (currentSolution.isComplete()) {
			auto solution = std::make_pair(currentSolution.getPath(), currentSolution.time);
			solutionsVec->push_back(solution);
			if (bestSolutions.size() < maxSolutionCount) {
				bestSolutions.push_back(solution);
			} else if (currentSolution.time < limit) {
				bestSolutions.back() = solution;
			}
			if (bestSolutions.size() >= maxSolutionCount) {
				std::sort(bestSolutions.begin(), bestSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
				limit = bestSolutions.back().second;
			}
		} else if (currentSolution.lowerBound < limit) {
			auto pivot = currentSolution.choosePivotEdge();
			if (pivot != NullEdge) {
				auto withPivot = currentSolution.withEdge(pivot, A);
				auto withoutPivot = currentSolution.withoutEdge(pivot);
				if (withPivot.lowerBound < limit)
					left = withPivot;
				if (withoutPivot.lowerBound < limit)
					right.push(withoutPivot);
			}
		}
	}
}

std::vector<std::pair<std::vector<int16_t>, float>> findSolutions(const std::vector<std::vector<float>>& A, int maxSolutionCount, float limit, ThreadSafeVec<std::pair<std::vector<int16_t>, float>>* solutionsVec) {
	std::vector<std::pair<std::vector<int16_t>, float>> bestSolutions;
	AdjList adjList(A.size());

	for (int i = 0; i < A.size(); ++i) {
		for (int j = 0; j < A[i].size(); ++j) {
			adjList.add(i, j);
		}
	}

	PartialSolution root = PartialSolution(A, adjList).withEdge(Edge(A.size() - 1, 0), A);
	findSolutions(root, A, maxSolutionCount, limit, bestSolutions, solutionsVec);
	std::sort(bestSolutions.begin(), bestSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
	return bestSolutions;
}

std::vector<std::vector<int>> addRepeatNodeEdges(std::vector<std::vector<float>>& A, float ignoredValue) {
	std::vector<std::vector<int>> adjList(A.size());
	for (int i = 0; i < A.size(); ++i) {
		for (int j = 0; j < A[i].size(); ++j) {
			if (A[j][i] != ignoredValue) {
				adjList[i].push_back(j);
			}
		}
	}

	std::vector<std::vector<int>> repeatEdgeMatrix(A.size(), std::vector<int>(A.size()));
	for (int i = 0; i < adjList.size(); ++i) {
		for (int j : adjList[i]) {
			for (int k : adjList[j]) {
				if (i == k)
					continue;
				float time = A[k][j] + A[j][i];
				if (time < A[k][i]) {
					repeatEdgeMatrix[k][i] = j;
					A[k][i] = time;
				}
			}
		}
	}
	
	return repeatEdgeMatrix;
}

std::vector<std::pair<std::vector<int16_t>, float>> runAlgorithm(const std::vector<std::vector<float>>& A_, float ignoredValue, float limit, int maxSolutionCount, ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& solutionsVec) {
	auto A = A_;
	A[0].back() = 0;
	M = A;
	return findSolutions(A, maxSolutionCount, limit, &solutionsVec);
}
