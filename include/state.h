#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace apple2e {

// Binary buffer for save states. Values are stored in host byte order: a
// state file is meant to be reloaded by the same build on the same machine.
class StateWriter {
public:
    template <typename T>
    void put(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        putBytes(&value, sizeof(T));
    }

    void putBytes(const void* data, size_t size) {
        auto* p = static_cast<const uint8_t*>(data);
        m_data.insert(m_data.end(), p, p + size);
    }

    void putString(const std::string& s) {
        put(static_cast<uint32_t>(s.size()));
        putBytes(s.data(), s.size());
    }

    const std::vector<uint8_t>& data() const { return m_data; }

private:
    std::vector<uint8_t> m_data;
};

// Reads back what StateWriter wrote. Reading past the end sets the failed
// flag and yields zeros, so callers can check ok() once at the end.
class StateReader {
public:
    explicit StateReader(const std::vector<uint8_t>& data) : m_data(data) {}

    template <typename T>
    void get(T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        getBytes(&value, sizeof(T));
    }

    template <typename T>
    T get() {
        T value{};
        get(value);
        return value;
    }

    void getBytes(void* out, size_t size) {
        if (m_failed || m_pos + size > m_data.size()) {
            m_failed = true;
            std::memset(out, 0, size);
            return;
        }
        std::memcpy(out, m_data.data() + m_pos, size);
        m_pos += size;
    }

    std::string getString() {
        auto size = get<uint32_t>();
        if (m_failed || m_pos + size > m_data.size()) {
            m_failed = true;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(m_data.data() + m_pos), size);
        m_pos += size;
        return s;
    }

    bool ok() const { return !m_failed; }
    bool atEnd() const { return m_pos == m_data.size(); }

private:
    const std::vector<uint8_t>& m_data;
    size_t m_pos = 0;
    bool m_failed = false;
};

} // namespace apple2e
