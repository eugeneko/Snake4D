file (GLOB SOURCE_FILES *.h *.cpp)

set (TARGET_NAME Snake4D)
add_executable(${TARGET_NAME} WIN32 ${SOURCE_FILES})
target_link_libraries (${TARGET_NAME} PRIVATE Urho3D)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 17)

# Configure deploy
if (WEB OR MOBILE)
    create_pak("${CMAKE_SOURCE_DIR}/bin/Data"     "${CMAKE_CURRENT_BINARY_DIR}/Data.pak")
    create_pak("${CMAKE_SOURCE_DIR}/bin/CoreData" "${CMAKE_CURRENT_BINARY_DIR}/CoreData.pak")
endif ()

if (WEB)
    web_executable(${TARGET_NAME})
    package_resources_web(
        FILES        "${CMAKE_CURRENT_BINARY_DIR}/Data.pak"
                     "${CMAKE_CURRENT_BINARY_DIR}/CoreData.pak"
        RELATIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}"
        OUTPUT       "Resources.js"
        INSTALL_TO   "${CMAKE_CURRENT_BINARY_DIR}"
    )
    web_link_resources(${TARGET_NAME} Resources.js)
    target_link_libraries(${TARGET_NAME} PRIVATE "--shell-file ${CMAKE_SOURCE_DIR}/3rdParty/rbfx/bin/shell.html")
endif ()
