add_library(solid-lang-parser STATIC
  src/parser.c
  src/parser_result.c
  src/syntax_node.c
  src/syntax_error.c
  src/parse_shared.c
  src/parse_common.c
  src/parse_type.c
  src/parse_expr.c
  src/parse_stmt.c
  src/parse_decl.c
  src/parse_primary.c)

target_include_directories(solid-lang-parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(solid-lang-parser PUBLIC solid-lang-common)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
