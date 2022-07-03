include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ktl
set(KTL_INSTALL CAPO_INSTALL)
FetchContent_Declare(
  ktl
  GIT_REPOSITORY https://github.com/karnkaul/ktl
  GIT_TAG v1.4.1 # manually update this to keep commits pinned
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ktl"
)
FetchContent_MakeAvailable(ktl)

# OpenAL
if(CAPO_USE_OPENAL)
  if(NOT TARGET OpenAL)
    set(OPENAL_INSTALL CAPO_INSTALL)
    set(ALSOFT_UTILS OFF)
    set(ALSOFT_EXAMPLES OFF)
    set(ALSOFT_INSTALL OFF)
    set(ALSOFT_INSTALL_CONFIG OFF)
    set(ALSOFT_INSTALL_HRTF_DATA OFF)
    set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
    set(ALSOFT_INSTALL_EXAMPLES OFF)
    set(ALSOFT_INSTALL_UTILS OFF)
    set(LIBTYPE STATIC)
    FetchContent_Declare(
      openal-soft
      GIT_REPOSITORY https://github.com/kcat/openal-soft
      GIT_TAG c1c63a2
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/openal-soft"
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(openal-soft)

    if(NOT MSVC)
      target_compile_options(OpenAL PRIVATE -w)
    endif()
  endif()

  add_library(${PROJECT_NAME}::OpenAL ALIAS OpenAL)
endif()

add_library(${PROJECT_NAME}-openal INTERFACE)
add_library(${PROJECT_NAME}::capo-openal ALIAS ${PROJECT_NAME}-openal)
target_link_libraries(${PROJECT_NAME}-openal INTERFACE $<$<BOOL:${CAPO_USE_OPENAL}>:${PROJECT_NAME}::OpenAL>)
target_compile_definitions(${PROJECT_NAME}-openal INTERFACE
  $<$<BOOL:${CAPO_USE_OPENAL}>:CAPO_USE_OPENAL>
  $<$<BOOL:${CAPO_VALID_IF_INACTIVE}>:CAPO_VALID_IF_INACTIVE>
)

# dr libs
FetchContent_Declare(
  dr_libs
  GIT_REPOSITORY https://github.com/capo-devs/dr_libs
  GIT_TAG 68be913
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/dr_libs"
)
FetchContent_MakeAvailable(dr_libs)

add_library(${PROJECT_NAME}::dr_libs ALIAS dr_libs)
