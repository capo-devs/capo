# ktl
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
	ktl
	GIT_REPOSITORY https://github.com/karnkaul/ktl
	GIT_TAG 75c233c # manually update this to keep commits pinned
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(ktl)

# dr_wav
add_library(dr_libs INTERFACE)
target_include_directories(dr_libs INTERFACE dr_libs)

set(ALSOFT_UTILS OFF)
set(ALSOFT_EXAMPLES OFF)
set(ALSOFT_INSTALL OFF)
set(ALSOFT_INSTALL_CONFIG OFF)
set(ALSOFT_INSTALL_HRTF_DATA OFF)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
set(ALSOFT_INSTALL_EXAMPLES OFF)
set(ALSOFT_INSTALL_UTILS OFF)
add_subdirectory(openal-soft)

target_link_libraries(${PROJECT_NAME} PUBLIC ktl::ktl)
