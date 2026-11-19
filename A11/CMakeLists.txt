cmake_minimum_required(VERSION 3.22)
project(
  groupChat
  DESCRIPTION "Simple group chat"
  LANGUAGES C)
include_directories(include)
add_executable(server server.c)
add_executable(client client.c)
