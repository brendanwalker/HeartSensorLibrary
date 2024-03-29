@echo off
setlocal

:: Cleanup any prior distribution folders
IF EXIST dist (
  del /f /s /q dist > nul
  rmdir /s /q dist
)

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

:: Compile and deploy the RELEASE|x64 build of HeartSensorLib to the distribution folder
pushd build\
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
MSBuild.exe CREATE_INSTALLER.vcxproj /p:configuration=RELEASE /p:Platform="x64" /t:Build
IF %ERRORLEVEL% NEQ 0 (
  echo "Error building HeartSensorLib RELEASE|x64 installer in distribution folder!"
  goto failure
) 
popd

:: Find the distribution folder
if exist "dist/version.txt" (
    set /p DISTRIBUTION_LABEL=<"dist/version.txt"
    echo "Found distribution folder for version: %DISTRIBUTION_LABEL%"
) else (
    echo "Failed to find the distribution folder generated from the builds!"
    goto failure
)

:: Create the distribution zip from the Release|x64 folder
set "DISTRIBUTION_FOLDER=dist\Win64\Release"
set "DISTRIBUTION_ZIP=%DISTRIBUTION_LABEL%.zip"
echo "Creating distribution zip (%DISTRIBUTION_ZIP%) from: %DISTRIBUTION_FOLDER%"
powershell -nologo -noprofile -command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::CreateFromDirectory('%DISTRIBUTION_FOLDER%', '%DISTRIBUTION_ZIP%'); }"
IF %ERRORLEVEL% NEQ 0 (
  echo "Failed to generate the distribution zip!"
  goto failure
)

echo "Successfully created distribution zip: %DISTRIBUTION_ZIP%"
pause
EXIT /B 0

:failure
pause
EXIT /B 1