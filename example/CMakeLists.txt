project(capo-example)

add_executable(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE capo::capo capo::options)
target_sources(${PROJECT_NAME} PRIVATE example.cpp)
