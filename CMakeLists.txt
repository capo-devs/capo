cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

enable_language(CXX)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)					# don't let OPTION erase non-cache variables
set(CMAKE_CXX_EXTENSIONS OFF CACHE INTERNAL "" FORCE)	# disable compiler extensions
set(CMAKE_POSITION_INDEPENDENT_CODE ON)					# enable PIC
set(CMAKE_DEBUG_POSTFIX "-d")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)			# ensure CMAKE_BUILD_TYPE is not empty
endif()

# project parameters
set(project_name capo)
set(project_version "1.0.1.0")
set(project_version_name capo_version) # name of variable in generated header
set(cxx_standard cxx_std_20)
set(cmake_var_prefix CAPO)

project(${project_name} VERSION "${project_version}")
set(is_root_project OFF)	# indicate if this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(is_root_project ON)
  message(STATUS "${PROJECT_NAME} is top level")
endif()

# options
option(CAPO_USE_OPENAL "Build and link to OpenAL (otherwise interface is inactive but buildable) (default ON)" ON)
option(CAPO_VALID_IF_INACTIVE "Have .valid() return true if lib is inactive (default ON)" ON)

# includes
include(CheckPIESupported)
check_pie_supported()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/platform.cmake)	# set up helper cache variables
include(cmake/utils.cmake)		# functions for git operations etc

# git operations
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  if(is_root_project)
    option(GIT_UPDATE_SUBMODULES "Init/Update submodules during generation" ${is_root_project})
    if(GIT_UPDATE_SUBMODULES)
      git_update_submodules(WARNING)	# update git submodules and warn on failure
    endif()
  endif()
endif()

add_subdirectory(cmake/interface)	# import interface library

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

# appveyor
if(NOT "${CI_BUILD}" AND "${${cmake_var_prefix}_CONFIGURE_CI_SCRIPTS}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.appveyor.yml.in")
  message(STATUS "Configuring ${CMAKE_CURRENT_SOURCE_DIR}/.appveyor.yml.in")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/.appveyor.yml.in" "${CMAKE_CURRENT_SOURCE_DIR}/.appveyor.yml")
endif()

# library
add_library(${PROJECT_NAME})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
add_subdirectory(include/capo)
add_subdirectory(src)
target_source_group(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PRIVATE include src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}::options)					# apply interface library options
target_link_libraries(${PROJECT_NAME} 
  PUBLIC
    ktl::ktl
  PRIVATE 
    dr_libs::dr_libs
    ${PROJECT_NAME}::openal
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:pthread>
)
add_subdirectory(lib)

# example
option(CAPO_BUILD_EXAMPLE "Build example" ON)
if(CAPO_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()
