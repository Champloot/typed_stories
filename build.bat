@echo off
rem Builds game.exe on Windows with gcc (MinGW-w64): double-click, or Ctrl+B in Sublime.
rem Needs gcc in PATH; curl and tar come with Windows 10 and 11.
setlocal enabledelayedexpansion
cd /d "%~dp0"
set RL=third_party\raylib-5.5_win64_mingw-w64

if not exist "%RL%\lib\libraylib.a" (
    echo Downloading raylib 5.5 for Windows, once...
    if not exist third_party mkdir third_party
    curl -fL -o third_party\raylib-win.zip https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip || goto fail
    tar -xf third_party\raylib-win.zip -C third_party || goto fail
    del third_party\raylib-win.zip
)

rem every source file except the terminal version
set SRC=
for %%f in (src\*.c) do if /I not "%%~nxf"=="ui_tty.c" set SRC=!SRC! %%f

echo Building game.exe...
gcc -O2 -Wall -Wextra -I%RL%\include -o game.exe %SRC% %RL%\lib\libraylib.a -lopengl32 -lgdi32 -lwinmm -static -mwindows || goto fail
echo Done: game.exe
exit /b 0

:fail
echo Build failed.
exit /b 1
