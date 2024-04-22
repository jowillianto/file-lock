module;
#include <filesystem>
module file_lock;

namespace file_lock {
  LargeFileMutex::LargeFileMutex(const std::filesystem::path &file_path) :
    _file_path(file_path),
    _lock_file_path(file_path.parent_path() / (file_path.stem().string() + ".sys_lock")),
    _file_lock(_lock_file_path) {}

  LargeFileMutex::LargeFileMutex(const std::string &file_path) :
    _file_path(file_path),
    _lock_file_path(_file_path.parent_path() / (_file_path.stem().string() + ".sys_lock")),
    _file_lock(_lock_file_path) {}

  void LargeFileMutex::lock() const {
    _file_lock.lock();
  }

  void LargeFileMutex::unlock() const {
    _file_lock.unlock();
  }

  bool LargeFileMutex::try_lock() const {
    return _file_lock.try_lock();
  }

  void LargeFileMutex::lock_shared() const {
    _file_lock.lock_shared();
  }

  void LargeFileMutex::unlock_shared() const {
    _file_lock.unlock_shared();
  }

  bool LargeFileMutex::try_lock_shared() const {
    return _file_lock.try_lock_shared();
  }
}