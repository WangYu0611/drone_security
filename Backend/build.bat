@echo off
setlocal EnableDelayedExpansion

set "ENV_FILE=%~dp0..\.env.local"

if exist "%ENV_FILE%" (
    for /f "usebackq tokens=1* delims==" %%A in ('type "%ENV_FILE%"') do (
        if not "%%A"=="" if not "%%A:~0,1"=="#" (
            set "%%A=%%B"
        )
    )
)

set "VS_BAT=%UE5DRONE_VS_DEV_CMD%"
if not defined VS_BAT (
    set "VS_WHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%VS_WHERE%" set "VS_WHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VS_WHERE%" (
        for /f "delims=" %%i in ('"%VS_WHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do (
            set "VS_PATH=%%i"
            set "VS_BAT=%VS_PATH%\Common7\Tools\VsDevCmd.bat"
        )
    )
)

if not defined VS_BAT (
    if exist "D:\Software\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" set "VS_BAT=D:\Software\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    if not exist "%VS_BAT%" if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" set "VS_BAT=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    if not exist "%VS_BAT%" if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" set "VS_BAT=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
)

if not exist "%VS_BAT%" (
    echo [ERROR] VsDevCmd.bat not found. Install Visual Studio 2022 Desktop C++ workload.
    exit /b 1
)

echo [INFO] VS batch: "%VS_BAT%"

call "%VS_BAT%" -arch=x64 2>nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize VS build environment.
    exit /b 1
)

set "VCPKG_TOOLCHAIN=%UE5DRONE_VCPKG_TOOLCHAIN%"
if not defined VCPKG_TOOLCHAIN if defined VS_PATH if exist "%VS_PATH%\VC\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_TOOLCHAIN=%VS_PATH%\VC\vcpkg\scripts\buildsystems\vcpkg.cmake"
if not defined VCPKG_TOOLCHAIN if exist "D:\Software\Microsoft Visual Studio\2022\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_TOOLCHAIN=D:\Software\Microsoft Visual Studio\2022\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake"

if /I "%UE5DRONE_SKIP_VCPKG%"=="1" set "VCPKG_TOOLCHAIN="

if defined VCPKG_TOOLCHAIN (
    echo [INFO] Vcpkg toolchain: %VCPKG_TOOLCHAIN%
) else (
    echo [WARN] Vcpkg toolchain not set or skipped. If dependency errors occur, set UE5DRONE_VCPKG_TOOLCHAIN and install dependencies.
)

set "CMAKE_EXE="
where cmake >nul 2>&1 && set "CMAKE_EXE=cmake"
if not defined CMAKE_EXE (
    if exist "D:\Software\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=D:\Software\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
if not defined CMAKE_EXE if defined VS_PATH if exist "%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not defined CMAKE_EXE (
    echo [ERROR] CMake executable not found.
    exit /b 1
)

echo [INFO] CMake: %CMAKE_EXE%

set "BUILD_DIR=%~dp0build"
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

if not defined UE5DRONE_BUILD_TESTS set "UE5DRONE_BUILD_TESTS=OFF"
if not defined UE5DRONE_USE_OFFLINE_DEPS set "UE5DRONE_USE_OFFLINE_DEPS=ON"

set "GEN=Visual Studio 17 2022"
set "GEN_ARGS=-A x64"
set "NINJA_PATH="

if exist "D:\\Software\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe" set "NINJA_PATH=D:\\Software\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja"
if not defined NINJA_PATH if exist "D:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe" set "NINJA_PATH=D:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja"
if not defined NINJA_PATH if exist "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe" set "NINJA_PATH=C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja"
if not defined NINJA_PATH if exist "%ProgramFiles%\\Git\\usr\\bin\\ninja.exe" set "NINJA_PATH=%ProgramFiles%\\Git\\usr\\bin"

if defined NINJA_PATH (
    set "PATH=%PATH%;%NINJA_PATH%"
    set "GEN=Ninja"
    set "GEN_ARGS="
    echo [INFO] Use Ninja generator: %NINJA_PATH%
)

echo [INFO] CMake generator: %GEN%

set "GEN_INIT_ARGS=-G \"%GEN%\" %GEN_ARGS%"

if defined VCPKG_TOOLCHAIN (
    set "VCPKG_HOME=%~dp0.vcpkg-home"
    if not exist "!VCPKG_HOME!" mkdir "!VCPKG_HOME!"
    set "LOCALAPPDATA=!VCPKG_HOME!\\localappdata"
    set "X_VCPKG_REGISTRIES_CACHE=!LOCALAPPDATA!\\registries"
    if not exist "!LOCALAPPDATA!" mkdir "!LOCALAPPDATA!"
    if not exist "!X_VCPKG_REGISTRIES_CACHE!" mkdir "!X_VCPKG_REGISTRIES_CACHE!"
    set "VCPKG_DEFAULT_BINARY_CACHE=!VCPKG_HOME!\\cache"
    if not exist "!VCPKG_DEFAULT_BINARY_CACHE!" mkdir "!VCPKG_DEFAULT_BINARY_CACHE!"

    "!CMAKE_EXE!" -S "%~dp0." -B "%BUILD_DIR%" -G "%GEN%" %GEN_ARGS% -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=x64-windows -DUE5DRONE_BUILD_TESTS=%UE5DRONE_BUILD_TESTS% -DUE5DRONE_USE_OFFLINE_DEPS=%UE5DRONE_USE_OFFLINE_DEPS%
) else (
    "!CMAKE_EXE!" -S "%~dp0." -B "%BUILD_DIR%" -G "%GEN%" %GEN_ARGS% -DCMAKE_BUILD_TYPE=Release -DUE5DRONE_BUILD_TESTS=%UE5DRONE_BUILD_TESTS% -DUE5DRONE_USE_OFFLINE_DEPS=%UE5DRONE_USE_OFFLINE_DEPS%
)

if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    echo [HINT] If dependency packages are missing, verify network or preinstall Boost/yaml-cpp/nlohmann-json/spdlog/gtest.
    if defined VCPKG_TOOLCHAIN (
        echo [HINT] Common vcpkg log: %BUILD_DIR%\vcpkg-manifest-install.log
        echo [HINT] If this environment is offline, run: set UE5DRONE_SKIP_VCPKG=1
    )
    exit /b 1
)

"!CMAKE_EXE!" --build "%BUILD_DIR%" --config Release --target DroneBackend

if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [OK] Build succeeded. Output: %BUILD_DIR%\Release\DroneBackend.exe
pause
