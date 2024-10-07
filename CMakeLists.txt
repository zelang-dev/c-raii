cmake_minimum_required(VERSION 2.8...3.14)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

project(
    raii
    VERSION 1.1.1
    DESCRIPTION "An general RAII implementation for C, with Defer, C11 Atomics/Threads with Thread Pool, and custom malloc/heap allocation."
    HOMEPAGE_URL "https://zelang-dev.github.io/c-raii/"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 90)

include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(FetchContent)
include(CTest)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_CONFIGURATION_TYPES=Debug;Release)
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/built")

option(BUILD_SHARED_LIBS    "Build the library as a shared (dynamically-linked) " OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(WIN32)
    if($ENV{Platform} STREQUAL x86)
        message("Building Windows x86-32bit")
        add_definitions(-D_WIN32_PLATFORM_X86=1)
    endif()
endif()

file(GLOB raii_files
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

if(BUILD_SHARED_LIBS)
    if(WIN32)
        add_definitions(-DMMAN_LIBRARY_DLL)
    endif()
    add_library(raii SHARED ${raii_files})
else()
    if(WIN32)
        add_definitions(-DPTW32_STATIC_LIB)
    endif()
    add_library(raii STATIC ${raii_files})
endif()

if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -D USE_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer -Wno-return-type")
else()
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D USE_DEBUG ")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions("/wd4244 /wd4267 /wd4033 /wd4715 /wd4996")
endif()

FetchContent_Declare(threads
 URL https://github.com/zelang-dev/cthread/archive/refs/tags/v4.0.2.0.zip
 URL_MD5 41cb53afb6ffb75b82dcc15bdcb2abf5
)
FetchContent_MakeAvailable(threads)

target_link_libraries(raii PUBLIC cthread)
set_property(TARGET raii PROPERTY POSITION_INDEPENDENT_CODE True)

target_include_directories(raii PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

set(_fmt TGZ)
if(WIN32)
  set(_fmt ZIP)
endif()

set(CPACK_GENERATOR ${_fmt})
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_NUGET_COMPONENT_INSTALL ON)
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_SOURCE_IGNORE_FILES
	"~$"
	"\\\\.svn/"
	"/CMakeFiles/"
	"/CMakeCache*"
	"/cmake_install\\\\.cmake$"
	"/cmake_uninstall\\\\.cmake$"
	"^_CPack_.*/"
	"/CPack*"
	"\\\\.o$"
	"/m4/"
	"/build/"
	"/built/"
)

set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    CACHE PATH "Location of header files" )

set(SYSCONFIG_INSTALL_DIR ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    CACHE PATH "Location of configuration files" )

configure_package_config_file(raiiConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/raiiConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/raiiConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/raiiConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/raiiConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME} )

set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Package")
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME})
set(CPACK_PACKAGE_INSTALL_DIRECTORY raii)

set(CPACK_PACKAGE_CONTACT "lstubbs@zelang.dev")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lawrence Stubbs <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
SET(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "Raii C library")
SET(CPACK_COMPONENT_LIBRARIES_DESCRIPTION "An general RAII implementation and memory/heap allocation for C.")

set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_URL "https://zelang-dev.github.io/c-raii/")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "noarch")

set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
install(
    TARGETS ${PROJECT_NAME} cthread
    EXPORT "${PROJECT_NAME}Targets"
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT "${PROJECT_NAME}Targets"
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
install(TARGETS ${PROJECT_NAME} DESTINATION lib)
if(WIN32)
    install(DIRECTORY include/ DESTINATION include)
else()
    install(FILES
 include/catomic.h
 include/cthread.h
 include/exception.h
 include/raii.h
 include/rpmalloc.h
 include/rtypes.h
    DESTINATION include)
endif()

set(CPACK_INSTALL_CMAKE_CONFIGURATIONS Release)
include(CPack)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(BUILD_EXAMPLES   "whether or not examples should be built" ON)
    option(BUILD_TESTS      "whether or not tests should be built" ON)

    if(BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
    if(BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
