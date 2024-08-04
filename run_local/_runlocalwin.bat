@echo off
SET BUILD_TYPE=Release

:: Create and navigate to the build directory
if not exist build mkdir build
cd build

:: Configure the project
cmake -S .. -B . -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

:: Build the project
cmake --build . --config %BUILD_TYPE%

echo Build completed successfully.
pause
