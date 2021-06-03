pushd ..\Snake4D-web
del /q %GITHUB_PAGE_ROOT_PATH%\Snake4D\*.*
copy Source\Resources.js* %GITHUB_PAGE_ROOT_PATH%\Snake4D\
copy Source\Snake4D.* %GITHUB_PAGE_ROOT_PATH%\Snake4D\
pushd %GITHUB_PAGE_ROOT_PATH%
git add .
git commit -m "Update Snake4D."
git push origin master
popd
popd
