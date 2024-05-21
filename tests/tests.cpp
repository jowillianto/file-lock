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
  void detach() {
    _thread.detach();
  }
  ~SafeThread() {
    if (_thread.joinable()) _thread.join();
  }
};

template <file_lock::SharedMutex T>
  requires(std::constructible_from<T, std::filesystem::path>)
auto mutex_tester(const std::string &test_suite, const std::filesystem::path &tmp_fd) {
  return test_lib::Tester{test_suite}
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
        auto thread_sender = thread_plus::channel::Channel<void>{};
        auto cur_sender = thread_plus::channel::Channel<void>{};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          T file_mutex{file_path};
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
        T file_mutex{file_path};
        auto thread_sig = thread_plus::channel::Channel<void>{};
        auto cur_sig = thread_plus::channel::Channel<void>{};
        SafeThread thread{std::thread{[&]() mutable {
          T file_mutex{file_path};
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
        T file_mutex{file_path};
        auto thread_sig = thread_plus::channel::Channel<void>{};
        auto cur_sig = thread_plus::channel::Channel<void>{};
        SafeThread thread{std::thread{[&]() mutable {
          T file_mutex{file_path};
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
    .add_test(
      "std::shared_lock_consistency",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        auto thread_sig = thread_plus::channel::Channel<void>{};
        auto cur_sig = thread_plus::channel::Channel<void>{};
        SafeThread thread{std::thread{[&]() mutable {
          T file_mutex{file_path};
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
      "std::unique_lock_consistency",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        auto thread_sig = thread_plus::channel::Channel<void>{};
        auto cur_sig = thread_plus::channel::Channel<void>{};
        // Lock File in another thread
        SafeThread thread{std::thread{[&]() mutable {
          T file_mutex{file_path};
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
        T file_mutex{file_path};
        std::unique_lock lock{file_mutex};
        process::Process process{process::StaticArgument{
          TEST_CHILD,
          file_path.string(),
          []() {
            if (std::same_as<T, file_lock::FileMutex>) return "f_mut";
            else
              return "lf_mut";
          }(),
          "test_not_unique_lockable"
        }};
        auto completed_process = process.wait();
        test_lib::assert_equal(completed_process.value().exit_code(), 0);
      }
    )
    .add_test(
      "allow_concurrent_multi_process_read",
      [&]() {
        std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
        T file_mutex{file_path};
        std::shared_lock lock{file_mutex};
        process::Process process{process::StaticArgument{
          TEST_CHILD,
          file_path.string(),
          []() {
            if (std::same_as<T, file_lock::FileMutex>) return "f_mut";
            else
              return "lf_mut";
          }(),
          "test_shared_lockable"
        }};
        auto completed_process = process.wait();
        test_lib::assert_equal(completed_process.value().exit_code(), 0);
      }
    )
    .add_test("no_concurrent_multi_process_write_and_read", [&]() {
      std::filesystem::path file_path = tmp_fd / test_lib::random_string(10);
      T file_mutex{file_path};
      std::unique_lock lock{file_mutex};
      process::Process process_unique{process::StaticArgument{
        TEST_CHILD,
        file_path.string(),
        []() {
          if (std::same_as<T, file_lock::FileMutex>) return "f_mut";
          else
            return "lf_mut";
        }(),
        "test_not_unique_lockable"
      }};
      process::Process process_shared{process::StaticArgument{
        TEST_CHILD,
        file_path.string(),
        []() {
          if (std::same_as<T, file_lock::FileMutex>) return "f_mut";
          else
            return "lf_mut";
        }(),
        "test_not_shared_lockable"
      }};
      auto completed_process_unique = process_unique.wait();
      auto completed_process_shared = process_shared.wait();
      test_lib::assert_equal(completed_process_unique.value().exit_code(), 0);
      test_lib::assert_equal(completed_process_shared.value().exit_code(), 0);
    });
}

template <file_lock::SharedMutex T>
  requires(std::constructible_from<T, std::filesystem::path>)
auto fuzzer_tester(const std::string &test_suite, const std::filesystem::path &tmp_fd) {
  return test_lib::Tester{test_suite}.add_test("fuzz_test", [&]() {
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
    for (uint32_t i = 0; i < 1000; i += 1) {
      if (test_lib::random_integer(0, 1) == 0) thread_count += 1;
      else
        fork_count += 1;
    }
    std::vector<pid_t> pid_list;
    pid_list.reserve(fork_count);
    for (uint32_t i = 0; i < fork_count; i += 1) {
      pid_t child_pid = fork();
      if (child_pid == -1) throw std::system_error{std::error_code{errno, std::generic_category()}};
      else if (child_pid == 0) {
        uint32_t todo = test_lib::random_integer(0, 1);
        T file_mutex{file_path};
        std::this_thread::sleep_for(std::chrono::microseconds{test_lib::random_integer(0, 500)});
        if (todo == 0) {
          std::shared_lock lock{file_mutex};
          std::ifstream f{file_path};
          std::stringstream buffer;
          buffer << f.rdbuf();
          if (buffer.str() != random_value) {
            std::cout << random_value << " is not equal to " << buffer.str() << std::endl;
            exit(1);
          }
        } else {
          std::unique_lock lock{file_mutex};
          std::ofstream f{file_path, std::ios_base::trunc | std::ios_base::out};
          f << random_value;
          f.flush();
          f.close();
        }
        exit(0);
      } else {
        pid_list.push_back(child_pid);
      }
    }
    std::vector<std::thread> thread_list;
    thread_list.reserve(thread_count);
    for (uint32_t i = 0; i < thread_count; i += 1) {
      thread_list.emplace_back(std::thread{[&]() mutable {
        T file_mutex{file_path};
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
    for (auto pid : pid_list) {
      int status = 0;
      if (waitpid(pid, &status, 0) == -1)
        throw std::system_error{std::error_code{errno, std::generic_category()}};
      if ((WIFEXITED(status) && WEXITSTATUS(status) == 1) || !WIFEXITED(status)) {
        throw std::bad_exception{};
      }
    }
  });
}

template <file_lock::SharedMutex T>
  requires(std::constructible_from<T, std::filesystem::path>)
auto undefined_behaviour_tester(
  const std::string &test_suite, const std::filesystem::path &tmp_fd
) {
  return test_lib::Tester{test_suite}
    .add_test(
      "unlock_without_lock",
      [&]() {
        auto fpath = tmp_fd / test_lib::random_string(10);
        T lock{fpath};
        lock.unlock();
      }
    )
    .add_test(
      "unlock_shared_without_lock",
      [&]() {
        auto fpath = tmp_fd / test_lib::random_string(10);
        T lock{fpath};
        lock.unlock_shared();
      }
    )
    .add_test("duplicate", [&]() {
      auto fpath = tmp_fd / test_lib::random_string(10);
      T lock{fpath};
      T lock_dup{fpath};
    });
}

auto mutex_store_tester =
  test_lib::Tester{"mutex_store_tester"}
    .add_test(
      "test_rw",
      []() {
        auto store = file_lock::MutexStore{10};
        auto fpath = std::filesystem::path{test_lib::random_string(10)};
        auto mutex = store.get_mutex(fpath);
        auto mutex_2 = store.get_mutex(fpath);
        test_lib::assert_equal(
          static_cast<void *>(mutex.get()), static_cast<void *>(mutex_2.get())
        );
      }
    )
    .add_test(
      "test_fuzz",
      []() {
        auto store = file_lock::MutexStore{10};
        auto thread_count = test_lib::random_integer(20, 30);
        auto path_count = test_lib::random_integer(20, 100);
        auto per_thread_task = test_lib::random_integer(10, 20);
        std::vector<std::thread> workers;
        workers.reserve(thread_count);
        std::vector<std::filesystem::path> paths;
        paths.reserve(path_count);
        for (size_t i = 0; i < path_count; i += 1)
          paths.emplace_back(std::filesystem::path{test_lib::random_string(10)});
        for (size_t i = 0; i < thread_count; i += 1)
          workers.emplace_back(std::thread{[&]() {
            // This ensure that the reference for mutexes are not gone. This will test the garbage
            // collection.
            std::vector<std::shared_ptr<std::shared_mutex>> mutexes;
            mutexes.reserve(per_thread_task);
            for (size_t i = 0; i < per_thread_task; i += 1) {
              std::this_thread::sleep_for(
                std::chrono::microseconds(test_lib::random_integer(50, 100))
              );
              int path_id = test_lib::random_integer(0, path_count - 1);
              std::filesystem::path &fpath = paths.at(path_id);
              // Randomly choose between saving or throwing away the mutex
              auto mutex = store.get_mutex(fpath);
              if (test_lib::random_real(0.0, 1.0) < 0.5) {
                mutexes.emplace_back(std::move(mutex));
              }
            }
          }});
        for (auto &worker : workers)
          worker.join();
      }
    )
    .add_test("test_gc", []() {
      auto store = file_lock::MutexStore{10};
      for (size_t i = 0; i < 11; i += 1)
        store.get_mutex(std::filesystem::path{test_lib::random_string(10)});
      // There will be one reference left.
      /*
        store size reaches 10.
        write_new()
        create_shared_for_current_path
        garbage_collect prunes all old paths.
        size is 1.
      */
      test_lib::assert_equal(store.size(), 1);
    });

int main(int argc, char **argv, const char **envp) {
  process::Env::init_global(envp);
  const std::filesystem::path tmp_fd{"tmp"};
  DirGuard dir_guard{tmp_fd};
  mutex_tester<file_lock::FileMutex>("basic::BasicMutex", tmp_fd).print_or_exit();
  mutex_tester<file_lock::LargeFileMutex>("basic::LargeFileMutex", tmp_fd).print_or_exit();
  fuzzer_tester<file_lock::FileMutex>("fuzzer::BasicMutex", tmp_fd).print_or_exit();
  fuzzer_tester<file_lock::LargeFileMutex>("fuzzer::LargeFileMutex", tmp_fd).print_or_exit();
  undefined_behaviour_tester<file_lock::FileMutex>("undefined::BasicMutex", tmp_fd).print_or_exit();
  undefined_behaviour_tester<file_lock::LargeFileMutex>("undefined::LargeFileMutex", tmp_fd)
    .print_or_exit();
  mutex_store_tester.print_or_exit();
}