# 기능 완성도 보완 — 3차 (전체 파일 점검 기반)

코드 전반을 점검(서브에이전트 감사)해 발견한 기능 공백 4종을 채웠다.

## 점검으로 찾은 공백
| 항목 | 점검 전 상태 |
|------|------|
| 음량 설정 영구 저장 | 없음 — 매 실행마다 초기화 |
| 전투 데미지 숫자 | 없음 — 토스트/HP바만 |
| 삭제 확인 다이얼로그 | 없음 — 즉시 삭제 |
| 필드 HUD 종합 스탯 | 없음 — 메뉴에서만 표시 |

## 추가 기능
- [x] **플로팅 데미지 숫자** — 적/플레이어 피격 시 숫자가 떠오르며 사라짐.
  가한 피해는 노란색, 받은 피해는 빨간 `-N`.
  - `src/game/PlayTypes.h` FloatingText, `GamePlayFx.cpp` spawnPopup/drawPopups,
    `GamePlayMonsters.cpp`/`GamePlayNpc.cpp` 피해 지점에 연결, `GamePlayRender.cpp` 월드 렌더
- [x] **음량 설정 영구 저장** — 마스터/음악/효과음 음량을 `settings.json`(앱 폴더)에
  저장하고 다음 실행 시 자동 적용. 설정 메뉴에서 조절하면 즉시 저장.
  - `src/core/Audio.{h,cpp}` loadSettings/saveSettings, `core/Engine.cpp` init 시 로드
- [x] **삭제 확인 다이얼로그** — 맵 삭제·전맵뷰어 복제본 삭제·에셋 삭제 전에
  `확인(삭제)/취소` 모달. 열린 동안 뒤쪽 UI 입력 차단.
  - `src/render/UI.{h,cpp}` g_inputEnabled 입력 게이트, `editor/Editor.{h,cpp}`
    askConfirm/drawConfirmOverlay, `EditorWorld.cpp`/`EditorChars.cpp` 삭제 지점 적용
- [x] **HUD 종합 스탯** — 필드 상단 HUD에 장비 보너스가 반영된 `공/방/속` 표시
  (기존엔 장비창에서만 확인 가능).
  - `src/game/GamePlayRender.cpp` HUD 상단 줄

## 부수
- 디버그 훅 `TSUKURU_CONFIRM`(삭제 확인창 미리보기) 추가

## 검증
- [x] 빌드 성공
- [x] 셀프테스트 99/99 통과(rc=0)
- [x] 200프레임 오토워크 스모크 정상 종료(rc=0)
- [x] 전투 스크린샷에서 데미지 숫자(가한 99·받은 -7) 확인
- [x] HUD 종합 스탯(공 12 방 0 속 5) 확인
- [x] 삭제 확인 다이얼로그 스크린샷 확인
