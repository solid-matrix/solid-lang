add_library(solid-lang-parser STATIC
  parser.c
  ast.c)

target_include_directories(solid-lang-parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(solid-lang-parser PUBLIC solid-lang-common)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
