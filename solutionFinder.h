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

enum class EdgeType {
	Outgoing,
	Incoming
};

struct PartialSolution {
	constexpr static float Inf = 1e10;

	bool operator>(const PartialSolution& other) const {
		return lowerBound > other.lowerBound;
	}

	PartialSolution withEdge(Edge pivot, const std::vector<std::vector<float>>& A) {
		auto i = pivot.first;
		auto j = pivot.second;

		PartialSolution child = *this;
		child.time += A[j][i];
		for (int k = 0; k < n; ++k) {
			child.matrix[i * n + k] = Inf;
			child.matrix[k * n + j] = Inf;
		}
		child.edges[int(EdgeType::Outgoing)][i] = j;
		child.edges[int(EdgeType::Incoming)][j] = i;

		auto subpathTo = child.traverseSubPath(i, EdgeType::Outgoing);
		auto subpathFrom = child.traverseSubPath(i, EdgeType::Incoming);
		if (subpathTo.size() + subpathFrom.size() - 1 != n) {
			child.matrix[subpathTo.back() * n + subpathFrom.back()] = Inf;
		}

		child.reduceMatrix();
		return child;
	}

	PartialSolution withoutEdge(Edge pivot) {
		auto i = pivot.first;
		auto j = pivot.second;

		PartialSolution child = *this;
		child.matrix[i * n + j] = Inf;
		child.reduceMatrix(EdgeType::Outgoing, i);
		child.reduceMatrix(EdgeType::Incoming, j);

		return child;
	}

	Edge choosePivotEdge() {
		auto minStride = [&](int except, int k, int kStride) {
			float m = Inf;
			for (int i = 0; i < n; i++)
				if (i != except)
					m = std::min(m, *(matrix.data() + IK(i, k, kStride)));
			return m;
		};
		auto rowMin = [&](int k, int except) { return minStride(except, k, n); };
		auto columnMin = [&](int k, int except) { return minStride(except, k, 1); };

		float bestIncrease = 0;
		Edge bestPivot = NullEdge;
		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < n; ++j) {
				if (matrix[i * n + j] == 0) {
					auto increase = rowMin(i, j) + columnMin(j, i);
					if (increase > bestIncrease) {
						bestIncrease = increase;
						bestPivot = Edge(i, j);
					}
				}
			}
		}
		return bestPivot;
	}

	std::vector<int> traverseSubPath(int cur, EdgeType edgeType) {
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

	void reduceMatrix(EdgeType edgeType, int i) {
		auto kStride = edgeType == EdgeType::Outgoing ? 1 : n;
		float m = Inf;
		for (int k = 0; k < n; ++k) {
			m = std::min(m, *(matrix.data() + IK(i, k, kStride)));
		}
		if (m != Inf) {
			for (int k = 0; k < n; ++k) {
				auto& val = *(matrix.data() + IK(i, k, kStride));
				if (val != Inf)
					val -= m;
			}
			lowerBound += m;
		}
	}

	void reduceMatrix() {
		for (int i = 0; i < n; ++i)
			reduceMatrix(EdgeType::Outgoing, i);
		for (int j = 0; j < n; ++j)
			reduceMatrix(EdgeType::Incoming, j);
	}

	bool isComplete() {
		auto path = traverseSubPath(0, EdgeType::Outgoing);
		return path.size() == n + 1 && path[n - 1] == n - 1;
	}

	std::vector<int16_t> getPath() {
		std::vector<int16_t> result;
		auto path = traverseSubPath(0, EdgeType::Outgoing);
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
	std::vector<float> matrix;
	std::array<std::vector<int>, 2> edges;

	PartialSolution() {}
	PartialSolution(const std::vector<std::vector<float>>& A) : n(A.size()), matrix(n* n) {
		for (int i = 0; i < A.size(); ++i) {
			for (int j = 0; j < A.size(); ++j) {
				matrix[i * n + j] = A[j][i];
			}
		}

		edges[0].resize(n);
		edges[1].resize(n);
		for (int i = 0; i < n; ++i) {
			matrix[i + i * n] = Inf;
			edges[0][i] = -1;
			edges[1][i] = -1;
		}
		time = 0;
		reduceMatrix();
	}
};

std::vector<std::pair<std::vector<int16_t>, float>> findSolutions(const std::vector<std::vector<float>>& A, int maxSolutionCount, float limit, ThreadSafeVec<std::pair<std::vector<int16_t>, float>>* solutionsVec) {
	std::vector<std::pair<std::vector<int16_t>, float>> bestSolutions;
	PartialSolution root = PartialSolution(A).withEdge(Edge(A.size() - 1, 0), A);

	std::priority_queue<PartialSolution, std::vector<PartialSolution>, std::greater<PartialSolution> > right;
	std::stack<PartialSolution> left;
	left.push(root);

	while (!left.empty() || !right.empty()) {
		auto currentSolution = !left.empty() ? left.top() : right.top();
		if (!left.empty())
			left.pop();
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
					left.push(withPivot);

				if (withoutPivot.lowerBound < limit)
					right.push(withoutPivot);
			}
		}
	}
	return bestSolutions;
}

std::vector<std::pair<std::vector<int16_t>, float>> runAlgorithm(const std::vector<std::vector<float>>& A, float ignoredValue, float limit, int maxSolutionCount, ThreadSafeVec<std::pair<std::vector<int16_t>, float>>& solutionsVec) {
	auto A_ = A;
	A_[0].back() = 0;
	return findSolutions(A_, maxSolutionCount, limit, &solutionsVec);
}
