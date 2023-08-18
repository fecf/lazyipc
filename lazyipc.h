#pragma once

#include <cstdint>
#include <vector>

namespace lazyipc {

class mmap {
 public:
  mmap() = delete;
  mmap(const char* name, uint32_t file_size = 0);
  ~mmap();

  operator bool() const { return file_size_ > 0; }

  void* view() const;
  uint32_t file_size() const;
  bool owner() const;

 private:
  uint32_t file_size_;
  bool owner_;
  void* handle_;
  void* view_;
};

struct mmap_spsc_ringbuffer_desc {
  uint32_t capacity;
  uint32_t buffer_size;
  int64_t read_idx;
  int64_t write_idx;
};

struct mmap_spsc_ringbuffer_metadata {
  uint32_t data_start;
  uint32_t data_size;
};

class mmap_spsc_ringbuffer : public mmap {
 public:
  mmap_spsc_ringbuffer(const char* name,
                       uint32_t capacity,
                       uint32_t buffer_size);
  bool enqueue(void* data, uint32_t size);
  bool dequeue(std::vector<uint8_t>& buf);
  size_t size() const;
  size_t capacity() const;
  bool empty() const;

 private:
  mmap_spsc_ringbuffer_desc* desc_;
  mmap_spsc_ringbuffer_metadata* metadata_;
  uint8_t* buffer_;
};

}  // namespace lazyipc