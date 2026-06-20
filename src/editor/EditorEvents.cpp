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
    ui::intStepper({ x, y, w, 24 }, "크기(칸%, 100=1칸)", ev.drawPct, 25, 25, 400); y += 28;
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

void Editor::drawEventsTab() {
    Rectangle canvasArea = { 0, kToolbarH, (float)screenW() - 320, (float)screenH() - kToolbarH };
    auto m = activeMap();
    BeginScissorMode((int)canvasArea.x, (int)canvasArea.y, (int)canvasArea.width, (int)canvasArea.height);
    DrawRectangleRec(canvasArea, Color{ 24, 26, 34, 255 });
    if (m) {
        int TS = m->tileset.tileWidth;
        BeginMode2D(cam_);
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
        EndMode2D();

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

    float y = panel.y + 44;
    const char* typeNames[] = { "메시지", "이동", "아이템지급", "스위치설정", "전투", "상점", "퀘스트", "엔딩" };
    if (ui::button({ panel.x + 12, y, 296, 26 }, TextFormat("종류: %s", typeNames[(int)ev->type]))) {
        ev->type = (EventType)(((int)ev->type + 1) % 8);
    }
    y += 32;
    const char* trigNames[] = { "말걸기", "접촉", "자동실행" };
    if (ui::button({ panel.x + 12, y, 296, 26 }, TextFormat("트리거: %s", trigNames[(int)ev->trigger]))) {
        ev->trigger = (TriggerType)(((int)ev->trigger + 1) % 3);
    }
    y += 36;

    // text field (used by Message/GiveItem/SetSwitch/Shop)
    ui::label("텍스트:", (int)panel.x + 12, (int)y, 14, ui::kTextDim); y += 18;
    Rectangle tf = { panel.x + 12, y, 296, 26 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) eventTextFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) eventTextFocus_ = false;
    ui::textField(tf, ev->text, eventTextFocus_, 120);
    y += 34;

    // type-specific params
    switch (ev->type) {
        case EventType::Teleport:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "대상맵", ev->targetMap, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "X", ev->targetX, 1, 0, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "Y", ev->targetY, 1, 0, 999); y += 28;
            break;
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
        case EventType::Quest:
            DrawTextU("텍스트 = HUD에 표시되는 목표.", (int)panel.x+12, (int)y, 12, ui::kTextDim); y += 20;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "스위치설정", ev->switchId, 1, -1, 999); y += 28;
            break;
        case EventType::Ending:
            DrawTextU("게임 클리어 화면을 표시합니다.", (int)panel.x+12, (int)y, 12, ui::kTextDim); y += 22;
            break;
        default: break;
    }
    y += 6;
    // condition switch
    ui::intStepper({ panel.x + 12, y, 296, 24 }, "조건스위치", ev->conditionSwitch, 1, -1, 999); y += 28;
    if (ui::button({ panel.x + 12, y, 144, 24 }, ev->once ? "1회만: 예" : "1회만: 아니오")) ev->once = !ev->once;
    // graphic asset cycle
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
    // ---- NPC settings (only meaningful when the event carries a sprite) ----
    if (ev->graphicAsset >= 0) {
        DrawTextU("─ NPC 설정 ─", (int)panel.x + 12, (int)y, 13, ui::kAccentHi); y += 18;
        if (ui::button({ panel.x + 12, y, 296, 22 }, "캐릭터 에셋 가져오기 (외부)", true)) {
            pendingNpcEventId_ = ev->id; pendingNpcCharImport_ = true;
        }
        y += 26;
        drawNpcStatRows(*ev, panel.x + 12, y, 296);   // 진영/AI/크기/전투 (shared)
    }
    y += 8;
    DrawTextU("트리거 '자동실행' = 맵 진입 시 1회 재생.", (int)panel.x + 12, (int)y, 12, ui::kTextDim);
    y += 22;
    if (ui::button({ panel.x + 12, y, 296, 28 }, "이벤트 삭제", false)) {
        auto& evs = m->events;
        evs.erase(std::remove_if(evs.begin(), evs.end(),
                  [&](const Event& e){ return e.id == editingEventId_; }), evs.end());
        editingEventId_ = -1;
    }
}


} // namespace tsukuru
