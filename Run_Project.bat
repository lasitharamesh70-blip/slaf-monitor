@echo off
title SLAF Power Monitoring System
echo Starting Node.js Server...
:: Node.js server එක run කිරීම
start /b node server.js
echo.
echo Waiting for server to start...
timeout /t 3 /nobreak > nul
echo Opening Dashboard...
:: Browser එකේ dashboard එක open කිරීම (Port එක 3000 නම්)
start http://localhost:3000
echo System is Running!
echo To stop the system, close this window.
pause