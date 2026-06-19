# Yeshuahamashiah Engine

A small **RPG Maker (쯔꾸르) 스타일 2D 게임 엔진** built on Python + pygame.

하나의 실행 파일로 **게임 플레이**와 **게임 편집(맵/충돌/이벤트/에셋 등록)** 을
모두 할 수 있습니다. `exe`를 더블클릭하면 곧바로 엔진 메뉴가 뜨고, 거기서
게임을 플레이하거나 에디터를 열 수 있으며, 에디터에서 `F1`을 누르면 즉시
플레이 테스트로 전환됩니다.

## 빠른 시작

```bash
pip install -r requirements.txt
python main.py
```

처음 실행하면 `projects/sample/` 에 예제 프로젝트(타일셋·캐릭터·맵)가
자동으로 생성됩니다.

## exe 빌드

```bash
pip install -r requirements.txt pyinstaller
python build_exe.py
# 결과물: dist/Yeshuahamashiah(.exe)
```

빌드된 실행 파일 옆에 `projects/` 폴더가 만들어지고, 그 안의 게임을 자유롭게
수정할 수 있습니다(재빌드 불필요).

## 조작법

### 메뉴
- `↑/↓` 프로젝트 선택, `Enter` 플레이, `E` 에디터, 버튼으로 새 프로젝트 생성

### 플레이 모드
| 키 | 동작 |
|----|------|
| 방향키 / WASD | 이동 |
| `Z` / `Space` | 상호작용(앞 칸의 이벤트 실행) |
| `F1` | 에디터로 전환 |
| `Esc` | 메뉴로 |

### 에디터 모드
| 키 | 동작 |
|----|------|
| `1` | 바닥(ground) 레이어 그리기 |
| `2` | 오브젝트(object) 레이어 그리기 |
| `3` | 충돌(collision) 토글 |
| `4` | 이벤트(메시지) 배치 |
| `5` | 플레이어 시작 위치 지정 |
| 좌클릭 | 적용 / 우클릭 | 지우기 |
| 중간버튼 드래그·방향키 | 화면 이동 |
| `I` | 이미지 파일을 타일셋으로 import·등록 |
| `N` | 새 맵 생성 |
| `Ctrl+S` | 저장 |
| `F1` | 플레이 테스트 |

## 구조

```
main.py                  # 실행 진입점 (exe가 호출)
engine/
  config.py              # 상수, 경로, 색상
  assets.py              # 에셋 import·등록·로드 (AssetManager)
  tileset.py             # 타일셋 이미지 슬라이싱
  tilemap.py             # 맵 데이터(레이어/충돌/이벤트) + 직렬화
  entity.py              # 캐릭터 애니메이션 + 플레이어 충돌 이동
  camera.py              # 맵을 따라가는 카메라
  project.py             # 프로젝트 매니페스트/맵 로드·저장
  placeholder.py         # 예제 에셋·맵 자동 생성
  ui.py                  # 버튼/텍스트/메시지 박스
  scene.py               # 씬 베이스 클래스
  menu.py                # 메인 메뉴 씬
  runtime.py             # 플레이(런타임) 씬
  editor.py              # 맵 에디터 씬
  app.py                 # 메인 루프 / 씬 전환
projects/<name>/         # 게임 데이터 (project.json, maps/, assets/)
tests/smoke_test.py      # 헤드리스 동작 검증
```

## 데이터 포맷

- `project.json` — 프로젝트 설정과 에셋 레지스트리(타일셋/캐릭터/오디오).
- `maps/<map>.json` — 맵별 레이어 격자, 충돌 격자, 이벤트 목록.
- `assets/<category>/` — import된 실제 에셋 파일(프로젝트가 자체 포함).

## 테스트

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy python tests/smoke_test.py
```
