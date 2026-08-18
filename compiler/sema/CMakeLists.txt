# sema: semantic analysis, consumes the AST produced by parser
# (solid_sema static library).
add_library(solid-lang-sema STATIC sema.c type.c)

target_include_directories(solid-lang-sema PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# AST types appear in sema's public interface, so the dependency is
# visible to downstream targets.
target_link_libraries(solid-lang-sema PUBLIC solid-lang-parser)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
