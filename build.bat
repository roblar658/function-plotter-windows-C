@echo off
echo Compiling C Interactive Function Plotter with Excel Export...
gcc -O2 -Wall -Wextra main.c -o plotter.exe -lgdi32 -luser32 -lcomdlg32 -lshell32 -lole32 -luuid -mwindows
if %ERRORLEVEL% EQU 0 (
    echo Build successful: plotter.exe
) else (
    echo Build failed!
)
