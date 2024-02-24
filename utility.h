#pragma once
#include <iostream>
#include <cstdlib>
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
#include <queue>
#include <condition_variable>

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
	std::condition_variable taskInQueueOrAbortCondVar;
	std::condition_variable taskDoneCondVar;
	std::queue<std::function<void(int)>> tasks;
	int activeTasksCount = 0;
	int maxQueuedTaskCount;
	std::mutex tasksMutex;
	std::vector<std::thread> threads;
	bool waiting = false;
	bool active = false;

	void workerThread(int threadId) {
		std::unique_lock tasksLock(tasksMutex);
		while (true) {
			activeTasksCount -= 1;
			tasksLock.unlock();
			if (waiting && activeTasksCount == 0 && tasks.empty())
				taskDoneCondVar.notify_all();
			tasksLock.lock();
			taskInQueueOrAbortCondVar.wait(tasksLock, [this] {
				return !tasks.empty() || !active;
			});
			if (!active)
				break;
			{
				auto task = std::move(tasks.front());
				tasks.pop();
				activeTasksCount += 1;
				tasksLock.unlock();
				task(threadId);
			}
			tasksLock.lock();
		}
	}

public:
	ThreadPool(int threadCount = 0, int maxQueuedTaskCount = 1000) : maxQueuedTaskCount(maxQueuedTaskCount) {
		if (threadCount <= 0) {
			threadCount = (std::thread::hardware_concurrency() > 0) ? std::thread::hardware_concurrency() : 1;
		}
		activeTasksCount = threadCount;
		active = true;
		for (int i = 0; i < threadCount; ++i) {
			threads.emplace_back(std::thread(&ThreadPool::workerThread, this, i));
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

	~ThreadPool() {
		wait();
		{
			const std::scoped_lock tasks_lock(tasksMutex);
			active = false;
		}
		taskInQueueOrAbortCondVar.notify_all();
		for (int i = 0; i < threads.size(); ++i) {
			threads[i].join();
		}
	}

	template<typename F> void addTask(F&& task) {
		{
			std::scoped_lock tasks_lock(tasksMutex);
			tasks.emplace(std::forward<F>(task));
		}
		taskInQueueOrAbortCondVar.notify_one();
		while (tasks.size() >= maxQueuedTaskCount) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void wait() {
		std::unique_lock tasks_lock(tasksMutex);
		waiting = true;
		taskDoneCondVar.wait(tasks_lock, [this] {
			return activeTasksCount == 0 && tasks.empty();
		});
		waiting = false;
	}
};


#if defined(__clang__)
#define COMPILER_CLANG
#elif defined(__INTEL_COMPILER)
#define COMPILER_INTEL
#elif defined(_MSC_VER)
#define COMPILER_MSVC
#elif defined(__GNUC__)
#define COMPILER_GCC
#endif

template<int Size, int Alignment> struct FreeList {
	std::vector<void*> list;

	FreeList() {}
	FreeList(const FreeList&) = delete;
	FreeList& operator=(const FreeList&) = delete;
	~FreeList() {
		while (!list.empty()) {
		#if defined(COMPILER_MSVC) || defined(COMPILER_CLANG)
			_aligned_free(list.back());
		#else
			std::free(list.back());
		#endif
			list.pop_back();
		}
	}

	void* allocate() {
		if (!list.empty()) {
			auto ptr = list.back();
			list.pop_back();
			return ptr;
		}
		#if defined(COMPILER_MSVC) || defined(COMPILER_CLANG)
		return _aligned_malloc(Size, Alignment);
		#else
		return std::aligned_alloc(Alignment, Size);
		#endif
	}
	void deallocate(void* ptr) {
		if (ptr)
			list.push_back(ptr);
	}
};

template<typename T, int Size> struct FreeListVector {
	static inline FreeList<sizeof(T) * Size, alignof(T)> allocator;
	T* data;
	int size_ = 0;

	FreeListVector() {
		data = (T*)allocator.allocate();
	}
	FreeListVector(FreeListVector&& other) : data(other.data), size_(other.size_) {
		other.data = nullptr;
		other.size_ = 0;
	}
	FreeListVector& operator=(FreeListVector&& other) {
		std::swap(data, other.data);
		std::swap(size_, other.size_);
		return *this;
	}
	FreeListVector(const FreeListVector& other) {
		data = (T*)allocator.allocate();
		size_ = other.size_;
		std::memcpy(data, other.data, size_ * sizeof(T));
	}
	FreeListVector& operator=(const FreeListVector& other) {
		size_ = other.size_;
		std::memcpy(data, other.data, size_ * sizeof(T));
		return *this;
	}
	~FreeListVector() {
		allocator.deallocate(data);
	}

	void push_back(const T& val) {
		static_assert(std::is_trivially_destructible_v<T>);
		new (&data[size_]) T(val);
		size_ += 1;
	}

	T* begin() { return &data[0]; }
	T* end()   { return &data[size_]; }
	T& back()  { return data[size_ - 1]; }
	T& operator[](int i) { return data[i]; }
	int size()   { return size_; }
	void resize(int size) { size_ = size; }
	void clear() { size_ = 0; }
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
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
	return 63 - __builtin_clzll(a);
#elif defined(COMPILER_MSVC)
	unsigned long index;
	_BitScanReverse64(&index, a);
	return index;
#else
	static_assert(false, "unsupported compiler. Cannot compute mostSignificantBit");
#endif
}

template<typename T> struct ThreadSafeVec {
	static constexpr int smallestPower2 = 13;
	std::vector<std::unique_ptr<std::vector<T>>> blocks;
	int totalCapacity;
	int size_;
	std::mutex mutex;

	ThreadSafeVec() {
		init();
	}
	int size() {
		return size_;
	}
	void push_back_not_thread_safe(const T& val) {
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
	void push_back(const T& val) {
		std::scoped_lock l{ mutex };
		push_back_not_thread_safe(val);
	}
	const T& operator[](int i) const {
		if (i == 0)
			return (*blocks[0])[0];
		int bitPos = mostSignificantBitPosition(i);
		int blockId = std::max(0, bitPos - smallestPower2 + 1);
		int posInBlock = (blockId == 0) ? i : (i - (1 << bitPos));
		return (*blocks[blockId])[posInBlock];
	}
	void clear() {
		blocks.clear();
		init();
	}
private:
	void init() {
		totalCapacity = 1 << smallestPower2;
		size_ = 0;
		blocks.emplace_back(std::make_unique<std::vector<T>>(totalCapacity));
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

std::vector<float> splitToFloats(std::string_view str, float ignoredValue) {
	constexpr auto PossiblyFloatChars = "0123456789.+-e";
	std::vector<float> result;
	while (true) {
		auto pos = str.find_first_not_of(PossiblyFloatChars);
		auto value = parseFloat(std::string(str.substr(0, pos)));
		if (value)
			result.emplace_back(*value);
		else
			result.emplace_back(ignoredValue);
		if (pos == std::string::npos)
			return result;
		auto pos2 = str.find_first_of(PossiblyFloatChars, pos);
		if (pos2 == std::string::npos)
			return result;
		str = str.substr(pos2);
	}
}
