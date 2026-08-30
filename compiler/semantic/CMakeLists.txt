add_library(solid-lang-semantic STATIC
  src/semantic_symboltable.c)

target_include_directories(solid-lang-semantic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(solid-lang-semantic PUBLIC solid-lang-syntax)

if(BUILD_TESTING)
  add_library(solid-lang-semantic-tests-support INTERFACE)
  target_include_directories(solid-lang-semantic-tests-support
                             INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(solid-lang-semantic-tests-support
                        INTERFACE solid-lang-test-support)

  add_subdirectory(tests)
endif()
