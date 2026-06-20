@echo off
chcp 65001 >nul
title Tsukuru Engine - 실행 허용
:: "ELEV" 마커가 있으면 이미 승격된 재실행이므로 다시 승격하지 않는다(무한 점멸 방지).
if "%~1"=="ELEV" goto run
net session >nul 2>&1
if %errorlevel%==0 goto run
echo 관리자 권한을 한 번 요청합니다... (UAC 창에서 "예"를 눌러주세요)
powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0' -ArgumentList 'ELEV'"
exit /b

:run
set "F=%~dp0"
echo.
echo [1/2] 이 폴더의 파일 차단을 해제합니다...
powershell -NoProfile -Command "Get-ChildItem -LiteralPath '%F%' -Recurse | Unblock-File -ErrorAction SilentlyContinue"
echo [2/2] Windows 보안(Defender) 예외에 이 폴더를 추가합니다...
powershell -NoProfile -Command "try { Add-MpPreference -ExclusionPath '%F%' -ErrorAction Stop; Write-Host '      완료' } catch { Write-Host '      건너뜀(관리자 권한이 아니면 생략됩니다)' }"
echo.
echo ============================================
echo  완료! 이제 TsukuruEngine.exe 를 실행하세요.
echo  (파란 SmartScreen 창이 뜨면 '추가 정보' - '실행')
echo ============================================
echo.
pause
