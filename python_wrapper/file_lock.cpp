#include <boost/python.hpp>
#include <string>
import file_lock;
using namespace boost::python;

BOOST_PYTHON_MODULE(file_lock){
  class_<file_lock::BasicMutex, boost::noncopyable>("BasicMutex", init<std::string>())
    .def("lock", &file_lock::BasicMutex::lock)
    .def("unlock", &file_lock::BasicMutex::unlock)
    .def("try_lock", &file_lock::BasicMutex::try_lock)
    .def("lock_shared", &file_lock::BasicMutex::lock_shared)
    .def("unlock_shared", &file_lock::BasicMutex::unlock_shared)
    .def("try_lock_shared", &file_lock::BasicMutex::try_lock_shared)
    .def("protected_path_string", &file_lock::BasicMutex::protected_path_string)
  ;
  class_<file_lock::LargeFileMutex, boost::noncopyable>("LargeFileMutex", init<std::string>())
    .def("lock", &file_lock::LargeFileMutex::lock)
    .def("unlock", &file_lock::LargeFileMutex::unlock)
    .def("try_lock", &file_lock::LargeFileMutex::try_lock)
    .def("lock_shared", &file_lock::LargeFileMutex::lock_shared)
    .def("unlock_shared", &file_lock::LargeFileMutex::unlock_shared)
    .def("try_lock_shared", &file_lock::LargeFileMutex::try_lock_shared)
    .def("protected_path_string", &file_lock::BasicMutex::protected_path_string)
  ;
}