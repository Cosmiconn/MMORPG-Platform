#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace seed::serialize {

class BinaryReader {
public:
    explicit BinaryReader(const std::vector<uint8_t>& data);

    uint8_t  readUInt8();
    uint16_t readUInt16();
    uint32_t readUInt32();
    uint64_t readUInt64();
    float    readFloat();
    double   readDouble();
    bool     readBool();
    void     readBytes(void* dest, size_t size);
    std::string readString();

    template<typename T>
    T readPOD() {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        T value;
        readBytes(&value, sizeof(T));
        return value;
    }

    bool eof() const noexcept;
    size_t position() const noexcept;
    size_t remaining() const noexcept;
    const std::vector<uint8_t>& data() const noexcept;

private:
    const std::vector<uint8_t>& m_data;
    size_t m_pos = 0;

    void ensureRemaining(size_t bytes) const;
};

} // namespace seed::serialize
