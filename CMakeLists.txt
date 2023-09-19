cmake_minimum_required(VERSION 3.10)

project(ConsistantCachingUtils)

set(SKIP_BUILD_TEST True)
add_subdirectory(deps/libpqxx build-pqxx)

add_subdirectory(src)
add_subdirectory(src/test)
