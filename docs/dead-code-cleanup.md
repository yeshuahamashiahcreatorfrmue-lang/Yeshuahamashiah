# 죽은 코드 제거 + 기능별 리팩토링

실시간 필드 전투로 통일되며 더 이상 쓰이지 않는 코드를 검증 후 삭제했다.

## 검증 방법
각 후보의 전체 사용처를 grep으로 확인 → 진입/호출 경로가 없음을 확認 → 삭제 →
빌드 + 셀프테스트 통과 확인(단계별).

## 삭제한 죽은 코드

- **턴제 전투 시스템 전체**
  - `src/battle/Battle.{h,cpp}` 파일 삭제(+ CMake 제외).
  - `Phase::Battle`, `GamePlay::updateBattle/drawBattle`, `battle_`, `battleMenu_`.
  - 근거: `Phase::Battle`에 진입하는 코드 없음(전투는 필드 소환으로 대체됨).
- **구 스킬 시스템 (턴제 전용)**
  - `Skill` 구조체, `Database::skills`, `Database::skill()`, `ActorDef.skills`.
  - 근거: 턴제 전투 메뉴에서만 사용 → 전투 제거로 고아화. (필드 스킬은
    `FieldSkill`/`CharacterDef.skills`가 담당, 유지)
- **구 장비 시스템 (Item 기반으로 대체됨)**
  - `Equipment`/`EquipSlot` 구조체, `Database::equipment`, `Database::equip()`,
    `PartyMember.weaponId/armorId`. `totalAtk/totalDef`는 atk/def를 그대로 반환하도록
    단순화(보너스는 장착 시 atk/def에 이미 합산됨).
  - 근거: 실제 장비는 Item(kind==2)+`gs.equipped`로 동작. 구 시스템은 어떤 UI도
    값을 설정하지 않는 사장 경로였음.
- **기타 잔여 필드**: `EnemyDef.maxMp`(전투 전용), `Event.battleTurnBased`(턴제 토글).

## 리팩토링 (기능별 정리)

- **DB 탭 5분류 → 3분류**: 아이템 / 액터 / 적. (장비=아이템(장비), 스킬=캐릭터
  필드스킬로 일원화되어 중복 탭 제거)
- 직렬화/생성기(gen_game, gen_world)/셀프테스트를 새 데이터 모델에 맞게 정리.
- 기존 데이터(database.json)의 "equipment"/"skills"/"maxMp" 키는 로드 시 무시되어
  하위 호환 유지(크래시 없음).

## 검증

- 전 타깃 빌드 성공(TsukuruEngine/gen_*/selftest).
- 셀프테스트 전체 통과(전투 테스트 제거, 장비 테스트는 Item 기반으로 재작성).
- 헤드리스: 플레이/에디터 정상 구동, DB 탭 3분류 표시 확인.
