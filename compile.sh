#!/bin/bash
while read -d "" event; do
  cd build;
  cmake .;
  ninja;
  cd ..;
done