cmake_minimum_required(VERSION 3.4)

if (WIN32)
  cmake_policy(SET CMP0020 NEW)
endif (WIN32)

if (POLICY CMP0028)
  cmake_policy(SET CMP0028 OLD)
endif ()

if (POLICY CMP0043)
  cmake_policy(SET CMP0043 OLD)
endif ()

if (POLICY CMP0042)
  cmake_policy(SET CMP0042 OLD)
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin_debug")
set(CMAKE_DEBUG_POSTFIX "d")

project(hifi)
add_definitions(-DGLM_FORCE_RADIANS)
set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

add_definitions(-DPROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

if (WIN32)
  add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)

  if (NOT WINDOW_SDK_PATH)
    set(DEBUG_DISCOVERED_SDK_PATH TRUE)
  endif()

  # sets path for Microsoft SDKs
  # if you get build error about missing 'glu32' this path is likely wrong
  if (MSVC10)
    set(WINDOW_SDK_PATH "C:\\Program Files\\Microsoft SDKs\\Windows\\v7.1 " CACHE PATH "Windows SDK PATH")
  elseif (MSVC12)
    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      set(WINDOW_SDK_FOLDER "x64")
    else()
      set(WINDOW_SDK_FOLDER "x86")
    endif()
    set(WINDOW_SDK_PATH "C:\\Program Files (x86)\\Windows Kits\\8.1\\Lib\\winv6.3\\um\\${WINDOW_SDK_FOLDER}" CACHE PATH "Windows SDK PATH")
  endif ()

  if (DEBUG_DISCOVERED_SDK_PATH)
    message(STATUS "The discovered Windows SDK path is ${WINDOW_SDK_PATH}")
  endif ()

  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${WINDOW_SDK_PATH})
  # /wd4351 disables warning C4351: new behavior: elements of array will be default initialized
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /wd4351")
  # /LARGEADDRESSAWARE enables 32-bit apps to use more than 2GB of memory.
  # Caveats: http://stackoverflow.com/questions/2288728/drawbacks-of-using-largeaddressaware-for-32-bit-windows-executables
  # TODO: Remove when building 64-bit.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
  # always produce symbols as PDB files
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG /OPT:REF /OPT:ICF")
else ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-strict-aliasing -Wno-unused-parameter")
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Woverloaded-virtual -Wdouble-promotion")
      if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "5.1") # gcc 5.1 and on have suggest-override
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsuggest-override")
      endif ()
  endif ()
endif(WIN32)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.3")
    # GLM 0.9.8 on Ubuntu 14 (gcc 4.4) has issues with the simd declarations 
    add_definitions(-DGLM_FORCE_PURE)
  endif()
endif()

if (NOT ANDROID)
  if ((NOT MSVC12) AND (NOT MSVC14))
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
    CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

    if (COMPILER_SUPPORTS_CXX11)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    elseif(COMPILER_SUPPORTS_CXX0X)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    else()
      message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
    endif()
  endif ()
else ()
  # assume that the toolchain selected for android has C++11 support
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif ()

if (APPLE)
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++0x")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --stdlib=libc++")
endif ()

if (NOT ANDROID_LIB_DIR)
  set(ANDROID_LIB_DIR $ENV{ANDROID_LIB_DIR})
endif ()

# Hide automoc folders (for IDEs)
set(AUTOGEN_TARGETS_FOLDER "hidden/generated")

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# setup for find modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

if (CMAKE_BUILD_TYPE)
  string(TOUPPER ${CMAKE_BUILD_TYPE} UPPER_CMAKE_BUILD_TYPE)
else ()
  set(UPPER_CMAKE_BUILD_TYPE DEBUG)
endif ()

set(HF_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(MACRO_DIR "${HF_CMAKE_DIR}/macros")
set(EXTERNAL_PROJECT_DIR "${HF_CMAKE_DIR}/externals")

file(GLOB HIFI_CUSTOM_MACROS "cmake/macros/*.cmake")
foreach(CUSTOM_MACRO ${HIFI_CUSTOM_MACROS})
  include(${CUSTOM_MACRO})
endforeach()

set(EXTERNAL_PROJECT_PREFIX "project")
set_property(DIRECTORY PROPERTY EP_PREFIX ${EXTERNAL_PROJECT_PREFIX})
setup_externals_binary_dir()

if (WIN32)
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4049 /ignore:4217")
endif()

set(TARGET_NAME gl_scratch)
project(${TARGET_NAME})

# grab the implemenation and header files
file(GLOB TARGET_SRCS src/*)

file(GLOB SRC_SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/*)

foreach(DIR ${SRC_SUBDIRS})
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/${DIR}")
    file(GLOB DIR_CONTENTS "src/${DIR}/*")
    set(TARGET_SRCS ${TARGET_SRCS} "${DIR_CONTENTS}")
endif ()
endforeach()

add_executable(${TARGET_NAME} ${TARGET_SRCS})

# disable /OPT:REF and /OPT:ICF for the Debug builds
# This will prevent the following linker warnings
# LINK : warning LNK4075: ignoring '/INCREMENTAL' due to '/OPT:ICF' specification
if (WIN32)
    set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG "/OPT:NOREF /OPT:NOICF")
endif()

target_glm()
target_gli()
target_glew()
target_glfw3()
target_opengl()
