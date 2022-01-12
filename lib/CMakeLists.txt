# dr_wav, dr_flac, dr_mp3
add_subdirectory(dr_libs)

if(CAPO_USE_OPENAL AND NOT TARGET OpenAL)
  set(ALSOFT_UTILS OFF)
  set(ALSOFT_EXAMPLES OFF)
  set(ALSOFT_INSTALL OFF)
  set(ALSOFT_INSTALL_CONFIG OFF)
  set(ALSOFT_INSTALL_HRTF_DATA OFF)
  set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
  set(ALSOFT_INSTALL_EXAMPLES OFF)
  set(ALSOFT_INSTALL_UTILS OFF)
  set(LIBTYPE "STATIC")
  add_subdirectory(openal-soft)
  if(NOT MSVC)
    target_compile_options(OpenAL PRIVATE -w)
  endif()
endif()

add_library(${PROJECT_NAME}-openal INTERFACE)
add_library(${PROJECT_NAME}::openal ALIAS ${PROJECT_NAME}-openal)
target_link_libraries(${PROJECT_NAME}-openal INTERFACE $<$<BOOL:${CAPO_USE_OPENAL}>:OpenAL>)
target_include_directories(${PROJECT_NAME}-openal INTERFACE openal-soft/include)
target_compile_definitions(${PROJECT_NAME}-openal INTERFACE
  $<$<BOOL:${CAPO_USE_OPENAL}>:CAPO_USE_OPENAL>
  $<$<BOOL:${CAPO_VALID_IF_INACTIVE}>:CAPO_VALID_IF_INACTIVE>
)
