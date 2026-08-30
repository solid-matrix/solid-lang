add_library(solid-lang-irgen STATIC src/irgen.c)

target_include_directories(solid-lang-irgen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(solid-lang-irgen PRIVATE ${LLVM_INCLUDE_DIRS})

target_link_libraries(solid-lang-irgen PUBLIC solid-lang-semantic)
target_link_libraries(solid-lang-irgen PRIVATE ${SOLID_LLVM})

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
