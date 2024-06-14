@echo off
if EXIST "c:\Program Files\tcc" (
    %comspec% /k "C:\Program Files\tcc\share\.tccVar.bat"
) else if EXIST "C:\Program Files (x86)\tcc" (
    %comspec% /k "C:\Program Files (x86)\tcc\share\.tccVar32.bat"
)
