# cli: command-line entry point, orchestrates the whole compilation
# pipeline (solid-lang-compiler executable).
add_executable(solid-lang-compiler main.c options.c)

# Depends only on irgen; parser/sema and LLVM-C.lib come transitively
# through the static library chain.
target_link_libraries(solid-lang-compiler PRIVATE solid_irgen)

# Runtime: copy LLVM-C.dll next to the exe (resolved via the imported
# target, so it works across platforms and directories).
add_custom_command(TARGET solid-lang-compiler POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:LLVM-C>
          $<TARGET_FILE_DIR:solid-lang-compiler>)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
