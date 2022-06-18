cmake_minimum_required(VERSION 3.17 FATAL_ERROR)

project(capo-examples)

if(NOT TARGET capo)
  find_package(capo REQUIRED CONFIG)
endif()

add_executable(${PROJECT_NAME}-sound)
target_link_libraries(${PROJECT_NAME}-sound PRIVATE capo::capo capo::capo-options)
target_sources(${PROJECT_NAME}-sound PRIVATE example_sound.cpp)

add_executable(${PROJECT_NAME}-music)
target_link_libraries(${PROJECT_NAME}-music PRIVATE capo::capo capo::capo-options)
target_sources(${PROJECT_NAME}-music PRIVATE example_music.cpp)

add_executable(${PROJECT_NAME}-player)
target_link_libraries(${PROJECT_NAME}-player PRIVATE capo::capo capo::capo-options)
target_sources(${PROJECT_NAME}-player PRIVATE music_player.cpp)
