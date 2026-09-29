@echo off
start cmd /k server.exe
timeout /t 1 >nul
start cmd /k client.exe
start cmd /k client.exe
