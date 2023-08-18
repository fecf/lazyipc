#include "lazyipc.h"

#include <cassert>
#include <stdexcept>

#include <windows.h>

namespace lazyipc {

namespace {

inline int64_t load_acquire(int64_t& target) {
  return ::InterlockedAddAcquire64(&target, 0);
}

inline int64_t load_release(int64_t& target) {
  return ::InterlockedAddRelease64(&target, 0);
}

inline int64_t load_relaxed(int64_t& target) {
  return ::InterlockedAddNoFence64(&target, 0);
}

inline int64_t store_acquire(int64_t& target, int64_t value) {
  return ::InterlockedExchangeAcquire64(&target, value);
}

inline int64_t store_release(int64_t& target, int64_t value) {
  return ::InterlockedExchange64(&target, value);
}

inline int64_t store_relaxed(int64_t& target, int64_t value) {
  return ::InterlockedExchangeNoFence64(&target, value);
}

}  // namespace

mmap::mmap(const char* name, uint32_t file_size) : file_size_(file_size) {
  assert(name != nullptr);
  assert(file_size > 0);

  handle_ = (void*)::CreateFileMapping(
      INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 0, file_size, name);
  if (handle_ == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("failed to CreateFileMapping().");
  }
  DWORD err = ::GetLastError();

  view_ = ::MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (view_ == nullptr) {
    throw std::runtime_error("failed to MapViewOfFile().");
  }

  owner_ = (err != ERROR_ALREADY_EXISTS);
  if (owner_) {
    ::memset(view_, 0x0, file_size);
  }
}

mmap::~mmap() {
  if (handle_) {
    ::UnmapViewOfFile(view_);
    ::CloseHandle((HANDLE)handle_);
  }
}

void* mmap::view() const {
  return view_;
}

uint32_t mmap::file_size() const {
  return file_size_;
}

bool mmap::owner() const {
  return owner_;
}

mmap_spsc_ringbuffer::mmap_spsc_ringbuffer(const char* name,
                                           uint32_t capacity,
                                           uint32_t buffer_size)
    : mmap(name,
           sizeof(mmap_spsc_ringbuffer_desc) +
               sizeof(mmap_spsc_ringbuffer_metadata) * capacity + buffer_size) {
  assert(capacity > 0);
  assert(buffer_size > 0);

  desc_ = (mmap_spsc_ringbuffer_desc*)view();
  if (owner()) {
    desc_->capacity = capacity;
    desc_->buffer_size = buffer_size;
  } else {
    assert(desc_->capacity == capacity);
    assert(desc_->buffer_size == buffer_size);
  }

  metadata_ =
      (mmap_spsc_ringbuffer_metadata*)((uint8_t*)mmap::view() +
                                       sizeof(mmap_spsc_ringbuffer_desc));
  buffer_ =
      (uint8_t*)metadata_ + sizeof(mmap_spsc_ringbuffer_metadata) * capacity;
}

bool mmap_spsc_ringbuffer::enqueue(void* data, uint32_t size) {
  assert(size <= desc_->buffer_size);

  int64_t write_idx = load_relaxed(desc_->write_idx);
  int64_t read_idx = load_acquire(desc_->read_idx);
  if (write_idx - read_idx == desc_->capacity) {
    return false;
  }

  uint32_t data_start = 0;
  if (write_idx > 0) {
    mmap_spsc_ringbuffer_metadata* back =
        metadata_ + ((write_idx - 1) % desc_->capacity);
    if (back != nullptr) {
      mmap_spsc_ringbuffer_metadata* front =
          metadata_ + (read_idx % desc_->capacity);
      data_start = back->data_start + back->data_size;
      if (data_start + size > desc_->buffer_size) {
        data_start = 0;
        if (front->data_start < data_start + size) {
          return false;
        }
      }
    }
  }

  mmap_spsc_ringbuffer_metadata* metadata =
      metadata_ + (write_idx % desc_->capacity);
  metadata->data_start = data_start;
  metadata->data_size = size;
  ::memcpy_s(buffer_ + data_start, size, data, size);
  store_release(desc_->write_idx, write_idx + 1);
  return true;
}

bool mmap_spsc_ringbuffer::dequeue(std::vector<uint8_t>& buf) {
  int64_t read_idx = load_relaxed(desc_->read_idx);
  int64_t write_idx = load_acquire(desc_->write_idx);
  if (read_idx == write_idx) {
    return false;
  }

  mmap_spsc_ringbuffer_metadata* back =
      metadata_ + (read_idx % desc_->capacity);
  buf.resize(back->data_size);
  ::memcpy_s(buf.data(), buf.size(), buffer_ + back->data_start,
             back->data_size);
  back->data_size = 0;
  back->data_start = 0;

  store_release(desc_->read_idx, read_idx + 1);
  return true;
}

size_t mmap_spsc_ringbuffer::size() const {
  int64_t write_idx = load_acquire(desc_->read_idx);
  int64_t read_idx = load_acquire(desc_->write_idx);
  return (size_t)(write_idx - read_idx);
}

size_t mmap_spsc_ringbuffer::capacity() const {
  return desc_->capacity;
}

bool mmap_spsc_ringbuffer::empty() const {
  int64_t write_idx = load_acquire(desc_->read_idx);
  int64_t read_idx = load_acquire(desc_->write_idx);
  return write_idx == read_idx;
}

}  // namespace lazyipc
