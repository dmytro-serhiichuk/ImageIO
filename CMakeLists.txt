cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_DEBUG_POSTFIX d)

project(ImageIO VERSION 0.1.0 LANGUAGES C CXX)

find_package(libjpeg-turbo REQUIRED)
find_package(libraw REQUIRED)
find_package(TIFF REQUIRED)
find_package(PNG REQUIRED)
find_package(lcms2 REQUIRED)

file(GLOB SOURCES "./src/*.cpp")

include_directories("./include")
add_executable(ImageIO ${SOURCES})
target_link_libraries(ImageIO PRIVATE
    libjpeg-turbo::turbojpeg
    libraw::raw_r
    TIFF::TIFF
    PNG::PNG
    lcms2::lcms2
)

# add_library(ImageIO_static STATIC ${SOURCES})
# set_target_properties(ImageIO_static PROPERTIES 
#     OUTPUT_NAME "ImageIO-static"
#     POSITION_INDEPENDENT_CODE ON
#     DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}" 
# )
# add_library(ImageIO_shared SHARED ${SOURCES})
# set_target_properties(ImageIO_shared PROPERTIES 
#     OUTPUT_NAME "ImageIO"
#     DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
#     WINDOWS_EXPORT_ALL_SYMBOLS ON # Needed by clang as well
# )

# foreach(target ImageIO_static ImageIO_shared)
#     target_include_directories(${target} 
#       PUBLIC
#         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#         $<INSTALL_INTERFACE:include>
#       PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/src
#     )
    
#     target_link_libraries(${target} PRIVATE
#         libjpeg-turbo::turbojpeg
#         PNG::PNG
#         libraw::raw_r
#         TIFF::TIFF
#     )
    
#     set_target_properties(${target} PROPERTIES
#         VERSION ${PROJECT_VERSION}
#         SOVERSION ${PROJECT_VERSION_MAJOR}
#         CXX_STANDARD 11
#     )
# endforeach()

# install(TARGETS ImageIO_static ImageIO_shared
#   EXPORT ImageIOTargets
#   LIBRARY DESTINATION lib
#   ARCHIVE DESTINATION lib
#   RUNTIME DESTINATION bin
#   INCLUDES DESTINATION include
# )

# install(DIRECTORY include/
#   DESTINATION include/ImageIO
# )

# install(EXPORT ImageIOTargets
#   FILE ImageIOTargets.cmake
#   NAMESPACE ImageIO::
#   DESTINATION lib/cmake/ImageIO
# )

# include(CMakePackageConfigHelpers)
# configure_package_config_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ImageIOConfig.cmake.in
#   ${CMAKE_CURRENT_BINARY_DIR}/ImageIOConfig.cmake
#   INSTALL_DESTINATION lib/cmake/ImageIO
# )
# write_basic_package_version_file(
#   ${CMAKE_CURRENT_BINARY_DIR}/ImageIOConfigVersion.cmake
#   VERSION ${PROJECT_VERSION}
#   COMPATIBILITY SameMajorVersion
# )

# install(FILES
#   ${CMAKE_CURRENT_BINARY_DIR}/ImageIOConfig.cmake
#   ${CMAKE_CURRENT_BINARY_DIR}/ImageIOConfigVersion.cmake
#   DESTINATION lib/cmake/ImageIO
# )