# 
#  Copyright 2015 High Fidelity, Inc.
#  Created by Bradley Austin Davis on 2015/10/10
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
# 
macro(TARGET_OPENGL)
  find_package(OpenGL REQUIRED)
  if (${OPENGL_INCLUDE_DIR})
    include_directories(SYSTEM "${OPENGL_INCLUDE_DIR}")
  endif()
  target_link_libraries(${TARGET_NAME} "${OPENGL_LIBRARY}")
  target_include_directories(${TARGET_NAME} PUBLIC ${OPENGL_INCLUDE_DIR})
endmacro()
