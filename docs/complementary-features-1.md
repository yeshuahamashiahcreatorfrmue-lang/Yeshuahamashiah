# 기능 완성도 보완 — 1차

기존 기능을 실사용 관점에서 둥글게 다듬는 보완 기능 4종을 추가했다.

## 추가 기능
- [x] **상점 되팔기(판매)** — 상점 창에 `구매`/`판매` 탭 토글. 판매 탭은
  소지품 중 가격이 있는 아이템을 절반가(`price/2`, 최소 1G)로 나열하고
  `판매` 버튼으로 1개씩 판매(골드 +절반가, 수량 -1). 구매는 기존과 동일.
  - `src/game/GamePlay.h`: `int shopMode_`(0=구매/1=판매)
  - `src/game/GamePlayEvents.cpp`: `openShop`에서 `shopMode_=0` 초기화,
    `drawShop`에 탭·판매 목록 추가
- [x] **퀵세이브(F9) / 퀵로드(F12)** — 메뉴를 거치지 않고 즉시 저장/복원.
  `save/quick.json`에 현재 게임 상태를 기록하고, 로드 시 해당 맵으로 복귀.
  토스트로 완료/실패 안내.
  - `src/game/GamePlay.{h,cpp}`: `quickSave()`/`quickLoad()`, `update()`에서 F9/F12 처리
- [x] **NPC 시선 맞춤** — 말을 걸면 NPC가 플레이어를 마주 보도록 방향 전환
  (플레이어 facing의 반대 방향), 이동 정지.
  - `src/game/GamePlayEvents.cpp`: `interact()`에서 `n->dir = opposite[dir_]`
- [x] **비전투 HP 자연 회복** — 주변에 몹이 없고(전투 아님) 포만/수분이
  남아 있을 때(잘 먹은 상태) HP가 3초당 1씩 천천히 회복. 탐험 피로를 줄이되
  전투를 무력화하지 않음.
  - `src/game/GamePlayMovement.cpp`: `updateField` HP 회복 블록
  - `src/game/GamePlay.h`: `float hpRegen_`

## 부수
- F1 도움말에 `F9 / F12 — 퀵세이브 / 퀵로드` 항목 추가
- 디버그 훅 `TSUKURU_SHOPSELL`(상점을 판매 탭으로 바로 열기) 추가

## 검증
- [x] 빌드 성공(경고는 기존 misleading-indentation만)
- [x] 셀프테스트 99/99 통과(rc=0)
- [x] 200프레임 오토워크 스모크 정상 종료(rc=0)
- [x] 상점 판매 탭 스크린샷 확인(포션 15G·빵 10G 절반가, 판매 버튼)
