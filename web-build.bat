pushd ..\Snake4D-web
del Source\*.pak
cmake --build . --config Debug -j12

del /Q Package\*.*
md Package
copy Source\Resources.* Package\Resources.*
copy Source\Snake4D.* Package\Snake4D.*
ren Package\Snake4D.html index.html
popd
