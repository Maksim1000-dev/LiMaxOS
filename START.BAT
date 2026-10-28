@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "ROOT=%~dp0"
set "NASM=%ROOT%nasm\nasm.exe"
set "GCC=%ROOT%GCC\bin\gcc.exe"
set "QEMU=%ROOT%qemu\qemu-system-i386.exe"

echo ============================================
echo   LiMaxOS build system
echo ============================================
echo.

if not exist "%NASM%" goto missing_tools
if not exist "%GCC%" goto missing_tools
if not exist "%QEMU%" goto missing_tools

if not exist "%ROOT%build" mkdir "%ROOT%build"

echo [1/4] Assembling boot sector...
"%NASM%" -f bin "%ROOT%boot\boot.asm" -o "%ROOT%build\boot.bin"
if errorlevel 1 goto build_failed

echo [2/4] Assembling boot manager...
"%NASM%" -f bin "%ROOT%boot\loader.asm" -o "%ROOT%build\loader.bin"
if errorlevel 1 goto build_failed

echo [3/4] Assembling kernel...
"%NASM%" -f bin "%ROOT%kernel\kernel.asm" -o "%ROOT%build\kernel.bin" -l "%ROOT%build\kernel.lst"
if errorlevel 1 goto build_failed

echo [4/4] Building 1.44 MB floppy image...
"%GCC%" -O2 -s -static -o "%ROOT%build\mkimg.exe" "%ROOT%tools\mkimg.c"
if errorlevel 1 goto build_failed
"%ROOT%build\mkimg.exe" "%ROOT%build\LiMaxOS.img" "%ROOT%build\boot.bin" "%ROOT%build\loader.bin" "%ROOT%build\kernel.bin"
if errorlevel 1 goto build_failed

echo.
echo Build OK: build\LiMaxOS.img

if /I "%~1"=="build" goto done

echo.
echo Starting QEMU...
echo.
set "GDK_SCALE=1"
set "GDK_DPI_SCALE=1"
if not exist "%ROOT%tests" mkdir "%ROOT%tests"
if exist "%ROOT%tests\winsize.log" del "%ROOT%tests\winsize.log" >nul 2>&1
start "" /b powershell -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "%ROOT%tools\winsize.ps1"
"%QEMU%" -name "LiMaxOS" -machine pc -m 16M -drive "file=%ROOT%build\LiMaxOS.img,format=raw,if=floppy" -boot a -rtc base=localtime -vga std -serial tcp:127.0.0.1:4444,server,nowait -display gtk,zoom-to-fit=on,show-menubar=on,keep-aspect-ratio=off,gl=off
if errorlevel 1 goto qemu_failed
goto done

:missing_tools
echo [ERROR] NASM, GCC or QEMU was not found in the project folder.
echo Expected:
echo   %NASM%
echo   %GCC%
echo   %QEMU%
goto failed

:build_failed
echo.
echo [ERROR] Build failed. Read the message above.
goto failed

:qemu_failed
echo.
echo [ERROR] QEMU exited with an error.
goto failed

:failed
echo.
pause
exit /b 1

:done
echo.
echo Done.
if /I not "%~1"=="nopause" pause
exit /b 0
