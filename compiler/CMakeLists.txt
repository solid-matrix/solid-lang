cmake_minimum_required (VERSION 3.10)

# Enable hot reload for the MSVC compiler where supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<C_COMPILER_ID:MSVC>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("solid-lang" LANGUAGES C)

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

# Executables (solid, test binaries) and their runtime DLLs go to
# <build>/bin instead of per-subdirectory folders. Libraries stay in
# their own subdirectories.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_PDB_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin)
endforeach()

# LLVM C API: LLVM_DIR points at the LLVM install location
# (either the root or lib/cmake/llvm works).
if(DEFINED ENV{LLVM_DIR} AND NOT LLVM_DIR)
  set(LLVM_DIR "$ENV{LLVM_DIR}")
endif()

if(NOT LLVM_DIR)
  # Debian/Ubuntu-style system installs expose /usr/bin/llvm-config,
  # which knows its own CMake package directory.
  find_program(LLVM_CONFIG_EXE "llvm-config")
  if(LLVM_CONFIG_EXE)
    execute_process(COMMAND "${LLVM_CONFIG_EXE}" --cmakedir
      OUTPUT_VARIABLE LLVM_CMAKE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(LLVM_CMAKE_DIR AND EXISTS "${LLVM_CMAKE_DIR}/LLVMConfig.cmake")
      set(LLVM_DIR "${LLVM_CMAKE_DIR}")
    endif()
  endif()
endif()

if(LLVM_DIR AND NOT EXISTS "${LLVM_DIR}/LLVMConfig.cmake")
  if(EXISTS "${LLVM_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
    set(LLVM_DIR "${LLVM_DIR}/lib/cmake/llvm")
  endif()
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")

# Pick the LLVM target that provides the C API for linking:
#   1. LLVM-C -- official LLVM releases and LLVM_BUILD_LLVM_C_DYLIB builds;
#   2. LLVM   -- monolithic builds (e.g. the Debian/Ubuntu system LLVM);
#   3. static components -- a full static install without the LLVM-C target.
if(TARGET LLVM-C)
  set(SOLID_LLVM_LIB LLVM-C)
elseif(TARGET LLVM)
  set(SOLID_LLVM_LIB LLVM)
else()
  llvm_map_components_to_libnames(SOLID_LLVM_COMPONENT_LIBS "core" "support")
  if(NOT SOLID_LLVM_COMPONENT_LIBS)
    message(FATAL_ERROR
      "The LLVM installation in ${LLVM_DIR} provides no usable C API library "
      "(no LLVM-C or LLVM target). Install a complete LLVM and point LLVM_DIR "
      "at it.")
  endif()
  set(SOLID_LLVM_LIB ${SOLID_LLVM_COMPONENT_LIBS})
endif()

message(STATUS "Linking LLVM C API via: ${SOLID_LLVM_LIB}")

add_subdirectory(common)
add_subdirectory(parser)
add_subdirectory(sema)
add_subdirectory(irgen)
add_subdirectory(cli)