# cli: command-line entry point, orchestrates the whole compilation
# pipeline (solid-lang-compiler executable).
add_executable(solid-lang-compiler main.c options.c)

# Depends only on irgen; parser/sema and LLVM-C.lib come transitively
# through the static library chain.
target_link_libraries(solid-lang-compiler PRIVATE solid-lang-irgen)

# Runtime: copy LLVM-C.dll next to the exe so the debugger can find it.
# Windows-only: on Unix the LLVM shared/static library is already reachable
# via the loader path or the link line, and copying a huge libLLVM.so into the
# build dir is pointless.
if(WIN32 AND TARGET LLVM-C)
  add_custom_command(TARGET solid-lang-compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:LLVM-C>
            $<TARGET_FILE_DIR:solid-lang-compiler>)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
