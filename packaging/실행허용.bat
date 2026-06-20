@echo off
chcp 65001 >nul
title Tsukuru Engine - 실행 허용
:: --- 관리자 권한으로 자동 재실행 ---
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo 관리자 권한을 요청합니다... (UAC 창에서 "예"를 눌러주세요)
  powershell -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
  exit /b
)
set "FOLDER=%~dp0"
echo.
echo [1/2] 이 폴더의 파일 차단을 해제합니다...
powershell -NoProfile -Command "Get-ChildItem -LiteralPath '%FOLDER%' -Recurse | Unblock-File -ErrorAction SilentlyContinue"
echo [2/2] Windows 보안(Defender) 예외에 이 폴더를 추가합니다...
powershell -NoProfile -Command "Add-MpPreference -ExclusionPath '%FOLDER%' -ErrorAction SilentlyContinue"
echo.
echo ============================================
echo  완료! 이제 TsukuruEngine.exe 를 실행하세요.
echo ============================================
echo  (파란 경고창이 또 뜨면 '추가 정보' - '실행')
echo.
pause
