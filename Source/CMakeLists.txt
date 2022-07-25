file (GLOB SOURCE_FILES *.h *.cpp)

set (TARGET_NAME Snake4D)
add_library (${TARGET_NAME} ${SOURCE_FILES})
define_static_plugin (${TARGET_NAME} "Snake4D.Main")
target_link_libraries (${TARGET_NAME} PRIVATE Urho3D)
