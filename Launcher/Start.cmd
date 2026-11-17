@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "PYTHONUTF8=1"
set "DRONE_PYTHON="
set "DRONE_PYTHON_ARGS="
set "DRONE_BOOTSTRAP=%~dp0bootstrap.py"
if exist "%~dp0python.path" goto configured_python
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
    echo Alternatively, put its absolute executable path in Launcher/python.path.
    pause
    exit /b 1
)
goto run_python

:configured_python
set /p "DRONE_PYTHON="<"%~dp0python.path"
rem Check the literal value before using it as a command. Delayed expansion
rem keeps punctuation in the file from being interpreted as command syntax.
setlocal EnableDelayedExpansion
if not defined DRONE_PYTHON goto configured_python_error
set "DRONE_UNQUOTED=!DRONE_PYTHON:"=!"
if not "!DRONE_UNQUOTED!"=="!DRONE_PYTHON!" goto configured_python_error
set "DRONE_ABSOLUTE="
if "!DRONE_PYTHON:~1,2!"==":\" set "DRONE_ABSOLUTE=1"
if "!DRONE_PYTHON:~1,2!"==":/" set "DRONE_ABSOLUTE=1"
if "!DRONE_PYTHON:~0,2!"=="\\" set "DRONE_ABSOLUTE=1"
if not defined DRONE_ABSOLUTE goto configured_python_error
if not exist "!DRONE_PYTHON!" goto configured_python_error
endlocal
"%DRONE_PYTHON%" "%DRONE_BOOTSTRAP%" --probe >nul 2>&1
if errorlevel 1 goto configured_python_error
goto run_python

:configured_python_error
echo Launcher/python.path is invalid or its Python probe failed.
echo Put one absolute Python 3.10+ executable path with Tcl/Tk in the file, without quotes.
echo Fix or remove Launcher/python.path to use automatic Python discovery.
pause
exit /b 1

:run_python
"%DRONE_PYTHON%" %DRONE_PYTHON_ARGS% "%DRONE_BOOTSTRAP%" %*
set "DRONE_EXIT=%ERRORLEVEL%"
if not "%DRONE_EXIT%"=="0" pause
if "%DRONE_EXIT%"=="0" if not "%~1"=="" pause
exit /b %DRONE_EXIT%
