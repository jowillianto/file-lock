#include <sys/wait.h>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <unistd.h>
import moderna.test_lib;
import moderna.file_lock;
import moderna.thread_plus;
import moderna.process;
using namespace moderna;

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
  template <typename... Args>
    requires std::is_constructible_v<std::thread, Args...>
  SafeThread(Args &&...args) : _thread{std::forward<Args>(args)...} {}
  void detach() {
    _thread.detach();
  }
  ~SafeThread() {
    if (_thread.joinable()) _thread.join();
  }
};

template <typename T>
auto mutex_tester(const std::string &test_suite, const std::filesystem::path &tmp_fd) {
  return test_lib::make_tester(test_suite, false)
    .add_test(
      "create_no_truncate",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        std::fstream file{file_path, std::ios_base::out | std::ios_base::trunc};
        std::string random_value = test_lib::random_string(20);
        file << random_value;
        file.close();
        auto file_mutex = T::create(file_path).value();
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
        auto file_mutex = T::create(file_path).value();
        auto thread_sender = thread_plus::void_channel{};
        auto cur_sender = thread_plus::void_channel{};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          auto file_mutex = T::create(file_path).value();
          file_mutex.lock();
          thread_sender.send();
          auto _ = cur_sender.recv();
          file_mutex.unlock();
        }}};
        auto _ = thread_sender.recv();
        if (file_mutex.try_lock()) {
          file_mutex.unlock();
          cur_sender.send();
          throw std::bad_exception{};
        }
        cur_sender.send();
      }
    )
    .add_test(
      "no_concurrent_writes_same_object",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sender = thread_plus::void_channel{};
        auto cur_sender = thread_plus::void_channel{};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          file_mutex.lock();
          thread_sender.send();
          auto _ = cur_sender.recv();
          file_mutex.unlock();
        }}};
        auto _ = thread_sender.recv();
        if (file_mutex.try_lock()) {
          file_mutex.unlock();
          cur_sender.send();
          throw std::bad_exception{};
        }
        cur_sender.send();
      }
    )
    .add_test(
      "allow_concurrent_reads",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        SafeThread thread{std::thread{[&]() mutable {
          auto file_mutex = T::create(file_path).value();
          file_mutex.try_lock_shared();
          // std::this_thread::sleep_for(std::chrono::milliseconds{300});
          thread_sig.send();
          auto _ = cur_sig.recv();
          file_mutex.unlock_shared();
        }}};
        // std::this_thread::sleep_for(std::chrono::milliseconds{100});
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock_shared()) {
          cur_sig.send();
          file_mutex.unlock_shared();
          return;
        }
        cur_sig.send();
        throw std::bad_exception{};
      }
    )
    .add_test(
      "allow_concurrent_reads_same_object",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        SafeThread thread{std::thread{[&]() mutable {
          file_mutex.try_lock_shared();
          // std::this_thread::sleep_for(std::chrono::milliseconds{300});
          thread_sig.send();
          auto _ = cur_sig.recv();
          file_mutex.unlock_shared();
        }}};
        // std::this_thread::sleep_for(std::chrono::milliseconds{100});
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock_shared()) {
          cur_sig.send();
          file_mutex.unlock_shared();
          return;
        }
        cur_sig.send();
        throw std::bad_exception{};
      }
    )
    .add_test(
      "no_concurrent_write_and_read",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        SafeThread thread{std::thread{[&]() mutable {
          auto file_mutex = T::create(file_path).value();
          file_mutex.lock_shared();
          thread_sig.send();
          auto _ = cur_sig.recv();
          // std::this_thread::sleep_for(std::chrono::milliseconds{300});
          file_mutex.unlock_shared();
        }}};
        // std::this_thread::sleep_for(std::chrono::milliseconds{100});
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock()) {
          cur_sig.send();
          file_mutex.unlock();
          throw std::bad_exception{};
        }
        cur_sig.send();
      }
    )
    /*
      This test ensures that the following works :
      - T2 obtains a read lock on the file
      - T1 cannot obtain a write lock on the file.
    */
    .add_test(
      "no_concurrent_write_and_read_same_object",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        SafeThread thread{std::thread{[&]() mutable {
          auto file_mutex = T::create(file_path).value();
          file_mutex.lock_shared();
          thread_sig.send();
          auto _ = cur_sig.recv();
          file_mutex.unlock_shared();
        }}};
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock()) {
          cur_sig.send();
          file_mutex.unlock();
          throw std::bad_exception{};
        }
        cur_sig.send();
      }
    )
    .add_test(
      "with std::shared_lock",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        SafeThread thread{std::thread{[&]() mutable {
          auto file_mutex = T::create(file_path).value();
          std::shared_lock lock{file_mutex};
          thread_sig.send();
          auto _ = cur_sig.recv();
        }}};
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock()) {
          cur_sig.send();
          file_mutex.unlock();
          throw std::bad_exception{};
        }
        cur_sig.send();
      }
    )
    .add_test(
      "with std::shared_lock same object",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        SafeThread thread{std::thread{[&]() mutable {
          std::shared_lock lock{file_mutex};
          thread_sig.send();
          auto _ = cur_sig.recv();
        }}};
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock()) {
          cur_sig.send();
          file_mutex.unlock();
          throw std::bad_exception{};
        }
        cur_sig.send();
      }
    )
    .add_test(
      "with std::unique_lock",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          auto file_mutex = T::create(file_path).value();
          std::unique_lock lock{file_mutex};
          thread_sig.send();
          auto _ = cur_sig.recv();
        }}};
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock()) {
          cur_sig.send();
          file_mutex.unlock();
          throw std::bad_exception{};
        }
        cur_sig.send();
      }
    )
    .add_test(
      "with std::unique_lock same object",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto thread_sig = thread_plus::void_channel{};
        auto cur_sig = thread_plus::void_channel{};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          std::unique_lock lock{file_mutex};
          thread_sig.send();
          auto _ = cur_sig.recv();
        }}};
        auto _ = thread_sig.recv();
        if (file_mutex.try_lock()) {
          cur_sig.send();
          file_mutex.unlock();
          throw std::bad_exception{};
        }
        cur_sig.send();
      }
    )
    .add_test(
      "no_conccurent_multi_process_write",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        std::unique_lock lock{file_mutex};
        auto completed_process = subprocess::run(process::static_argument{
          TEST_CHILD,
          file_path.string(),
          []() {
            if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
            else
              return "lf_mut";
          }(),
          "test_not_unique_lockable"
        });
        test_lib::assert_equal(completed_process.value().exit_code(), 0);
      }
    )
    .add_test(
      "allow_concurrent_multi_process_read",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        std::shared_lock lock{file_mutex};
        auto completed_process = subprocess::run(process::static_argument{
          TEST_CHILD,
          file_path.string(),
          []() {
            if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
            else
              return "lf_mut";
          }(),
          "test_shared_lockable"
        });
        test_lib::assert_equal(completed_process.value().exit_code(), 0);
      }
    )
    .add_test(
      "no_concurrent_multi_process_write_and_read",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        std::unique_lock lock{file_mutex};
        auto completed_process_unique = subprocess::run(process::static_argument{
          TEST_CHILD,
          file_path.string(),
          []() {
            if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
            else
              return "lf_mut";
          }(),
          "test_not_unique_lockable"
        });
        auto completed_process_shared = subprocess::run(process::static_argument{
          TEST_CHILD,
          file_path.string(),
          []() {
            if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
            else
              return "lf_mut";
          }(),
          "test_not_shared_lockable"
        });
        test_lib::assert_equal(completed_process_unique.value().exit_code(), 0);
        test_lib::assert_equal(completed_process_shared.value().exit_code(), 0);
      }
    )
    .add_test(
      "unlock_reader_twice",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        auto file_mutex = T::create(file_path).value();
        auto unlock_sig = thread_plus::void_channel{};
        auto unlock_done_sig = thread_plus::void_channel{};
        auto exit_sig = thread_plus::void_channel{};
        SafeThread t1{[&]() {
          {
            std::shared_lock l1{file_mutex};
            auto _ = unlock_sig.recv();
          }
          unlock_done_sig.send(1);
          auto __ = exit_sig.recv();
        }};
        SafeThread t2{[&]() {
          std::shared_lock l1{file_mutex};
          unlock_sig.send(1);
          auto _ = exit_sig.recv();
        }};
        auto _ = unlock_done_sig.recv();
        auto completed_process = subprocess::run(process::static_argument{
          TEST_CHILD,
          file_path.string(),
          []() {
            if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
            else
              return "lf_mut";
          }(),
          "test_not_unique_lockable"
        });
        exit_sig.send(2);
        test_lib::assert_equal(completed_process.value().exit_code(), 0);
      }
    )
    .add_test("unlock_flock_twice_in_a_thread", [&]() {
      std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
      auto file_mutex = T::create(file_path).value();
      auto file_mutex2 = T::create(file_path).value();
      std::shared_lock lock{file_mutex};
      std::shared_lock lock2{file_mutex2};
      lock.unlock();
      lock.release();
      auto completed_process = subprocess::run(process::static_argument{
        TEST_CHILD,
        file_path.string(),
        []() {
          if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
          else
            return "lf_mut";
        }(),
        "test_not_unique_lockable"
      });
      test_lib::assert_equal(completed_process.value().exit_code(), 0);
    });
}

template <typename T>
auto fuzzer_tester(const std::string &test_suite, const std::filesystem::path &tmp_fd) {
  return test_lib::make_tester(test_suite).add_test("fuzz_test", [&]() {
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
    auto file_mutex = T::create(file_path).value();
    uint32_t fork_count = 0;
    uint32_t thread_count = 0;
    for (uint32_t i = 0; i < 1000; i += 1) {
      if (test_lib::random_integer(0, 1) == 0) thread_count += 1;
      else
        fork_count += 1;
    }
    std::vector<subprocess> process_list;
    process_list.reserve(fork_count);
    for (uint32_t i = 0; i < fork_count; i += 1) {
      process_list.emplace_back(subprocess::spawn(process::static_argument{
                                                    TEST_CHILD,
                                                    file_path.string(),
                                                    []() {
                                                      if (std::same_as<T, file_lock::file_mutex>)
                                                        return "f_mut";
                                                      else
                                                        return "lf_mut";
                                                    }(),
                                                    "fuzz_test",
                                                    random_value
                                                  })
                                  .value());
    }
    std::vector<std::thread> thread_list;
    thread_list.reserve(thread_count);
    for (uint32_t i = 0; i < thread_count; i += 1) {
      thread_list.emplace_back(std::thread{[&]() mutable {
        auto file_mutex = T::create(file_path).value();
        uint32_t todo = test_lib::random_integer(0, 1);
        std::this_thread::sleep_for(std::chrono::microseconds{test_lib::random_integer(0, 10)});
        if (todo == 0) {
          std::shared_lock lock{file_mutex};
          std::ifstream f{file_path};
          std::stringstream buffer;
          buffer << f.rdbuf();
          test_lib::assert_equal(buffer.str(), random_value);
          f.close();
        } else {
          std::unique_lock lock{file_mutex};
          std::ofstream f{file_path, std::ios_base::trunc | std::ios_base::out};
          f << random_value;
          f.flush();
          f.close();
        }
      }});
    }
    for (auto &thread : thread_list)
      thread.join();
    for (auto &process : process_list) {
      process.wait().transform_error([](auto &&e) -> bool { throw e; }).value();
    }
  });
}

template <typename T>
auto undefined_behaviour_tester(
  const std::string &test_suite, const std::filesystem::path &tmp_fd
) {
  return test_lib::make_tester(test_suite)
    .add_test(
      "unlock_without_lock",
      [&]() {
        auto fpath = tmp_fd / test_lib::random_string(10);
        auto lock = T::create(fpath).value();
        lock.unlock();
      }
    )
    .add_test(
      "unlock_shared_without_lock",
      [&]() {
        auto fpath = tmp_fd / test_lib::random_string(10);
        T::create(fpath).value().unlock_shared();
      }
    )
    .add_test("duplicate", [&]() {
      auto fpath = tmp_fd / test_lib::random_string(10);
      auto lock = T::create(fpath).value();
      auto lock_dup = T::create(fpath).value();
    });
}

int main(int argc, char **argv, const char **envp) {
  process::env::init_global(envp);
  const std::filesystem::path tmp_fd{"tmp"};
  DirGuard dir_guard{tmp_fd};
  // mutex_tester<file_lock::FcntlMutex>("basic::FcntlMutex", tmp_fd).print_or_exit();
  mutex_tester<file_lock::file_mutex>("basic::file_mutex", tmp_fd).print_or_exit();
  mutex_tester<file_lock::lf_mutex>("basic::lf_mutex", tmp_fd).print_or_exit();
  fuzzer_tester<file_lock::file_mutex>("fuzzer::file_mutex", tmp_fd).print_or_exit();
  fuzzer_tester<file_lock::lf_mutex>("fuzzer::lf_mutex", tmp_fd).print_or_exit();
  undefined_behaviour_tester<file_lock::file_mutex>("undefined::file_mutex", tmp_fd)
    .print_or_exit();
  undefined_behaviour_tester<file_lock::lf_mutex>("undefined::lf_mutex", tmp_fd).print_or_exit();
  // mutex_store_tester.print_or_exit();
}