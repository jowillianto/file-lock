#include <boost/python.hpp>
#include <string>
import file_lock;
using namespace boost::python;

BOOST_PYTHON_MODULE(file_lock) {
  class_<file_lock::SysFileLock, boost::noncopyable>(
    "SysFileLock", init<std::string>()
  )
    .def("lock", &file_lock::SysFileLock::lock)
    .def("unlock", &file_lock::SysFileLock::unlock)
    .def("try_lock", &file_lock::SysFileLock::try_lock)
    .def("lock_shared", &file_lock::SysFileLock::lock_shared)
    .def("unlock_shared", &file_lock::SysFileLock::unlock_shared)
    .def("try_lock_shared", &file_lock::SysFileLock::try_lock_shared)
    .def("protected_path_string", &file_lock::FileMutex::protected_path_string);
  class_<file_lock::FileMutex, boost::noncopyable>(
    "FileMutex", init<std::string>()
  )
    .def("lock", &file_lock::FileMutex::lock)
    .def("unlock", &file_lock::FileMutex::unlock)
    .def("try_lock", &file_lock::FileMutex::try_lock)
    .def("lock_shared", &file_lock::FileMutex::lock_shared)
    .def("unlock_shared", &file_lock::FileMutex::unlock_shared)
    .def("try_lock_shared", &file_lock::FileMutex::try_lock_shared)
    .def("protected_path_string", &file_lock::FileMutex::protected_path_string)
    .def(
      "file_lock",
      &file_lock::FileMutex::file_lock,
      return_value_policy<reference_existing_object>()
    );
  class_<file_lock::LargeFileMutex, boost::noncopyable>(
    "LargeFileMutex", init<std::string>()
  )
    .def("lock", &file_lock::LargeFileMutex::lock)
    .def("unlock", &file_lock::LargeFileMutex::unlock)
    .def("try_lock", &file_lock::LargeFileMutex::try_lock)
    .def("lock_shared", &file_lock::LargeFileMutex::lock_shared)
    .def("unlock_shared", &file_lock::LargeFileMutex::unlock_shared)
    .def("try_lock_shared", &file_lock::LargeFileMutex::try_lock_shared)
    .def("protected_path_string", &file_lock::FileMutex::protected_path_string)
    .def(
      "file_lock",
      &file_lock::LargeFileMutex::file_lock,
      return_value_policy<reference_existing_object>()
    );
}