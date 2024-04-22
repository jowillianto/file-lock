# File Lock 
A multithreaded multiprocessed file locking mechanism compatible with `std::unique_lock` and `std::shared_lock`. A caveat to this FileLock system is that : 
- Multithreaded file sharing have to be done on the same FileLock object.

## Requirements
Since this process uses C++ modules, the following are the requirements : 
```
boost > 1.84 (with python)
cmake >= 3.28
clang >= 17 or equivalent gcc (with modules support p1689)
ninja >= 1.11.1
```

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

## Compiling for a Python Build
```sh
CXX=$(<path to your clang>) cmake -B build -GNinja
cd build
ninja multiprocessing_fileLock_python
```
The above commands will produce a `file-lock.so` file which you can import into a python application. 

# Python API Documentation
Assume that `file_lock.so` contains the following python definition with strict type definitions. 
```py
class FileMutex:
    def __init__(self, file_path : str):
        ...
      
    def lock() -> None:
        ...
    def unlock() -> None:
        ...
    def try_lock() -> bool:
        ...
    def lock_shared() -> None:
        ...
    def unlock_shared() -> None:
        ...
    def try_unlock_shared() -> bool:
        ...
    def file_path() -> str:
        ...
```