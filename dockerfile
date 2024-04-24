FROM jowillianto/cpp-module-cont AS base
ARG PYTHON_VERSION=3.11
RUN add-apt-repository ppa:deadsnakes/ppa
RUN apt update && DEBIAN_FRONTEND=noninteractive apt install -y \
  python${PYTHON_VERSION}-dev \
  python${PYTHON_VERSION}-distutils
RUN python${PYTHON_VERSION} --version
WORKDIR /app
COPY . .
RUN CXX=clang++-18 cmake \
  -B build \
  -GNinja \
  -DPython3_EXECUTABLE=/usr/bin/python${PYTHON_VERSION} \
  -DPYBIND11_PYTHON_VERSION=${PYTHON_VERSION} \
  -DCMAKE_BUILD_TYPE=Release .
WORKDIR /app/build
RUN ninja multiprocessing_file_lock_tests
RUN ./multiprocessing_file_lock_tests
RUN ninja multiprocessing_file_lock_python