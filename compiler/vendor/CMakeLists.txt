# Consumes the vendored mimalloc submodule as the backing allocator of
# common/arena. Static library only, no global malloc override (callers
# opt in through common/arena), no tests.

set(MI_OVERRIDE     OFF CACHE BOOL "" FORCE)
set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS  OFF CACHE BOOL "" FORCE)

add_subdirectory(mimalloc mimalloc.build)

# Consumes the vendored Unity submodule as the unit-test framework.
# Tests consume it through the unity::framework alias.
add_subdirectory(unity unity.build)
