#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <thread>
import moderna.file_lock;
import moderna.test_lib;

namespace mt = moderna::test_lib;

template <typename Mut> void act_or_exit(Mut &&m, std::string_view act_type) {
  if (act_type == "test_shared_lockable") {
    if (m.try_lock_shared()) {
      m.unlock_shared();
      exit(0);
    }
    exit(1);
  } else if (act_type == "test_not_shared_lockable") {
    if (m.try_lock_shared()) {
      m.unlock_shared();
      exit(1);
    }
    exit(0);
  } else if (act_type == "test_unique_lockable") {
    if (m.try_lock()) {
      m.unlock();
      exit(0);
    }
    exit(1);
  } else if (act_type == "test_not_unique_lockable") {
    if (m.try_lock()) {
      m.unlock();
      exit(1);
    }
    exit(0);
  } else
    throw std::bad_exception{};
}

template <typename Mut> void fuzz_test(Mut &&m, const std::filesystem::path& file_path, std::string_view buf) {
  uint32_t todo = mt::random_integer(0, 1);
  std::this_thread::sleep_for(std::chrono::microseconds{mt::random_integer(0, 500)});
  if (todo == 0) {
    std::shared_lock l{m};
    std::ifstream f{file_path};
    std::stringstream buffer;
    buffer << f.rdbuf();
    if (buffer.str() != buf) {
      std::cout << buf << " is not equal to " << buffer.str() << std::endl;
      exit(1);
    }
  } else {
    std::unique_lock l{m};
    std::ofstream f{file_path, std::ios_base::trunc | std::ios_base::out};
    f << buf;
    f.flush();
    f.close();
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cerr << "Wrong amount of arguments" << std::endl;
    exit(1);
  }
  std::filesystem::path file_path{argv[1]};
  std::string_view mut_type{argv[2]};
  std::string_view act_type{argv[3]};
  if (act_type == "fuzz_test") {
    if (argc != 5) {
      std::cerr << "Wrong amount of arguments" << std::endl;
      exit(1);
    }
    if (mut_type == "lf_mut") {
      fuzz_test(moderna::file_lock::lf_mutex::create(file_path).value(), file_path, argv[4]);
    } else if (mut_type == "f_mut") {
      fuzz_test(moderna::file_lock::file_mutex::create(file_path).value(), file_path, argv[4]);
    }
  }
  else if (mut_type == "lf_mut") {
    act_or_exit(moderna::file_lock::lf_mutex::create(file_path).value(), act_type);
  } else if (mut_type == "f_mut") {
    act_or_exit(moderna::file_lock::file_mutex::create(file_path).value(), act_type);
  }
}