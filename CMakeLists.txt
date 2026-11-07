cmake_minimum_required(VERSION 3.28.3)
project(
  A10
  DESCRIPTION "A simple MapReduce."
  LANGUAGES C CXX)
include_directories(include)
file(GLOB SOURCES "src/*.c")
add_executable(mapreduce ${SOURCES})
add_compile_options(-fsanitize=address -g -01)
add_link_options(-fsanitize=address)
