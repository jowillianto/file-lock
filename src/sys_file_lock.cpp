module;
#include <sys/fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <sys/file.h>
module file_lock;

namespace file_lock {
  void SysFileLock::_throw_sys_error() {
    throw std::filesystem::filesystem_error{
      std::strerror(errno), _file_path, std::error_code{errno, std::generic_category()}
    };
  }

  void SysFileLock::_open_file_and_absol_path(int additional_flags) {
    if (!std::filesystem::exists(_file_path)) {
      _fd = open(_file_path.c_str(), O_CREAT | O_RDWR, 0664);
      if (_fd == -1) _throw_sys_error();
      close(_fd);
    }
    _file_path = std::filesystem::absolute(_file_path);
    _fd = open(_file_path.c_str(), O_RDONLY);
    if (_fd == -1) _throw_sys_error();
  }

  SysFileLock::SysFileLock(const std::filesystem::path &file_path) : _file_path(file_path) {
    _open_file_and_absol_path(0);
  }

  SysFileLock::SysFileLock(const std::string &file_path) : _file_path(file_path) {
    _open_file_and_absol_path(0);
  }

  void SysFileLock::lock() {
    int lock_st = flock(_fd, LOCK_EX);
    if (lock_st == -1) _throw_sys_error();
  }

  void SysFileLock::unlock() {
    int lock_st = flock(_fd, LOCK_UN);
    if (lock_st == -1) _throw_sys_error();
  }

  bool SysFileLock::try_lock() {
    int lock_st = flock(_fd, LOCK_EX | LOCK_NB);
    return lock_st != -1;
  }

  void SysFileLock::lock_shared() {
    int lock_st = flock(_fd, LOCK_SH);
    if (lock_st == -1) _throw_sys_error();
  }

  void SysFileLock::unlock_shared() {
    int lock_st = flock(_fd, LOCK_UN);
    if (lock_st == -1) _throw_sys_error();
  }

  bool SysFileLock::try_lock_shared() {
    int lock_st = flock(_fd, LOCK_SH | LOCK_NB);
    return lock_st != -1;
  }

  const std::filesystem::path &SysFileLock::protected_path() const {
    return _file_path;
  }

  std::string SysFileLock::protected_path_string() const {
    return _file_path.string();
  }

  SysFileLock::~SysFileLock() {
    if (_fd != -1) {
      int close_st = close(_fd);
      if (close_st == -1) _throw_sys_error();
    }
  }
}