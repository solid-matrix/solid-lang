add_library(solid-lang-common STATIC
  src/source.c
  src/strview.c
  src/span.c
  src/xmem.c
  src/arena.c)

target_include_directories(solid-lang-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# mimalloc backs common/arena; the dependency is build-internal (the
# public headers never mention it), so it stays PRIVATE.
target_link_libraries(solid-lang-common PRIVATE mimalloc-static)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
