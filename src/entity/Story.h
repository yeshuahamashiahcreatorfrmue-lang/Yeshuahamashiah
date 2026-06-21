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
};
inline const char* const kDlgRespNames[6] = { "없음","보상","몹소환","적대NPC","우호NPC","추종NPC" };

// One selectable answer under a dialogue line.
struct DialogueAnswer {
    std::string text;             // 대답 버튼 텍스트
    int   respType = DR_None;
    int   rewardGold = 0, rewardExp = 0, rewardItemId = -1, rewardItemCount = 1;
    int   mobId = -1;             // DR_SpawnMob: db.mobs id
    int   npcCharId = -1;         // DR_Npc*: db.mobs/characters id used as the NPC graphic
    float durationSecs = 0;       // 적대/우호/추종 시간제한 (0 = 무제한)
    int   gotoLine = -1;          // 이 대답 뒤 이동할 라인(-1 = 다음 라인/종료)
    bool  dismissFollowers = false; // 추종 NPC를 떠나보냄(대화로 떠나게)
};

// One line of dialogue. If `answers` is empty it just advances on confirm.
struct DialogueLine {
    std::string speaker;          // 말하는 이(빈칸 가능)
    std::string text;             // 대사
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
};
inline const char* const kSceneActNames[6] = { "이동","대화","이펙트","등장","제거","대기" };

struct SceneAction {
    int   type = SA_Wait;
    int   targetId = -1;   // SA_Spawn/Remove/MoveChar: a tag id (1..) the scene assigns
    int   refId = -1;      // mob/char/dialogue/effect-asset id depending on type
    int   x = 0, y = 0;    // tile position (move/effect/spawn)
    float time = 1.0f;     // duration / wait seconds
};

struct Scene {
    int id = -1;
    std::string name = "장면";
    std::vector<SceneAction> actions;
};

// ---- json ----
nlohmann::json dialogueToJson(const DialogueScenario& d);
DialogueScenario dialogueFromJson(const nlohmann::json& j);
nlohmann::json sceneToJson(const Scene& s);
Scene sceneFromJson(const nlohmann::json& j);

} // namespace tsukuru
