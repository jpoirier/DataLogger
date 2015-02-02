@ECHO OFF

set ARCH=X64
set XPLM_LIB="XPLM_64.lib"
set vs_toolset=x86_amd64

if "%1"=="386" (
set ARCH=X86
set vs_toolset=x86
set XPLM_LIB="XPLM.lib"
)

:: this assumes we have the proper tag
:: if created via the github website then do $ git fetch --tags
:: this doesn't seem to work: git describe --abbrev=0 --tags
set GIT_VER="vX.Y.Z"
for /f "delims=" %%i in ('git describe --tags') do @set GIT_VER=%%i

echo -
echo -
echo ----------------------------------------------------
echo Building DataLogger %GIT_VER%
echo ARCH=%ARCH% XPLM_LIB=%XPLM_LIB%
echo ----------------------------------------------------
echo -
echo -

:: Visual Studio 2013
:vc-set-2013
if not defined VS120COMNTOOLS  goto vc-set-2012
if not exist "%VS120COMNTOOLS%\..\..\vc\vcvarsall.bat" goto vc-set-2012
echo -
echo - Visual C++ 2013 found.
echo -
call "%VS120COMNTOOLS%\..\..\vc\vcvarsall.bat" %vs_toolset%
goto STARTCOMPILING

:: Visual Studio 2012
:vc-set-2012
if not defined VS110COMNTOOLS  goto vc-set-notfound
if not exist "%VS110COMNTOOLS%\..\..\vc\vcvarsall.bat" vc-set-notfound
echo -
echo - Visual C++ 2011 found.
echo -
call "%VS110COMNTOOLS%\..\..\vc\vcvarsall.bat" %vs_toolset%
goto STARTCOMPILING

:vc-set-notfound
echo -
echo - No Visual C++ found, please set the enviroment variable
echo -
echo - VCToolkitInstallDir  or  VS71COMNTOOLS or VS80COMNTOOLS
echo -
echo - to your Visual Studio folder which contains vsvars32.bat.
echo -
echo - Or call the vsvars32.bat.
echo -

goto ERROR

:STARTCOMPILING

:: buid process

set CL_OPTS=/c /GS /W3 /Gy /Zc:wchar_t /Zi /Gm- /O2 /Ob1 /fp:precise /GF /WX- /Zc:forScope /Gd /MT /EHsc /nologo

:: /D TOGGLE_TEST_FEATURE
set CL_DEFS=/D "VERSION=%GIT_VER%" /D "NDEBUG" /D "WIN32" /D "_MBCS"  /D "XPLM200" /D "XPLM210" /D "_USRDLL" /D "_WINDLL" /D "APL=0" /D "IBM=1" /D "LIN=0" /D "WIN32" /D "_WINDOWS" /D "LOGPRINTF" /D "SIMDATA_EXPORTS" /D "_CRT_SECURE_NO_WARNINGS" /D "_VC80_UPGRADE=0x0600"

set CL_FILES="main_win.cpp" /TP "main.cpp"

:: /MACHINE:X86 /MACHINE:X64  /MANIFEST:NO
set LINK_OPTS=/MACHINE:%ARCH% /OUT:win.xpl /INCREMENTAL:NO /NOLOGO /DLL /NXCOMPAT /DYNAMICBASE /SUBSYSTEM:CONSOLE /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /LIBPATH:"SDK\Libraries\Win" /TLBID:1

:: "XPLM_64.lib" "XPLM.lib"
set LINK_LIBS=%XPLM_LIB% "user32.lib" "Opengl32.lib" "odbc32.lib" "odbccp32.lib" "kernel32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib"
set LINK_OBJS="main.obj" "main_win.obj"

@ECHO ON

cl.exe  %CL_OPTS% %CL_DEFS% %CL_FILES%
link.exe  %LINK_OPTS% %LINK_LIBS% %LINK_OBJS%

@ECHO OFF

del *.pdb *.exp *.obj

goto LEAVE

:ERROR
echo -
echo -
echo - An error occured. Compiling aborted.
echo -
pause



:LEAVE

