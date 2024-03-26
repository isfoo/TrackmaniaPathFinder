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

using Bool = int8_t;
using EdgeCostType = int32_t;
using NodeType = uint8_t;
using Edge = std::pair<NodeType, NodeType>;

#if defined(__clang__)
#define COMPILER_CLANG
#elif defined(__INTEL_COMPILER)
#define COMPILER_INTEL
#elif defined(_MSC_VER)
#define COMPILER_MSVC
#elif defined(__GNUC__)
#define COMPILER_GCC
#endif

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

struct PoolAllocator {
	std::vector<void*> freeList;
	std::vector<char*> allocatedBlocks;
	std::mutex m;
	int elementSize;
	int elementCount;
	int indexInCurrentBlock;

	PoolAllocator(int elementSize, int elementCount) : 
		elementSize(elementSize), elementCount(elementCount), indexInCurrentBlock(elementCount)
	{};
	PoolAllocator(PoolAllocator&& other) {
		freeList.swap(other.freeList);
		allocatedBlocks.swap(other.allocatedBlocks);
		elementSize = other.elementSize;
		elementCount = other.elementCount;
		indexInCurrentBlock = other.indexInCurrentBlock;
	}
	PoolAllocator(const PoolAllocator&) = delete;
	PoolAllocator operator=(const PoolAllocator&) = delete;

	~PoolAllocator() {
		for (auto& block : allocatedBlocks) {
			free(block);
		}
	}

	void* allocate() {
		std::scoped_lock l{ m };
		if (!freeList.empty()) {
			auto ptr = freeList.back();
			freeList.pop_back();
			return ptr;
		}
		if (indexInCurrentBlock >= elementCount) {
			allocatedBlocks.push_back((char*)malloc(elementSize * elementCount));
			indexInCurrentBlock = 0;
		}
		return allocatedBlocks.back() + ((indexInCurrentBlock++) * elementSize);
	}

	void deallocate(void* ptr) {
		std::scoped_lock l{ m };
		if (ptr)
			freeList.push_back(ptr);
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
	int size() const {
		return size_;
	}
	void push_back_not_thread_safe(const T& val) {
		if (size_ == 0) {
			(*blocks[0])[0] = val;
			size_ += 1;
			return;
		}
		int bitPos = int(mostSignificantBitPosition(size_));
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
		int bitPos = int(mostSignificantBitPosition(i));
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


std::optional<int> parseInt(const std::string& s) {
	if (s.empty())
		return std::nullopt;
	if (s[0] == '0')
		return 0;
	auto result = atoi(s.c_str());
	return result != 0 ? std::optional{ result } : std::nullopt;
}
std::optional<int> parseFloatAsInt(const std::string& s, int multiplyFactor=1) {
	if (s.empty())
		return std::nullopt;
	if (s[0] == '0')
		return 0;
	auto result = int(atof(s.c_str()) * multiplyFactor);
	return result != 0 ? std::optional{ result } : std::nullopt;
}

std::vector<int> splitLineOfFloatsToInts(std::string_view str, int ignoredValue, int multiplyFactor=1) {
	constexpr auto PossiblyFloatChars = "0123456789.+-e";
	std::vector<int> result;

	auto pos = str.find_first_of(PossiblyFloatChars);
	if (pos == std::string::npos)
		return result;
	str = str.substr(pos);
	while (true) {
		auto pos = str.find_first_not_of(PossiblyFloatChars);
		auto value = parseFloatAsInt(std::string(str.substr(0, pos)), multiplyFactor);
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

template<typename T> struct VectorView {
	T* data; int size;
	VectorView(T* data, int size) : data(data), size(size) {}
	T& operator[](int i) { return data[i]; }
	T& back() { return (*this)[size - 1]; }
};
template<typename T> struct ConstVectorView {
	const T* data; int size;
	ConstVectorView(const T* data, int size) : data(data), size(size) {}
	const T& operator[](int i) const { return data[i]; }
	const T& back() const { return (*this)[size - 1]; }
};
template<typename T> struct VectorView2d {
	T* data; int size;
	VectorView2d(T* data, int size) : data(data), size(size) {}
	VectorView<T> operator[](int i) { return VectorView<T>(&data[i * size], size); }
};
template<typename T> struct ConstVectorView2d {
	const T* data; int size;
	ConstVectorView2d(const T* data, int size) : data(data), size(size) {}
	const ConstVectorView<T> operator[](int i) const  { return ConstVectorView<T>(&data[i * size], size); }
};
template<typename T> struct Vector3d {
	std::vector<T> data; int size_;
	Vector3d(int size=0) : data(size * size * size), size_(size) {}
	VectorView2d<T> operator[](int i)            { return VectorView2d<T>(&data[size_ * size_ * i], size_); }
	ConstVectorView2d<T> operator[](int i) const { return ConstVectorView2d<T>(&data[size_ * size_ * i], size_); }
	bool empty() const { return size() == 0; }
	int size() const   { return size_; }
	void clear()       { data.clear(); size_ = 0; }
};

template<typename T> struct FastSmallVector {
	constexpr static int MaxSize = 5;
	std::array<T, MaxSize> data;
	int size_ = 0;
	FastSmallVector() {}
	static std::optional<FastSmallVector> Combine(const FastSmallVector& a, const T& val, const FastSmallVector& b) {
		if (a.size() + b.size() + 1 > MaxSize)
			return std::nullopt;
		FastSmallVector result;
		std::copy(&a.data[0], &a.data[a.size_], &result.data[0]);
		result.data[a.size()] = val;
		std::copy(&b.data[0], &b.data[b.size_], &result.data[a.size() + 1]);
		result.size_ = a.size() + b.size() + 1;
		return result;
	}
	int size() const { return size_; }
	bool empty() const { return size() == 0; }
	const T& operator[](int i) const { return data[i]; }
};

struct XorShift64 {
	using result_type = uint64_t;
	static constexpr result_type min() { return 0; }
	static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

	XorShift64() {
		state = 0x12345678;
		xorShift64();
	}
	result_type operator()() {
		xorShift64();
		return state;
	}
private:
	void xorShift64() {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
	}
	result_type state;
};

struct DynamicBitset {
	using IntType = uint64_t;
	static constexpr int IntTypeBitSize = sizeof(IntType) * 8;
	std::array<IntType, 4> bits;

	DynamicBitset() { std::fill(bits.begin(), bits.end(), 0); }
	bool test(int i) const { return bits[i / IntTypeBitSize] & singleBit(i); }
	void set(int i)        { bits[i / IntTypeBitSize] |= singleBit(i); }
	void reset(int i)      { bits[i / IntTypeBitSize] &= ~singleBit(i); }

private:
	IntType singleBit(int i) const { return (1ull << (i % IntTypeBitSize)); }
};

namespace std {
	template <> struct hash<std::vector<NodeType>> {
		std::size_t operator()(const std::vector<NodeType>& k) const {
			uint64_t hash = 5381;
			for (int i = 0; i < k.size(); ++i) {
				hash = hash * 33 + k[i];
			}
			return hash;
		}
	};
}

template<typename T> class FastThreadSafeishHashSet {
	struct Node {
		Node(const T& value) { new (&value_) T(value); }
		~Node() { value().~T(); }
		Node(const Node&) = delete;
		Node& operator=(const Node&) = delete;
		T& value() { return *(T*)value_; }
		Node* next = nullptr;
	private:
		alignas(T) char value_[sizeof(T)];
	};
	
	Node** data;
	int capacity_;
	PoolAllocator allocator;

	Node*& node(std::size_t index) { return data[index]; }
	int capacity() { return capacity_; }

	Node** allocData(int size) {
		auto x = (Node**)::operator new(size * sizeof(Node*), std::align_val_t(alignof(Node*)));
		memset(x, 0, size * sizeof(Node*));
		return x;
	}
	void deallocData(Node** data) {
		::operator delete(data, capacity_ * sizeof(Node*), std::align_val_t(alignof(Node*)));
	}
	Node* allocNode(T&& value) {
		auto node = (Node*)allocator.allocate();
		new (node) Node(std::move(value));
		return node;
	}
	Node* allocNode(const T& value) {
		allocNode(T(value));
	}
	void deallocNode(Node* node) {
		node->~Node();
		allocator.deallocate(node);
	}
	std::size_t hash(const T& value) {
		return std::hash<T>{}(value);
	}
	std::size_t modIndex(std::size_t index) {
		return index & (capacity_ - 1);
	}
	std::size_t getIndex(const T& value) {
		return modIndex(hash(value));
	}

public:
	FastThreadSafeishHashSet(int power2Capacity = 8) : allocator(sizeof(Node), 8192), data(allocData(1 << power2Capacity)), capacity_(1 << power2Capacity) {}

	T* find(const T& value) {
		Node* n = node(getIndex(value));
		while (n) {
			if (n->value() == value)
				return &n->value();
			n = n->next;
		}
		return nullptr;
	}
	void emplace(T&& value) {
		// This is not thread safe, but good enough in practice.
		// worst case what happens is I will add into same location twice which will result in orphan node which won't be findable,
		// or it will add duplicate of the same node.
		// however niether is a big problem, because orphan pointers are going to get deallocated anyway and having some duplicates
		// is not a problem.
		auto i = getIndex(value);
		auto n = node(i);
		Node* allocatedNode = allocNode(std::move(value));
		if (!n) {
			node(i) = allocatedNode;
			return;
		}
		while (n->next) {
			n = n->next;
		};
		n->next = allocatedNode;
	}
	void insert(T value) {
		emplace(std::move(value));
	}
};

template<typename T, int Size=256> struct FixedStackVector {
	alignas(T) char data[sizeof(T) * Size];
	int size_ = 0;
	template<typename... Args> void emplace_back(Args&&... args) { new (&data[sizeof(T) * size_++]) T(std::forward<Args>(args)...); }
	int size() { return size_; }
	T* begin() { return (T*)&data[0]; }
	T* end()   { return (T*)&data[sizeof(T) * size_]; }
};
