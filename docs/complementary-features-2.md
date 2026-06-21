# 기능 완성도 보완 — 2차

실제 플레이/저장 흐름을 더 매끄럽게 만드는 보완 기능 6종을 추가했다.

## 추가 기능
- [x] **레벨업 알림** — 전투 처치/퀘스트 보상으로 레벨이 오르면 토스트·보상창에
  `★레벨 업! Lv N` 표시(기존엔 효과음만).
  - `src/game/GamePlayMonsters.cpp` onMonsterKilled, `GamePlayEvents.cpp` grantQuestReward
- [x] **플레이타임 기록** — `GameState::playSeconds` 누적(전투/필드/메뉴 중 증가,
  게임오버/클리어 제외), 직렬화 포함.
  - `src/game/GameState.{h,cpp}`, `GamePlay.cpp` update()
- [x] **세이브 슬롯 메타데이터** — 저장 메뉴의 각 슬롯에 `Lv · 맵이름 · 플레이타임 · 골드`
  표시(기존엔 저장됨/비어있음만).
  - `src/game/SaveMeta.h`(공용 리더), `src/game/Menu.cpp` drawSave
- [x] **자동 저장** — 맵을 이동할 때마다 `save/auto.json`에 조용히 자동 저장
  (최초 진입/퀵로드 직후는 제외). "자동 저장됨" 토스트.
  - `src/game/GamePlay.{h,cpp}` autoSave(), loadMap
- [x] **이어하기 확장** — 타이틀의 "이어하기"가 슬롯1~3 + 퀵세이브 + 오토세이브
  중 가장 최근 파일을 불러옴. 옆에 `(Lv N · 플레이타임)` 미리보기 표시.
  - `src/game/TitleScreen.cpp` latestSave, draw
- [x] **전체 지도(M)** — 현재 맵 전체를 한눈에 보는 오버레이. 플레이어/NPC/적/이벤트
  마커 + 범례. M 또는 ESC로 닫기(열린 동안 필드 정지).
  - `src/game/GamePlayRender.cpp` drawFullMap, `GamePlayMovement.cpp` M 토글,
    `GamePlay.cpp` draw, F1 도움말에 항목 추가

## 부수
- 디버그 훅 `TSUKURU_FULLMAP`(전체 지도 바로 열기) 추가
- F1 도움말에 `M — 전체 지도`, `F9/F12 — 퀵세이브/퀵로드` 정리(박스 높이 확장)

## 검증
- [x] 빌드 성공
- [x] 셀프테스트 99/99 통과(rc=0)
- [x] 200프레임 오토워크 스모크 정상 종료(rc=0)
- [x] 전체 지도(M) 스크린샷 확인 — 마커·범례 정상
- [x] 세이브 슬롯 메타데이터 스크린샷 확인(Lv 7 · 마을 · 1시간 15분 · 1234G)
