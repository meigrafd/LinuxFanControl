#pragma once
// Shared-memory telemetry ring (header-only).
// Binary frames: {ts_ns, id[64], duty, temp}. Simple SPSC-style ring.
// Comments in English per project guideline.

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cstring>
#include <string>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace lfc { namespace shm {

    constexpr uint32_t kMagic   = 0x4C464354; // 'LFCT'
    constexpr uint32_t kVersion = 1;

    struct RingHeader {
        uint32_t magic;
        uint32_t version;
        uint32_t capacity; // number of frames
        alignas(64) std::atomic<uint32_t> write_idx;
        char _pad[64 - sizeof(std::atomic<uint32_t>) % 64];
    };

    struct TelemetryFrame {
        uint64_t ts_ns;
        char     id[64];  // channel id (null-terminated, truncated if longer)
        double   duty;
        double   temp;
    };

    struct Mapping {
        int fd{-1};
        size_t size{0};
        RingHeader* hdr{nullptr};
        TelemetryFrame* frames{nullptr};
        bool owner{false};
        std::string name;
    };

    inline void destroy(Mapping& m) {
        if (m.hdr) {
            ::munmap(static_cast<void*>(m.hdr), m.size);
            m.hdr = nullptr; m.frames = nullptr;
        }
        if (m.fd >= 0) { ::close(m.fd); m.fd = -1; }
    }

    inline bool createOrOpen(Mapping& m, const char* shmName, uint32_t capacity, bool create) {
        m.name = shmName ? shmName : std::string();
        int flags = O_RDWR;
        if (create) flags |= O_CREAT;
        m.fd = ::shm_open(shmName, flags, 0600);
        if (m.fd < 0) return false;

        size_t need = sizeof(RingHeader) + sizeof(TelemetryFrame) * capacity;
        if (create) {
            if (::ftruncate(m.fd, static_cast<off_t>(need)) < 0) { destroy(m); return false; }
        } else {
            struct stat st{}; if (::fstat(m.fd, &st) < 0) { destroy(m); return false; }
            if (static_cast<size_t>(st.st_size) < sizeof(RingHeader)) { destroy(m); return false; }
            need = static_cast<size_t>(st.st_size);
        }

        void* p = ::mmap(nullptr, need, PROT_READ | PROT_WRITE, MAP_SHARED, m.fd, 0);
        if (p == MAP_FAILED) { destroy(m); return false; }

        m.size = need;
        m.hdr = reinterpret_cast<RingHeader*>(p);
        m.frames = reinterpret_cast<TelemetryFrame*>(reinterpret_cast<char*>(p) + sizeof(RingHeader));

        if (create) {
            m.hdr->magic = kMagic;
            m.hdr->version = kVersion;
            m.hdr->capacity = capacity;
            m.hdr->write_idx.store(0, std::memory_order_release);
        } else {
            if (m.hdr->magic != kMagic || m.hdr->version != kVersion) { destroy(m); return false; }
        }
        m.owner = create;
        return true;
    }

    inline bool writeFrame(Mapping& m, const char* id, double duty, double temp, uint64_t ts_ns) {
        if (!m.hdr) return false;
        const uint32_t idx = m.hdr->write_idx.fetch_add(1, std::memory_order_acq_rel);
        const uint32_t pos = (m.hdr->capacity ? idx % m.hdr->capacity : 0);
        TelemetryFrame& f = m.frames[pos];
        f.ts_ns = ts_ns;
        std::memset(f.id, 0, sizeof(f.id));
        if (id && *id) {
            std::strncpy(f.id, id, sizeof(f.id)-1);
        }
        f.duty = duty;
        f.temp = temp;
        return true;
    }

}} // namespace lfc::shm
