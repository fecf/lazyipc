# lazyipc

lazyipc provides a memory-mapped file wrapper and a SPSC queue that can contain elements of arbitary sizes.

```
// process a
lazyipc::mmap mmap("lazyipc", 1024);  // create a memory-mapped file
assert(mmap.owner() == true);
::memcpy(mmap.view(), data, size);  // call view() to get a pointer

lazyipc::mmap_spsc_ringbuffer queue("queue", 1000, 1024768)  // queue capacity, memory-mapped file size
std::string str = "adlkfjalkdjafd";
bool success = queue.enqueue(str.data(), str.size()+1);  // +1 for '\0'

// process b
lazyipc::mmap mmap2("lazyipc", 1024);  // open an existing memory-mapped file (second argument is ignored)
assert(mmap2.owner() == false);
::memcpy(mmap2.view(), data, size);

lazyipc::mmap_spsc_ringbuffer queue2("queue", 1000, 1024768)  // throws an exception if sizes do not match
std::vector<uint8_t> vec;
bool success = queue2.dequeue(vec);
```
