#pragma once
// Simple POSIX SHM ringbuffer writing JSON lines

#include <string>
#include <cstddef>
#include <cstdint>

namespace lfc {

    class ShmTelemetry {
    public:
        ShmTelemetry() = default;
        ~ShmTelemetry();

        bool openOrCreate(const std::string& shmPath, std::size_t capacityBytes = 1<<20); // 1MB
        void close();

        // Append a JSON line (adds '\n')
        void appendJsonLine(const std::string& jsonLine);

    private:
        int fd_{-1};
        void* mem_{nullptr};
        std::size_t cap_{0};

        struct Header {
            std::uint32_t magic;     // 'LFC1'
            std::uint32_t writeOff;  // ring write offset
            std::uint32_t size;      // capacity of data region
        };
        Header* hdr_{nullptr};
        char*   data_{nullptr};
    };

} // namespace lfc
