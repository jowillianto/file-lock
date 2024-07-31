#include <pybind11/pybind11.h>
#include <filesystem>
#include <string>
import moderna.file_lock;
namespace py = pybind11;
class PythonMutex{
  moderna::file_lock::lf_mutex _internal_mutex;
  std::string _file_path;
  public:
    PythonMutex(const std::string& file_path) : 
      _internal_mutex { file_path }
    {
      _file_path = std::filesystem::absolute(std::filesystem::path{ file_path }).string();
    }
    void lock () { 
      py::gil_scoped_release release;
      _internal_mutex.lock();
    }
    void unlock() { _internal_mutex.unlock(); }
    bool try_lock() { return _internal_mutex.try_lock(); }
    void lock_shared() {
      py::gil_scoped_release release;
      _internal_mutex.lock_shared();
    }
    bool try_lock_shared() {return _internal_mutex.try_lock_shared(); }
    void unlock_shared() { _internal_mutex.unlock_shared(); }
    const std::string& file_path() const {
      return _file_path;
    }
};

PYBIND11_MODULE(file_lock, m){
  py::class_<PythonMutex>(m, "FileMutex")
    .def(py::init<const std::string&>())
    .def("lock", &PythonMutex::lock)
    .def("unlock", &PythonMutex::unlock)
    .def("try_lock", &PythonMutex::try_lock)
    .def("lock_shared", &PythonMutex::lock_shared)
    .def("unlock_shared", &PythonMutex::unlock_shared)
    .def("try_lock_shared", &PythonMutex::try_lock_shared)
    .def("file_path", &PythonMutex::file_path, py::return_value_policy::copy)
  ;
}