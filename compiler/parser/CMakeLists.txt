add_library(solid-lang-parser STATIC
  src/parser.c
  src/parser_result.c
  src/syntax_node.c
  src/syntax_error.c
  src/parse_internal.c
  src/parse_primary.c
  src/parse.c)

target_include_directories(solid-lang-parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(solid-lang-parser PUBLIC solid-lang-common)

if(BUILD_TESTING)
  # Parser-domain test surface (parser_fixture.h): lifecycle helpers on
  # top of the shared Unity glue; test executables only.
  add_library(solid-lang-parser-tests-support INTERFACE)
  target_include_directories(solid-lang-parser-tests-support
                             INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(solid-lang-parser-tests-support
                        INTERFACE solid-lang-test-support)

  add_subdirectory(tests)
endif()
