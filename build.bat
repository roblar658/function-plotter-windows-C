@echo off
echo Compiling C Interactive Function Plotter...
gcc -O2 -Wall -Wextra main.c -o plotter.exe -lgdi32 -luser32 -mwindows
if %ERRORLEVEL% EQU 0 (
    echo Build successful: plotter.exe
) else (
    echo Build failed!
)

