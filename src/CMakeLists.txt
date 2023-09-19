set(EXECUTABLE "redis_invalidator")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES
    main.cpp
    utils/utils.cpp
)
add_executable(${EXECUTABLE} ${SOURCES})

include_directories(${CMAKE_SOURCE_DIR}/deps/CLI11/include)
include_directories(${CMAKE_SOURCE_DIR}/src/utils)

# Find the required packages: libpqxx
set(SKIP_BUILD_TEST True)

target_link_libraries(${EXECUTABLE} PRIVATE pqxx)

# <------------ add hiredis dependency --------------->
find_path(HIREDIS_HEADER hiredis)
target_include_directories(${EXECUTABLE} PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)
target_link_libraries(${EXECUTABLE} PUBLIC ${HIREDIS_LIB})

# <------------ add redis-plus-plus dependency -------------->
# NOTE: this should be *sw* NOT *redis++*
find_path(REDIS_PLUS_PLUS_HEADER sw REQUIRED)
target_include_directories(${EXECUTABLE} PUBLIC ${REDIS_PLUS_PLUS_HEADER} REQUIRED)

find_library(REDIS_PLUS_PLUS_LIB redis++ REQUIRED)
target_link_libraries(${EXECUTABLE} PUBLIC ${REDIS_PLUS_PLUS_LIB})
target_link_libraries(${EXECUTABLE} PUBLIC pthread)

set_target_properties(${EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Additional compiler flags if needed
target_compile_options(${EXECUTABLE} PRIVATE -Wno-unused-parameter -Wall -Wextra -g -O2)