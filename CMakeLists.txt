project (Snake4D)
cmake_minimum_required(VERSION 3.14)

# Setup Urho3D
set (CMAKE_CXX_STANDARD 17)

set (URHO3D_MONOLITHIC_HEADER ON CACHE BOOL "")
set (BUILD_SHARED_LIBS OFF CACHE BOOL "")
set (URHO3D_GRAPHICS_API OpenGL CACHE STRING "")

set (URHO3D_GLOW OFF CACHE BOOL "")
set (URHO3D_IK OFF CACHE BOOL "")
set (URHO3D_NAVIGATION OFF CACHE BOOL "")
set (URHO3D_NETWORK OFF CACHE BOOL "")
set (URHO3D_PHYSICS OFF CACHE BOOL "")
set (URHO3D_PHYSICS2D OFF CACHE BOOL "")
set (URHO3D_URHO2D OFF CACHE BOOL "")
set (URHO3D_SAMPLES OFF CACHE BOOL "")
set (URHO3D_PROFILER OFF CACHE BOOL "")

set (URHO3D_PLAYER OFF CACHE BOOL "")
set (URHO3D_EDITOR OFF CACHE BOOL "")
set (URHO3D_PLUGIN_LIST "Snake4D" CACHE STRING "")

include (${CMAKE_SOURCE_DIR}/3rdParty/rbfx/cmake/Modules/UrhoCommon.cmake)
add_subdirectory (${CMAKE_SOURCE_DIR}/3rdParty/rbfx)

# Setup Snake4D
add_subdirectory (${CMAKE_SOURCE_DIR}/Source)
add_subdirectory (${CMAKE_SOURCE_DIR}/3rdParty/rbfx/Source/Player)
add_subdirectory (${CMAKE_SOURCE_DIR}/3rdParty/rbfx/Source/Editor)

# Setup deployment
if (WEB OR MOBILE)
    create_pak("${CMAKE_SOURCE_DIR}/bin/Data"     "${CMAKE_CURRENT_BINARY_DIR}/Data.pak")
    create_pak("${CMAKE_SOURCE_DIR}/bin/CoreData" "${CMAKE_CURRENT_BINARY_DIR}/CoreData.pak")
endif ()

if (WEB)
    web_executable(Player)
    package_resources_web(
        FILES        "${CMAKE_CURRENT_BINARY_DIR}/Data.pak"
                     "${CMAKE_CURRENT_BINARY_DIR}/CoreData.pak"
        RELATIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        OUTPUT       "Resources.js"
        INSTALL_TO   "${CMAKE_CURRENT_BINARY_DIR}"
    )
    web_link_resources(Player Resources.js)
    target_link_libraries(Player PRIVATE "--shell-file ${CMAKE_SOURCE_DIR}/3rdParty/rbfx/bin/shell.html")
endif ()
