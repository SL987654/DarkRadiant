@echo off
rem Launch this file in the boost/ folder.

rem Check if we're in the correct folder
if not exist libs goto :error
if not exist boost goto :error

mkdir stage

cd libs\filesystem\build
bjam --toolset=msvc link=static threading=multi release stage
bjam --toolset=msvc link=static threading=multi debug stage
copy stage\*.lib ..\..\..\stage

cd ..\..\..\libs\system\build
bjam --toolset=msvc link=static threading=multi release stage
bjam --toolset=msvc link=static threading=multi debug stage
copy stage\*.lib ..\..\..\stage

cd ..\..\..\libs\python\build
bjam --toolset=msvc link=static threading=multi release stage
bjam --toolset=msvc link=static threading=multi debug stage
copy stage\*.lib ..\..\..\stage

cd ..\..\..\libs\regex\build
bjam --toolset=msvc link=static threading=multi release stage
bjam --toolset=msvc link=static threading=multi debug stage
copy stage\*.lib ..\..\..\stage

rem cd ..\..\..\libs\signals\build
rem bjam --toolset=msvc link=static threading=multi release stage
rem bjam --toolset=msvc link=static threading=multi debug stage
rem copy stage\*.lib ..\..\..\stage

cd ..\..\..\stage
start .

goto :success

:error
echo Please launch this file in the boost folder you downloaded and extracted from sourceforge.
echo Note that you need to have the path to bjam.exe in your PATH environment variable.
echo 
echo Example: 
echo   cd c:\Downloads\boost_1_55_0\
echo   c:\Games\DarkRadiant\tools\scripts\build_boost_libs.cmd
goto :eof

:success
echo Successfully built the libraries, please copy them to the w32deps/boost/libs/ folder now.