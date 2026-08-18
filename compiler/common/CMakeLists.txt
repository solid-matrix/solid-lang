# common: shared data structures and helpers (solid_common static library).
# Headers and implementations compile together, reused by parser/sema/irgen/cli.
add_library(solid-lang-common STATIC source.c string_view.c
    arena.c)

target_include_directories(solid-lang-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
