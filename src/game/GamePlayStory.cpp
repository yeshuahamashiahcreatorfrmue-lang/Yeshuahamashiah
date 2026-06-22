// GamePlayStory: runtime playback of 대화로그 시나리오 (branching dialogue) and
// 스토리 시나리오 (scene sequencer).
#include "game/GamePlay.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "database/Database.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace tsukuru {

// editor playtest entry points (public): jump straight into the authored content.
void GamePlay::beginScene(int id)    { sceneQueue_.clear(); startScene(id); }
void GamePlay::beginDialogue(int id) { startDialogue(id); }
void GamePlay::beginSceneChain(std::vector<int> ids) {   // 첫 장면 시작, 나머지는 큐에
    sceneQueue_.clear();
    if (ids.empty()) return;
    for (size_t i = 1; i < ids.size(); ++i) sceneQueue_.push_back(ids[i]);
    startScene(ids[0]);
}

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
    // 컷신(장면) 중의 대사는 영상처럼 타이머로 자동 진행(입력 시 즉시). 선택지의 분기/보상
    // 부작용은 일으키지 않고 다음 줄로 넘어가 시네마틱 재생이 멈추지 않게 한다.
    if (sceneRunId_ >= 0) {
        if (IsKeyPressed(KEY_ESCAPE)) { dlgRunId_ = -1; phase_ = Phase::Field; previewDlgT_ = 0; return; }
        previewDlgT_ += GetFrameTime();
        if (previewDlgT_ >= 1.6f || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            previewDlgT_ = 0; ++dlgRunLine_; showDialogueLine();
        }
        return;
    }
    static const bool autodismiss = getenv("TSUKURU_AUTOWALK") != nullptr;
    bool hasAns = dlgRunLine_ < (int)d->lines.size() && !d->lines[dlgRunLine_].answers.empty();
    if (hasAns) {
        // ESC cancels the whole conversation (never leaves the player stuck);
        // number keys 1–4 pick an answer by keyboard (in addition to clicking);
        // autowalk auto-picks the first answer so headless smoke never hangs.
        int nAns = (int)d->lines[dlgRunLine_].answers.size();
        if (IsKeyPressed(KEY_ESCAPE)) { dlgRunId_ = -1; phase_ = Phase::Field; }
        else if (autodismiss) applyDialogueAnswer(0);
        else for (int k = 0; k < nAns && k < 4; ++k)
            if (IsKeyPressed(KEY_ONE + k) || IsKeyPressed(KEY_KP_1 + k)) { applyDialogueAnswer(k); break; }
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
        case DR_Scene:
            if (a.sceneId >= 0) {                 // 대화에서 시나리오(장면) 시작
                dlgRunId_ = -1; phase_ = Phase::Field;
                startScene(a.sceneId);
                return;
            }
            break;
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
    bool isMob = db.mob(charId) != nullptr;
    const CharacterDef* cd = isMob ? db.mob(charId) : db.character(charId);
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
    n.charId = cd ? cd->id : -1; n.charIsMob = isMob;   // 방향별(상하좌우) 렌더
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
    // 말하는 NPC 초상(설정돼 있으면): 박스 왼쪽 위에 표시 — 여러 NPC가 번갈아 대화하는 느낌
    float textX = box.x + 20;
    if (ln.speakerAsset >= 0) {
        const Texture2D& tex = engine_.assetTexture(ln.speakerAsset);
        if (tex.id) {
            float fw = tex.width >= tex.height*2 ? tex.width/4.0f : (float)tex.width;  // 4방향 시트면 1프레임
            float ps = 84;
            Rectangle pr = { box.x + 10, box.y - ps + 8, ps, ps };
            DrawRectangleRec(pr, Fade(Color{ 12,14,22,255 }, 0.96f));
            DrawTexturePro(tex, { 0,0,fw,(float)tex.height }, pr, {0,0}, 0, WHITE);
            DrawRectangleLinesEx(pr, 2, Fade(ui::kAccent, 0.7f));
            textX = box.x + ps + 24;
        }
    }
    if (!ln.speaker.empty()) {
        int nw = MeasureTextU(ln.speaker.c_str(), 18) + 20;
        DrawRectangle((int)textX - 4, (int)box.y - 16, nw, 26, ui::kAccent);
        DrawTextU(ln.speaker.c_str(), (int)textX + 6, (int)box.y - 12, 18, BLACK);
    }
    DrawTextU(ln.text.c_str(), (int)textX, (int)box.y + 16, 20, ui::kText);

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
    // 음악 폴백: 장면 음악 → 제목(그룹) 음악 → (없으면 맵 배경음 유지)
    int bgm = sc->bgmAsset;
    if (bgm < 0 && !sc->group.empty())
        for (size_t i = 0; i < db.sceneGroups.size(); ++i)
            if (db.sceneGroups[i] == sc->group) { if (i < db.sceneGroupBgm.size()) bgm = db.sceneGroupBgm[i]; break; }
    if (bgm >= 0) engine_.audio().playBgm(engine_.assetPath(bgm));   // playBgm은 같은 곡이면 무시
}

void GamePlay::updateScene(float dt) {
    if (sceneRunId_ < 0) return;
    if (phase_ == Phase::Dialogue) return;          // a scene dialogue is playing; wait
    // 시네마틱이 끝나거나 건너뛸 때 플레이어 조작이 깔끔히 복구되도록 정리: 이동 상태를
    // 끄고, 칸 격자에 맞춰 위치를 스냅하고, 동작을 걷기 기본으로 되돌린다.
    auto releasePlayer = [&]() {
        int ts = map_ ? map_->tileset.tileWidth : 32;
        destX_ = (int)std::lround(pxX_ / ts); destY_ = (int)std::lround(pxY_ / ts);
        pxX_ = destX_ * (float)ts; pxY_ = destY_ * (float)ts;
        moving_ = false; playMotion_ = MO_Walk; motionFrame_ = 0; motionAnim_ = 0;
        for (auto& n : npcs_) n.moving = false;
    };
    // ESC skips the rest of a cutscene (and the whole chain).
    if (IsKeyPressed(KEY_ESCAPE)) {
        sceneRunId_ = -1; sceneStep_ = -1; sceneQueue_.clear(); toast_ = "컷신 건너뜀"; toastTimer_ = 1.0f;
        releasePlayer();
        if (map_ && map_->bgmAsset >= 0) engine_.audio().playBgm(engine_.assetPath(map_->bgmAsset));
        return;
    }
    const Database& db = engine_.project().database;
    const Scene* sc = nullptr;
    for (const auto& s : db.scenes) if (s.id == sceneRunId_) sc = &s;
    if (!sc || sceneStep_ >= (int)sc->actions.size()) {
        sceneRunId_ = -1; sceneStep_ = -1;
        if (!sceneQueue_.empty()) { int nx = sceneQueue_.front(); sceneQueue_.erase(sceneQueue_.begin()); startScene(nx); }  // 다음 장면 이어재생
        else { releasePlayer();                                  // 체인 종료 → 조작 복구
            if (map_ && map_->bgmAsset >= 0) engine_.audio().playBgm(engine_.assetPath(map_->bgmAsset)); }  // 맵 배경음 복귀
        return;
    }
    int TS = map_ ? map_->tileset.tileWidth : 32;
    auto npcByTag = [&](int tag)->NpcInst* {
        auto it = sceneTags_.find(tag); if (it == sceneTags_.end()) return nullptr;
        for (auto& n : npcs_) if (n.eventId == it->second) return &n;
        return nullptr;
    };
    auto isConc = [](int t){ return t == SA_MoveChar || t == SA_Effect || t == SA_Motion; };
    const SceneAction& first = sc->actions[sceneStep_];

    // ── blocking actions (대화/대기/등장/제거): 한 번에 하나씩 ──
    if (!isConc(first.type)) {
        moving_ = false; for (auto& n : npcs_) n.moving = false;   // 멈춤 구간엔 서있기
        bool advance = false;
        switch (first.type) {
            case SA_Wait: sceneTimer_ += dt; if (sceneTimer_ >= first.time) advance = true; break;
            case SA_Dialogue:
                if (first.refId >= 0 && db.dialogue(first.refId)) startDialogue(first.refId);
                advance = true; break;
            case SA_Spawn: {
                bool isMob = first.refId >= 0; int cid = isMob ? first.refId : (-first.refId - 1);
                const CharacterDef* md = isMob ? db.mob(cid) : db.character(cid);
                if (md) {
                    NpcInst n; n.eventId = -2000 - (int)npcs_.size();
                    n.charId = cid; n.charIsMob = isMob;
                    n.spriteAsset = md->motions[MO_Walk].frames.empty() ? -1 : md->motions[MO_Walk].frames.front();
                    n.faction = NpcFaction::Neutral; n.behavior = NpcBehavior::Idle;
                    n.drawPct = md->drawPct; n.drawTilesW = std::max(1,md->drawTilesW); n.drawTilesH = std::max(1,md->drawTilesH);
                    n.x=n.destX=n.homeX=first.x; n.y=n.destY=n.homeY=first.y; n.px=first.x*(float)TS; n.py=first.y*(float)TS;
                    sceneTags_[first.targetId] = n.eventId; npcs_.push_back(n);
                }
                advance = true; break;
            }
            case SA_Remove: {
                std::vector<int> tags = first.removeTags;
                if (first.targetId >= 0) tags.push_back(first.targetId);
                for (int tag : tags) {
                    auto it = sceneTags_.find(tag); if (it == sceneTags_.end()) continue;
                    int eid = it->second;
                    npcs_.erase(std::remove_if(npcs_.begin(), npcs_.end(),
                                [eid](const NpcInst& n){ return n.eventId == eid; }), npcs_.end());
                    sceneTags_.erase(it);
                }
                advance = true; break;
            }
            default: advance = true; break;
        }
        if (advance) { ++sceneStep_; sceneTimer_ = 0; }
        return;
    }

    // ── concurrent batch: 동시에 일어나는 이동/이펙트/동작을 부드럽게 재생 ──
    // 단, 같은 대상의 '다음 이동'은 같은 배치에 넣지 않는다(웨이포인트를 건너뛰지 않고
    // 한 칸씩 이어 걷도록). 다른 대상끼리·이펙트·동작은 동시에 묶는다.
    int endi = sceneStep_; float dur = 0; std::vector<int> movedTargets;
    while (endi < (int)sc->actions.size() && isConc(sc->actions[endi].type)) {
        const SceneAction& a = sc->actions[endi];
        if (a.type == SA_MoveChar) {
            if (std::find(movedTargets.begin(), movedTargets.end(), a.targetId) != movedTargets.end()) break;
            movedTargets.push_back(a.targetId);
        }
        dur = std::max(dur, a.time); ++endi;
    }
    if (sceneTimer_ == 0.0f) {                       // 배치 시작 1회: 시작좌표 캡처·이펙트·동작 발동
        sceneMoveFrom_.clear(); sceneMoveTo_.clear(); sceneBatchDur_ = std::max(0.05f, dur);
        for (int i = sceneStep_; i < endi; ++i) {
            const SceneAction& a = sc->actions[i];
            if (a.type == SA_MoveChar) {
                Vector2 from = { (float)a.x*TS, (float)a.y*TS };
                if (a.targetId == 0) from = { pxX_, pxY_ };
                else if (NpcInst* n = npcByTag(a.targetId)) from = { n->px, n->py };
                sceneMoveFrom_[a.targetId] = from;
                sceneMoveTo_[a.targetId]   = { (float)a.x*TS, (float)a.y*TS };
            } else if (a.type == SA_Effect) {
                spawnFx(3, a.x*(float)TS, a.y*(float)TS, 0, a.refId, std::max(0.2f, a.time), TS*1.2f*std::max(1,a.radius));
            } else if (a.type == SA_Motion) {
                int mo = std::max(0, std::min((int)MO_COUNT-1, a.refId));
                if (a.targetId == 0) { playMotion_ = mo; motionFrame_ = 0; motionAnim_ = 0; }
                else if (NpcInst* n = npcByTag(a.targetId)) { n->sceneMotion = mo; n->sceneMotionT = std::max(0.2f, a.time); }
            }
        }
    }
    sceneTimer_ += dt;
    for (int i = sceneStep_; i < endi; ++i) {       // 각 이동을 자기 time에 맞춰 보간
        const SceneAction& a = sc->actions[i];
        if (a.type != SA_MoveChar) continue;
        float ut = a.time > 0 ? std::min(1.0f, sceneTimer_ / a.time) : 1.0f;
        Vector2 f = sceneMoveFrom_[a.targetId], t = sceneMoveTo_[a.targetId];
        float px = f.x + (t.x-f.x)*ut, py = f.y + (t.y-f.y)*ut;
        int dir = std::fabs(t.x-f.x) > std::fabs(t.y-f.y) ? (t.x >= f.x ? 2 : 1) : (t.y >= f.y ? 0 : 3);
        if (a.targetId == 0) {
            pxX_ = px; pxY_ = py; dir_ = dir; moving_ = (ut < 1.0f);
            // 주인공 걷기 프레임도 함께 돌려 이동 중 그림이 멈춰 보이지 않게 한다
            if (ut < 1.0f) { animTime_ += dt; if (animTime_ > 0.12f) { animTime_ = 0; frame_ = (frame_ + 1) % std::max(1, engine_.project().playerFrames); } }
            if (ut >= 1.0f) { destX_=a.x; destY_=a.y; }
        }
        else if (NpcInst* n = npcByTag(a.targetId)) {
            n->px = px; n->py = py; n->dir = dir; n->moving = (ut < 1.0f);
            n->animTime += dt; if (n->animTime > 0.12f) { n->animTime = 0; n->frame = (n->frame+1)%4; }
            if (ut >= 1.0f) { n->x=n->destX=a.x; n->y=n->destY=a.y; n->moving = false; }
        }
    }
    if (sceneTimer_ >= dur) {
        sceneStep_ = endi; sceneTimer_ = 0;
        // 다음 동작도 곧바로 이동/연출 배치면 멈춤 플래그를 초기화하지 않아(=서있는 1프레임
        // 제거) 끊김 없이 연속해서 걷게 한다. 멈춤 구간(대기/대사 등)에서만 정지한다.
        bool moreConc = endi < (int)sc->actions.size() && isConc(sc->actions[endi].type);
        if (!moreConc) { for (auto& n : npcs_) n.moving = false; moving_ = false; }
    }
}

} // namespace tsukuru
