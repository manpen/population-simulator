cmake_minimum_required(VERSION 3.10)
set (CMAKE_CXX_STANDARD 17)

project(population-simulator)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")

add_subdirectory(libs/googletest)
add_subdirectory(libs/tlx)

find_package(OpenMP)

include_directories(include)

add_executable(clock source/main_clock.cpp)
target_link_libraries(clock tlx)

add_executable(clock_find_gap source/main_clock_find_gap.cpp)
target_link_libraries(clock_find_gap tlx)

add_executable(leader_election source/main_leader_election.cpp)
target_link_libraries(leader_election tlx)

add_executable(majority source/main_majority.cpp)
target_link_libraries(majority tlx)

add_executable(sim_benchmark source/main_benchmark.cpp)
target_link_libraries(sim_benchmark tlx)

enable_testing()
add_subdirectory(tests)
