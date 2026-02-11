# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/john/esp/esp-idf/components/bootloader/subproject"
  "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader"
  "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader-prefix"
  "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader-prefix/tmp"
  "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader-prefix/src/bootloader-stamp"
  "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader-prefix/src"
  "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/john/automaker/Static Test Stand Controller/build-remote/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
