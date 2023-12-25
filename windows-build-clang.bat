@echo off
setlocal enabledelayedexpansion

set top=%cd%

if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
	echo ERROR: cannot find vswhere.exe
	pause
	exit
)

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property catalog_productLineVersion`) do (
  set version=%%i
)
if %version%==2022 (
	echo Found VS2022.
) else (
	echo ERROR: could not find Visual Studio 2022.
	pause
	exit
)

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
  set msbuild=%%i
)
if not exist "%msbuild%" (
	echo ERROR: cannot find msbuild.exe
	pause
	exit
)

cd scripts
call .\windows-prebuild.bat %version%-clang
cd %top%
"%msbuild%" /t:Build /m /p:PreferredToolArchitecture=x64 /p:Configuration=Release;Platform=x64 %CD%\build-vs%version%-clang\CoolVLViewer.sln

echo.
echo Please, be aware that VS%version%-clang viewer builds are currently unsupported.
echo.
echo Compilation finished (and hopefully successful). Press a key to exit.

pause
