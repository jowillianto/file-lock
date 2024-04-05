#include <boost/python.hpp>
#include <string>
#include <Python.h>
import file_lock;
using namespace boost::python;

class PythonMutex{
  file_lock::LargeFileMutex _internal_mutex;
  public:
    PythonMutex(const std::string& file_path) : 
      _internal_mutex { file_path }
    {}
    void lock () { 
      Py_BEGIN_ALLOW_THREADS
      _internal_mutex.lock();
      Py_END_ALLOW_THREADS
    }
    void unlock() { _internal_mutex.unlock(); }
    bool try_lock() { return _internal_mutex.try_lock(); }
    void lock_shared() {
      Py_BEGIN_ALLOW_THREADS
      _internal_mutex.lock_shared();
      Py_END_ALLOW_THREADS
    }
    bool try_lock_shared() {return _internal_mutex.try_lock_shared(); }
    void unlock_shared() { _internal_mutex.unlock_shared(); }
    std::string protected_path_string() const {
      return _internal_mutex.protected_path_string();
    }
};

BOOST_PYTHON_MODULE(file_lock){
  class_<PythonMutex, boost::noncopyable>("FileMutex", init<std::string>())
    .def("lock", &PythonMutex::lock)
    .def("unlock", &PythonMutex::unlock)
    .def("try_lock", &PythonMutex::try_lock)
    .def("lock_shared", &PythonMutex::lock_shared)
    .def("unlock_shared", &PythonMutex::unlock_shared)
    .def("try_lock_shared", &PythonMutex::try_lock_shared)
    .def("protected_path_string", &PythonMutex::protected_path_string)
  ;
}