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
	std::chrono::time_point<std::chrono::steady_clock> endTime;
	bool stopped = false;

public:
	Timer() : startTime(std::chrono::steady_clock::now()), endTime(startTime) {}
	void start() {
		startTime = std::chrono::steady_clock::now();
	}
	void stop() {
		endTime = std::chrono::steady_clock::now();
		stopped = true;
	}
	double getTime() {
		auto end = std::chrono::steady_clock::now();
		if (stopped) {
			end = endTime;
		}
		return std::chrono::duration<double>(end - startTime).count();
	}
};

template<typename T> bool isRunning(const std::future<T>& f) {
	return f.valid() && f.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

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

uint64_t mostSignificantBitPosition(uint64_t a) {
	unsigned long index;
	_BitScanReverse64(&index, a);
	return index;
}

template<typename T> struct ThreadSafeVec {
	static constexpr int smallestPower2 = 13;
	std::vector<std::unique_ptr<std::vector<T>>> blocks;
	int totalCapacity;
	int size_;

	ThreadSafeVec() : totalCapacity(1 << smallestPower2), size_(0) {
		blocks.emplace_back(std::make_unique<std::vector<T>>(totalCapacity));
	}
	int size() {
		return size_;
	}
	void push_back(const T& val) {
		if (size_ == 0) {
			(*blocks[0])[0] = val;
			size_ += 1;
			return;
		}
		int bitPos = mostSignificantBitPosition(size_);
		int blockId = std::max(0, bitPos - smallestPower2 + 1);
		if (blockId >= blocks.size()) {
			while (blockId >= blocks.size()) {
				blocks.emplace_back(std::make_unique<std::vector<T>>(totalCapacity));
				totalCapacity *= 2;
			}
		}
		int posInBlock = (blockId == 0) ? size_ : (size_ - (1 << bitPos));
		(*blocks[blockId])[posInBlock] = val;
		size_ += 1;
	}
	const T& operator[](int i) const {
		if (i == 0)
			return (*blocks[0])[0];
		int bitPos = mostSignificantBitPosition(i);
		int blockId = std::max(0, bitPos - smallestPower2 + 1);
		int posInBlock = (blockId == 0) ? i : (i - (1 << bitPos));
		return (*blocks[blockId])[posInBlock];
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
