set objFSO = CreateObject("Scripting.FileSystemObject")
scriptdir = objFSO.GetParentFolderName(WScript.ScriptFullName)
confname = "Release"
wcx_file_x86 = scriptdir & "\" & confname & "_Win32\paxz.wcx" 
wcx_file_x64 = scriptdir & "\" & confname & "_x64\paxz.wcx64"
wcx_ver = objFSO.GetFileVersion(wcx_file_x64)
dotPos = InStr(wcx_ver, ".")
dotPos = InStr(dotPos+1, wcx_ver, ".")
wcx_ver = left(wcx_ver, dotPos-1)
target = scriptdir & "\wcx_paxz_v" & wcx_ver & ".zip" 

pos = InStrRev(scriptdir, "\")
rootdir = left(scriptdir, pos-1)
langdir = rootdir & "\src\res\lang\"

set zip = objFSO.OpenTextFile(target, 2, vbtrue)
zip.Write "PK" & Chr(5) & Chr(6) & String( 18, Chr(0) )
zip.Close
set zip = nothing

wscript.sleep 150

set objApp = CreateObject("Shell.Application")
objApp.NameSpace(target).CopyHere (wcx_file_x64)
wscript.sleep 150
objApp.NameSpace(target).CopyHere (wcx_file_x86)
wscript.sleep 150
objApp.NameSpace(target).CopyHere (scriptdir & "\pluginst.inf")
wscript.sleep 100
objApp.NameSpace(target).CopyHere (scriptdir & "\paxz.ini")
wscript.sleep 100
objApp.NameSpace(target).CopyHere (langdir)
wscript.sleep 100


set obj = nothing
set objApp = nothing
set objFSO = nothing
