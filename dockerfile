FROM ubuntu:24.04 as compile-test

RUN apt update && apt install -y \
  cmake \
  clang-18 \
  clang-tools-18 \
  ninja-build \
  python3-dev \
  libboost-all-dev

WORKDIR /app
COPY . .
RUN CXX=clang++-18 cmake -GNinja -DCMAKE_BUILD_TYPE=Release -B build .
WORKDIR /app/build
RUN ninja multiprocessing_file_lock_tests
RUN ./multiprocessing_file_lock_tests
RUN ninja multiprocessing_file_lock_python

FROM scratch as final-stage
COPY --from=compile-test /app/build/file_lock.so /