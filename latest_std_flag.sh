#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright © 2023–2024 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
#                       Matthias Kretz <m.kretz@gsi.de>

for std in gnu++23 gnu++2b c++23 c++2b gnu++20 gnu++2a c++20 c++2a gnu++17 c++17; do
  if $CXX -x c++ -c -std=$std /dev/null 2>/dev/null; then
    echo $std
    exit
  fi
done
