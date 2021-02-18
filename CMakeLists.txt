cmake_minimum_required(VERSION 3.7)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake 
        CACHE STRING "Vcpkg toolchain file")

project(raytracing C CXX)

find_package(glm CONFIG REQUIRED)
find_package(OpenCL REQUIRED)
find_package(Assimp CONFIG REQUIRED)
find_package(GLUT REQUIRED)
find_package(GLEW REQUIRED)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/RTAPI/RayTracing.cl
          ${CMAKE_CURRENT_SOURCE_DIR}/RTAPI/KDstruct.h
          ${CMAKE_CURRENT_SOURCE_DIR}/RTAPI/RTstruct.h
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(GLOB_RECURSE sources 
        CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/RTAPI/*.cpp)
add_executable(${CMAKE_PROJECT_NAME} ${sources})
target_link_libraries(${CMAKE_PROJECT_NAME}
    PRIVATE
        glm
        OpenCL::OpenCL
        assimp::assimp
        GLUT::GLUT
        GLEW::GLEW)


