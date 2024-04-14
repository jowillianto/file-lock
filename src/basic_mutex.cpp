module;
#include <filesystem>
#include <string>
module file_lock;

namespace file_lock {
  FileMutex::FileMutex(const std::filesystem::path &file_path) : _file_lock{file_path} {}

  FileMutex::FileMutex(const std::string &file_path) : _file_lock{file_path} {}

  void FileMutex::lock() {
    _mutex.lock();
    _file_lock.lock();
  }

  void FileMutex::unlock() {
    _file_lock.unlock();
    _mutex.unlock();
  }

  bool FileMutex::try_lock() {
    if (!_mutex.try_lock()) return false;
    if (!_file_lock.try_lock()) {
      _mutex.unlock();
      return false;
    }
    return true;
  }

  void FileMutex::lock_shared() {
    _mutex.lock_shared();
    _file_lock.lock_shared();
  }

  void FileMutex::unlock_shared() {
    _file_lock.unlock_shared();
    _mutex.unlock_shared();
  }

  bool FileMutex::try_lock_shared() {
    if (!_mutex.try_lock_shared()) return false;
    if (!_file_lock.try_lock_shared()) {
      _mutex.unlock_shared();
      return false;
    }
    return true;
  }

  const std::filesystem::path &FileMutex::protected_path() const {
    return _file_lock.protected_path();
  }

  std::string FileMutex::protected_path_string() const {
    return _file_lock.protected_path_string();
  }

  SysFileLock &FileMutex::file_lock() {
    return _file_lock;
  }
}