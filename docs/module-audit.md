# 모듈 점검 & 재배치 기록 (Module Audit)

기능별 분리 후, 각 함수가 "그 파일의 목적"에 맞는지 점검한 기록.
`이동` = 다른 파일로 옮김, `유지` = 검토했으나 현 위치가 적절.

## GamePlay 런타임

| 항목 | 현재 위치 | 판정 | 사유 |
|------|-----------|------|------|
| `walkable()` | Monsters | **이동 → Movement** | 몬스터 전용이 아니라 플레이어/NPC/스킬 이동까지 쓰는 일반 통행 판정 |
| `damageMonster()` | Skills | **이동 → Monsters** | 스킬이 아니라 몬스터 HP/사망 상태 변경. `onMonsterKilled` 옆이 맞음 |
| `spawnFx/updateFx/drawFx` | Skills | **이동 → Fx(신규)** | 스킬 "정의/시전"과 별개인 이펙트 시뮬/렌더 |
| `updateProjectiles/drawProjectiles` | Skills | **이동 → Fx(신규)** | 투사체 시뮬/렌더는 독립 기능 |
| `rotateToFacing()` | Skills | 유지 | 시전 패턴 회전 전용 기하 헬퍼 |
| `onMonsterKilled()` | Monsters | 유지 | 처치 보상/레벨업 = 몬스터 사망 처리 |
| `drawSkillPanel/handleSkillClicks` | Skills | 유지 | 스킬 쿨다운 HUD + 입력 |
| `drawMessage()` | Events | 유지 | 메시지 박스 = 대화 시스템 일부 |
| `drawCharacter()` | Render | 유지 | NPC/플레이어 공용 렌더 헬퍼 |
| `isign()` | Monsters | 유지 | 몬스터 AI 전용 헬퍼 |

### 재배치 후 구성 (8 TU)
- `GamePlay` 수명주기/디스패치
- `GamePlayMovement` 필드 루프 · 이동 · **walkable**
- `GamePlayEvents` 이벤트 · 상호작용 · 메시지
- `GamePlayMonsters` 스폰/AI · **damageMonster** · 처치 · 렌더
- `GamePlaySkills` 스킬 정의/시전 · 스킬 HUD
- `GamePlayFx` **투사체 · 이펙트** (신규)
- `GamePlayNpc` NPC · 자동실행
- `GamePlayRender` 날씨/미니맵/조명/컬링/필드 렌더

## Editor

| 항목 | 현재 위치 | 판정 | 사유 |
|------|-----------|------|------|
| `EditorHistory`(undo/redo) | 단독 | 유지 | 코어 단축키 + 맵편집 양쪽서 쓰는 횡단 관심사, 단일 목적 |
| `EditorStamps`(prefab) | 단독 | 유지 | 프리팹 스탬핑 = 단일 목적, 일관됨 |
| `generate*`(char/effect) | Chars | 유지 | 캐릭터 탭 워크플로(색상/스타일 상태) 밀착 |
| `isImageExt/isAudioExt` | AssetIO | 유지 | 등록 로직 전용 |

→ Editor는 추가 이동 없음 (탭별 + 횡단 관심사로 이미 목적 정합).
