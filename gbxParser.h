#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <string>
#include <variant>
#define NOMINMAX
#include <Windows.h>
#include "utility.h"
#include "immintrin.h"
#include "libdeflate.h"

using byte = uint8_t;
using sbyte = int8_t;
using u16 = uint16_t;
using u32 = uint32_t;

constexpr u32 CGameArenaPlayer = 0x032CB000;
constexpr u32 CSceneVehicleVis = 0x0A018000;
constexpr u32 CPlugEntRecordData = 0x0911F000;

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
struct vec3 {
    float x;
    float y;
    float z;
};
struct EntRecordSample {
    byte UO_0_1[2];
    u16 sideSpeedInt;
    byte UO_4[1];
    byte rpm;
    byte flWheelRotation;
    byte flWheelRotationCount;
    byte frWheelRotation;
    byte frWheelRotationCount;
    byte rrWheelRotation;
    byte rrWheelRotationCount;
    byte rlWheelRotation;
    byte rlWheelRotationCount;
    byte steer;
    byte UO_15_17[3];
    byte brake;
    byte UO_19_20[2];
    byte turboTimeByte;
    byte UO_22[1];
    byte flDampenLen;
    byte flGroundContactMaterial;
    byte frDampenLen;
    byte frGroundContactMaterial;
    byte rrDampenLen;
    byte rrGroundContactMaterial;
    byte rlDampenLen;
    byte rlGroundContactMaterial;
    byte isTurbo;
    byte slipCoef1;
    byte slipCoef2;
    byte UO_34_46[13];
    vec3 position;
    u16 axisHeading;
    u16 axisPitch;
    u16 speed;
    sbyte velocityHeading;
    sbyte velocityPitch;
    byte UO_67_75[9];
    byte vechicleState;
    byte UO_77_80[4];
    byte flIceByte;
    byte frIceByte;
    byte rrIceByte;
    byte rlIceByte;
    byte UO_85_88[4];
    byte groundMode;
    byte boosterAirControl;
    byte gearByte;
    byte UO5;
    byte flDirtByte;
    byte UO6;
    byte frDirtByte;
    byte UO7;
    byte rrDirtByte;
    byte UO8;
    byte rlDirtByte;
    byte UO9;
    byte waterByte;
    byte simulationTimeCoef;
    byte UO_103_[1]; // variable size
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
    auto d = libdeflate_alloc_decompressor();
    auto result = libdeflate_zlib_decompress(d, compressedData, compressedSize, decompressedData, decompressedSize, nullptr);
    libdeflate_free_decompressor(d);
    return result == LIBDEFLATE_SUCCESS;
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
    template<typename T1, typename T2, typename... Ts> std::tuple<T1, T2, Ts...> readNext() {
        auto v = std::tuple{ readNext<T1>() };
        return std::tuple_cat(v, std::tuple{ readNext<T2, Ts...>() });
    }
    template<typename T> T* readNextGetPtr(int size) {
        auto ptr = (T*)&data[readIndex];
        readIndex += size;
        return ptr;
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
    void skipNextLookbackString(bool& isFirstTime) {
        if (isFirstTime) { // maybe version
            auto version = readNext<u32>();
            if (version & 0xE0000000)
                readIndex -= 4;
            isFirstTime = false;
        }
        auto index = readNext<u32>();
        if ((index & 0xE0000000) && !(index & 0x1FFFFFFF)) {
            skipNextArray();
        }
    }
    void skipNextArray() {
        skipNext(readNext<u32>());
    }
    void skipNext(int byteCount) {
        readIndex += byteCount;
    }
};

ReplayData readSamplesData(DataBuffer& entRecordBuffer, int version) {
    ReplayData replayData;
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
        AssertReturnEmpty(type < entRecordClassIds.size());
        entRecordBuffer.skipNext(sizeof(u32) * 3);
        auto u4 = entRecordBuffer.readNext<u32>();

        if (entRecordClassIds[type] == CSceneVehicleVis && (u4 == 0 || u4 == 0xED00000 || u4 == 0xDA00000 || u4 == 0xDC00000 || u4 == 0xEB00000)) {
            ReplaySamples samples;
            if (version == 10) {
                while (entRecordBuffer.readNext<byte>()) {
                    auto [time, size] = entRecordBuffer.readNext<u32, u32>();
                    auto sample = entRecordBuffer.readNextGetPtr<EntRecordSample>(size);
                    auto speed = 3.6f * std::exp(sample->speed / 1000.0f);
                    samples.ghostSamples.push_back(GhostSample{ time, Position{sample->position.x, sample->position.y, sample->position.z}, speed });
                }
            } else {
                auto sampleCount = entRecordBuffer.readNext<u32>();
                if (sampleCount != 0) {
                    auto sampleSize = entRecordBuffer.readNext<u32>();
                    std::vector<std::pair<u32, std::vector<byte>>> sampleBuffers;
                    for (int i = 0; i < sampleCount; ++i) {
                        auto deltaTime = entRecordBuffer.readNext<u32>();
                        sampleBuffers.push_back({ deltaTime, std::vector<byte>(sampleSize) });
                    }
                    for (int i = 0; i < sampleSize; ++i) {
                        auto slice = entRecordBuffer.readNextGetPtr<byte>(sampleCount);
                        byte accumulator = 0;
                        for (int b = 0; b < sampleCount; ++b) {
                            accumulator += slice[b];
                            sampleBuffers[b].second[i] = accumulator;
                        }
                    }
                    u32 time = 0;
                    for (int i = 0; i < sampleBuffers.size(); ++i) {
                        time += sampleBuffers[i].first;
                        auto sample = (EntRecordSample*)sampleBuffers[i].second.data();
                        auto speed = 3.6f * std::exp(sample->speed / 1000.0f);
                        samples.ghostSamples.push_back(GhostSample{ time, Position{sample->position.x, sample->position.y, sample->position.z}, speed });
                    }
                }
            }
            hasNextElement = entRecordBuffer.readNext<byte>();
            while (entRecordBuffer.readNext<byte>()) {
                auto [type, time] = entRecordBuffer.readNext<u32, u32>();
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
        } else if (entRecordClassIds[type] == CGameArenaPlayer && (u4 == 0 || u4 == 0xED00000 || u4 == 0xDA00000)) {
            std::vector<u32> cpTimes;
            int cpNumber = 1;
            if (version == 10) {
                while (entRecordBuffer.readNext<byte>()) {
                    entRecordBuffer.skipNext(sizeof(u32));
                    entRecordBuffer.skipNextArray();
                }
            } else {
                auto sampleCount = entRecordBuffer.readNext<u32>();
                if (sampleCount != 0) {
                    auto sampleSize = entRecordBuffer.readNext<u32>();
                    entRecordBuffer.skipNext(sampleCount * (4 + sampleSize));
                }
            }
            hasNextElement = entRecordBuffer.readNext<byte>();
            while (entRecordBuffer.readNext<byte>()) {
                auto [type, time] = entRecordBuffer.readNext<u32, u32>();
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
            if (version == 10) {
                while (entRecordBuffer.readNext<byte>()) {
                    entRecordBuffer.skipNext(sizeof(u32));
                    entRecordBuffer.skipNextArray();
                }
            } else {
                auto sampleCount = entRecordBuffer.readNext<u32>();
                if (sampleCount != 0) {
                    auto sampleSize = entRecordBuffer.readNext<u32>();
                    entRecordBuffer.skipNext(sampleCount * (4 + sampleSize));
                }
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

struct BodyReadState {
    enum ErrorType {
        None = 0, 
        Unexpected,
        UnsupportedChunk,
        UnknownRecordDataVersion,
        MultipleRecordDataEntries
    };
    ErrorType error = None;
    bool isFirstLookbackString = true;
    DataBuffer& buffer;
    ReplayData& replayData;
    std::unordered_map<u32, std::function<void(BodyReadState& state)>>& functions;
    BodyReadState(DataBuffer& buffer, ReplayData& replayData, std::unordered_map<u32, std::function<void(BodyReadState& state)>>& functions) : 
        buffer(buffer), replayData(replayData), functions(functions)
    {}
};

void readBody(BodyReadState& state) {
    while (true) {
        if (state.error)
            return;
        if (!state.buffer.canReadNext<u32>()) {
            state.error = BodyReadState::ErrorType::Unexpected;
            return;
        }
        auto chunkId = state.buffer.readNext<u32>();
        if (chunkId == 0xFACADE01)
            return;
        if (auto fun = state.functions.find(chunkId); fun != state.functions.end()) {
            fun->second(state);
        } else {
            state.error = BodyReadState::ErrorType::UnsupportedChunk;
            return;
        }
    }
}

void rangesImpl(std::vector<u32>& data) {}
template<typename... Args> void rangesImpl(std::vector<u32>& data, std::pair<u32, u32> a, Args... rest);
template<typename... Args> void rangesImpl(std::vector<u32>& data, u32 a, Args... rest) {
    data.push_back(a);
    rangesImpl(data, rest...);
}
template<typename... Args> void rangesImpl(std::vector<u32>& data, std::pair<u32, u32> a, Args... rest) {
    for (auto v = a.first; v <= a.second; ++v)
        data.push_back(v);
    rangesImpl(data, rest...);
}
template<typename... Args> std::vector<u32> ranges(Args... args) {
    std::vector<u32> data;
    rangesImpl(data, args...);
    return data;
}

BodyReadState::ErrorType readBody(DataBuffer& buffer, ReplayData& replayData, bool isFirstLookbackString=true) {
    static std::unordered_map<u32, std::function<void(BodyReadState& state)>> functions;
    static bool wasInitialized = false;
    static std::mutex initMutex;

    if (!wasInitialized) {
        std::scoped_lock l{ initMutex };
        if (!wasInitialized) {
            #define ReadBodyAssertNoError(State) do { readBody((State)); if ((State).error) return; } while (false)
            #define AssertReturnWithError(Exp, Error) if (!(Exp)) { state.error = Error; return; }

            // skip chunks:
            std::vector<u32> skipChunks;
            for (u32 chunkPart : ranges(0x07, 0x08, 0x0F, 0x13, 0x18, std::pair(0x1A, 0x23), std::pair(0x25, 0x29)))
                skipChunks.push_back(0x03093000 | chunkPart); // CGameCtnReplay
            skipChunks.push_back(0x0303F007); // CGameGhost
            for (u32 chunkPart : ranges(0x04, 0x05, 0x08, 0x09, 0x0A, 0x0B, 0x13, 0x14, 0x17, 0x1A, 0x1B, 0x1D, 0x1F, std::pair(0x21, 0x2E)))
                skipChunks.push_back(0x03092000 | chunkPart); // CGameCtnGhost
            auto skipFunction = [](BodyReadState& state) {
                AssertReturnWithError(state.buffer.readNext<u32>() == 'SKIP', BodyReadState::ErrorType::Unexpected);
                auto size = state.buffer.readNext<u32>();
                state.buffer.skipNext(size);
            };
            for (auto& skipChunk : skipChunks) {
                functions.emplace(skipChunk, skipFunction);
            }

            // CPlugEntRecordData:
            functions.emplace(CPlugEntRecordData, [](BodyReadState& state) {
                auto recordDataVersion = state.buffer.readNext<u32>();
                AssertReturnWithError(recordDataVersion == 10 || recordDataVersion == 11, BodyReadState::ErrorType::UnknownRecordDataVersion);
                auto entRecordBuffer = state.buffer.readNextCompressed(zlibDecompress);
                if (state.replayData.replaySamples.empty()) {
                    state.replayData = readSamplesData(entRecordBuffer, recordDataVersion);
                } else {
                    state.error = BodyReadState::ErrorType::MultipleRecordDataEntries;
                }
            });

            // CGameCtnReplay:
            functions.emplace(0x03093002, [](BodyReadState& state) {
                auto size = state.buffer.readNext<u32>();
                state.buffer.skipNext(size); // map GBX
            });
            functions.emplace(0x03093014, [](BodyReadState& state) {
                state.buffer.readNext<u32>();
                auto numGhosts = state.buffer.readNext<u32>();
                for (int i = 0; i < numGhosts; ++i) {
                    auto noderef_index = state.buffer.readNext<u32>();
                    auto classId = state.buffer.readNext<u32>();
                    ReadBodyAssertNoError(state);
                }
                state.buffer.readNext<u32>();
                auto numExtras = state.buffer.readNext<u32>();
                state.buffer.skipNext(numExtras * sizeof(uint64_t));
            });
            functions.emplace(0x03093015, [](BodyReadState& state) {
                auto noderef_index = state.buffer.readNext<u32>();
                if (noderef_index != u32(~0)) {
                    auto classId = state.buffer.readNext<u32>();
                    ReadBodyAssertNoError(state);
                }
            });
            functions.emplace(0x03093024, [](BodyReadState& state) {
                auto version = state.buffer.readNext<u32>();
                auto UO2 = state.buffer.readNext<u32>();
                auto index = state.buffer.readNext<u32>();
                if (index != u32(~0)) {
                    AssertReturnWithError(state.buffer.readNext<u32>() == CPlugEntRecordData, BodyReadState::ErrorType::Unexpected);
                    ReadBodyAssertNoError(state);
                }
            });
            
            // CGameCtnGhost:
            functions.emplace(0x03092000, [](BodyReadState& state) {
                AssertReturnWithError(state.buffer.readNext<u32>() == 'SKIP', BodyReadState::ErrorType::Unexpected);
                auto size = state.buffer.readNext<u32>();
                auto version = state.buffer.readNext<u32>();
                u32 appearanceVersion = 0;
                if (version >= 9) {
                    appearanceVersion = state.buffer.readNext<u32>();
                }

                for (int i = 0; i < 3; ++i) {
                    state.buffer.skipNextLookbackString(state.isFirstLookbackString);
                }
                state.buffer.skipNext(3 * sizeof(float)); // vec3
                auto fileRefCount = state.buffer.readNext<u32>();
                for (int i = 0; i < fileRefCount; ++i) {
                    auto fileRefVersion = state.buffer.readNext<byte>();
                    if (fileRefVersion >= 3) {
                        state.buffer.skipNext(32); // checksum
                    }
                    auto filePathSize = state.buffer.readNext<u32>();
                    state.buffer.skipNext(filePathSize); // filePath
                    if (fileRefVersion >= 3 || (fileRefVersion >= 1 && filePathSize > 0)) {
                        state.buffer.skipNextArray(); // locatorUrl
                    }
                }
                auto hasBadges = state.buffer.readNext<u32>();
                if (hasBadges) {
                    auto badgeVersion = state.buffer.readNext<u32>();
                    state.buffer.skipNext(3 * sizeof(float)); // vec3
                    if (badgeVersion == 0) {
                        state.buffer.readNext<u32>();
                        state.buffer.skipNextArray();
                    }
                    auto stickersArraySize = state.buffer.readNext<u32>();
                    for (int i = 0; i < stickersArraySize; ++i) {
                        state.buffer.skipNextArray();
                        state.buffer.skipNextArray();
                    }
                    auto layerArraySize = state.buffer.readNext<u32>();
                    for (int i = 0; i < layerArraySize; ++i) {
                        state.buffer.skipNextArray();
                    }
                }
                if (appearanceVersion >= 1) {
                    state.buffer.skipNextArray(); // UO7
                }
                state.buffer.skipNextArray(); // ghostNickname
                state.buffer.skipNextArray(); // ghostAvatarName
                state.buffer.skipNextArray(); // recordingContext
                auto U03 = state.buffer.readNext<u32>();

                auto index = state.buffer.readNext<u32>();
                if (index != u32(~0)) {
                    AssertReturnWithError(state.buffer.readNext<u32>() == CPlugEntRecordData, BodyReadState::ErrorType::Unexpected);
                    ReadBodyAssertNoError(state);
                }
                state.buffer.skipNext(state.buffer.readNext<u32>() * sizeof(u32)); // UO4
                state.buffer.skipNextArray(); // ghostTrigram
                state.buffer.skipNextArray(); // ghostZone
                if (version >= 8) {
                    state.buffer.skipNextArray(); // ghostClubTag
                }
            });
            functions.emplace(0x0309200C, [](BodyReadState& state) {
                state.buffer.readNext<u32>();
            });
            functions.emplace(0x0309201C, [](BodyReadState& state) {
                state.buffer.skipNext(32);
            });
            functions.emplace(0x0309200E, [](BodyReadState& state) {
                state.buffer.skipNextLookbackString(state.isFirstLookbackString);
            });
            functions.emplace(0x0309200F, [](BodyReadState& state) {
                auto size = state.buffer.readNext<u32>();
                state.buffer.skipNext(size);
            });
            functions.emplace(0x03092010, [](BodyReadState& state) {
                state.buffer.skipNextLookbackString(state.isFirstLookbackString);
            });
            
            // CGameGhost:
            functions.emplace(0x0303F005, [](BodyReadState& state) {
                auto uncompressedSize = state.buffer.readNext<u32>();
                auto compressedSize = state.buffer.readNext<u32>();
                state.buffer.skipNext(compressedSize);
            });
            functions.emplace(0x0303F006, [](BodyReadState& state) {
                bool isReplaying = state.buffer.readNext<u32>();
                auto uncompressedSize = state.buffer.readNext<u32>();
                auto compressedSize = state.buffer.readNext<u32>();
                state.buffer.skipNext(compressedSize);
            });

            #undef AssertReturnWithError

            wasInitialized = true;
        }
    }
    BodyReadState bodyReadState(buffer, replayData, functions);
    readBody(bodyReadState);
    return bodyReadState.error;
}

ReplayData getReplaySamplesList(const std::wstring& fileName, BodyReadState::ErrorType& error) {
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
    error = readBody(buffer, replayData);
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
    return (b.speed < 500 && dist > 15) || (b.speed >= 500 && dist > 40);
}
std::vector<ReplaySamples> getReplaySamples(const std::wstring& fileName, std::string& outError) {
    BodyReadState::ErrorType error;
    auto replayDataSamples = getReplaySamplesList(fileName, error);
    if (error == BodyReadState::ErrorType::MultipleRecordDataEntries) {
        outError = "Encountered multiple RecordData entries";
    } else if (error == BodyReadState::ErrorType::UnknownRecordDataVersion) {
        outError = "Encountered unsupported RecordData version";
    } else if (error == BodyReadState::ErrorType::UnsupportedChunk) {
        outError = "Encountered unsupported ChunkID";
    } else if (error == BodyReadState::ErrorType::Unexpected) {
        outError = "Unexpected problem";
    }
    if (!outError.empty())
        return {};

    auto& samplesLists = replayDataSamples.replaySamples;
    if (samplesLists.empty())
        return {};
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

    return samplesLists;
}
std::vector<ContinuousReplayData> getReplayData(const std::wstring& fileName, std::string& outError) {
    auto samplesLists = getReplaySamples(fileName, outError);

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
    std::vector<ContinuousReplayData> replayData;
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

ContinuousReplayData getNoRespawnReplayData(const std::wstring& fileName, std::string& outError) {
    auto listX = getReplaySamples(fileName, outError);
    auto list = listX[0];
    list.events.erase(std::remove_if(list.events.begin(), list.events.end(), [](ReplayEvent& e) { return e.type != ReplayEvent::CpCross; }), list.events.end());

    for (int j = 0; j < list.ghostSamples.size() - 1; ++j) {
        if (isRespawnBehaviour(list.ghostSamples[j], list.ghostSamples[j + 1])) {
            int i = 0;
            while (list.events[i].time < list.ghostSamples[j + 2].time) {
                i += 1;
            }
            i -= 1;
            
            int k = j + 2;
            while (list.ghostSamples[k].time >= list.events[i].time) {
                k -= 1;
            }
            k += 1;

            list.ghostSamples.erase(list.ghostSamples.begin() + k, list.ghostSamples.begin() + j + 1);
            j = k + 19;
        }
    }

    ContinuousReplayData replayData;
    for (auto& event : list.events) {
        if (event.type == ReplayEvent::CpCross) {
            replayData.cpTimes.push_back(event.time);
        }
    }
    for (auto& ghostSample : list.ghostSamples) {
        replayData.ghostSamples.push_back(ghostSample);
    }
    return replayData;
}