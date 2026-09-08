@echo off
rem build.cmd -- build EreBUS Gate with the .NET Framework compiler (no SDK needed).
rem   output: build\gate\EreBUS-Gate.exe
setlocal
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
if not exist "%CSC%" set CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe
if not exist "%CSC%" ( echo the .NET Framework C# compiler was not found & exit /b 1 )
set ROOT=%~dp0..\..
if not exist "%ROOT%\build\gate" mkdir "%ROOT%\build\gate"
set ICON=
if exist "%~dp0gate.ico" set ICON=/win32icon:"%~dp0gate.ico"
"%CSC%" /nologo /target:winexe /platform:anycpu ^
  /out:"%ROOT%\build\gate\EreBUS-Gate.exe" %ICON% ^
  /reference:System.Windows.Forms.dll /reference:System.Drawing.dll ^
  "%~dp0EreBUSGate.cs"
if errorlevel 1 ( echo build failed & exit /b 1 )
echo built %ROOT%\build\gate\EreBUS-Gate.exe
