#include <string_view>
#include <filesystem>
#include <exception>
#include <mutex>
#include <iostream>
#include <shared_mutex>
import moderna.file_lock;

template<moderna::file_lock::shared_mutex Mut>
void act_or_exit(const Mut& m, std::string_view act_type) {
  if (act_type == "test_shared_lockable") {
    if (m.try_lock_shared()) {
      m.unlock_shared();
      exit(0);
    }
    exit(1);
  } 
  else if (act_type == "test_not_shared_lockable") {
    if (m.try_lock_shared()) {
      m.unlock_shared();
      exit(1);
    }
    exit(0);
  }
  else if (act_type == "test_unique_lockable") {
    if (m.try_lock()) {
      m.unlock();
      exit(0);
    }
    exit(1);
  }
  else if (act_type == "test_not_unique_lockable") {
    if (m.try_lock()) {
      m.unlock();
      exit(1);
    }
    exit(0);
  }
  else 
    throw std::bad_exception{ };
}

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr<< "Wrong amount of arguments" <<std::endl;
    exit(1);
  }
  std::filesystem::path file_path { argv[1] };
  std::string_view mut_type { argv[2] };
  std::string_view act_type { argv[3] };
  if (mut_type == "lf_mut") {
    act_or_exit(moderna::file_lock::lf_mutex{ file_path }, act_type);
  }
  else if (mut_type == "f_mut") {
    act_or_exit(moderna::file_lock::file_mutex{ file_path }, act_type);
  }
  else throw std::bad_exception{};
}