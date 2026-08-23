add_executable(solid src/main.c)

target_link_libraries(solid PRIVATE solid-lang-irgen)

if(TARGET LLVM-C)
  get_target_property(_llvm_c_type LLVM-C TYPE)
  if(_llvm_c_type STREQUAL "SHARED_LIBRARY" OR _llvm_c_type STREQUAL "MODULE_LIBRARY")
    add_custom_command(TARGET solid POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_FILE:LLVM-C>
              $<TARGET_FILE_DIR:solid>)
  endif()
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
