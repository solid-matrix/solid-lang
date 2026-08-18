# parser: lexing + parsing, produces the AST (solid_parser static library).
add_library(solid-lang-parser STATIC parser.c)

# Headers reference common types (source.h, string_view.h), so the
# dependency is visible to downstream targets.
target_include_directories(solid-lang-parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(solid-lang-parser PUBLIC solid-lang-common)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
