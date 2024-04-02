#include <sys/wait.h>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <array>
#include <iostream>
import test_lib;
import file_lock;

class DirGuard {
  std::filesystem::path _path;

public:
  DirGuard(std::filesystem::path path) : _path(std::move(path)) {
    std::filesystem::create_directories(_path);
  }
  ~DirGuard() {
    std::filesystem::remove_all(_path);
  }
};

class SafeThread {
  std::thread _thread;

public:
  SafeThread(std::thread &&thread) : _thread(std::move(thread)) {}
  void detach() {
    _thread.detach();
  }
  ~SafeThread() {
    if (_thread.joinable()) _thread.join();
  }
};

template <file_lock::SharedMutex T>
  requires(std::constructible_from<T, std::filesystem::path>)
auto mutex_tester(
  const std::string &test_suite, const std::filesystem::path &tmp_fd
) {
  return test_lib::Tester<>{test_suite}
    .add_test(
      "create_no_truncate",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        std::fstream file{file_path, std::ios_base::out | std::ios_base::trunc};
        std::string random_value = test_lib::random_string(20);
        file << random_value;
        file.close();
        T file_mutex{file_path};
        std::ifstream read_file{file_path};
        std::stringstream buffer;
        buffer << read_file.rdbuf();
        test_lib::assert_equal(buffer.str(), random_value);
        read_file.close();
      }
    )
    .add_test(
      "no_concurrent_writes",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          file_mutex.lock();
          std::this_thread::sleep_for(std::chrono::seconds(3));
          file_mutex.unlock();
        }}};
        std::this_thread::sleep_for(std::chrono::seconds{1});
        if (file_mutex.try_lock()) {
          file_mutex.unlock();
          throw std::bad_exception{};
        }
      }
    )
    .add_test(
      "allow_concurrent_reads",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        SafeThread thread{std::thread{[&]() mutable {
          file_mutex.try_lock_shared();
          std::this_thread::sleep_for(std::chrono::seconds{3});
          file_mutex.unlock_shared();
        }}};
        std::this_thread::sleep_for(std::chrono::seconds{1});
        if (file_mutex.try_lock_shared()) {
          file_mutex.unlock_shared();
          return;
        }
        throw std::bad_exception{};
      }
    )
    .add_test(
      "no_concurrent_write_and_read",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        SafeThread thread{std::thread{[&]() mutable {
          file_mutex.try_lock_shared();
          std::this_thread::sleep_for(std::chrono::seconds{3});
          file_mutex.unlock_shared();
        }}};
        std::this_thread::sleep_for(std::chrono::seconds{1});
        if (file_mutex.try_lock()) {
          file_mutex.unlock();
          throw std::bad_exception{};
        }
      }
    )
    .add_test(
      "std::shared_lock_consistency",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        SafeThread thread{std::thread{[&]() mutable {
          std::shared_lock lock{file_mutex};
          std::this_thread::sleep_for(std::chrono::seconds{3});
        }}};
        std::this_thread::sleep_for(std::chrono::seconds{1});
        if (file_mutex.try_lock()) {
          file_mutex.unlock();
          throw std::bad_exception{};
        }
      }
    )
    .add_test(
      "std::unique_lock_consistency",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          std::unique_lock lock{file_mutex};
          std::this_thread::sleep_for(std::chrono::seconds{3});
        }}};
        std::this_thread::sleep_for(std::chrono::seconds{1});
        if (file_mutex.try_lock()) {
          file_mutex.unlock();
          throw std::bad_exception{};
        }
      }
    )
    .add_test(
      "no_conccurent_multi_process_write",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        int child_pid = fork();
        if (child_pid == -1)
          throw std::system_error{
            std::error_code{errno, std::generic_category()}
          };
        else if (child_pid == 0) {
          T file_mutex{file_path};
          std::this_thread::sleep_for(std::chrono::seconds{3});
          if (file_mutex.try_lock()) {
            file_mutex.unlock();
            exit(1);
          }
          exit(0);
        }
        T file_mutex{file_path};
        std::unique_lock lock{file_mutex};
        int status = 0;
        if (waitpid(child_pid, &status, 0) == -1) {
          throw std::system_error{
            std::error_code{errno, std::generic_category()}
          };
        }
        if ((WIFEXITED(status) && WEXITSTATUS(status) == 1) || !WIFEXITED(status))
          throw std::bad_exception{};
      }
    )
    .add_test(
      "allow_concurrent_multi_process_read",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        int child_pid = fork();
        if (child_pid == -1)
          throw std::system_error{
            std::error_code{errno, std::generic_category()}
          };
        else if (child_pid == 0) {
          T file_mutex{file_path};
          std::this_thread::sleep_for(std::chrono::seconds{3});
          if (file_mutex.try_lock_shared()) {
            file_mutex.unlock_shared();
            exit(0);
          }
          exit(1);
        }
        T file_mutex{file_path};
        std::shared_lock lock{file_mutex};
        int status = 0;
        if (waitpid(child_pid, &status, 0) == -1) {
          throw std::system_error{
            std::error_code{errno, std::generic_category()}
          };
        }
        if ((WIFEXITED(status) && WEXITSTATUS(status) == 1) || !WIFEXITED(status))
          throw std::bad_exception{};
      }
    )
    .add_test("no_concurrent_multi_process_write_and_read", [&]() {
      std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
      int child_pid = fork();
      if (child_pid == -1)
        throw std::system_error{std::error_code{errno, std::generic_category()}
        };
      else if (child_pid == 0) {
        T file_mutex{file_path};
        std::this_thread::sleep_for(std::chrono::seconds{3});
        if (file_mutex.try_lock_shared()) {
          file_mutex.unlock_shared();
          exit(1);
        }
        exit(0);
      }
      T file_mutex{file_path};
      std::unique_lock lock{file_mutex};
      int status = 0;
      if (waitpid(child_pid, &status, 0) == -1) {
        throw std::system_error{std::error_code{errno, std::generic_category()}
        };
      }
      if ((WIFEXITED(status) && WEXITSTATUS(status) == 1) || !WIFEXITED(status))
        throw std::bad_exception{};
    });
}

template <file_lock::SharedMutex T>
  requires(std::constructible_from<T, std::filesystem::path>)
auto fuzzer_tester(
  const std::string &test_suite, const std::filesystem::path &tmp_fd
) {
  return test_lib::Tester<>{test_suite}.add_test("fuzz_test", [&]() {
    std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
    std::string random_value = test_lib::random_string(1500);
    std::fstream file{file_path, std::ios_base::out | std::ios_base::trunc};
    file << random_value;
    file.flush();
    file.close();
    // Assertion that the file is indeed written
    file.open(file_path, std::ios_base::in);
    std::stringstream buffer;
    buffer << file.rdbuf();
    test_lib::assert_equal(buffer.str(), random_value);
    file.close();
    // Start the fuzzing;
    T file_mutex{file_path};
    uint32_t fork_count = 0;
    uint32_t thread_count = 0;
    for (uint32_t i = 0; i < 1000; i += 1){
      if (test_lib::random_integer(0, 1) == 0) thread_count += 1;
      else fork_count += 1;
    }
    std::vector<pid_t> pid_list;
    pid_list.reserve(fork_count);
    for (uint32_t i = 0; i < fork_count; i += 1) {
      pid_t child_pid = fork();
      if (child_pid == -1)
        throw std::system_error{
          std::error_code{errno, std::generic_category()}
        };
      else if (child_pid == 0) {
        uint32_t todo = test_lib::random_integer(0, 1);
        T file_mutex{file_path};
        std::this_thread::sleep_for(
          std::chrono::milliseconds{test_lib::random_integer(0, 500)}
        );
        if (todo == 0) {
          std::shared_lock lock{file_mutex};
          std::ifstream f{file_path};
          std::stringstream buffer;
          buffer << f.rdbuf();
          if (buffer.str() != random_value) {
            std::cout<<random_value<<" is not equal to "<<buffer.str()<<std::endl;
            exit(1);
          }
        } else {
          std::unique_lock lock{file_mutex};
          std::ofstream f{
            file_path, std::ios_base::trunc | std::ios_base::out
          };
          f << random_value;
          f.flush();
          f.close();
        }
        exit(0);
      }
      else {
        pid_list.push_back(child_pid);
      }
    }
    std::vector<std::thread> thread_list;
    thread_list.reserve(thread_count);
    for (uint32_t i = 0; i < thread_count; i += 1) {
      thread_list.emplace_back(
        std::thread{[&]() mutable {
          uint32_t todo = test_lib::random_integer(0, 1);
          std::this_thread::sleep_for(
            std::chrono::milliseconds{test_lib::random_integer(0, 10)}
          );
          if (todo == 0) {
            std::shared_lock lock{file_mutex};
            std::ifstream f{file_path};
            std::stringstream buffer;
            buffer << f.rdbuf();
            test_lib::assert_equal(buffer.str(), random_value);
            f.close();
          } else {
            std::unique_lock lock{file_mutex};
            std::ofstream f{
              file_path, std::ios_base::trunc | std::ios_base::out
            };
            f << random_value;
            f.flush();
            f.close();
          }
        }}
      );
    }
    for (auto &thread : thread_list)
      thread.join();
    for (auto pid : pid_list) {
      int status = 0;
      if (waitpid(pid, &status, 0) == -1)
        throw std::system_error{std::error_code{errno, std::generic_category()}
        };
      if ((WIFEXITED(status) && WEXITSTATUS(status) == 1) || !WIFEXITED(status)){
        throw std::bad_exception{};
      }
    }
  });
}

int main() {
  const std::filesystem::path tmp_fd{"tmp"};
  DirGuard dir_guard{tmp_fd};
  auto basic_mutex_tester =
    mutex_tester<file_lock::BasicMutex>("basic::BasicMutex", tmp_fd);
  auto large_mutex_tester =
    mutex_tester<file_lock::LargeFileMutex>("basic::LargeFileMutex", tmp_fd);
  auto basic_fuzzer_tester =
    fuzzer_tester<file_lock::BasicMutex>("fuzzer::BasicMutex", tmp_fd);
  auto large_fuzzer_tester =
    fuzzer_tester<file_lock::LargeFileMutex>("fuzzer::LargeFileMutex", tmp_fd);

  test_lib::print_result(basic_mutex_tester, basic_mutex_tester.run_all());
  test_lib::print_result(large_mutex_tester, large_mutex_tester.run_all());
  test_lib::print_result(basic_fuzzer_tester, basic_fuzzer_tester.run_all());
  test_lib::print_result(large_fuzzer_tester, large_fuzzer_tester.run_all());
}