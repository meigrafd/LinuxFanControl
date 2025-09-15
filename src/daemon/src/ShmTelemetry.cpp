#include "ShmTelemetry.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

using namespace lfc;

ShmTelemetry::~ShmTelemetry() { close(); }

bool ShmTelemetry::openOrCreate(const std::string& shmPath, std::size_t capacityBytes) {
    close();
    std::string name = shmPath;
    if (name.empty() || name[0] != '/') name = "/" + name;
    fd_ = ::shm_open(name.c_str(), O_CREAT|O_RDWR, 0644);
    if (fd_ < 0) return false;

    cap_ = sizeof(Header) + capacityBytes;
    if (::ftruncate(fd_, cap_) < 0) { close(); return false; }

    mem_ = ::mmap(nullptr, cap_, PROT_READ|PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mem_ == MAP_FAILED) { mem_=nullptr; close(); return false; }

    hdr_  = reinterpret_cast<Header*>(mem_);
    data_ = reinterpret_cast<char*>(hdr_+1);

    if (hdr_->magic != 0x4C464331 /*'LFC1'*/) {
        std::memset(mem_, 0, cap_);
        hdr_->magic = 0x4C464331;
        hdr_->writeOff = 0;
        hdr_->size = static_cast<std::uint32_t>(capacityBytes);
    }
    return true;
}

void ShmTelemetry::close() {
    if (mem_) { ::msync(mem_, cap_, MS_SYNC); ::munmap(mem_, cap_); mem_ = nullptr; }
    if (fd_>=0) { ::close(fd_); fd_=-1; }
    hdr_=nullptr; data_=nullptr; cap_=0;
}

void ShmTelemetry::appendJsonLine(const std::string& line) {
    if (!hdr_ || !data_ || hdr_->size==0) return;
    std::string s = line; s.push_back('\n');
    std::uint32_t n = (std::uint32_t)s.size();
    auto cap = hdr_->size;
    auto& w  = hdr_->writeOff;

    // wrap if needed
    if (w + n > cap) {
        // write two parts
        std::uint32_t first = cap - w;
        std::memcpy(data_ + w, s.data(), first);
        std::memcpy(data_, s.data()+first, n-first);
        w = (n-first) % cap;
    } else {
        std::memcpy(data_ + w, s.data(), n);
        w += n;
        if (w >= cap) w %= cap;
    }
}
