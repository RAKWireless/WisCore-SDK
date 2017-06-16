# wiscore - RAK alexa voice service demo
#
# Copyright (c) 2012, 2013, 2014, 2015 Rak
# Copyright (c) 2016 Rak
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
message("first not creat root project")
set(CMAKE_FIND_NO_INSTALL_PREFIX TRUE)
project(WisCoreSDK VERSION 0.0.1)

message("set include directories")
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/common/include)

message("add_subdirectory(libavio)")
add_subdirectory(libavio)

message("add_subdirectory(alexa_demo)")
add_subdirectory(alexa_demo)

message("add_subdirectory(tunnel)")
add_subdirectory(tunnel)

message("add_subdirectory(luci_service)")
add_subdirectory(luci_service)

message("add_subdirectory(gpioctrl)")
add_subdirectory(gpioctrl)






