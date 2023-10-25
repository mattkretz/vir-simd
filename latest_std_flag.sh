#!/bin/bash
for std in gnu++23 gnu++2b c++23 c++2b gnu++20 gnu++2a c++20 c++2a gnu++17 c++17; do
  if $CXX -x c++ -c -std=$std /dev/null 2>/dev/null; then
    echo $std
    exit
  fi
done
