@echo off
rem Sign the Pro Tools (AAX) plug-in with PACE's wraptool so retail Pro Tools will load it.
rem Needs: PACE Eden tools (wraptool), your iLok (or iLok Cloud), and a code-signing certificate (.p12/.pfx).
rem Check the flags against the wraptool guide that comes with PACE's tools.
rem Run from an Administrator command prompt:
rem   sign_aax_windows.bat <PACE account> <wrap config GUID> <certificate.p12> <certificate password>
setlocal
set ACCOUNT=%~1
set WCGUID=%~2
set KEYFILE=%~3
set KEYPASS=%~4
set AAX=%CommonProgramFiles%\Avid\Audio\Plug-Ins\MasterDesk Mastering Suite.aaxplugin
if "%KEYPASS%"=="" ( echo Usage: sign_aax_windows.bat ACCOUNT WCGUID certificate.p12 PASSWORD & exit /b 1 )
if not exist "%AAX%" ( echo Can't find "%AAX%" - install the Pro Tools (AAX) component first. & exit /b 1 )
if "%WRAPTOOL%"=="" set WRAPTOOL=wraptool
"%WRAPTOOL%" sign --verbose --account %ACCOUNT% --wcguid %WCGUID% --keyfile "%KEYFILE%" --keypassword %KEYPASS% --in "%AAX%" --out "%AAX%"
if errorlevel 1 exit /b 1
"%WRAPTOOL%" verify --verbose --in "%AAX%"
echo Done. Start Pro Tools: the plug-in appears under EQ and Dynamics.
