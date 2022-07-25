pushd ..\Snake4D-web
del Source\*.pak
cmake --build . --config Debug -j12

del /Q Package\*.*
md Package
copy Resources.* Package\Resources.*
copy 3rdParty\rbfx\Source\Player\Player.* Package\Player.*
ren Package\Player.html index.html
popd
