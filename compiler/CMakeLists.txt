# CMakeList.txt: the CMake project of solid-lang-compiler. Top level
# aggregates five subprojects: common (data structures/helpers),
# parser (syntax parser), sema (semantic analyzer), irgen (IR generator),
# cli (command-line tool).
#
cmake_minimum_required (VERSION 3.10)

# Enable hot reload for the MSVC compiler where supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<C_COMPILER_ID:MSVC>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("solid-lang-compiler" LANGUAGES C)

# C17 applies to all subprojects.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Sources are UTF-8 without BOM. MSVC must parse them as UTF-8 explicitly,
# otherwise it misreads them on non-UTF-8 system code pages.
if(MSVC)
  add_compile_options(/utf-8)
endif()

# Enable CTest (defines the BUILD_TESTING option); each subproject adds
# tests/ when BUILD_TESTING is on.
include(CTest)

# LLVM C API: LLVM_DIR points at the LLVM install location
# (either the root or lib/cmake/llvm works).
if(DEFINED ENV{LLVM_DIR} AND NOT LLVM_DIR)
  set(LLVM_DIR "$ENV{LLVM_DIR}")
endif()
if(LLVM_DIR AND NOT EXISTS "${LLVM_DIR}/LLVMConfig.cmake")
  if(EXISTS "${LLVM_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
    set(LLVM_DIR "${LLVM_DIR}/lib/cmake/llvm")
  endif()
endif()
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")

add_subdirectory(common)
add_subdirectory(parser)
add_subdirectory(sema)
add_subdirectory(irgen)
add_subdirectory(cli)

# TODO: add install targets if needed.
