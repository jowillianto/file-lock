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

  void LargeFileMutex::lock() {
    _file_lock.lock();
  }

  void LargeFileMutex::unlock() {
    _file_lock.unlock();
  }

  bool LargeFileMutex::try_lock() {
    return _file_lock.try_lock();
  }

  void LargeFileMutex::lock_shared() {
    _file_lock.lock_shared();
  }

  void LargeFileMutex::unlock_shared() {
    _file_lock.unlock_shared();
  }

  bool LargeFileMutex::try_lock_shared() {
    return _file_lock.try_lock_shared();
  }

  SysFileLock &LargeFileMutex::file_lock() {
    return _file_lock.file_lock();
  }

  const std::filesystem::path &LargeFileMutex::protected_path() const {
    return _file_path;
  }

  std::string LargeFileMutex::protected_path_string() const {
    return _file_path.string();
  }
}