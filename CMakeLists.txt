cmake_minimum_required(VERSION 3.11.0)
set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)
project(anyToImage VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
cmake_policy(SET CMP0072 NEW)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MAKE_PROGRAM anyToImage)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# find_package(glfw3 3.3 REQUIRED)
# find_package(OpenGL REQUIRED)

file(GLOB_RECURSE APP_SOURCES "src/*")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")

add_executable(anyToImage ${APP_SOURCES})
target_include_directories(anyToImage PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(anyToImage)
