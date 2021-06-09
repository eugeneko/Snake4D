if not exist ..\Snake4D-web\* mkdir ..\Snake4D-web
pushd ..\Snake4D-web
rem EMSCRIPTEN_ROOT_PATH=path/to/emsdk/upstream/emscripten
cmake -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=3rdParty/rbfx/cmake/Toolchains/Emscripten.cmake -DCMAKE_INSTALL_PREFIX=SDK -DCMAKE_BUILD_TYPE=RelWithDebInfo -DURHO3D_PACKAGING=ON -DEMSCRIPTEN_ROOT_PATH=%EMSCRIPTEN_ROOT_PATH% -S ../Snake4D -B ../Snake4D-web
popd
