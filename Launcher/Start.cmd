@echo off
setlocal
set "PYTHONUTF8=1"
set "DRONE_PYTHON="
set "DRONE_PYTHON_ARGS="
where py >nul 2>&1
if not errorlevel 1 (
    py -3 "%~dp0bootstrap.py" --probe >nul 2>&1
    if not errorlevel 1 (
        set "DRONE_PYTHON=py"
        set "DRONE_PYTHON_ARGS=-3"
    )
)
if not defined DRONE_PYTHON (
    where python >nul 2>&1
    if not errorlevel 1 (
        python "%~dp0bootstrap.py" --probe >nul 2>&1
        if not errorlevel 1 set "DRONE_PYTHON=python"
    )
)
if not defined DRONE_PYTHON (
    echo Install Python 3.10+ with Tcl/Tk and Python Launcher or PATH support.
    pause
    exit /b 1
)
%DRONE_PYTHON% %DRONE_PYTHON_ARGS% "%~dp0bootstrap.py" %*
set "DRONE_EXIT=%ERRORLEVEL%"
if not "%DRONE_EXIT%"=="0" pause
if "%DRONE_EXIT%"=="0" if not "%~1"=="" pause
exit /b %DRONE_EXIT%
