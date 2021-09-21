project(capo-example)

add_executable(${PROJECT_NAME}-version)
target_link_libraries(${PROJECT_NAME}-version PRIVATE capo::capo capo::options)
target_sources(${PROJECT_NAME}-version PRIVATE version.cpp)

add_executable(${PROJECT_NAME}-openal-sound)
target_link_libraries(${PROJECT_NAME}-openal-sound PRIVATE OpenAL capo::capo capo::options)
target_sources(${PROJECT_NAME}-openal-sound PRIVATE sound.cpp)
