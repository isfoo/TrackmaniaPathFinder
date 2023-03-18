#pragma once
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
#include <array>


class Timer {
	std::chrono::time_point<std::chrono::steady_clock> startTime;
public:
	Timer() : startTime(std::chrono::steady_clock::now()) {}
	void start() {
		startTime = std::chrono::steady_clock::now();
	}
	double getTime() {
		auto endTime = std::chrono::steady_clock::now();
		return std::chrono::duration<double>(endTime - startTime).count();
	}
};

class ThreadPool {
	std::vector<std::future<void>> tasks;
public:
	void addTask(std::function<void(void)> task) {
		tasks.emplace_back(std::async(std::launch::async | std::launch::deferred, task));
	}
	void completeTasksAndStop() {
		for (auto& task : tasks)
			task.wait();
		tasks.clear();
	}
};

struct DynamicBitset {
	using IntType = uint64_t;
	static constexpr int IntTypeBitSize = sizeof(IntType) * 8;
	std::array<IntType, 10> bits;

	DynamicBitset(int size = 0) {
		std::fill(bits.begin(), bits.end(), 0);
		for (int i = size % IntTypeBitSize; i < IntTypeBitSize; ++i) {
			bits.back() |= singleBit(i);
		}
	}

	bool test(int i) const {
		return bits[i / IntTypeBitSize] & singleBit(i);
	}
	void set(int i) {
		bits[i / IntTypeBitSize] |= singleBit(i);
	}
	void reset(int i) {
		bits[i / IntTypeBitSize] &= ~singleBit(i);
	}

private:
	IntType singleBit(int i) const {
		return (1ull << (i % IntTypeBitSize));
	}
};


std::optional<float> parseFloat(const std::string& s) {
	if (s.empty())
		return std::nullopt;
	if (s[0] == '0')
		return 0;
	auto result = atof(s.c_str());
	return result != 0 ? std::optional{ result } : std::nullopt;
}

std::vector<float> splitToFloats(std::string_view str, float ignoredValue, std::string_view delim = ",") {
	std::vector<float> result;
	while (true) {
		std::size_t pos = pos = str.find(delim);
		auto s = std::string(str.substr(0, pos));
		auto value = parseFloat(s);
		if (value)
			result.emplace_back(*value);
		else if (!std::all_of(s.begin(), s.end(), isspace))
			result.emplace_back(ignoredValue);
		if (pos == std::string::npos)
			return result;
		str = str.substr(pos + delim.size());
	}
}
