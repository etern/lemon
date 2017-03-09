project(lemon)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_BUILD_TYPE Debug)

include_directories(${PROJECT_SOURCE_DIR}/src)
set (SOURCES
  src/action.c
  src/build.c
  src/configlist.c
  src/error.c
  src/main.c
  src/msort.c
  src/option.c
  src/parse.c
  src/plink.c
  src/report.c
  src/set.c
  src/table.c
  )

add_executable(lemon ${SOURCES})