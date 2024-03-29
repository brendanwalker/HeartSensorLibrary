@echo off
setlocal

:: Cleanup any prior distribution folders
IF EXIST dist (
  del /f /s /q dist > nul
  rmdir /s /q dist
)

:: Generate the 32-bit project files and dependencies for HeartSensorLib
call InitialSetup_Win32.bat || goto failure
IF %ERRORLEVEL% NEQ 0 (
  echo "Error setting up 32-bit HeartSensorLib project"
  goto failure
)

:: Add MSVC build tools to the path
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
IF %ERRORLEVEL% NEQ 0 (
  echo "Unable to initialize 32-bit visual studio build environment"
  goto failure
)

:: Compile and install the DEBUG|Win32 build of HeartSensorLib to the distribution folder
pushd build\
echo "Building HeartSensorLib DEBUG|Win32..."
MSBuild.exe HeartSensorLib.sln /p:configuration=DEBUG /p:Platform="Win32" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error building HeartSensorLib DEBUG|Win32!"
  goto failure
) 
MSBuild.exe INSTALL.vcxproj /p:configuration=DEBUG /p:Platform="Win32" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error deploying HeartSensorLib DEBUG|Win32 to distribution folder!"
  goto failure
) 

:: Compile and install the RELEASE|Win32 build of HeartSensorLib to the distribution folder
MSBuild.exe HeartSensorLib.sln /p:configuration=RELEASE /p:Platform="Win32" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error building HeartSensorLib RELEASE|Win32!"
  goto failure
) 
MSBuild.exe INSTALL.vcxproj /p:configuration=RELEASE /p:Platform="Win32" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error deploying HeartSensorLib RELEASE|Win32 to distribution folder!"
  goto failure
) 
popd

:: Generate the 64-bit project files and dependencies for HeartSensorLib
call InitialSetup_X64.bat || goto failure
IF %ERRORLEVEL% NEQ 0 (
  echo "Error setting up 64-bit HeartSensorLib project"
  goto failure
)

:: Add MSVC build tools to the path
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64
IF %ERRORLEVEL% NEQ 0 (
  echo "Unable to initialize 64-bit visual studio build environment"
  goto failure
)

:: Compile and install the DEBUG|x64 build of HeartSensorLib to the distribution folder
pushd build\
echo "Building HeartSensorLib DEBUG|x64..."
MSBuild.exe HeartSensorLib.sln /p:configuration=DEBUG /p:Platform="x64" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error building HeartSensorLib DEBUG|x64!"
  goto failure
) 
MSBuild.exe INSTALL.vcxproj /p:configuration=DEBUG /p:Platform="x64" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error deploying HeartSensorLib DEBUG|x64 to distribution folder!"
  goto failure
) 

:: Compile and install the RELEASE|x64 build of HeartSensorLib to the distribution folder
echo "Building HeartSensorLib RELEASE|x64..."
MSBuild.exe opencv.vcxproj /p:configuration=RELEASE /p:Platform="x64" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error building OpenCV RELEASE|x64!"
  goto failure
) 
MSBuild.exe HeartSensorLib.sln /p:configuration=RELEASE /p:Platform="x64" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error building HeartSensorLib RELEASE|x64!"
  goto failure
) 
MSBuild.exe INSTALL.vcxproj /p:configuration=RELEASE /p:Platform="x64" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error deploying HeartSensorLib RELEASE|x64 to distribution folder!"
  goto failure
) 
popd

echo "Successfully Built All Configurations!"
pause
EXIT /B 0

:failure
pause
EXIT /B 1