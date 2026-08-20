@echo off
rem Penumbra VR installer - double-click to install.
rem Forwards its arguments to Install-PenumbraVR.ps1; see the readme for
rem options such as -InstallRoot and -Restore.
setlocal
cd /d "%~dp0"
if not exist "%~dp0Install-PenumbraVR.ps1" (
    echo Install-PenumbraVR.ps1 is missing next to this file.
    echo Extract the complete package first and run from the extracted folder.
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-PenumbraVR.ps1" %*
set "EXITCODE=%ERRORLEVEL%"
if not "%EXITCODE%"=="0" (
    echo.
    echo Installation failed with exit code %EXITCODE%. See the message above.
    pause
    exit /b %EXITCODE%
)
echo.
echo Done. Start SteamVR and launch Penumbra: Overture with Steam's Play button.
pause