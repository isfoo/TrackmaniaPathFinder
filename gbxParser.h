#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <string>
#define NOMINMAX
#include <Windows.h>
#include "utility.h"
#include "immintrin.h"
#include "zlib.h"

struct LzoReader {
    const uint8_t* data;
    LzoReader(const uint8_t* data) : data(data) {}
    struct Byte {
        uint8_t value;
        operator uint8_t() { return value; }
        uint8_t operator()(uint8_t offset, uint8_t size) { return uint8_t(value >> (8 - offset - size)) & ((1 << size) - 1); }
    };
    Byte peakNextByte() { return Byte{ *data }; }
    Byte readNextByte() { return Byte{ *data++ }; }
    void read16BitDistAndInputCopyCount(uint64_t& lookbackDist, uint64_t& lastInputCopyCount, uint64_t distConstant) {
        uint16_t value = *(uint16_t*)data;
        data += 2;
        lookbackDist = (value >> 2) + distConstant;
        lastInputCopyCount = value & 0x3;
    }
    uint64_t readZeroCount() {
        int zeroByteCount = 0;
        while (peakNextByte() == 0) {
            readNextByte();
            zeroByteCount += 1;
        }
        return zeroByteCount * 255 + readNextByte();
    }
    uint64_t readCopyCount(uint8_t base, uint8_t constant, uint8_t lengthBitCount) {
        return base == 0 ? (constant + ((1 << lengthBitCount) - 1) + readZeroCount()) : base + constant;
    }
    void readBytes(uint8_t*& dst, uint64_t count) {
        std::memcpy(dst, data, count);
        dst += count;
        data += count; 
    }
};
void backCopy(uint8_t*& dst, uint64_t dist, uint64_t count) {
    auto src = dst - dist;
    auto srcEnd = src + count;
    if (dist >= 8) {
        auto srcEnd = src + count - 8;
        while (src <= srcEnd) {
            *(uint64_t*)dst = *(uint64_t*)src;
            dst += 8;
            src += 8;
        }
    }
    while (src < srcEnd)
        *dst++ = *src++;
}
bool lzoDecompress(uint8_t* compressedData, int compressedSize, uint8_t* decompressedData, int decompressedSize) {
    LzoReader reader(compressedData);
    uint64_t lastInputCopyCount = 0;
    uint64_t lookbackCopyCount = 0;
    uint64_t lookbackDist = 0;

    if (reader.peakNextByte() >= 18) {
        lastInputCopyCount = reader.readNextByte() - uint8_t(17);
        reader.readBytes(decompressedData, lastInputCopyCount);
    }
    while (true) {
        auto inst = reader.readNextByte();
        if (inst >= 64) {
            lookbackCopyCount = reader.readCopyCount(inst(0, 3), 1, 3);
            lastInputCopyCount = inst(6, 2);
            lookbackDist = (uint64_t(reader.readNextByte()) << 3) + inst(3, 3) + 1;
        } else if (inst >= 32) {
            lookbackCopyCount = reader.readCopyCount(inst(3, 5), 2, 5);
            reader.read16BitDistAndInputCopyCount(lookbackDist, lastInputCopyCount, 1);
        } else if (inst >= 16) {
            lookbackCopyCount = reader.readCopyCount(inst(5, 3), 2, 3);
            reader.read16BitDistAndInputCopyCount(lookbackDist, lastInputCopyCount, 16384 + (uint64_t(inst(4, 1)) << 14));
            if (lookbackDist == 16384)
                break;
        } else if (lastInputCopyCount > 0) {
            lookbackCopyCount = 2 + (lastInputCopyCount >= 4);
            lastInputCopyCount = inst(6, 2);
            lookbackDist = (uint64_t(reader.readNextByte()) << 2) + inst(4, 2) + 1 + 2048 * (lastInputCopyCount >= 4);
        } else {
            lookbackCopyCount = 0;
            lastInputCopyCount = reader.readCopyCount(inst(4, 4), 3, 4);
            lookbackDist = 0;
        }
        backCopy(decompressedData, lookbackDist, lookbackCopyCount);
        reader.readBytes(decompressedData, lastInputCopyCount);
    }
    return true;
}
int lzoDummyMaxCompressSize(int dataSize) {
    return dataSize + dataSize / 255 + 5;
}
std::vector<uint8_t> lzoDummyCompress(uint8_t* data, int dataSize) {
    std::vector<uint8_t> compressedData;
    if (dataSize + 17 <= 255) {
        compressedData.resize(dataSize + 4); // 2 instructions, 2 zero bytes for end instruction 
        compressedData[0] = dataSize + 17;
        std::memcpy(&compressedData[1], data, dataSize);
    } else {
        int zeroBytesCount = dataSize / 255;
        if (zeroBytesCount * 255 + 18 >= dataSize)
            zeroBytesCount -= 1;
        uint8_t nonZeroByte = dataSize - zeroBytesCount * 255 - 18;

        compressedData.resize(dataSize + zeroBytesCount + 5); // 2 instructions, 1 zero byte terminator, 2 zero bytes for end instruction 
        compressedData[0] = 0;
        std::memset(&compressedData[1], 0, zeroBytesCount);
        compressedData[zeroBytesCount + 1] = nonZeroByte;
        std::memcpy(&compressedData[zeroBytesCount + 2], data, dataSize);
    }
    compressedData[compressedData.size() - 3] = 0b00010001;
    compressedData[compressedData.size() - 2] = 0;
    compressedData[compressedData.size() - 1] = 0;
    return compressedData;
}

using byte = uint8_t;
using u32 = uint32_t;

constexpr u32 CGameArenaPlayer = 0x032CB000;
constexpr u32 CSceneVehicleVis = 0x0A018000;
constexpr u32 CPlugEntRecordData = 0x0911F000;
constexpr u32 CGameCtnGhostCheckpoints = 0x0309200B;

#define AssertReturnEmpty(Exp) if (!(Exp)) return {}

#pragma pack(push, 1)
struct GbxHeaderV6 {
    char magic[3];
    uint16_t version;
    char format;
    char compressionRefTable;
    char compressionBody;
    char _unknown0;
    u32 classId;
};
#pragma pack(pop)

struct Position {
    float x;
    float y;
    float z;
};
struct GhostSample {
    u32 time; 
    Position pos;
    float speed;
};
struct ReplayEvent {
    enum EventType { CpCross, CarSwitch };
    EventType type;
    u32 time;
};
struct ReplaySamples {
    std::vector<GhostSample> ghostSamples;
    std::vector<ReplayEvent> events;
};
struct ReplayData {
    std::vector<ReplaySamples> replaySamples;
    std::vector<u32> cpTimes;
};
struct ContinuousReplayData {
    std::vector<u32> cpTimes;
    std::vector<GhostSample> ghostSamples;
};
struct Connection {
    int src;
    int dst;
    int time;
};

bool zlibDecompress(byte* compressedData, int compressedSize, byte* decompressedData, int decompressedSize) {
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = (uInt)compressedSize;
    infstream.next_in = (Bytef*)compressedData;
    infstream.avail_out = (uInt)decompressedSize;
    infstream.next_out = (Bytef*)decompressedData;
    inflateInit(&infstream);
    auto result = inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);
    return result == Z_STREAM_END;
}

struct MemoryMappedFile {
    HANDLE file = nullptr;
    HANDLE map  = nullptr;
    void* ptr   = nullptr;
    int size    = 0;

    MemoryMappedFile() {}
    MemoryMappedFile(const MemoryMappedFile&) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
    MemoryMappedFile(MemoryMappedFile&& other) {
        std::swap(file, other.file);
        std::swap(map, other.map);
        std::swap(ptr, other.ptr);
        std::swap(size, other.size);
    }
    ~MemoryMappedFile() {
        if (ptr != nullptr)  UnmapViewOfFile(ptr);
        if (map != nullptr)  CloseHandle(map);
        if (file != nullptr) CloseHandle(file);
        ptr  = nullptr;
        map  = nullptr;
        file = nullptr;
    }
};

std::optional<MemoryMappedFile> openMemoryMappedFile(const std::wstring& fileName) {
    MemoryMappedFile file;
    file.file = CreateFileW(fileName.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    AssertReturnEmpty(file.file != INVALID_HANDLE_VALUE);
    LARGE_INTEGER fileSize;
    AssertReturnEmpty(GetFileSizeEx(file.file, &fileSize));
    AssertReturnEmpty(fileSize.QuadPart > 0);
    file.size = int(fileSize.QuadPart);
    file.map = CreateFileMappingA(file.file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    AssertReturnEmpty(file.map);
    file.ptr = MapViewOfFile(file.map, FILE_MAP_READ, 0, 0, 0);
    AssertReturnEmpty(file.ptr);
    return file;
}

struct DataBuffer {
    struct NoInitByte {
        byte value;
        NoInitByte() {}
    };

    std::vector<NoInitByte> data_;
    byte* data;
    int size;
    uint64_t readIndex = 0;

    DataBuffer(byte* data, int size) : data(data), size(size) {}
    DataBuffer(int size) : data_(size), data((byte*)data_.data()), size(size) {}
    template<typename T> T peakNext() {
        return readIndex >= size ? T() : *(T*)&data[readIndex];
    }
    template<typename T> T readNext() {
        auto value = peakNext<T>();
        readIndex += sizeof(T);
        return value;
    }
    template<typename T> bool canReadNext() {
        return readIndex + sizeof(T) <= size;
    }
    template<typename Func> DataBuffer readNextCompressed(Func decompressFunction) {
        auto decompressedSize = readNext<u32>();
        auto compressedSize = readNext<u32>();
        if (readIndex + compressedSize > size)
            return DataBuffer(0);
        DataBuffer decompressedBuffer(decompressedSize);
        if (!decompressFunction(&data[readIndex], compressedSize, decompressedBuffer.data, decompressedSize))
            return DataBuffer(0);
        readIndex += compressedSize;
        return decompressedBuffer;
    }
    void skipNextArray() {
        skipNext(readNext<u32>());
    }
    void skipNext(int byteCount) {
        readIndex += byteCount;
    }
};

ReplayData readSamplesData(DataBuffer& buffer) {
    ReplayData replayData;
    AssertReturnEmpty(buffer.readNext<u32>() == CPlugEntRecordData);
    AssertReturnEmpty(buffer.readNext<u32>() == CPlugEntRecordData);
    AssertReturnEmpty(buffer.readNext<u32>() == 10); // version
    auto entRecordBuffer = buffer.readNextCompressed(zlibDecompress);
    AssertReturnEmpty(entRecordBuffer.size > 1'000);
    AssertReturnEmpty(entRecordBuffer.peakNext<u32>() >> 16 == 0); // start of sample range should have zeros in high bits
    entRecordBuffer.skipNext(sizeof(u32) * 2); // start and end of sample range

    auto entRecordDescSize = entRecordBuffer.readNext<u32>();
    std::vector<u32> entRecordClassIds(entRecordDescSize);
    for (u32 i = 0; i < entRecordDescSize; ++i) {
        entRecordClassIds[i] = entRecordBuffer.readNext<u32>();
        entRecordBuffer.skipNext(sizeof(u32) * 3);
        entRecordBuffer.skipNextArray();
        entRecordBuffer.skipNext(sizeof(u32));
    }

    auto recordNoticeDescSize = entRecordBuffer.readNext<u32>();
    std::vector<std::pair<u32, u32>> entRecordNoticeDescriptions(recordNoticeDescSize);
    for (u32 i = 0; i < recordNoticeDescSize; ++i) {
        entRecordNoticeDescriptions[i].first  = entRecordBuffer.readNext<u32>();
        entRecordNoticeDescriptions[i].second = entRecordBuffer.readNext<u32>();
        entRecordBuffer.skipNext(sizeof(u32));
    }

    bool hasNextElement = entRecordBuffer.readNext<byte>();
    while (entRecordBuffer.canReadNext<byte>() && hasNextElement) {
        auto type = entRecordBuffer.readNext<u32>();
        if (type >= entRecordClassIds.size())
            break;
        entRecordBuffer.skipNext(sizeof(u32) * 3);
        auto u4 = entRecordBuffer.readNext<u32>();

        if (entRecordClassIds[type] == CSceneVehicleVis && (u4 == 0 || u4 == 0xED00000)) {
            ReplaySamples samples;
            while (entRecordBuffer.readNext<byte>()) {
                auto time = entRecordBuffer.readNext<u32>();
                auto size = entRecordBuffer.readNext<u32>();
                entRecordBuffer.skipNext(47);
                auto posX = entRecordBuffer.readNext<float>();
                auto posY = entRecordBuffer.readNext<float>();
                auto posZ = entRecordBuffer.readNext<float>();
                entRecordBuffer.skipNext(6);
                auto speed = 3.6f * std::exp(entRecordBuffer.readNext<int16_t>() / 1000.0f);
                entRecordBuffer.skipNext(size - 47 - 3 * sizeof(float) - 8);
                samples.ghostSamples.push_back(GhostSample{ time, Position{posX, posY, posZ}, speed });
            }
            hasNextElement = entRecordBuffer.readNext<byte>();
            while (entRecordBuffer.readNext<byte>()) {
                auto type = entRecordBuffer.readNext<u32>();
                auto time = entRecordBuffer.readNext<u32>();
                entRecordBuffer.skipNextArray();
                if (entRecordNoticeDescriptions[type].second == 20) {
                    samples.events.push_back(ReplayEvent{ ReplayEvent::CpCross, time });
                } else if (entRecordNoticeDescriptions[type].second == 24) {
                    samples.events.push_back(ReplayEvent{ ReplayEvent::CarSwitch, time });
                }
            }
            if (samples.ghostSamples.size() > 0) {
                replayData.replaySamples.push_back(samples);
            }
        } else if (entRecordClassIds[type] == CGameArenaPlayer && (u4 == 0 || u4 == 0xED00000)) {
            std::vector<u32> cpTimes;
            int cpNumber = 1;
            while (entRecordBuffer.readNext<byte>()) {
                entRecordBuffer.skipNext(sizeof(u32));
                entRecordBuffer.skipNextArray();
            }
            hasNextElement = entRecordBuffer.readNext<byte>();
            while (entRecordBuffer.readNext<byte>()) {
                auto type = entRecordBuffer.readNext<u32>();
                auto time = entRecordBuffer.readNext<u32>();
                auto dataSize = entRecordBuffer.readNext<u32>();
                entRecordBuffer.skipNext(dataSize - 2);
                auto readCpNumber = entRecordBuffer.readNext<uint16_t>();
                if (entRecordNoticeDescriptions[type].second == 21 && readCpNumber == cpNumber) {
                    cpTimes.push_back(time);
                    cpNumber += 1;
                }
            }
            replayData.cpTimes = cpTimes;
        } else {
            while (entRecordBuffer.readNext<byte>()) {
                entRecordBuffer.skipNext(sizeof(u32));
                entRecordBuffer.skipNextArray();
            }
            hasNextElement = entRecordBuffer.readNext<byte>();
            while (entRecordBuffer.readNext<byte>()) {
                entRecordBuffer.skipNext(sizeof(u32) * 2);
                entRecordBuffer.skipNextArray();
            }
        }
    }
    return replayData;
}

std::optional<int> findSubsequenceSlow(byte* data, int dataSize, u32 v) {
    for (int pos = 0; pos < dataSize - 4; ++pos) {
        auto dataVal = *(u32*)(&data[pos]);
        if (dataVal == v)
            return pos;
    }
    return std::nullopt;
}

u32 trailingZeroBitCount(u32 a) {
#if defined(COMPILER_MSVC)
    return __popcnt(~a & (a - 1));
#else
    return __builtin_ctz(a);
#endif
}

std::optional<int> findSubsequence(byte* data, int dataSize, u32 v) {
#ifdef __AVX2__
    byte* s = (byte*)&v;
    auto b1 = _mm256_set1_epi8(s[0]);
    auto b2 = _mm256_set1_epi8(s[1]);
    auto dataStart = data;
    auto dataEnd = data + dataSize;
    auto dataEndAvx = dataEnd - 32 - sizeof(u32);
    for (; data < dataEndAvx; data += 32) {
        auto loadByte1 = _mm256_lddqu_si256((__m256i*)data);
        auto loadByte2 = _mm256_lddqu_si256((__m256i*)(data + 1));
        u32 matches = _mm256_movemask_epi8(_mm256_cmpeq_epi8(b1, loadByte1)) & _mm256_movemask_epi8(_mm256_cmpeq_epi8(b2, loadByte2));
        while (matches) {
            int potentialOffset = trailingZeroBitCount(matches);
            if (*(u32*)(data + potentialOffset) == v)
                return int(data - dataStart + potentialOffset);
            matches &= matches - 1;
        }
    }
    auto result = findSubsequenceSlow(data, int(dataEnd - data), v);
    if (!result)
        return std::nullopt;
    return *result + int(data - dataStart);
#else
    return findSubsequenceSlow(data, dataSize, v);
#endif
}

ReplayData getReplaySamplesList(const std::wstring& fileName) {
    ReplayData replayData;
    auto memoryFile = openMemoryMappedFile(fileName);
    AssertReturnEmpty(memoryFile);
    auto buffer = DataBuffer((byte*)memoryFile->ptr, memoryFile->size);
    AssertReturnEmpty(memoryFile->size >= sizeof(GbxHeaderV6));
    auto header = buffer.readNext<GbxHeaderV6>();
    AssertReturnEmpty(header.magic[0] == 'G' && header.magic[1] == 'B' && header.magic[2] == 'X');
    AssertReturnEmpty(header.version == 6);
    AssertReturnEmpty(header.compressionBody == 'C');
    buffer.skipNextArray(); // header data
    buffer.skipNext(sizeof(u32)); // number of nodes
    AssertReturnEmpty(buffer.readNext<u32>() == 0); // number of external nodes
    buffer = buffer.readNextCompressed(lzoDecompress); // compressed body data
    while (true) {
        auto pos = findSubsequence((byte*)&buffer.data[buffer.readIndex], buffer.size - int(buffer.readIndex), CPlugEntRecordData);
        if (!pos)
            break;
        buffer.readIndex += *pos;
        replayData = readSamplesData(buffer);
    }
    return replayData;
}

float dist3d(Position p1, Position p2) {
    auto dx = p2.x - p1.x;
    auto dy = p2.y - p1.y;
    auto dz = p2.z - p1.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
bool isRespawnBehaviour(const GhostSample& a, const GhostSample& b) {
    auto dist = dist3d(a.pos, b.pos);
    return (b.speed < 500 && dist > 10) || (b.speed >= 500 && dist > 30);
}
std::vector<ContinuousReplayData> getReplayData(const std::wstring& fileName) {
    std::vector<ContinuousReplayData> replayData;

    auto replayDataSamples = getReplaySamplesList(fileName);
    auto& samplesLists = replayDataSamples.replaySamples;
    if (samplesLists.empty())
        return replayData;
    std::sort(samplesLists.begin(), samplesLists.end(), [](ReplaySamples& a, ReplaySamples& b) { return a.ghostSamples[0].time < b.ghostSamples[0].time; });
    
    // add finish cross event if it's missing (happens often/always? with ghost files that end exactly when finish is crossed)
    if (!replayDataSamples.cpTimes.empty()) {
        auto finishCrossTime = replayDataSamples.cpTimes.back();
        auto& lastEvents = samplesLists.back().events;
        auto lastCpCrossEvent = std::find_if(lastEvents.rbegin(), lastEvents.rend(), [&](auto& e) { return e.type == ReplayEvent::CpCross; });
        if (lastCpCrossEvent == lastEvents.rend() || std::abs(int(lastCpCrossEvent->time) - int(finishCrossTime)) > 200) {
            lastEvents.push_back(ReplayEvent{ ReplayEvent::CpCross, finishCrossTime });
        }
    }

    // combine sample lists that were split because of car switch
    for (int i = 0; i < int(samplesLists.size()) - 1; ++i) {
        if (samplesLists[i + 1].events.size() > 0 && samplesLists[i + 1].events[0].type == ReplayEvent::CarSwitch) {
            samplesLists[i].events.insert(samplesLists[i].events.end(), samplesLists[i + 1].events.begin(), samplesLists[i + 1].events.end());
            samplesLists[i].ghostSamples.insert(samplesLists[i].ghostSamples.end(), samplesLists[i + 1].ghostSamples.begin(), samplesLists[i + 1].ghostSamples.end());
            samplesLists.erase(samplesLists.begin() + i + 1);
            i -= 1;
        }
    }

    // split sample lists when you see a respawn behaviour
    for (int i = 0; i < samplesLists.size(); ++i) {
        auto& list = samplesLists[i];
        for (int j = 0; j < list.ghostSamples.size() - 1; ++j) {
            if (isRespawnBehaviour(list.ghostSamples[j], list.ghostSamples[j + 1])) {
                ReplaySamples newList;

                newList.ghostSamples.insert(newList.ghostSamples.begin(), list.ghostSamples.begin(), list.ghostSamples.begin() + j + 1);
                list.ghostSamples.erase(list.ghostSamples.begin(), list.ghostSamples.begin() + newList.ghostSamples.size());

                auto lastEventTime = list.ghostSamples[0].time;
                for (int k = 0; k < list.events.size() && list.events[k].time < lastEventTime; ++k) {
                    newList.events.push_back(list.events[k]);
                }
                list.events.erase(list.events.begin(), list.events.begin() + newList.events.size());

                samplesLists.insert(samplesLists.begin() + i, newList);
                break;
            }
        }
    }

    // save sample lists treating each one as if it was a seperate replay 
    for (auto& sampleList : samplesLists) {
        ContinuousReplayData continuousReplay;
        auto startTime = sampleList.ghostSamples[0].time;
        for (auto& event : sampleList.events) {
            if (event.type == ReplayEvent::CpCross) {
                continuousReplay.cpTimes.push_back(event.time - startTime);
            }
        }
        for (auto ghostSample : sampleList.ghostSamples) {
            ghostSample.time -= startTime;
            continuousReplay.ghostSamples.push_back(ghostSample);
        }
        replayData.push_back(continuousReplay);
    }

    return replayData;
}

void addCpPositions(std::vector<Position>& cpPositions, const ContinuousReplayData& replayData, int i = 0) {
    for (auto cpTime : replayData.cpTimes) {
        while (i < replayData.ghostSamples.size() && replayData.ghostSamples[i].time < cpTime) {
            i += 1;
        }
        if (i < replayData.ghostSamples.size())
            cpPositions.push_back(replayData.ghostSamples[i].pos);
        else
            cpPositions.push_back(replayData.ghostSamples.back().pos);
    }
}
std::vector<Position> getCpPositions(const ContinuousReplayData& replayData, int i = 0) {
    std::vector<Position> cpPositions;
    cpPositions.push_back(replayData.ghostSamples[i].pos);
    addCpPositions(cpPositions, replayData, i);
    return cpPositions;
}
std::vector<Position> getCpPositions(const std::vector<ContinuousReplayData>& replayData) {
    std::vector<Position> cpPositions;
    if (replayData.empty())
        return cpPositions;
    cpPositions.push_back(replayData[0].ghostSamples[0].pos); // start
    for (auto& rep : replayData) {
        addCpPositions(cpPositions, rep);
    }
    return cpPositions;
}

std::pair<int, float> findClosestPosition(Position pos, const std::vector<Position>& positions) {
    float minDist = std::numeric_limits<float>::max();
    int minPos = -1;
    for (int i = 0; i < positions.size(); ++i) {
        auto dist = dist3d(pos, positions[i]);
        if (dist < minDist) {
            minDist = dist;
            minPos = i;
        }
    }
    return { minPos, minDist };
}
std::vector<Connection> getConnections(const ContinuousReplayData& replayData, const std::vector<Position>& cpPositions) {
    std::vector<Connection> connections;
    if (replayData.ghostSamples.size() <= 2)
        return connections;
    int i = 0;
    if (replayData.ghostSamples[0].speed > 5) { 
        // it's likely flying respawn. Look in the next second for the sample where position is closest to some CP.
        // Most likely that sample is when car crosses the CP.
        float minDist = std::numeric_limits<float>::max();
        int minI = 0;
        while (replayData.ghostSamples[i].time <= 1'100) {
            auto [pos, dist] = findClosestPosition(replayData.ghostSamples[i].pos, cpPositions);
            if (dist <= minDist) {
                minDist = dist;
                minI = i;
            }
            i += 1;
            if (i >= replayData.ghostSamples.size())
                return connections;
        }
        i = minI;
    } else {
        // it's likely standing respawn. Get first next sample where speed is bigger than 1
        i = 1;
        while (replayData.ghostSamples[i].speed < 1) {
            i += 1;
            if (i >= replayData.ghostSamples.size())
                return connections;
        }
    }
    auto replayCpPositions = getCpPositions(replayData, i);
    auto prevTime = replayData.ghostSamples[i].time;
    auto [prevPos, _] = findClosestPosition(replayCpPositions[0], cpPositions);
    for (int i = 1; i < replayCpPositions.size(); ++i) {
        auto [pos, _] = findClosestPosition(replayCpPositions[i], cpPositions);
        auto time = (replayData.cpTimes[i - 1] < prevTime) ? 0 : int((replayData.cpTimes[i - 1] - prevTime) / 100);
        connections.push_back(Connection{ prevPos, pos, time });
        prevPos = pos;
        prevTime = replayData.cpTimes[i - 1];
    }
    return connections;
}
std::vector<Connection> getConnections(const std::vector<ContinuousReplayData>& replayData, const std::vector<Position>& cpPositions) {
    std::vector<Connection> connections;
    for (auto& continuousReplayData : replayData) {
        auto con = getConnections(continuousReplayData, cpPositions);
        connections.insert(connections.end(), con.begin(), con.end());
    }
    return connections;
}
