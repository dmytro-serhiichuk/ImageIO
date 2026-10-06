cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_DEBUG_POSTFIX d)

project(ImageIO VERSION 1.0.0 LANGUAGES C CXX)

find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(libraw        CONFIG REQUIRED)
find_package(TIFF                 REQUIRED)
find_package(PNG           CONFIG REQUIRED)
find_package(lcms2         CONFIG REQUIRED)

file(GLOB SOURCES CONFIGURE_DEPENDS "./src/*.cpp")

# include_directories("./include")
# add_executable(ImageIO ${SOURCES})
# target_link_libraries(ImageIO PRIVATE
#     libjpeg-turbo::turbojpeg
#     libraw::raw_r
#     TIFF::TIFF
#     PNG::PNG
#     lcms2::lcms2
# )

set(target_static ${PROJECT_NAME}_static)
set(target_shared ${PROJECT_NAME}_shared)
set(target_default ${PROJECT_NAME})

add_library(${target_static} STATIC ${SOURCES})
set_target_properties(${target_static} PROPERTIES 
    OUTPUT_NAME   "${PROJECT_NAME}-static"
    POSITION_INDEPENDENT_CODE ON
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    CXX_STANDARD  11
)
add_library(${target_shared} SHARED ${SOURCES})
set_target_properties(${target_shared} PROPERTIES 
    OUTPUT_NAME   "${PROJECT_NAME}"
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    VERSION        ${PROJECT_VERSION}
    SOVERSION      ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD   11
)

foreach(target ${target_static} ${target_shared})
    target_include_directories(${target} 
      PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(${target} PRIVATE
        libjpeg-turbo::turbojpeg
        PNG::PNG
        libraw::raw_r
        TIFF::TIFF
        lcms2::lcms2
    )
endforeach()

# INTERFACE: ImageIO::ImageIO → shared
add_library(${target_default} INTERFACE)
target_link_libraries(${target_default} INTERFACE ${target_shared})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${target_default})

install(TARGETS ${target_default} ${target_static} ${target_shared}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include/)

install(EXPORT      ${PROJECT_NAME}Targets
        FILE        ${PROJECT_NAME}Targets.cmake
        NAMESPACE   ${PROJECT_NAME}::
        DESTINATION lib/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION lib/cmake/${PROJECT_NAME}
)