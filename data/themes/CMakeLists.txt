# Copyright (c) 2016 Jean Guyomarc'h
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

add_custom_command(
   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/default.edj
   COMMAND edje_cc ${CMAKE_CURRENT_SOURCE_DIR}/default.edc
      ${CMAKE_CURRENT_BINARY_DIR}/default.edj
      -id ${CMAKE_CURRENT_SOURCE_DIR}
      -author ${CMAKE_SOURCE_DIR}/AUTHORS
      -license ${CMAKE_SOURCE_DIR}/COPYING
   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/default.edc
)
add_custom_target(
   themes
   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/default.edj
)

list(APPEND THEME_FILES
   ${CMAKE_CURRENT_BINARY_DIR}/default.edj
)

install(
   FILES ${THEME_FILES}
   DESTINATION share/${CMAKE_PROJECT_NAME}/themes
)
