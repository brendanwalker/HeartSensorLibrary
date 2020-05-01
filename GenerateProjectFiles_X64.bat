@echo off

IF NOT EXIST build mkdir build
pushd build

echo "Rebuilding HeartSensorLib x64 Project files..."
cmake .. -G "Visual Studio 16 2019" -A x64
IF %ERRORLEVEL% NEQ 0 (
  echo "Error generating HeartSensorLib 64-bit project files"
  goto failure
)

popd
EXIT /B 0

:failure
pause
EXIT /B 1