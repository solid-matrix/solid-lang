# sema: semantic analysis, consumes the AST produced by parser
# (solid_sema static library).
add_library(solid_sema STATIC sema.c type.c)

target_include_directories(solid_sema PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# AST types appear in sema's public interface, so the dependency is
# visible to downstream targets.
target_link_libraries(solid_sema PUBLIC solid_parser)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
