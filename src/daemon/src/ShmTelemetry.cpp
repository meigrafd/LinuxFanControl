#include "ShmTelemetry.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <algorithm>

ShmPublisher::~ShmPublisher() {
    if (map_) { ::munmap(map_, mapLen_); map_ = nullptr; }
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool ShmPublisher::openOrCreate(const std::string& name, uint32_t slotSize, uint32_t capacity, std::string& err) {
    name_ = name;
    fd_ = ::shm_open(name.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd_ < 0) { err = "shm_open failed"; return false; }

    size_t hdr = sizeof(Header);
    size_t slots = (size_t)slotSize * (size_t)capacity;
    mapLen_ = hdr + slots;

    struct stat st{};
    if (fstat(fd_, &st) != 0) { err = "fstat failed"; return false; }
    bool needInit = (size_t)st.st_size != mapLen_;

    if (needInit) {
        if (ftruncate(fd_, mapLen_) != 0) { err = "ftruncate failed"; return false; }
    }

    map_ = ::mmap(nullptr, mapLen_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (map_ == MAP_FAILED) { map_ = nullptr; err = "mmap failed"; return false; }

    header_ = reinterpret_cast<Header*>(map_);
    slots_  = reinterpret_cast<char*>(header_ + 1);

    if (needInit || std::strncmp(header_->magic, "LFCshm", 6) != 0) {
        std::memset(map_, 0, mapLen_);
        std::memcpy(header_->magic, "LFCshm", 6);
        header_->version = 1;
        header_->capacity = capacity;
        header_->slotSize = slotSize;
        header_->writeIndex = 0;
    }
    return true;
}

void ShmPublisher::publish(const std::string& line) {
    if (!header_) return;
    uint32_t idx = __sync_fetch_and_add(&header_->writeIndex, 1);
    uint32_t slot = header_->capacity ? (idx % header_->capacity) : 0;
    char* dst = slots_ + (size_t)slot * (size_t)header_->slotSize;

    size_t n = std::min((size_t)header_->slotSize - 1, line.size());
    std::memcpy(dst, line.data(), n);
    if (n < (size_t)header_->slotSize) dst[n++] = '\n';
    if (n < (size_t)header_->slotSize) dst[n] = '\0';
}
