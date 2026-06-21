// GamePlayStory: runtime playback of 대화로그 시나리오 (branching dialogue) and
// 스토리 시나리오 (scene sequencer).
#include "game/GamePlay.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "database/Database.h"
#include <algorithm>
#include <cstdlib>

namespace tsukuru {

// ----------------------------- dialogue playback -----------------------------
void GamePlay::startDialogue(int id) {
    if (!engine_.project().database.dialogue(id)) return;
    dlgRunId_ = id; dlgRunLine_ = 0;
    phase_ = Phase::Dialogue;
    showDialogueLine();
}

void GamePlay::showDialogueLine() {
    const DialogueScenario* d = engine_.project().database.dialogue(dlgRunId_);
    if (!d || dlgRunLine_ < 0 || dlgRunLine_ >= (int)d->lines.size()) {  // end
        dlgRunId_ = -1; dlgAnswerTexts_.clear();
        phase_ = Phase::Field;   // return control (a running scene resumes from Field)
        return;
    }
    dlgAnswerTexts_.clear();
    for (const auto& a : d->lines[dlgRunLine_].answers)
        if (!a.text.empty()) dlgAnswerTexts_.push_back(a.text);
}

void GamePlay::updateDialogue() {
    const DialogueScenario* d = engine_.project().database.dialogue(dlgRunId_);
    if (!d) { phase_ = Phase::Field; return; }
    static const bool autodismiss = getenv("TSUKURU_AUTOWALK") != nullptr;
    bool hasAns = dlgRunLine_ < (int)d->lines.size() && !d->lines[dlgRunLine_].answers.empty();
    if (hasAns) {
        // ESC cancels the whole conversation (never leaves the player stuck);
        // autowalk auto-picks the first answer so headless smoke never hangs.
        if (IsKeyPressed(KEY_ESCAPE)) { dlgRunId_ = -1; phase_ = Phase::Field; }
        else if (autodismiss) applyDialogueAnswer(0);
        return;
    }
    if (autodismiss || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
        ++dlgRunLine_;
        showDialogueLine();
    }
}

void GamePlay::applyDialogueAnswer(int idx) {
    const DialogueScenario* d = engine_.project().database.dialogue(dlgRunId_);
    if (!d || dlgRunLine_ >= (int)d->lines.size()) return;
    const DialogueLine& ln = d->lines[dlgRunLine_];
    if (idx < 0 || idx >= (int)ln.answers.size()) return;
    DialogueAnswer a = ln.answers[idx];   // copy (the db may change underneath)
    GameState& gs = engine_.state();
    switch (a.respType) {
        case DR_Reward: {
            if (a.rewardGold > 0) gs.inventory.gold += a.rewardGold;
            if (a.rewardExp > 0 && !gs.party.empty()) gs.party[0].gainExp(a.rewardExp);
            if (a.rewardItemId >= 0) gs.inventory.addItem(a.rewardItemId, std::max(1, a.rewardItemCount));
            engine_.audio().playSfx("coin"); toast_ = "보상 획득!"; toastTimer_ = 1.4f;
            break;
        }
        case DR_SpawnMob:
            if (a.mobId >= 0) {                       // spawn a CharacterDef mob near the player
                if (const CharacterDef* md = engine_.project().database.mob(a.mobId)) {
                    int TS = map_ ? map_->tileset.tileWidth : 32;
                    FieldMonster m; m.mobCharId = md->id; m.name = md->name;
                    int fx = destX_+1, fy = destY_;
                    for (int r=1;r<=4;++r){ if(walkable(destX_+r,destY_)){fx=destX_+r;fy=destY_;break;} }
                    m.x=m.destX=fx; m.y=m.destY=fy; m.px=fx*(float)TS; m.py=fy*(float)TS;
                    m.hp=m.maxHp=md->maxHp; m.atk=md->atk; m.def=md->def;
                    m.expReward=md->expReward; m.goldReward=md->goldReward;
                    m.spawnFreeze=md->spawnFreezeSecs; m.homeX=fx; m.homeY=fy; m.respawnSecs=md->respawnSecs;
                    monsters_.push_back(m);
                    toast_ = md->name + " 출현!"; toastTimer_ = 1.4f;
                }
            }
            break;
        case DR_NpcHostile:  spawnTimedNpc(a.npcCharId, (int)NpcFaction::Enemy, a.durationSecs, false); break;
        case DR_NpcFriendly: spawnTimedNpc(a.npcCharId, (int)NpcFaction::Ally,  a.durationSecs, false); break;
        case DR_NpcFollow:   spawnTimedNpc(a.npcCharId, (int)NpcFaction::Ally,  a.durationSecs, true);  break;
        default: break;
    }
    if (a.dismissFollowers) {   // 대화로 추종 NPC 떠나보내기
        npcs_.erase(std::remove_if(npcs_.begin(), npcs_.end(),
                    [](const NpcInst& n){ return n.follower; }), npcs_.end());
        toast_ = "동료가 떠났다."; toastTimer_ = 1.4f;
    }
    dlgRunLine_ = (a.gotoLine >= 0) ? a.gotoLine : dlgRunLine_ + 1;
    showDialogueLine();
}

// Spawn a dialogue-summoned NPC near the player (timed; follower follows & can be dismissed).
void GamePlay::spawnTimedNpc(int charId, int faction, float dur, bool follower) {
    if (!map_) return;
    const Database& db = engine_.project().database;
    const CharacterDef* cd = db.mob(charId);
    if (!cd) cd = db.character(charId);
    int TS = map_->tileset.tileWidth;
    int fx = destX_, fy = destY_; bool found = false;
    for (int r=1;r<=5 && !found;++r) for(int dy=-r;dy<=r&&!found;++dy) for(int dx=-r;dx<=r&&!found;++dx){
        int x=destX_+dx, y=destY_+dy;
        if (x<0||y<0||x>=map_->tilemap.width()||y>=map_->tilemap.height()) continue;
        if ((x==destX_&&y==destY_)||!walkable(x,y)||npcAt(x,y)) continue;
        fx=x; fy=y; found=true;
    }
    NpcInst n;
    n.eventId = -1000 - (int)npcs_.size();   // synthetic id (not from a map event)
    n.spriteAsset = (cd && !cd->motions[MO_Walk].frames.empty()) ? cd->motions[MO_Walk].frames.front() : -1;
    n.faction = (NpcFaction)faction;
    n.behavior = NpcBehavior::Chase;          // allies follow / enemies chase
    n.drawPct = cd ? cd->drawPct : 100;
    n.drawTilesW = cd ? std::max(1,cd->drawTilesW) : 1; n.drawTilesH = cd ? std::max(1,cd->drawTilesH) : 1;
    n.x=n.destX=n.homeX=fx; n.y=n.destY=n.homeY=fy; n.px=fx*(float)TS; n.py=fy*(float)TS;
    if (faction != (int)NpcFaction::Neutral) { n.hp=n.maxHp=cd?cd->maxHp:20; n.atk=cd?cd->atk:6; n.def=cd?cd->def:2; }
    n.lifeTimer = dur; n.follower = follower;
    npcs_.push_back(n);
    const char* lbl = faction==(int)NpcFaction::Enemy ? "적대 NPC 출현!" : follower ? "동료가 따라온다!" : "우호 NPC 출현!";
    toast_ = lbl; toastTimer_ = 1.6f;
}

void GamePlay::drawDialogueOverlay() {
    const DialogueScenario* d = engine_.project().database.dialogue(dlgRunId_);
    if (!d || dlgRunLine_ < 0 || dlgRunLine_ >= (int)d->lines.size()) return;
    const DialogueLine& ln = d->lines[dlgRunLine_];
    int sw = screenW(), sh = screenH();
    int boxH = 150;
    Rectangle box = { 40, (float)sh - boxH - 16, (float)sw - 80, (float)boxH };
    ui::panel(box, Fade(Color{ 12, 14, 22, 255 }, 0.96f));
    DrawRectangleLinesEx(box, 2, Fade(ui::kAccent, 0.7f));
    if (!ln.speaker.empty()) {
        int nw = MeasureTextU(ln.speaker.c_str(), 18) + 20;
        DrawRectangle((int)box.x + 16, (int)box.y - 16, nw, 26, ui::kAccent);
        DrawTextU(ln.speaker.c_str(), (int)box.x + 26, (int)box.y - 12, 18, BLACK);
    }
    DrawTextU(ln.text.c_str(), (int)box.x + 20, (int)box.y + 16, 20, ui::kText);

    if (!ln.answers.empty()) {
        float by = box.y + 56;
        for (int i = 0; i < (int)ln.answers.size() && i < 4; ++i) {
            Rectangle b = { box.x + 24, by, box.width - 48, 22 };
            if (ui::button(b, (std::to_string(i+1) + ". " + ln.answers[i].text).c_str(), false))
                applyDialogueAnswer(i);
            by += 24;
        }
    } else {
        DrawTextU("[Enter]", (int)(box.x + box.width - 80), (int)(box.y + box.height - 26), 16, ui::kTextDim);
    }
}

// ----------------------------- scene sequencer -----------------------------
void GamePlay::startScene(int id) {
    const Database& db = engine_.project().database;
    const Scene* sc = nullptr;
    for (const auto& s : db.scenes) if (s.id == id) sc = &s;
    if (!sc) return;
    sceneRunId_ = id; sceneStep_ = 0; sceneTimer_ = 0; sceneTags_.clear();
}

void GamePlay::updateScene(float dt) {
    if (sceneRunId_ < 0) return;
    if (phase_ == Phase::Dialogue) return;          // a scene dialogue is playing; wait
    const Database& db = engine_.project().database;
    const Scene* sc = nullptr;
    for (const auto& s : db.scenes) if (s.id == sceneRunId_) sc = &s;
    if (!sc || sceneStep_ >= (int)sc->actions.size()) { sceneRunId_ = -1; sceneStep_ = -1; return; }
    const SceneAction& a = sc->actions[sceneStep_];
    int TS = map_ ? map_->tileset.tileWidth : 32;
    bool advance = false;
    switch (a.type) {
        case SA_Wait:
            sceneTimer_ += dt; if (sceneTimer_ >= a.time) advance = true; break;
        case SA_MoveChar: {                         // tag 0 = player; else a spawned npc tag
            if (a.targetId == 0) { destX_ = a.x; destY_ = a.y; pxX_ = a.x*(float)TS; pxY_ = a.y*(float)TS; }
            else { auto it = sceneTags_.find(a.targetId);
                   if (it != sceneTags_.end()) for (auto& n : npcs_) if (n.eventId == it->second) { n.x=n.destX=a.x; n.y=n.destY=a.y; n.px=a.x*(float)TS; n.py=a.y*(float)TS; } }
            sceneTimer_ += dt; if (sceneTimer_ >= a.time) advance = true; break;
        }
        case SA_Dialogue:
            if (a.refId >= 0 && db.dialogue(a.refId)) startDialogue(a.refId);
            advance = true; break;                  // dialogue runs in its own phase; we resume after
        case SA_Effect:
            spawnFx(3, a.x*(float)TS, a.y*(float)TS, 0, a.refId, std::max(0.2f, a.time), TS*1.2f);
            sceneTimer_ += dt; if (sceneTimer_ >= a.time) advance = true; break;
        case SA_Spawn: {
            if (const CharacterDef* md = db.mob(a.refId)) {
                NpcInst n; n.eventId = -2000 - (int)npcs_.size();
                n.spriteAsset = md->motions[MO_Walk].frames.empty()? -1 : md->motions[MO_Walk].frames.front();
                n.faction = NpcFaction::Neutral; n.behavior = NpcBehavior::Idle;
                n.drawPct = md->drawPct; n.drawTilesW = std::max(1,md->drawTilesW); n.drawTilesH = std::max(1,md->drawTilesH);
                n.x=n.destX=n.homeX=a.x; n.y=n.destY=n.homeY=a.y; n.px=a.x*(float)TS; n.py=a.y*(float)TS;
                sceneTags_[a.targetId] = n.eventId; npcs_.push_back(n);
            }
            advance = true; break;
        }
        case SA_Remove: {
            auto it = sceneTags_.find(a.targetId);
            if (it != sceneTags_.end()) {
                int eid = it->second;
                npcs_.erase(std::remove_if(npcs_.begin(), npcs_.end(),
                            [eid](const NpcInst& n){ return n.eventId == eid; }), npcs_.end());
                sceneTags_.erase(it);
            }
            advance = true; break;
        }
    }
    if (advance) { ++sceneStep_; sceneTimer_ = 0; }
}

} // namespace tsukuru
