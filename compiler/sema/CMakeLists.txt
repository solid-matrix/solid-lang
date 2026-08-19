add_library(solid-lang-sema STATIC sema.c)

target_include_directories(solid-lang-sema PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(solid-lang-sema PUBLIC solid-lang-parser)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
