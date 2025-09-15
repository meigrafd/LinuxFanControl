#pragma once
/*
 * POSIX shared-memory ring buffer for telemetry (newline-delimited JSON).
 * Writer: daemon, Reader(s): GUI/tools.
 * (c) 2025 LinuxFanControl contributors
 */
#include <string>
#include <cstdint>

class ShmPublisher {
public:
    ShmPublisher() = default;
    ~ShmPublisher();

    // Create or open ring. name example: "/lfc_telemetry"
    // slotSize: bytes per record (recommend >= 1024)
    // capacity: number of slots in ring (power-of-two not required)
    bool openOrCreate(const std::string& name, uint32_t slotSize, uint32_t capacity, std::string& err);

    // Publish a line (auto-truncated to slotSize-1; newline appended if room)
    void publish(const std::string& jsonLine);

    // For debugging / introspection
    uint32_t slotSize() const { return header_ ? header_->slotSize : 0; }
    uint32_t capacity() const { return header_ ? header_->capacity : 0; }
    std::string name() const { return name_; }

private:
    struct Header {
        char     magic[8];     // "LFCshm\0"
        uint32_t version;      // 1
        uint32_t capacity;     // slots
        uint32_t slotSize;     // bytes
        uint32_t writeIndex;   // monotonically increasing
        uint32_t reserved;
    };

    int fd_ = -1;
    void* map_ = nullptr;
    size_t mapLen_ = 0;
    Header* header_ = nullptr;
    char* slots_ = nullptr;
    std::string name_;
};
