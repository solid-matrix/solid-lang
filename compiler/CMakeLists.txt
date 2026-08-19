cmake_minimum_required (VERSION 3.19)
project ("solid-lang" LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CTest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")

if(TARGET LLVM-C)
    set(SOLID_LLVM LLVM-C)
    message(STATUS "Using LLVM-C (shared library)")
elseif(TARGET LLVM)
    set(SOLID_LLVM LLVM)
    message(STATUS "Using LLVM (static library)")
else()
    llvm_map_components_to_libnames(SOLID_LLVM_COMPONENT_LIBS "core" "support")
    if(NOT SOLID_LLVM_COMPONENT_LIBS)
        message(FATAL_ERROR "No usable LLVM libraries found.")
    endif()
    set(SOLID_LLVM ${SOLID_LLVM_COMPONENT_LIBS})
    message(STATUS "Using LLVM components: ${SOLID_LLVM}")
endif()

add_subdirectory(common)
add_subdirectory(parser)
add_subdirectory(sema)
add_subdirectory(irgen)
add_subdirectory(cli)