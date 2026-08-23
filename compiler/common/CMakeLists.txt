add_library(solid-lang-common STATIC
  src/source.c
  src/strview.c
  src/span.c
  src/xmem.c)

target_include_directories(solid-lang-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
