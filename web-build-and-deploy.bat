cd ..\Snake4D-web
del Source\*.pak
cmake --build . --config Debug -j12
del /q %GITHUB_PAGE_ROOT_PATH%\Snake4D\*.*
copy Source\Resources.js* %GITHUB_PAGE_ROOT_PATH%\Snake4D\
copy Source\Snake4D.* %GITHUB_PAGE_ROOT_PATH%\Snake4D\
cd %GITHUB_PAGE_ROOT_PATH%
git add .
git commit -m "Update Snake4D."
git push origin master
