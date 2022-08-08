cmake_minimum_required(VERSION 3.17 FATAL_ERROR)

enable_language(CXX)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # don't let OPTION erase non-cache variables
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # disable compiler extensions
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # enable PIC
set(CMAKE_DEBUG_POSTFIX "-d")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug) # ensure CMAKE_BUILD_TYPE is not empty
endif()

# project parameters
set(project_name capo)
set(project_version "1.3.0")
set(project_version_name capo_version) # name of variable in generated header
set(cxx_standard cxx_std_20)
set(cmake_var_prefix CAPO)

project(${project_name} VERSION "${project_version}")
set(is_root_project OFF) # indicate if this is the top-level project

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(is_root_project ON)
  message(STATUS "${PROJECT_NAME} is top level")
endif()

# options
option(CAPO_USE_OPENAL "Build and link to OpenAL (otherwise interface is inactive but buildable) (default ON)" ON)
option(CAPO_VALID_IF_INACTIVE "Have .valid() return true if lib is inactive (default ON)" ON)
option(CAPO_INSTALL "Setup capo install and package config" ${is_root_project})

if(CAPO_INSTALL AND NOT CAPO_USE_OPENAL)
  message(FATAL_ERROR "Cannot have CAPO_INSTALL set without CAPO_USE_OPENAL")
endif()

# includes
include(CheckPIESupported)
check_pie_supported()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/platform.cmake) # set up helper cache variables

add_subdirectory(cmake/interface) # import interface library
add_subdirectory(ext)

# header text
if(is_root_project)
  set(build_config ${CMAKE_BUILD_TYPE})

  if(CMAKE_CONFIGURATION_TYPES)
    set(build_config "Multi-config")
  endif()

  message(STATUS "[${CMAKE_SYSTEM_PROCESSOR}] [${PLATFORM}] [${CMAKE_GENERATOR}] [${CMAKE_CXX_COMPILER_ID}] [${build_config}]")
endif()

# compile commands, ci scripts
if(is_root_project AND NOT "${CI_BUILD}")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(${cmake_var_prefix}_CONFIGURE_CI_SCRIPTS ON)
  message(STATUS "${PROJECT_NAME}: Exporting compile commands, configuring CI scripts")
endif()

# version
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/capo_version.hpp.in")
  message(STATUS "Configuring ${CMAKE_CURRENT_SOURCE_DIR}/src/capo_version.hpp.in")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/capo_version.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/capo_version.hpp")
endif()

# library
add_library(${PROJECT_NAME})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
add_subdirectory(include/capo)
add_subdirectory(src)
target_include_directories(${PROJECT_NAME} PRIVATE include src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}::capo-options) # apply interface library options
target_link_libraries(${PROJECT_NAME}
  PUBLIC
  ktl::ktl
  PRIVATE
  capo::dr_libs
  capo::capo-openal
  $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:pthread>
)
target_source_group(TARGET ${PROJECT_NAME})

# example
option(CAPO_BUILD_EXAMPLE "Build example" ON)

if(CAPO_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()

if(CAPO_INSTALL)
  install_targets(
    TARGETS
    dr_libs
    OpenAL
    capo-options
    capo-interface
    capo-openal
    capo
    EXPORT
    capo-targets
  )
  install_target_headers()
  install_export_package(EXPORT capo-targets NAMESPACE capo PACKAGE capo)
  install_package_config(PACKAGE capo IN cmake/config.cmake.in)
  install_package_version(PACKAGE capo VERSION ${project_version})
endif()
