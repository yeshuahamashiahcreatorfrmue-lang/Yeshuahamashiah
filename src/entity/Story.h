#pragma once
// Story authoring data: branching dialogue scenarios (이벤트 대화로그) and scene
// scenarios (스토리 시나리오 시퀀서). Both are project data, edited in their own
// editor tabs and played back at runtime.
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tsukuru {

// What happens when the player picks a dialogue answer.
enum DlgRespType {
    DR_None = 0,        // nothing (just advance / branch)
    DR_Reward = 1,      // 보상: gold/exp/item
    DR_SpawnMob = 2,    // 몹 소환 (db.mobs)
    DR_NpcHostile = 3,  // NPC 적대 (시간제한)
    DR_NpcFriendly = 4, // NPC 우호 (시간제한)
    DR_NpcFollow = 5,   // NPC 추종 (시간제한, 대화로 떠나보낼 수 있음)
    DR_Scene = 6,       // 스토리 시나리오(장면) 시작
};
inline const char* const kDlgRespNames[7] = { "없음","보상","몹소환","적대NPC","우호NPC","추종NPC","시나리오" };

// One selectable answer under a dialogue line.
struct DialogueAnswer {
    std::string text;             // 대답 버튼 텍스트
    int   respType = DR_None;
    int   rewardGold = 0, rewardExp = 0, rewardItemId = -1, rewardItemCount = 1;
    int   mobId = -1;             // DR_SpawnMob: db.mobs id
    int   npcCharId = -1;         // DR_Npc*: db.mobs/characters id used as the NPC graphic
    float durationSecs = 0;       // 적대/우호/추종 시간제한 (0 = 무제한)
    int   sceneId = -1;           // DR_Scene: 시작할 시나리오(장면) id
    int   gotoLine = -1;          // 이 대답 뒤 이동할 라인(-1 = 다음 라인/종료)
    bool  dismissFollowers = false; // 추종 NPC를 떠나보냄(대화로 떠나게)
};

// One line of dialogue. If `answers` is empty it just advances on confirm.
struct DialogueLine {
    std::string speaker;          // 말하는 이(빈칸 가능)
    std::string text;             // 대사
    int   speakerAsset = -1;      // 말하는 NPC의 초상(그래픽 에셋) — 맵에서 NPC 선택 시 설정
    std::vector<DialogueAnswer> answers;
};

// A full branching conversation, referenced by an event (Event::dialogueId).
struct DialogueScenario {
    int id = -1;
    std::string name = "대화";
    std::vector<DialogueLine> lines;
};

// ---- Story scene sequencer ----
enum SceneActType {
    SA_MoveChar = 0,   // 캐릭터/NPC 이동 (targetId, x, y, time)
    SA_Dialogue = 1,   // 대화 시작 (dialogueId)
    SA_Effect   = 2,   // 이펙트 작동 (effectAsset, x, y, time)
    SA_Spawn    = 3,   // NPC/몹/오브젝트 등장 (targetId=mob/char id, x, y)
    SA_Remove   = 4,   // 등장한 대상 제거 (targetId)
    SA_Wait     = 5,   // 대기 (time)
    SA_Motion   = 6,   // 동작 전환: 대상(targetId)이 모션(refId=MO_*)을 time초간 재생 (죽음/공격 등)
};
inline const char* const kSceneActNames[7] = { "이동","대화","이펙트","등장","제거","대기","동작" };

struct SceneAction {
    int   type = SA_Wait;
    int   targetId = -1;   // SA_Spawn/Remove/MoveChar/Wait: a tag id (1..) the scene assigns
    int   refId = -1;      // mob/char/dialogue/effect-asset id depending on type
    int   x = 0, y = 0;    // tile position (move/effect/spawn) — set on the map
    float time = 1.0f;     // duration / wait seconds
    int   radius = 1;      // SA_Effect: 영향 타일 반경 (맵에서 지정)
    std::vector<int> removeTags;  // SA_Remove: 복수 선택한 제거 대상 태그들
};

struct Scene {
    int id = -1;
    std::string name = "장면";
    int editMapId = -1;    // 편집 시 배경으로 보는 맵(런타임 동작과 무관)
    std::string group;     // 소속 제목(그룹) — 좌측 패널에서 이 제목 아래로 묶임(빈칸=미분류)
    // 시점(카메라): 재생 시 화면이 어디를 어느 배율로 비출지
    int   camMode = 0;     // 0=플레이어중심 1=전체맵 2=특정위치(camX,camY) 3=특정유닛중심(camTag)
    float camZoom = 2.0f;  // 확대 배율(전체맵 모드는 자동 맞춤)
    int   camX = 0, camY = 0; // camMode==2 특정 위치 타일
    int   camTag = 0;      // camMode==3 중심 유닛 태그(0=플레이어)
    int   bgmAsset = -1;   // 장면 음악(오디오 에셋). -1이면 제목 음악→맵 배경음 순으로 폴백
    std::vector<SceneAction> actions;
};

// ---- json ----
nlohmann::json dialogueToJson(const DialogueScenario& d);
DialogueScenario dialogueFromJson(const nlohmann::json& j);
nlohmann::json sceneToJson(const Scene& s);
Scene sceneFromJson(const nlohmann::json& j);

} // namespace tsukuru
