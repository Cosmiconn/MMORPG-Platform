#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <type_traits>
#include <bit>

namespace seed::serialize {

class BinaryWriter {
public:
    BinaryWriter() = default;
    explicit BinaryWriter(size_t reserveBytes) { m_buffer.reserve(reserveBytes); }

    void writeUInt8(uint8_t v);
    void writeUInt16(uint16_t v);
    void writeUInt32(uint32_t v);
    void writeUInt64(uint64_t v);
    void writeInt8(int8_t v)   { writeUInt8(static_cast<uint8_t>(v)); }
    void writeInt16(int16_t v) { writeUInt16(static_cast<uint16_t>(v)); }
    void writeInt32(int32_t v) { writeUInt32(static_cast<uint32_t>(v)); }
    void writeInt64(int64_t v) { writeUInt64(static_cast<uint64_t>(v)); }
    void writeFloat(float v);
    void writeDouble(double v);
    void writeBool(bool v)     { writeUInt8(v ? 1u : 0u); }

    void writeBytes(const void* data, size_t size);
    void writeString(const std::string& s);

    template<typename T>
    void writePOD(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "writePOD requires trivially copyable type");
        writeBytes(&value, sizeof(T));
    }

    const std::vector<uint8_t>& data() const { return m_buffer; }
    std::vector<uint8_t>& data() { return m_buffer; }
    size_t size() const { return m_buffer.size(); }
    void clear() { m_buffer.clear(); }

private:
    std::vector<uint8_t> m_buffer;
};

class BinaryReader {
public:
    BinaryReader() = default;
    explicit BinaryReader(const std::vector<uint8_t>& data) : m_data(data.data()), m_size(data.size()) {}
    explicit BinaryReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    void reset(const uint8_t* data, size_t size) { m_data = data; m_size = size; m_offset = 0; }
    void reset(const std::vector<uint8_t>& data) { reset(data.data(), data.size()); }

    uint8_t  readUInt8();
    uint16_t readUInt16();
    uint32_t readUInt32();
    uint64_t readUInt64();
    int8_t   readInt8()   { return static_cast<int8_t>(readUInt8()); }
    int16_t  readInt16()  { return static_cast<int16_t>(readUInt16()); }
    int32_t  readInt32()  { return static_cast<int32_t>(readUInt32()); }
    int64_t  readInt64()  { return static_cast<int64_t>(readUInt64()); }
    float    readFloat();
    double   readDouble();
    bool     readBool()   { return readUInt8() != 0; }

    void readBytes(void* out, size_t size);
    std::string readString();

    template<typename T>
    T readPOD() {
        static_assert(std::is_trivially_copyable_v<T>, "readPOD requires trivially copyable type");
        T value;
        readBytes(&value, sizeof(T));
        return value;
    }

    bool eof() const { return m_offset >= m_size; }
    size_t offset() const { return m_offset; }
    size_t remaining() const { return m_size > m_offset ? m_size - m_offset : 0; }

private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_offset = 0;
};

} // namespace seed::serialize
