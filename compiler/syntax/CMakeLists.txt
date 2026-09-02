add_library(solid-lang-syntax STATIC
  src/error.c
  src/node.c
  src/parse.c
  src/parse_aux.c
  src/parse_common.c
  src/parse_decl.c
  src/parse_expr.c
  src/parse_lit.c
  src/parse_stmt.c
  src/parse_type.c
  src/result.c
  )

target_include_directories(solid-lang-syntax PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(solid-lang-syntax PUBLIC solid-lang-common)

if(BUILD_TESTING)
  # Syntax-domain test surface (parser_fixture.h): lifecycle helpers on
  # top of the shared Unity glue; test executables only.
  add_library(solid-lang-syntax-tests-support INTERFACE)
  target_include_directories(solid-lang-syntax-tests-support
                             INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests
                                       ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(solid-lang-syntax-tests-support
                        INTERFACE solid-lang-test-support)

  add_subdirectory(tests)
endif()
