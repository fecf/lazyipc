#include "lazyipc.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
  uint32_t size = 10000;
  lazyipc::mmap mmap("lazyipc", size);
  assert(mmap.owner() == true);
  size = mmap.file_size();

  lazyipc::mmap mmap2("lazyipc");
  assert(mmap2.owner() == false);
  assert(mmap2.file_size() == size);

  lazyipc::mmap mmap3("lazyipc", 20000);
  assert(mmap3.owner() == false);
  assert(mmap3.file_size() == size);

  uint32_t size2 = 30000;
  lazyipc::mmap mmap4("lazyipc2", size2);
  size2 = mmap4.file_size();
  assert(mmap4.owner() == true);
  assert(mmap4.file_size() == size2);

  // taken from https://github.com/rigtorp/SPSCQueue/blob/master/src/SPSCQueueTest.cpp
  const size_t iter = 100000;
  std::atomic<bool> flag(false);
  std::thread producer([&] {
    lazyipc::mmap_spsc_ringbuffer t1("spsc_test", iter / 1000 + 1, (iter / 1000 + 1) * sizeof(size_t));
    while (!flag) {}
    for (size_t i = 0; i < iter; ++i) {
      while (!t1.enqueue(&i, sizeof(size_t))) {}
    }
  });

  lazyipc::mmap_spsc_ringbuffer t2("spsc_test", iter / 1000 + 1, (iter / 1000 + 1) * sizeof(size_t));
  size_t sum = 0;
  auto start = std::chrono::system_clock::now();
  flag = true;
  std::vector<uint8_t> buf;
  for (size_t i = 0; i < iter; ++i) {
    while (t2.empty()) {}
    bool ret = t2.dequeue(buf);
    assert(ret);
    assert(buf.size() == sizeof(size_t));
    sum += *(size_t*)(buf.data());
  }
  auto end = std::chrono::system_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
  size_t expected = iter * (iter - 1) / 2;
  assert(sum == iter * (iter - 1) / 2);
  producer.join();
  std::cout << duration.count() / iter << " ns/iter" << std::endl;

  return 0;
}