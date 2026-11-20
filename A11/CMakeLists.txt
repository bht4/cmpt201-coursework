cmake_minimum_required(VERSION 3.28.3)
project(
  A11
  DESCRIPTION "Simple group chat"
  LANGUAGES C)
include_directories(include)
add_executable(server src/server.c)
add_executable(client src/client.c)
