#!/bin/bash
fswatch -0 ./src ./tests ./libs CMakeLists.txt | ./compile.sh