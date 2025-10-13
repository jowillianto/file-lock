#include <jowi/test_lib.hpp>
#include <sys/wait.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unistd.h>
import jowi.test_lib;
import jowi.file_lock;
import jowi.process;

namespace test_lib = jowi::test_lib;
namespace file_lock = jowi::file_lock;
namespace fs = std::filesystem;
namespace proc = jowi::process;

struct dir_guard {
private:
  fs::path __path;

public:
  dir_guard(fs::path dir) : __path{std::move(dir)} {
    fs::create_directories(__path);
  }
  ~dir_guard() {
    fs::remove_all(__path);
  }
};

struct safe_thread {
private:
  std::thread __t;

public:
  template <class... Args>
    requires(std::constructible_from<std::thread, Args...>)
  safe_thread(Args &&...args) : __t{std::forward<Args>(args)...} {}
  void detach() {
    __t.detach();
  }
  ~safe_thread() {
    if (__t.joinable()) {
      __t.join();
    }
  }
};

static auto tmp_fd = fs::path{"tmp"};
static auto g = dir_guard{tmp_fd};

template <class T> void create_no_truncate() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  std::fstream file{file_path, std::ios_base::out | std::ios_base::trunc};
  std::string random_value = test_lib::random_string(20);
  file << random_value;
  file.close();
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::ifstream read_file{file_path};
  std::stringstream buffer;
  buffer << read_file.rdbuf();
  test_lib::assert_equal(buffer.str(), random_value);
  read_file.close();
}

template <class T> void no_concurrent_writes() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sender{false};
  std::atomic_flag cur_sender{false};
  // Lock File in another thread
  safe_thread thread{[&]() mutable {
    auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
    file_mutex.lock();
    thread_sender.test_and_set(std::memory_order_acq_rel);
    while (!cur_sender.test(std::memory_order_relaxed)) {
    }
    file_mutex.unlock();
  }};

  while (!thread_sender.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    file_mutex.unlock();
    cur_sender.test_and_set(std::memory_order_acq_rel);
    test_lib::assert_true(false);
  }
  cur_sender.test_and_set(std::memory_order_acq_rel);
}

template <class T> void no_concurrent_write_to_same_object() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sender{false};
  std::atomic_flag cur_sender{false};
  // Lock File in another thread
  safe_thread thread{[&]() mutable {
    file_mutex.lock();
    thread_sender.test_and_set(std::memory_order_acq_rel);
    while (!cur_sender.test(std::memory_order_relaxed)) {
    }
    file_mutex.unlock();
  }};
  while (!thread_sender.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    file_mutex.unlock();
    cur_sender.test_and_set(std::memory_order_acq_rel);
    test_lib::assert_true(false);
  }
  cur_sender.test_and_set(std::memory_order_acq_rel);
}

template <class T> void allow_concurrent_reads() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{[&]() mutable {
    auto file_mutex = T::create(file_path).value();
    file_mutex.lock_shared();
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
    file_mutex.unlock_shared();
  }};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock_shared()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock_shared();
    return;
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
  test_lib::assert_true(false);
}

template <class T> void allow_concurrent_read_same_object() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{[&]() mutable {
    file_mutex.lock_shared();
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
    file_mutex.unlock_shared();
  }};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock_shared()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock_shared();
    return;
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
  test_lib::assert_true(false);
}

template <class T> void no_concurrent_read_writes() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{std::thread{[&]() mutable {
    auto file_mutex = T::create(file_path).value();
    file_mutex.lock_shared();
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
    file_mutex.unlock_shared();
  }}};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock();
    test_lib::assert_true(false);
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
}

template <class T> void no_concurrent_read_writes_same_object() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{std::thread{[&]() mutable {
    file_mutex.lock_shared();
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
    file_mutex.unlock_shared();
  }}};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock();
    test_lib::assert_true(false);
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
}

template <class T> void with_std_shared_lock() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{std::thread{[&]() mutable {
    auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
    std::shared_lock l{file_mutex};
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
  }}};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock();
    test_lib::assert_true(false);
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
}

template <class T> void with_std_shared_lock_same_object() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{std::thread{[&]() mutable {
    std::shared_lock l{file_mutex};
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
  }}};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock();
    test_lib::assert_true(false);
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
}

template <class T> void with_std_unique_lock() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{std::thread{[&]() mutable {
    auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
    std::unique_lock l{file_mutex};
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
  }}};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock();
    test_lib::assert_true(false);
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
}

template <class T> void with_std_unique_lock_same_object() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag thread_sig{false};
  std::atomic_flag cur_sig{false};
  safe_thread thread{std::thread{[&]() mutable {
    std::unique_lock l{file_mutex};
    // std::this_thread::sleep_for(std::chrono::milliseconds{300});
    thread_sig.test_and_set(std::memory_order_acq_rel);
    while (!cur_sig.test(std::memory_order_relaxed)) {
    }
  }}};
  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  while (!thread_sig.test(std::memory_order_relaxed)) {
  }
  if (file_mutex.try_lock()) {
    cur_sig.test_and_set(std::memory_order_acq_rel);
    file_mutex.unlock();
    test_lib::assert_true(false);
  }
  cur_sig.test_and_set(std::memory_order_acq_rel);
}

template <class T> void no_concurrent_multi_process_write() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::unique_lock lock{file_mutex};
  auto completed_process = test_lib::assert_expected_value(
    proc::run(
      {TEST_CHILD,
       file_path.string(),
       []() {
         if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
         else
           return "lf_mut";
       }(),
       "test_not_unique_lockable"}
    )
  );
  test_lib::assert_equal(completed_process.exit_code(), 0);
}

template <class T> void allow_concurrent_multiprocess_read() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::shared_lock lock{file_mutex};
  auto completed_process = test_lib::assert_expected_value(
    proc::run(
      {TEST_CHILD,
       file_path.string(),
       []() {
         if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
         else
           return "lf_mut";
       }(),
       "test_shared_lockable"}
    )
  );
  test_lib::assert_equal(completed_process.exit_code(), 0);
}

template <class T> void no_concurent_multi_process_write_and_read() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::unique_lock lock{file_mutex};
  auto completed_process_unique = test_lib::assert_expected_value(
    proc::run(
      {TEST_CHILD,
       file_path.string(),
       []() {
         if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
         else
           return "lf_mut";
       }(),
       "test_not_unique_lockable"}
    )
  );
  auto completed_process_shared = test_lib::assert_expected_value(
    proc::run(
      {TEST_CHILD,
       file_path.string(),
       []() {
         if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
         else
           return "lf_mut";
       }(),
       "test_not_shared_lockable"}
    )
  );
  test_lib::assert_equal(completed_process_unique.exit_code(), 0);
  test_lib::assert_equal(completed_process_shared.exit_code(), 0);
}

template <class T> void unlock_reader_twice() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  std::atomic_flag unlock_sig{false};
  std::atomic_flag unlock_done_sig{false};
  std::atomic_flag exit_sig{false};
  safe_thread t1{[&]() {
    {
      std::shared_lock l1{file_mutex};
      while (!unlock_sig.test(std::memory_order_relaxed)) {
      }
    }
    unlock_done_sig.test_and_set(std::memory_order_acq_rel);
    while (!exit_sig.test(std::memory_order_relaxed)) {
    }
  }};
  safe_thread t2{[&]() {
    std::shared_lock l1{file_mutex};
    unlock_sig.test_and_set(std::memory_order_acq_rel);
    while (!exit_sig.test(std::memory_order_relaxed)) {
    }
  }};
  while (!unlock_done_sig.test(std::memory_order_relaxed)) {
  }
  auto completed_process = test_lib::assert_expected_value(
    proc::run(
      {TEST_CHILD,
       file_path.string(),
       []() {
         if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
         else
           return "lf_mut";
       }(),
       "test_not_unique_lockable"}
    )
  );
  exit_sig.test_and_set(std::memory_order_acq_rel);
  test_lib::assert_equal(completed_process.exit_code(), 0);
}

template <class T> void unlock_flock_twice_in_thread() {
  fs::path file_path = tmp_fd / test_lib::random_string(10);
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  auto file_mutex2 = test_lib::assert_expected_value(T::create(file_path));
  std::shared_lock lock{file_mutex};
  std::shared_lock lock2{file_mutex2};
  lock.unlock();
  lock.release();
  auto completed_process = test_lib::assert_expected_value(
    proc::run(
      {TEST_CHILD,
       file_path.string(),
       []() {
         if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
         else
           return "lf_mut";
       }(),
       "test_not_unique_lockable"}
    )
  );
  test_lib::assert_equal(completed_process.exit_code(), 0);
}

template <class T> void fuzz_tester() {
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
  auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
  uint32_t fork_count = 0;
  uint32_t thread_count = 0;
  for (uint32_t i = 0; i < 1000; i += 1) {
    if (test_lib::random_integer(0, 1) == 0) thread_count += 1;
    else
      fork_count += 1;
  }
  std::vector<proc::subprocess> process_list;
  process_list.reserve(fork_count);
  for (uint32_t i = 0; i < fork_count; i += 1) {
    process_list.emplace_back(
      test_lib::assert_expected_value(
        proc::spawn(
          {TEST_CHILD,
           file_path.string(),
           []() {
             if (std::same_as<T, file_lock::file_mutex>) return "f_mut";
             else
               return "lf_mut";
           }(),
           "fuzz_test",
           random_value}
        )
      )
    );
  }
  std::vector<std::thread> thread_list;
  thread_list.reserve(thread_count);
  for (uint32_t i = 0; i < thread_count; i += 1) {
    thread_list.emplace_back([&]() mutable {
      auto file_mutex = test_lib::assert_expected_value(T::create(file_path));
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
    });
  }
  for (auto &thread : thread_list)
    thread.join();
  for (auto &process : process_list) {
    process.wait().transform_error([](auto &&e) -> bool { throw e; }).value();
  }
}

template <class T> void unlock_without_lock() {
  auto fpath = tmp_fd / test_lib::random_string(10);
  auto lock = test_lib::assert_expected_value(T::create(fpath));
  lock.unlock();
}

template <class T> void unlock_shared_without_lock() {
  auto fpath = tmp_fd / test_lib::random_string(10);
  auto lock = test_lib::assert_expected_value(T::create(fpath));
  lock.unlock_shared();
}

template <class T> void duplicate() {
  auto fpath = tmp_fd / test_lib::random_string(10);
  auto lock = test_lib::assert_expected_value(T::create(fpath));
  auto lock_dup = test_lib::assert_expected_value(T::create(fpath));
}

JOWI_ADD_TEST(file_mutex_create_no_truncate) {
  create_no_truncate<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_no_concurrent_writes) {
  no_concurrent_writes<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_no_concurrent_write_to_same_object) {
  no_concurrent_write_to_same_object<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_allow_concurrent_reads) {
  allow_concurrent_reads<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_allow_concurrent_read_same_object) {
  allow_concurrent_read_same_object<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_no_concurrent_read_writes) {
  no_concurrent_read_writes<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_no_concurrent_read_writes_same_object) {
  no_concurrent_read_writes_same_object<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_with_std_shared_lock) {
  with_std_shared_lock<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_with_std_shared_lock_same_object) {
  with_std_shared_lock_same_object<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_with_std_unique_lock) {
  with_std_unique_lock<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_with_std_unique_lock_same_object) {
  with_std_unique_lock_same_object<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_no_concurrent_multi_process_write) {
  no_concurrent_multi_process_write<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_allow_concurrent_multiprocess_read) {
  allow_concurrent_multiprocess_read<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_no_concurent_multi_process_write_and_read) {
  no_concurent_multi_process_write_and_read<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_unlock_reader_twice) {
  unlock_reader_twice<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_unlock_flock_twice_in_thread) {
  unlock_flock_twice_in_thread<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_fuzz_tester) {
  fuzz_tester<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_unlock_without_lock) {
  unlock_without_lock<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_unlock_shared_without_lock) {
  unlock_shared_without_lock<file_lock::file_mutex>();
}
JOWI_ADD_TEST(file_mutex_duplicate) {
  duplicate<file_lock::file_mutex>();
}
JOWI_ADD_TEST(lf_mutex_duplicate) {
  duplicate<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_create_no_truncate) {
  create_no_truncate<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_no_concurrent_writes) {
  no_concurrent_writes<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_no_concurrent_write_to_same_object) {
  no_concurrent_write_to_same_object<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_allow_concurrent_reads) {
  allow_concurrent_reads<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_allow_concurrent_read_same_object) {
  allow_concurrent_read_same_object<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_no_concurrent_read_writes) {
  no_concurrent_read_writes<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_no_concurrent_read_writes_same_object) {
  no_concurrent_read_writes_same_object<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_with_std_shared_lock) {
  with_std_shared_lock<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_with_std_shared_lock_same_object) {
  with_std_shared_lock_same_object<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_with_std_unique_lock) {
  with_std_unique_lock<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_with_std_unique_lock_same_object) {
  with_std_unique_lock_same_object<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_no_concurrent_multi_process_write) {
  no_concurrent_multi_process_write<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_allow_concurrent_multiprocess_read) {
  allow_concurrent_multiprocess_read<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_no_concurent_multi_process_write_and_read) {
  no_concurent_multi_process_write_and_read<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_unlock_reader_twice) {
  unlock_reader_twice<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_unlock_flock_twice_in_thread) {
  unlock_flock_twice_in_thread<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_fuzz_tester) {
  fuzz_tester<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_unlock_without_lock) {
  unlock_without_lock<file_lock::lf_mutex>();
}
JOWI_ADD_TEST(lf_mutex_unlock_shared_without_lock) {
  unlock_shared_without_lock<file_lock::lf_mutex>();
}
