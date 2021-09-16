project(capo-example)
set(cxx_standard cxx_std_20)

add_executable(${PROJECT_NAME}-version)
target_link_libraries(${PROJECT_NAME}-version PRIVATE capo::capo capo::options)
target_sources(${PROJECT_NAME}-version PRIVATE version.cpp)

add_executable(${PROJECT_NAME}-openal-sound)
target_include_directories(${PROJECT_NAME}-openal-sound PRIVATE ${dr_libs_SOURCE_DIR} ${OPENAL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME}-openal-sound PRIVATE ${OPENAL_LIBRARY})
target_sources(${PROJECT_NAME}-openal-sound PRIVATE sound.cpp)