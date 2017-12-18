cmake_minimum_required(VERSION 2.8.4)
project("radiance")

if(NOT ${CMAKE_VERSION} VERSION_LESS "2.8.11")
  cmake_policy(SET CMP0020 NEW)
  if(NOT ${CMAKE_VERSION} VERSION_LESS "3.0")
    cmake_policy(SET CMP0042 NEW)
    if(NOT ${CMAKE_VERSION} VERSION_LESS "3.1")
      cmake_policy(SET CMP0054 NEW)
    endif()
  endif()
endif()

enable_testing()
include(CTest)

option(BUILD_SHARED_LIBS "Build radiance using shared libraries?" OFF)
option(BUILD_HEADLESS "Build radiance without any GUI components?" OFF)
option(BUILD_PABOPTO_UTILS "Build PABOpto Utilities?" OFF)

option(CPACK_NREL_SUPPORT "Enable NREL/OpenStudio release tagging?" OFF)

if( CPACK_NREL_SUPPORT )
  # find git so we can query for the current git tag
  find_program(git git)
  if (NOT git)
    message(ERROR "Please specify the path to the git executable")
  endif()

  # call git log to get output containing the tag
  execute_process(
    COMMAND ${git} describe --tag --exact-match
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE git_tag_output)

  # create & install the source version .txt file
  file(WRITE "NREL_ver.txt" "Radiance version ${git_tag_output}")
  install(FILES NREL_ver.txt DESTINATION bin/..)
endif()
 
if(NOT WIN32)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 1)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/man/ DESTINATION man)
  set(qt_plugin_term "PLUGIN")
endif()

if (WIN32)
  set(CPACK_NSIS_MODIFY_PATH 1)
  set(qt_plugin_term "LIBRARY")

  # Set RAYPATH on install
  set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
    "WriteRegExpandStr HKLM \\\"SYSTEM\\\\CurrentControlSet\\\\Control\\\\Session Manager\\\\Environment\\\" RAYPATH '\\\$INSTDIR\\\\lib\\\;.'"
  )

  # Unset RAYPATH on uninstall
  set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
    "DeleteRegValue HKLM \\\"SYSTEM\\\\CurrentControlSet\\\\Control\\\\Session Manager\\\\Environment\\\" RAYPATH"
  )
endif()

if(UNIX)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local/radiance")
  add_definitions(-Dlinux -D_FILE_OFFSET_BITS=64 -DNOSTEREO)
else()
  set(CPACK_PACKAGING_INSTALL_PREFIX "/")
endif()

if(APPLE)
  add_definitions(-DBSD -DNOSTEREO -Dfreebsd)
  include_directories(/usr/X11R6/include)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.6")
endif()

IF( BUILD_PACKAGE )
  INCLUDE(radianceCPack.cmake )
ENDIF( BUILD_PACKAGE )

set(CPACK_PACKAGE_VERSION_MAJOR 5)
set(CPACK_PACKAGE_VERSION_MINOR 1)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Radiance")
SET(CPACK_RESOURCE_FILE_WELCOME "${radiance_SOURCE_DIR}/Welcome.txt")
SET(CPACK_RESOURCE_FILE_README "${radiance_SOURCE_DIR}/README.txt")
SET(CPACK_RESOURCE_FILE_LICENSE "${radiance_SOURCE_DIR}/License.txt") 

include(CPack)

if(NOT BUILD_HEADLESS)
  find_package(Qt5Widgets)
  find_package(X11)
endif()

SET(RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

if(WIN32 AND "${CMAKE_C_COMPILER_ID}" MATCHES "^(Intel)$")
  set(_INTEL_WINDOWS 1)
endif()
if(MSVC OR _INTEL_WINDOWS)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
endif()
if(WIN32)
  add_definitions(-DEZXML_NOMMAP -Dfseeko=fseek -Dstrcasecmp=_stricmp)
endif()

add_subdirectory("src")

option(BUILD_TESTING "Enable testing?" ON)
if(BUILD_TESTING)
  add_subdirectory(resources/cmake_tests)
endif()
  
include(InstallRequiredSystemLibraries)
add_subdirectory(InstallRules)
