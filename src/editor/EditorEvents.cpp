// EditorEvents: event placement + inspector.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {

// Shared faction / AI / size / combat-stat rows (one source of truth for the
// Map-tab NPC inspector AND the Events-tab NPC block).
void Editor::drawNpcStatRows(Event& ev, float x, float& y, float w) {
    static const char* fac[3] = { "중립", "아군", "적군" };
    static const Color fcol[3] = { Color{200,200,200,255}, Color{120,200,255,255}, Color{255,130,130,255} };
    if (ui::button({ x, y, w, 24 }, TextFormat("진영: %s", fac[(int)ev.faction]))) {
        ev.faction = (NpcFaction)(((int)ev.faction + 1) % 3);
    }
    DrawRectangle((int)(x + w - 16), (int)y + 6, 12, 12, fcol[(int)ev.faction]);
    y += 28;
    static const char* beh[5] = { "대기", "배회", "순찰", "추격", "도망" };
    if (ui::button({ x, y, w, 24 }, TextFormat("AI 행동: %s", beh[(int)ev.behavior]))) {
        ev.behavior = (NpcBehavior)(((int)ev.behavior + 1) % 5);
    }
    y += 28;
    // tile footprint (칸): drag/click the grid; the sprite fits the chosen block
    drawFootprintControl(x, y, w, ev.drawTilesW, ev.drawTilesH, ev.drawPct, ev.graphicAsset, true);
    if (ev.faction != NpcFaction::Neutral) {
        ui::intStepper({ x, y, w, 24 }, "체력",   ev.npcHp,  5, 1, 9999); y += 26;
        ui::intStepper({ x, y, w, 24 }, "공격력", ev.npcAtk, 1, 0, 999);  y += 26;
        ui::intStepper({ x, y, w, 24 }, "방어력", ev.npcDef, 1, 0, 999);  y += 26;
        DrawTextU(ev.faction == NpcFaction::Enemy ? "적군: 추격 시 플레이어를 공격"
                                                  : "아군: 주변 적/몬스터와 싸움",
                  (int)x, (int)y, 11, ui::kTextDim); y += 18;
    } else {
        DrawTextU("중립: 전투 없음 (대화·분위기용)", (int)x, (int)y, 11, ui::kTextDim); y += 18;
    }
}

// NPC data panel: character sprite (cycle + import) + the shared stat rows +
// dialogue + delete. Used by the Map tab's NPC mode.
void Editor::drawNpcInspector(Event& ev, Rectangle panel) {
    float x = panel.x + 12, y = panel.y + 10;
    ui::label("NPC 데이터", (int)x, (int)y, 22, ui::kAccent); y += 38;

    if (ui::button({ x, y, 300, 26 }, std::string("캐릭터: ") + assetName(ev.graphicAsset), ev.graphicAsset >= 0))
        cycleAsset(ev.graphicAsset, AssetType::Image);
    y += 30;
    if (ui::button({ x, y, 300, 24 }, "캐릭터 에셋 가져오기 (외부 이미지)", true)) {
        pendingNpcEventId_ = ev.id; pendingNpcCharImport_ = true;
    }
    y += 28;
    DrawTextU("4방향(세로4×가로4) 캐릭터 시트 권장.", (int)x, (int)y, 11, ui::kTextDim); y += 22;

    drawNpcStatRows(ev, x, y, 300); y += 6;

    // --- dialogue ---
    DrawTextU("대사:", (int)x, (int)y, 13, ui::kTextDim); y += 18;
    Rectangle tf = { x, y, 300, 26 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) eventTextFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) eventTextFocus_ = false;
    ui::textField(tf, ev.text, eventTextFocus_, 120);
    y += 34;

    if (ui::button({ x, y, 300, 28 }, "NPC 삭제", false)) {
        if (auto m = activeMap()) {
            auto& evs = m->events;
            evs.erase(std::remove_if(evs.begin(), evs.end(),
                      [&](const Event& e){ return e.id == ev.id; }), evs.end());
        }
        editingEventId_ = -1;
    }
    y += 34;
    DrawTextU("정밀 설정(이동/전투/조건)은 '이벤트' 탭.", (int)x, (int)y, 11, ui::kTextDim);
}

// Player editor: pick which character drives the player + edit its core stats and
// tile footprint. Deep motion/skill editing links out to the 캐릭터 탭.
void Editor::drawPlayerEditor(Rectangle panel) {
    Project& p = engine_.project();
    auto& db = p.database;
    float x = panel.x + 12, y = panel.y + 12;
    ui::label("플레이어 데이터", (int)x, (int)y, 22, ui::kAccent); y += 40;

    CharacterDef* cd = nullptr;
    for (auto& c : db.characters) if (c.id == p.playerCharId) cd = &c;
    std::string who = cd ? cd->name : "(없음 — 시트 스프라이트)";
    if (ui::button({ x, y, 320, 28 }, std::string("플레이어 캐릭터: ") + who, cd != nullptr)) {
        int idx = -1;
        for (int i = 0; i < (int)db.characters.size(); ++i) if (db.characters[i].id == p.playerCharId) idx = i;
        idx++;
        p.playerCharId = (idx >= (int)db.characters.size()) ? -1 : db.characters[idx].id;
        p.save();
        cd = nullptr; for (auto& c : db.characters) if (c.id == p.playerCharId) cd = &c;
    }
    y += 34;
    DrawTextU("위 버튼 = 플레이어로 쓸 캐릭터 순환 선택", (int)x, (int)y, 12, ui::kTextDim); y += 24;

    if (!cd) {
        DrawTextU("캐릭터 탭에서 캐릭터를 만든 뒤 위에서 선택하세요.", (int)x, (int)y, 14, ui::kText); y += 26;
        if (ui::button({ x, y, 240, 28 }, "캐릭터 탭으로 이동")) tab_ = Tab::Chars;
        return;
    }
    ui::intStepper({ x, y, 300, 26 }, "체력 (HP)",   cd->maxHp, 10, 1, 99999); y += 32;
    ui::intStepper({ x, y, 300, 26 }, "기력 (GP)",   cd->maxGp,  5, 0, 9999);  y += 32;
    ui::intStepper({ x, y, 300, 26 }, "공격력",      cd->atk,    1, 0, 9999);  y += 32;
    ui::intStepper({ x, y, 300, 26 }, "방어력",      cd->def,    1, 0, 9999);  y += 32;
    ui::intStepper({ x, y, 300, 26 }, "속도 (이동)", cd->spd,    1, 0, 999);   y += 32;
    ui::intStepper({ x, y, 300, 26 }, "포만치 (허기)",   cd->maxHunger, 1000, 0, 999999); y += 32;
    ui::intStepper({ x, y, 300, 26 }, "수분치 (목마름)", cd->maxThirst, 1000, 0, 999999); y += 36;
    {
        int prev = cd->motions[MO_Walk].frames.empty() ? -1 : cd->motions[MO_Walk].frames.front();
        drawFootprintControl(x, y, 300, cd->drawTilesW, cd->drawTilesH, cd->drawPct, prev, false);
    }
    if (ui::button({ x, y, 300, 28 }, "캐릭터 탭에서 모션·스킬 상세 편집")) {
        for (int i = 0; i < (int)db.characters.size(); ++i) if (db.characters[i].id == cd->id) charDefSel_ = i;
        charDataEdit_ = true; tab_ = Tab::Chars;
    }
    y += 34;
    if (ui::button({ x, y, 300, 26 }, "변경사항 저장")) { p.save(); setStatus("플레이어 데이터 저장됨"); }
    y += 32;
    DrawTextU("스탯은 플레이 시작 시 파티에 적용됩니다.", (int)x, (int)y, 12, ui::kTextDim);
}

// NPC tab: one place to see & edit every NPC and mob (across all maps) plus the
// player. The left list groups them with a faction colour dot; the right panel
// edits the selected one (reusing the NPC inspector / player editor).
void Editor::drawNpcTab() {
    Project& p = engine_.project();
    Rectangle area = { 0, kToolbarH, (float)screenW(), (float)screenH() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });

    float lx = 12, ly = kToolbarH + 12, lw = 360;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("NPC · 몹 · 플레이어", (int)lx + 12, (int)ly + 10, 20, ui::kAccent);
    int npcCount = 0, mobCount = 0;
    for (auto& m : p.maps) for (auto& e : m->events) if (e.graphicAsset >= 0) {
        if (e.faction == NpcFaction::Enemy) ++mobCount; else ++npcCount;
    }
    DrawTextU(TextFormat("NPC %d개 · 몹 %d개  (클릭=편집)", npcCount, mobCount),
              (int)lx + 12, (int)ly + 38, 13, ui::kTextDim);

    Rectangle listR = { lx + 6, ly + 60, lw - 12, area.height - 24 - 60 - 6 };
    const float rowH = 30;
    int rows = 1;                                       // player + every npc/mob
    for (auto& m : p.maps) for (auto& e : m->events) if (e.graphicAsset >= 0) ++rows;
    float contentH = rows * rowH + 4;
    uiScissor((int)listR.x, (int)listR.y, (int)listR.width, (int)listR.height);
    if (ui::mouseIn(listR)) npcListScroll_ -= (int)(GetMouseWheelMove() * 42);
    int maxS = (int)std::max(0.0f, contentH - listR.height);
    npcListScroll_ = std::max(0, std::min(npcListScroll_, maxS));
    float ry = listR.y - npcListScroll_;
    // player row
    if (ry + 28 >= listR.y && ry <= listR.y + listR.height) {
        if (ui::button({ listR.x + 4, ry, listR.width - 8, 28 }, "★ 플레이어", npcTabPlayer_)) {
            npcTabPlayer_ = true; editingEventId_ = -1;
        }
    }
    ry += rowH;
    for (auto& m : p.maps) for (auto& e : m->events) {
        if (e.graphicAsset < 0) continue;
        if (ry + 28 >= listR.y && ry <= listR.y + listR.height) {
            const char* fac = e.faction == NpcFaction::Enemy ? "적"
                            : e.faction == NpcFaction::Ally  ? "아군" : "중립";
            std::string lbl = std::string("[") + m->name + "] " + assetName(e.graphicAsset) + " (" + fac + ")";
            bool sel = (!npcTabPlayer_ && activeMapId_ == m->id && editingEventId_ == e.id);
            Rectangle r = { listR.x + 4, ry, listR.width - 8, 28 };
            if (ui::button(r, lbl, sel)) { npcTabPlayer_ = false; activeMapId_ = m->id; editingEventId_ = e.id; }
            DrawCircle((int)(r.x + r.width - 13), (int)(r.y + 14), 5, ui::factionColor((int)e.faction));
        }
        ry += rowH;
    }
    EndScissorMode();

    // right editor
    Rectangle panel = { lx + lw + 16, ly, area.width - (lx + lw + 16) - 12, area.height - 24 };
    ui::panel(panel, ui::kPanel);
    if (npcTabPlayer_) {
        drawPlayerEditor(panel);
    } else {
        Event* ev = nullptr;
        if (auto m = p.map(activeMapId_)) for (auto& e : m->events) if (e.id == editingEventId_) ev = &e;
        if (ev && ev->graphicAsset >= 0) drawNpcInspector(*ev, panel);
        else {
            ui::label("좌측에서 NPC/몹/플레이어를 선택하세요.", (int)panel.x + 14, (int)panel.y + 16, 16, ui::kTextDim);
            DrawTextU("· NPC/몹: 맵에 배치된 캐릭터(진영=중립/아군/적)", (int)panel.x + 14, (int)panel.y + 48, 13, ui::kText);
            DrawTextU("· 새 NPC 추가는 '맵' 탭의 NPC 모드에서.", (int)panel.x + 14, (int)panel.y + 70, 13, ui::kTextDim);
        }
    }
    DrawTextU(kBuildTag, 12, screenH() - 22, 13, ui::kGood);
}

// ---- placeable object presets (the Map-tab 오브젝트 palette) ----
namespace {
struct ObjPreset { const char* name; EventType type; bool sprite; NpcFaction fac; TriggerType trig; const char* dft; };
static const ObjPreset kObjPresets[] = {
    { "표지판/대화",  EventType::Message,    false, NpcFaction::Neutral, TriggerType::ActionButton, "안녕하세요!" },
    { "NPC (중립)",   EventType::Message,    true,  NpcFaction::Neutral, TriggerType::ActionButton, "안녕하세요!" },
    { "NPC (아군)",   EventType::Message,    true,  NpcFaction::Ally,    TriggerType::ActionButton, "함께 싸우자!" },
    { "적 몹",        EventType::Message,    true,  NpcFaction::Enemy,   TriggerType::PlayerTouch,  "" },
    { "문/이동",      EventType::Teleport,   false, NpcFaction::Neutral, TriggerType::PlayerTouch,  "" },
    { "아이템 지급",  EventType::GiveItem,   false, NpcFaction::Neutral, TriggerType::ActionButton, "아이템을 얻었다!" },
    { "전투 발생",    EventType::StartBattle,false, NpcFaction::Neutral, TriggerType::PlayerTouch,  "" },
    { "상점",         EventType::Shop,       false, NpcFaction::Neutral, TriggerType::ActionButton, "어서 오세요!" },
    { "스위치",       EventType::SetSwitch,  false, NpcFaction::Neutral, TriggerType::ActionButton, "" },
    { "퀘스트",       EventType::Quest,      false, NpcFaction::Neutral, TriggerType::Autorun,      "목표: " },
    { "엔딩",         EventType::Ending,     false, NpcFaction::Neutral, TriggerType::Autorun,      "" },
};
static const int kObjCount = (int)(sizeof(kObjPresets)/sizeof(kObjPresets[0]));
} // namespace

// Create the currently-selected object preset on a tile (Map-tab 오브젝트 모드).
void Editor::newObjectAt(Map& m, int tx, int ty) {
    const ObjPreset& pr = kObjPresets[std::max(0, std::min(kObjCount-1, objPlaceType_))];
    Event ne; ne.id = m.nextEventId(); ne.x = tx; ne.y = ty;
    ne.type = pr.type; ne.trigger = pr.trig; ne.text = pr.dft;
    if (pr.sprite) {
        auto imgs = engine_.project().assets.byType(AssetType::Image);
        ne.graphicAsset = imgs.empty() ? -1 : imgs.front()->id;
        ne.faction = pr.fac;
        ne.behavior = (pr.fac == NpcFaction::Neutral) ? NpcBehavior::Wander : NpcBehavior::Chase;
        if (pr.fac == NpcFaction::Enemy) { ne.npcHp = 30; ne.npcAtk = 8; ne.npcDef = 2; }
    }
    m.events.push_back(ne);
    editingEventId_ = ne.id;
    setStatus(std::string("오브젝트 추가: ") + pr.name);
}

// Left palette listing every placeable object type (Map-tab 오브젝트 모드).
void Editor::drawObjectPalette(Rectangle area) {
    ui::panel(area, ui::kPanel);
    ui::label("오브젝트 종류", (int)area.x + 10, (int)area.y + 8, 16, ui::kAccent);
    DrawTextU("종류 선택 → 빈 칸 클릭=배치", (int)area.x + 10, (int)area.y + 32, 12, ui::kTextDim);
    DrawTextU("기존 클릭=편집 · 우클릭=삭제", (int)area.x + 10, (int)area.y + 48, 12, ui::kTextDim);
    float by = area.y + 72;
    for (int i = 0; i < kObjCount; ++i) {
        if (ui::button({ area.x + 8, by, area.width - 16, 26 }, kObjPresets[i].name, objPlaceType_ == i))
            objPlaceType_ = i;
        by += 30;
    }
}

// Full event editor for ANY object type + delete. Shared by the Events tab and
// the Map-tab 오브젝트 inspector.
void Editor::drawEventInspector(Event& evRef, Map& m, Rectangle panel) {
    Event* ev = &evRef;
    Database& db = engine_.project().database;
    if (ev->id != eventScrollId_) { eventScroll_ = 0; eventScrollId_ = ev->id; } // reset on select
    // The quest panel can be tall — make the whole inspector scrollable.
    Rectangle clipR = { panel.x, panel.y + 40, panel.width, panel.height - 40 };
    if (ui::mouseIn(clipR)) eventScroll_ -= GetMouseWheelMove() * 40;
    if (eventScroll_ < 0) eventScroll_ = 0;
    uiScissor((int)clipR.x, (int)clipR.y, (int)clipR.width, (int)clipR.height);
    float y = panel.y + 44 - eventScroll_;

    auto stepN = [&](const char* lbl, int& v, int s, int lo, int hi) {
        ui::intStepper({ panel.x + 12, y, 296, 24 }, lbl, v, s, lo, hi); y += 26;
    };
    auto nameHint = [&](const std::string& s, Color c) {
        DrawTextU(("→ " + s).c_str(), (int)panel.x + 16, (int)y, 12, c); y += 18;
    };

    // ---- event type, laid out as a labelled grid so every kind is visible ----
    DrawTextU("이벤트 종류 (탭에서 선택)", (int)panel.x + 12, (int)y, 13, ui::kAccentHi); y += 20;
    const char* typeNames[8] = { "메시지", "이동", "아이템지급", "스위치", "전투", "상점", "퀘스트·보상", "엔딩" };
    for (int i = 0; i < 8; ++i) {
        Rectangle b = { panel.x + 12 + (i % 2) * 150.0f, y + (i / 2) * 30.0f, 144, 26 };
        if (ui::button(b, typeNames[i], (int)ev->type == i)) ev->type = (EventType)i;
    }
    y += 4 * 30 + 8;
    const char* trigNames[3] = { "말걸기", "접촉", "자동실행" };
    for (int i = 0; i < 3; ++i)
        if (ui::button({ panel.x + 12 + i * 100.0f, y, 96, 24 }, trigNames[i], (int)ev->trigger == i))
            ev->trigger = (TriggerType)i;
    y += 32;

    const char* txtLbl = ev->type == EventType::Quest ? "안내문 (말 걸 때 대사):"
                       : ev->type == EventType::Shop  ? "상점 이름:" : "텍스트 / 대사:";
    ui::label(txtLbl, (int)panel.x + 12, (int)y, 14, ui::kTextDim); y += 18;
    Rectangle tf = { panel.x + 12, y, 296, 26 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) eventTextFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) eventTextFocus_ = false;
    ui::textField(tf, ev->text, eventTextFocus_, 120);
    y += 34;
    switch (ev->type) {
        case EventType::Teleport: {
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "대상맵", ev->targetMap, 1, -1, 999); y += 26;
            std::shared_ptr<Map> tm = engine_.project().map(ev->targetMap);
            DrawTextU(tm ? ("→ " + tm->name).c_str() : "→ (없는 맵)",
                      (int)panel.x + 16, (int)y, 12, tm ? ui::kAccentHi : ui::kDanger); y += 20;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "X", ev->targetX, 1, 0, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "Y", ev->targetY, 1, 0, 999); y += 28;
            break;
        }
        case EventType::GiveItem:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "아이템ID", ev->itemId, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "수량", ev->amount, 1, 1, 99); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "스위치설정", ev->switchId, 1, -1, 999); y += 28;
            break;
        case EventType::SetSwitch:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "스위치ID", ev->switchId, 1, 0, 999); y += 28;
            if (ui::button({ panel.x + 12, y, 296, 24 }, ev->switchValue ? "값: 켜짐" : "값: 꺼짐"))
                ev->switchValue = !ev->switchValue;
            y += 28;
            break;
        case EventType::StartBattle:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "적ID", ev->itemId, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "수", ev->amount, 1, 1, 6); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "처치스위치", ev->switchId, 1, -1, 999); y += 28;
            break;
        case EventType::Shop:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "아이템ID", ev->itemId, 1, -1, 999); y += 28;
            break;
        case EventType::Quest: {
            DrawTextU("─ 완료 조건 ─", (int)panel.x + 12, (int)y, 13, ui::kAccentHi); y += 18;
            const char* objs[4] = { "즉시 지급(대화)", "몬스터 처치", "아이템 수집", "지역 도달" };
            if (ui::button({ panel.x + 12, y, 296, 24 }, TextFormat("조건: %s", objs[ev->questObjective % 4])))
                ev->questObjective = (ev->questObjective + 1) % 4;
            y += 28;
            if (ev->questObjective == 1) {            // 처치
                stepN("대상 적ID(-1=아무거나)", ev->questTarget, 1, -1, 999);
                const EnemyDef* en = db.enemy(ev->questTarget);
                nameHint(ev->questTarget < 0 ? "아무 몬스터나" : (en ? en->name : "(없는 적)"),
                         ev->questTarget < 0 || en ? ui::kAccentHi : ui::kDanger);
                stepN("처치 수", ev->questCount, 1, 1, 99);
            } else if (ev->questObjective == 2) {     // 수집
                stepN("대상 아이템ID", ev->questTarget, 1, -1, 999);
                const Item* it = db.item(ev->questTarget);
                nameHint(it ? it->name : "(없는 아이템)", it ? ui::kAccentHi : ui::kDanger);
                stepN("수집 수", ev->questCount, 1, 1, 99);
                if (ui::button({ panel.x + 12, y, 296, 24 }, ev->questTakeItems ? "제출 시 아이템 회수: 예" : "제출 시 아이템 회수: 아니오"))
                    ev->questTakeItems = !ev->questTakeItems;
                y += 28;
            } else if (ev->questObjective == 3) {     // 도달
                stepN("대상 맵ID", ev->questTarget, 1, -1, 999);
                std::shared_ptr<Map> tm = engine_.project().map(ev->questTarget);
                nameHint(tm ? tm->name : "(없는 맵)", tm ? ui::kAccentHi : ui::kDanger);
            }
            DrawTextU("─ 보상 ─", (int)panel.x + 12, (int)y, 13, ui::kAccentHi); y += 18;
            stepN("골드", ev->rewardGold, 10, 0, 999999);
            stepN("경험치", ev->rewardExp, 5, 0, 99999);
            stepN("보상 아이템ID(-1=없음)", ev->rewardItemId, 1, -1, 999);
            if (ev->rewardItemId >= 0) {
                const Item* it = db.item(ev->rewardItemId);
                nameHint(it ? it->name : "(없는 아이템)", it ? ui::kAccentHi : ui::kDanger);
                stepN("보상 수량", ev->rewardItemCount, 1, 1, 99);
            }
            break;
        }
        case EventType::Ending:
            DrawTextU("게임 클리어 화면을 표시합니다.", (int)panel.x+12, (int)y, 12, ui::kTextDim); y += 22;
            break;
        default: break;
    }
    y += 6;
    ui::intStepper({ panel.x + 12, y, 296, 24 }, "조건스위치", ev->conditionSwitch, 1, -1, 999); y += 28;
    if (ui::button({ panel.x + 12, y, 144, 24 }, ev->once ? "1회만: 예" : "1회만: 아니오")) ev->once = !ev->once;
    if (ui::button({ panel.x + 164, y, 144, 24 }, ev->graphicAsset >= 0 ? "그래픽: 있음" : "그래픽: 없음")) {
        auto imgs = engine_.project().assets.byType(AssetType::Image);
        if (imgs.empty()) ev->graphicAsset = -1;
        else {
            int idx = -1;
            for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == ev->graphicAsset) idx = i;
            idx++;
            ev->graphicAsset = (idx >= (int)imgs.size()) ? -1 : imgs[idx]->id;
        }
    }
    y += 32;
    if (ev->graphicAsset >= 0) {
        DrawTextU("─ NPC 설정 ─", (int)panel.x + 12, (int)y, 13, ui::kAccentHi); y += 18;
        if (ui::button({ panel.x + 12, y, 296, 22 }, "캐릭터 에셋 가져오기 (외부)", true)) {
            pendingNpcEventId_ = ev->id; pendingNpcCharImport_ = true;
        }
        y += 26;
        drawNpcStatRows(*ev, panel.x + 12, y, 296);
    }
    y += 8;
    DrawTextU("트리거 '자동실행' = 맵 진입 시 1회 재생.", (int)panel.x + 12, (int)y, 12, ui::kTextDim);
    y += 22;
    if (ui::button({ panel.x + 12, y, 296, 28 }, "이벤트(오브젝트) 삭제", false)) {
        auto& evs = m.events;
        evs.erase(std::remove_if(evs.begin(), evs.end(),
                  [&](const Event& e){ return e.id == editingEventId_; }), evs.end());
        editingEventId_ = -1;
    }
    y += 36;
    EndScissorMode();
    float maxScroll = std::max(0.0f, (y + eventScroll_) - (panel.y + panel.height));
    if (eventScroll_ > maxScroll) eventScroll_ = maxScroll;
}

void Editor::drawEventsTab() {
    Rectangle canvasArea = { 0, kToolbarH, (float)screenW() - 320, (float)screenH() - kToolbarH };
    auto m = activeMap();
    uiScissor((int)canvasArea.x, (int)canvasArea.y, (int)canvasArea.width, (int)canvasArea.height);
    DrawRectangleRec(canvasArea, Color{ 24, 26, 34, 255 });
    if (m) {
        int TS = m->tileset.tileWidth;
        uiBeginWorld(cam_);
        const Texture2D& tex = engine_.assetTexture(m->tileset.assetId);
        const Tileset& set = m->tileset;
        int w = m->tilemap.width(), h = m->tilemap.height();
        Vector2 etl = GetScreenToWorld2D({ canvasArea.x, canvasArea.y }, cam_);
        Vector2 ebr = GetScreenToWorld2D({ canvasArea.x+canvasArea.width, canvasArea.y+canvasArea.height }, cam_);
        int ex0=std::max(0,(int)(etl.x/TS)-1), ey0=std::max(0,(int)(etl.y/TS)-1);
        int ex1=std::min(w-1,(int)(ebr.x/TS)+1), ey1=std::min(h-1,(int)(ebr.y/TS)+1);
        for (int layer = 0; layer < kLayerCount; ++layer)
            for (int y = ey0; y <= ey1; ++y)
                for (int x = ex0; x <= ex1; ++x) {
                    int t = m->tilemap.tile(layer, x, y);
                    if (t < 0 || set.assetId < 0) continue;
                    int sx, sy; set.srcOf(t, sx, sy);
                    DrawTexturePro(tex, { (float)sx,(float)sy,(float)set.tileWidth,(float)set.tileHeight },
                                   { (float)x*TS,(float)y*TS,(float)TS,(float)TS }, {0,0}, 0, WHITE);
                }
        for (int x = 0; x <= w; ++x) DrawLine(x*TS, 0, x*TS, h*TS, Fade(BLACK, 0.25f));
        for (int y = 0; y <= h; ++y) DrawLine(0, y*TS, w*TS, y*TS, Fade(BLACK, 0.25f));
        // event markers
        for (auto& e : m->events) {
            DrawRectangle(e.x*TS, e.y*TS, TS, TS, Fade(ui::kAccent, 0.5f));
            DrawRectangleLinesEx({ (float)e.x*TS,(float)e.y*TS,(float)TS,(float)TS }, 2,
                                 e.id == editingEventId_ ? ui::kAccentHi : ui::kAccent);
            DrawTextU(TextFormat("%d", e.id), e.x*TS+3, e.y*TS+2, 14, WHITE);
        }
        uiEndWorld();

        // click to select/create event
        if (ui::mouseIn(canvasArea) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
            int tx = (int)(world.x / TS), ty = (int)(world.y / TS);
            if (m->tilemap.inBounds(tx, ty)) {
                Event* existing = m->eventAt(tx, ty);
                if (existing) editingEventId_ = existing->id;
                else {
                    Event ne; ne.id = m->nextEventId(); ne.x = tx; ne.y = ty;
                    ne.text = "안녕하세요!";
                    m->events.push_back(ne);
                    editingEventId_ = ne.id;
                }
                eventTextFocus_ = false;
            }
        }
    }
    EndScissorMode();

    // ---- event inspector panel ----
    Rectangle panel = { (float)screenW() - 320, kToolbarH, 320, (float)screenH() - kToolbarH };
    ui::panel(panel, ui::kPanel);
    ui::label("이벤트", (int)panel.x + 12, (int)panel.y + 10, 22, ui::kAccent);
    Event* ev = nullptr;
    if (m) for (auto& e : m->events) if (e.id == editingEventId_) ev = &e;
    if (!ev) { ui::label("타일을 클릭해 추가하거나", (int)panel.x + 12, (int)panel.y + 48, 16, ui::kTextDim);
               ui::label("이벤트를 선택하세요.", (int)panel.x + 12, (int)panel.y + 68, 16, ui::kTextDim);
               return; }
    drawEventInspector(*ev, *m, panel);
}


} // namespace tsukuru
