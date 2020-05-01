@echo off
setlocal

::Clean up the old HeartSensorLib build folder
IF EXIST build (
del /f /s /q build > nul
rmdir /s /q build
)

::Clean up the old HeartSensorLib deps folder
IF EXIST deps (
del /f /s /q deps > nul
rmdir /s /q deps
)

:: Add MSVC build tools to the path
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
IF %ERRORLEVEL% NEQ 0 (
  echo "Unable to initialize 32-bit visual studio build environment"
  goto failure
)

:: Generate the project files for HeartSensorLib
call GenerateProjectFiles_Win32.bat || goto failure
EXIT /B 0

:failure
pause
EXIT /B 1