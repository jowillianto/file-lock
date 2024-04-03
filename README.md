# File Lock 
A multithreaded multiprocessed file locking mechanism compatible with `std::unique_lock` and `std::shared_lock`. A caveat to this FileLock system is that : 
- Multithreaded file sharing have to be done on the same FileLock object.

## Constructing a File Lock
To construct a file 
```cpp
#include <filesystem>
#include <unique_lock>
#include <shared_lock>
import file_lock;

auto file_path = std::filesystem::path {R"(some path to your file)"};
auto lock = file_lock::BasicMutex { file_path };
lock.lock(); // Locks the file for writing (blocking)
lock.unlock(); // Unlocks the file
/*
  Try to lock the file (nonblocking), returns true if the file is successfully locked
  otherwise returns false.
*/
lock.try_lock(); 

// Shared version
lock.lock_shared();
lock.unlock_shared();
lock.try_lock_shared();

// Also works with unique_lock and shared_lock RAII based locks
std::unique_lock unique_lock { lock };
std::shared_lock shared_lock { lock };
```

## Linking via CMake
```cmake

target_link_libraries(<target> multiprocessing_fileLock)
```