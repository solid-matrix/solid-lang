add_library(solid-lang-parser STATIC
  parser.c
  syntax_node.c
  syntax_error.c
  parse.c
  parse_type.c
  parse_expr.c
  parse_stmt.c
  parse_decl.c
  parse_primary.c)

target_include_directories(solid-lang-parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(solid-lang-parser PUBLIC solid-lang-common)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
