add_executable(${PROJECT_NAME}-version)
target_link_libraries(${PROJECT_NAME}-version PRIVATE capo::capo capo::options)
target_sources(${PROJECT_NAME}-version PRIVATE version.cpp)

add_executable(${PROJECT_NAME}-example)
target_link_libraries(${PROJECT_NAME}-example PRIVATE capo::capo capo::options)
target_sources(${PROJECT_NAME}-example PRIVATE example.cpp)
