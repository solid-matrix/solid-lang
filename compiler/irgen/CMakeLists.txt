# irgen: AST -> LLVM IR (solid_irgen static library).
# This is the only target that touches the LLVM C API: both the headers
# and the link are PRIVATE, so LLVM details never leak out.
add_library(solid_irgen STATIC irgen.c)

target_include_directories(solid_irgen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(solid_irgen PRIVATE ${LLVM_INCLUDE_DIRS})
# AST types appear in irgen's public interface, so sema and the AST
# dependency are visible to downstream targets.
target_link_libraries(solid_irgen PUBLIC solid_sema)
target_link_libraries(solid_irgen PRIVATE ${SOLID_LLVM_LIB})

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
