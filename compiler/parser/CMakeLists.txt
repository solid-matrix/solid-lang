# parser: lexing + parsing, produces the AST (solid_parser static library).
add_library(solid_parser STATIC parser.c)

# Headers reference common types (source.h, string_view.h), so the
# dependency is visible to downstream targets.
target_include_directories(solid_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(solid_parser PUBLIC solid_common)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
