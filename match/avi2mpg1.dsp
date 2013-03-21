# Microsoft Developer Studio Project File - Name="avi2mpg1" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=avi2mpg1 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "avi2mpg1.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "avi2mpg1.mak" CFG="avi2mpg1 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "avi2mpg1 - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "avi2mpg1 - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "avi2mpg1 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x804 /d "NDEBUG"
# ADD RSC /l 0x804 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "avi2mpg1 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Vfw32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "avi2mpg1 - Win32 Release"
# Name "avi2mpg1 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Avi2m1v.c
# End Source File
# Begin Source File

SOURCE=.\AVI2MP2.C
# End Source File
# Begin Source File

SOURCE=.\Avi2mpg1.c
# End Source File
# Begin Source File

SOURCE=.\BUFFER.C
# End Source File
# Begin Source File

SOURCE=.\COMMON.C
# End Source File
# Begin Source File

SOURCE=.\CONFORM.C
# End Source File
# Begin Source File

SOURCE=.\Encode.c
# End Source File
# Begin Source File

SOURCE=.\FDCTREF.C
# End Source File
# Begin Source File

SOURCE=.\IDCT.C
# End Source File
# Begin Source File

SOURCE=.\INITS.C
# End Source File
# Begin Source File

SOURCE=.\INPTSTRM.C
# End Source File
# Begin Source File

SOURCE=.\INTERACT.C
# End Source File
# Begin Source File

SOURCE=.\MOTION.C
# End Source File
# Begin Source File

SOURCE=.\MPLEX.C
# End Source File
# Begin Source File

SOURCE=.\MULTPLEX.C
# End Source File
# Begin Source File

SOURCE=.\PREDICT.C
# End Source File
# Begin Source File

SOURCE=.\PSY.C
# End Source File
# Begin Source File

SOURCE=.\PUTBITS.C
# End Source File
# Begin Source File

SOURCE=.\PUTHDR.C
# End Source File
# Begin Source File

SOURCE=.\PUTMPG.C
# End Source File
# Begin Source File

SOURCE=.\PUTPIC.C
# End Source File
# Begin Source File

SOURCE=.\Putseq.c
# End Source File
# Begin Source File

SOURCE=.\PUTVLC.C
# End Source File
# Begin Source File

SOURCE=.\QUANTIZE.C
# End Source File
# Begin Source File

SOURCE=.\RATECTL.C
# End Source File
# Begin Source File

SOURCE=.\Readpic.c
# End Source File
# Begin Source File

SOURCE=.\STATS.C
# End Source File
# Begin Source File

SOURCE=.\SUBS.C
# End Source File
# Begin Source File

SOURCE=.\SYSTEMS.C
# End Source File
# Begin Source File

SOURCE=.\TIMECODE.C
# End Source File
# Begin Source File

SOURCE=.\TRANSFRM.C
# End Source File
# Begin Source File

SOURCE=.\WRITEPIC.C
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ABSTHR_1.H
# End Source File
# Begin Source File

SOURCE=.\ALLOC_0.H
# End Source File
# Begin Source File

SOURCE=.\ALLOC_1.H
# End Source File
# Begin Source File

SOURCE=.\ALLOC_2.H
# End Source File
# Begin Source File

SOURCE=.\AVI2M1V.H
# End Source File
# Begin Source File

SOURCE=.\AVI2MPG1.H
# End Source File
# Begin Source File

SOURCE=.\BITSTRM.H
# End Source File
# Begin Source File

SOURCE=.\COMMON.H
# End Source File
# Begin Source File

SOURCE=.\ENCODER.H
# End Source File
# Begin Source File

SOURCE=.\ENWINDOW.H
# End Source File
# Begin Source File

SOURCE=.\Global.h
# End Source File
# Begin Source File

SOURCE=.\MPLEX.H
# End Source File
# Begin Source File

SOURCE=.\VIDEO.H
# End Source File
# Begin Source File

SOURCE=.\VLC.H
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
