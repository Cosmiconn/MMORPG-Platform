#include "core/serialize/binary_writer.h"
#include "core/profiling/seed_assert.h"

#if defined(_WIN32)
#  include <stdlib.h>
#  define SEED_BYTE_SWAP_16(x) _byteswap_ushort(x)
#  define SEED_BYTE_SWAP_32(x) _byteswap_ulong(x)
#  define SEED_BYTE_SWAP_64(x) _byteswap_uint64(x)
#elif defined(__GNUC__) || defined(__clang__)
#  define SEED_BYTE_SWAP_16(x) __builtin_bswap16(x)
#  define SEED_BYTE_SWAP_32(x) __builtin_bswap32(x)
#  define SEED_BYTE_SWAP_64(x) __builtin_bswap64(x)
#else
#  error "Byte-swap intrinsics not defined for this compiler"
#endif

namespace seed::serialize {

static inline bool isLittleEndian() {
    constexpr uint16_t test = 0x0102;
    return *reinterpret_cast<const uint8_t*>(&test) == 0x02;
}

static inline uint16_t toLittleEndian(uint16_t v) {
    if constexpr (isLittleEndian()) return v;
    return SEED_BYTE_SWAP_16(v);
}

static inline uint32_t toLittleEndian(uint32_t v) {
    if constexpr (isLittleEndian()) return v;
    return SEED_BYTE_SWAP_32(v);
}

static inline uint64_t toLittleEndian(uint64_t v) {
    if constexpr (isLittleEndian()) return v;
    return SEED_BYTE_SWAP_64(v);
}

void BinaryWriter::writeUInt8(uint8_t v) {
    m_buffer.push_back(v);
}

void BinaryWriter::writeUInt16(uint16_t v) {
    uint16_t le = toLittleEndian(v);
    writeBytes(&le, sizeof(le));
}

void BinaryWriter::writeUInt32(uint32_t v) {
    uint32_t le = toLittleEndian(v);
    writeBytes(&le, sizeof(le));
}

void BinaryWriter::writeUInt64(uint64_t v) {
    uint64_t le = toLittleEndian(v);
    writeBytes(&le, sizeof(le));
}

void BinaryWriter::writeFloat(float v) {
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    writeUInt32(*reinterpret_cast<const uint32_t*>(&v));
}

void BinaryWriter::writeDouble(double v) {
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    writeUInt64(*reinterpret_cast<const uint64_t*>(&v));
}

void BinaryWriter::writeBytes(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_buffer.insert(m_buffer.end(), bytes, bytes + size);
}

void BinaryWriter::writeString(const std::string& s) {
    writeUInt32(static_cast<uint32_t>(s.size()));
    if (!s.empty()) {
        writeBytes(s.data(), s.size());
    }
}

uint8_t BinaryReader::readUInt8() {
    SEED_ASSERT(m_offset < m_size, "BinaryReader: read past end of buffer");
    return m_data[m_offset++];
}

uint16_t BinaryReader::readUInt16() {
    uint16_t v;
    readBytes(&v, sizeof(v));
    return toLittleEndian(v);
}

uint32_t BinaryReader::readUInt32() {
    uint32_t v;
    readBytes(&v, sizeof(v));
    return toLittleEndian(v);
}

uint64_t BinaryReader::readUInt64() {
    uint64_t v;
    readBytes(&v, sizeof(v));
    return toLittleEndian(v);
}

float BinaryReader::readFloat() {
    uint32_t v = readUInt32();
    return *reinterpret_cast<float*>(&v);
}

double BinaryReader::readDouble() {
    uint64_t v = readUInt64();
    return *reinterpret_cast<double*>(&v);
}

void BinaryReader::readBytes(void* out, size_t size) {
    SEED_ASSERT(m_offset + size <= m_size, "BinaryReader: read past end of buffer");
    std::memcpy(out, m_data + m_offset, size);
    m_offset += size;
}

std::string BinaryReader::readString() {
    uint32_t len = readUInt32();
    SEED_ASSERT(m_offset + len <= m_size, "BinaryReader: string read past end of buffer");
    std::string s;
    if (len > 0) {
        s.assign(reinterpret_cast<const char*>(m_data + m_offset), len);
        m_offset += len;
    }
    return s;
}

} // namespace seed::serialize
