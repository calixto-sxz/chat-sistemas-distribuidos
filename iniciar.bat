@echo off
cd /d %~dp0
start "Servidor" cmd /k server.exe
timeout /t 1 >nul
start "Cliente 1" cmd /k client.exe
start "Cliente 2" cmd /k client.exe
