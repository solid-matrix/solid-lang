add_library(solid-lang-common STATIC 
  source.c
  string_view.c
  span.c
  mem.c)

target_include_directories(solid-lang-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
