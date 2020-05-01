@echo off

IF NOT EXIST build mkdir build
pushd build

echo "Rebuilding HeartSensorLib x86 Project files..."
cmake .. -G "Visual Studio 16 2019" -A Win32
IF %ERRORLEVEL% NEQ 0 (
  echo "Error generating HeartSensorLib 32-bit project files"
  goto failure
)

popd
EXIT /B 0

:failure
pause
EXIT /B 1