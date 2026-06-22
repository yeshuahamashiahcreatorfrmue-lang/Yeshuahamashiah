// EditorStory: the 대화 (map-centric NPC dialogue) and 스토리 시나리오 (RTS-style
// scene sequencer) editor tabs. Both author data on Database (dialogues / scenes)
// and are built around a LARGE map: in 대화 you click an NPC to edit its talk; in
// 시나리오 you drag NPCs/effects on the map and pick where/who triggers the scene.
#include "editor/Editor.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include <algorithm>

namespace tsukuru {

// ============================ 대화 (map-centric) ============================
void Editor::drawDialogueTab() {
    float W = (float)screenW(), H = (float)screenH();
    DrawRectangleRec({ 0, kToolbarH, W, H - kToolbarH }, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    auto& list = db.dialogues;
    ui::label("대화 — 맵에서 NPC를 클릭해 그 NPC의 대화를 편집 (NPC 중심)", 14, (int)kToolbarH + 8, 17, ui::kAccent);
    float top = kToolbarH + 34, botY = H - 10, panelH = botY - top;

    auto npcName = [&](const Event& e)->std::string {
        if (!e.speakerName.empty()) return e.speakerName;
        if (!e.label.empty()) return e.label;
        return e.graphicAsset >= 0 ? assetName(e.graphicAsset) : ("NPC#" + std::to_string(e.id));
    };
    auto openDialogueForNpc = [&](Event& e) {
        if (e.dialogueId < 0 || !db.dialogue(e.dialogueId)) {
            DialogueScenario nd; nd.id = (int)list.size() + 1; while (db.dialogue(nd.id)) ++nd.id;
            nd.name = npcName(e) + " 대화";
            DialogueLine l0; l0.speaker = npcName(e); l0.text = e.text.empty() ? "..." : e.text; l0.speakerAsset = e.graphicAsset;
            nd.lines.push_back(l0); list.push_back(nd); e.dialogueId = nd.id; p.save();
        }
        for (int k = 0; k < (int)list.size(); ++k) if (list[k].id == e.dialogueId) dlgSel_ = k;
        dlgLineSel_ = 0; dlgFocus_ = -1; dlgPopupOpen_ = true;
    };

    if (dlgMapId_ < 0 || !p.map(dlgMapId_)) {
        for (auto& m2 : p.maps) if (m2->placed) { dlgMapId_ = m2->id; break; }
        if (dlgMapId_ < 0 && !p.maps.empty()) dlgMapId_ = p.maps.front()->id;
    }

    bool editing = dlgPopupOpen_ && dlgSel_ >= 0 && dlgSel_ < (int)list.size();
    float popW = 432.0f;
    float mapX = 8, mapW = W - 16 - (editing ? popW + 8 : 0);

    // map chooser
    std::vector<std::string> mopts; std::vector<int> mvals;
    for (auto& m2 : p.maps) { mopts.push_back(m2->name); mvals.push_back(m2->id); }
    if (!mopts.empty()) optionButton({ mapX + 4, top + 2, std::min(260.0f, mapW - 12), 24 }, "맵", mopts, mvals, dlgMapId_, 9050);

    // ---- big map: NPC markers (sprite thumbnails); click an NPC to edit ----
    Rectangle canvas = { mapX, top + 30, mapW, panelH - 30 };
    DrawRectangleRec(canvas, Color{ 18, 20, 26, 255 });
    auto m = p.map(dlgMapId_);
    const RenderTexture2D* th = m ? bestThumb(dlgMapId_) : nullptr;
    if (th && m && m->tilemap.width() > 0) {
        float tw = (float)th->texture.width, tht = (float)th->texture.height;
        float s = std::min(canvas.width / tw, canvas.height / tht);
        float pw = tw * s, ph = tht * s, bx = canvas.x + (canvas.width - pw) / 2, by = canvas.y + (canvas.height - ph) / 2;
        Rectangle imgR = { bx, by, pw, ph };
        DrawTexturePro(th->texture, { 0, 0, tw, -tht }, imgR, { 0, 0 }, 0, WHITE);
        DrawRectangleLinesEx(imgR, 1, Fade(BLACK, 0.6f));
        drawMapElementMarkers(*m, bx, by, pw, ph, false);   // 미리보기처럼 이벤트/몹/오브젝트 표시(NPC는 아래 스프라이트로)
        int mwT = m->tilemap.width(), mhT = m->tilemap.height();
        Vector2 mouse = GetMousePosition();
        bool overPopup = editing && mouse.x > mapX + mapW;
        float r = std::max(11.0f, std::min(pw / mwT, ph / mhT) * 0.7f);
        for (auto& e : m->events) {
            if (e.graphicAsset < 0 && e.charId < 0) continue;   // NPC = 등록 캐릭터 또는 그래픽
            Vector2 sp = { bx + (e.x + 0.5f) / mwT * pw, by + (e.y + 0.5f) / mhT * ph };
            bool sel = (dlgNpcEventId_ == e.id);
            int spr = eventSpriteAsset(e);
            if (spr >= 0) drawSpriteCentered(spr, sp, r * 2);    // 실게임처럼 캐릭터 이미지
            else DrawCircleV(sp, r, ui::factionColor((int)e.faction));
            if (sel) DrawCircleLines((int)sp.x, (int)sp.y, r + 3, ui::kAccentHi);
            bool hot = !overPopup && CheckCollisionPointCircle(mouse, sp, r + 2);
            if (hot) {
                DrawCircleLines((int)sp.x, (int)sp.y, r + 3, WHITE);
                DrawTextU(npcName(e).c_str(), (int)sp.x + (int)r, (int)sp.y - 8, 13, WHITE);
                if (lclick) { dlgNpcEventId_ = e.id; activeMapId_ = dlgMapId_; editingEventId_ = e.id; openDialogueForNpc(e); }
            }
        }
        DrawTextU("NPC 클릭 = 대화 편집", (int)canvas.x + 8, (int)(canvas.y + canvas.height - 22), 13, ui::kTextDim);
    } else {
        DrawTextU("맵/미리보기 없음", (int)canvas.x + 10, (int)canvas.y + 10, 14, ui::kTextDim);
    }

    if (!editing) return;

    // ---- docked edit popup (web-like; appears on NPC click) ----
    Rectangle pop = { W - popW - 8, top, popW, panelH };
    ui::panel(pop, ui::kPanelHi);
    DrawRectangleLinesEx(pop, 2, ui::kAccent);
    DialogueScenario& d = list[dlgSel_];
    Event* npc = nullptr; if (m) for (auto& e : m->events) if (e.id == dlgNpcEventId_ && e.graphicAsset >= 0) npc = &e;
    float px = pop.x + 10, pw2 = pop.width - 20, py = pop.y + 8;
    DrawTextU((npc ? ("대화 편집 — " + npcName(*npc)) : ("대화: " + d.name)).c_str(), (int)px, (int)py, 15, ui::kAccent);
    if (ui::button({ pop.x + pop.width - 158, pop.y + 6, 86, 24 }, "테스트")) {
        engine_.startPlaytestDialogue(d.id, dlgMapId_, npc ? npc->x : -1, npc ? npc->y : -1); return;
    }
    if (ui::button({ pop.x + pop.width - 66, pop.y + 6, 58, 24 }, "닫기")) { dlgPopupOpen_ = false; return; }
    py += 28;
    if (npc) {
        if (ui::button({ px, py, pw2 / 2 - 4, 24 }, "등록된 캐릭터")) {
            int mid = dlgMapId_, eid = npc->id;
            openCharBrowser([this, mid, eid](int cid){ applyCharToNpc(mid, eid, cid); });
        }
        Rectangle nf = { px + pw2 / 2 + 4, py, pw2 / 2 - 4, 24 };
        if (ui::mouseIn(nf) && lclick) dlgFocus_ = 50; else if (lclick && !ui::mouseIn(nf) && dlgFocus_ == 50) dlgFocus_ = -1;
        ui::textField(nf, npc->speakerName, dlgFocus_ == 50, 40);
        py += 30;
    }
    // line list (chips)
    DrawTextU(TextFormat("대사 라인 (%d)", (int)d.lines.size()), (int)px, (int)py, 12, ui::kAccentHi);
    if (ui::button({ px + pw2 - 70, py - 2, 70, 22 }, "+ 라인")) { d.lines.push_back({ "", "...", -1, {} }); dlgLineSel_ = (int)d.lines.size() - 1; p.save(); }
    py += 20;
    Rectangle lreg = { px, py, pw2, 96 };
    uiScissor((int)lreg.x, (int)lreg.y, (int)lreg.width, (int)lreg.height);
    if (ui::mouseIn(lreg)) dlgLineScroll_ -= GetMouseWheelMove() * 30;
    if (dlgLineScroll_ < 0) dlgLineScroll_ = 0;
    float ly = py - dlgLineScroll_;
    for (int i = 0; i < (int)d.lines.size(); ++i) {
        if (ly + 24 > py && ly < py + lreg.height) {
            std::string lbl = std::to_string(i + 1) + ". " + (d.lines[i].speaker.empty() ? "" : ("[" + d.lines[i].speaker + "] ")) + d.lines[i].text;
            if ((int)lbl.size() > 30) lbl = lbl.substr(0, 30) + "..";
            if (ui::button({ px, ly, pw2 - 78, 22 }, lbl, dlgLineSel_ == i)) { dlgLineSel_ = i; dlgFocus_ = -1; }
            if (ui::button({ px + pw2 - 76, ly, 22, 22 }, "위")) { if (i > 0) { std::swap(d.lines[i], d.lines[i-1]); if (dlgLineSel_==i) dlgLineSel_=i-1; else if (dlgLineSel_==i-1) dlgLineSel_=i; p.save(); } }
            if (ui::button({ px + pw2 - 52, ly, 26, 22 }, "아래")) { if (i+1 < (int)d.lines.size()) { std::swap(d.lines[i], d.lines[i+1]); if (dlgLineSel_==i) dlgLineSel_=i+1; else if (dlgLineSel_==i+1) dlgLineSel_=i; p.save(); } }
            if (ui::button({ px + pw2 - 24, ly, 24, 22 }, "x")) { d.lines.erase(d.lines.begin()+i); if (dlgLineSel_ >= (int)d.lines.size()) dlgLineSel_ = (int)d.lines.size()-1; p.save(); EndScissorMode(); return; }
        }
        ly += 26;
    }
    EndScissorMode();
    py += lreg.height + 6;
    DrawLine((int)px, (int)py, (int)(px + pw2), (int)py, ui::kPanel); py += 6;
    if (dlgLineSel_ < 0 && !d.lines.empty()) dlgLineSel_ = 0;
    if (dlgLineSel_ < 0 || dlgLineSel_ >= (int)d.lines.size()) return;
    DialogueLine& ln = d.lines[dlgLineSel_];
    DrawTextU(TextFormat("라인 %d — 말하는 NPC", dlgLineSel_ + 1), (int)px, (int)py, 12, ui::kTextDim); py += 16;
    {
        std::vector<std::string> so, sv;
        if (m) for (auto& e : m->events) if (e.graphicAsset >= 0) { std::string nm = npcName(e); so.push_back(nm); sv.push_back(nm); }
        float dw = so.empty() ? 0 : 140;
        Rectangle sf = { px, py, pw2 - (dw > 0 ? dw + 4 : 0), 24 };
        if (ui::mouseIn(sf) && lclick) dlgFocus_ = 1; else if (lclick && !ui::mouseIn(sf) && dlgFocus_ == 1) dlgFocus_ = -1;
        ui::textField(sf, ln.speaker, dlgFocus_ == 1, 30);
        if (dw > 0) optionButtonStr({ px + pw2 - dw, py, dw, 24 }, "NPC", so, sv, ln.speaker, 9070);
        if (m) for (auto& e : m->events) if (e.graphicAsset >= 0 && npcName(e) == ln.speaker) ln.speakerAsset = e.graphicAsset;
    }
    py += 28;
    Rectangle tf = { px, py, pw2, 24 };
    if (ui::mouseIn(tf) && lclick) dlgFocus_ = 2; else if (lclick && !ui::mouseIn(tf) && dlgFocus_ == 2) dlgFocus_ = -1;
    ui::textField(tf, ln.text, dlgFocus_ == 2, 200);
    py += 30;
    DrawTextU(TextFormat("대답 (%d) — 비우면 다음 라인", (int)ln.answers.size()), (int)px, (int)py, 12, ui::kAccentHi);
    if (ui::button({ px + pw2 - 70, py - 2, 70, 22 }, "+ 대답")) { ln.answers.push_back({}); p.save(); }
    py += 20;
    Rectangle areg = { px, py, pw2, pop.y + pop.height - py - 8 };
    uiScissor((int)areg.x, (int)areg.y, (int)areg.width, (int)areg.height);
    if (ui::mouseIn(areg)) dlgAnsScroll_ -= GetMouseWheelMove() * 36;
    if (dlgAnsScroll_ < 0) dlgAnsScroll_ = 0;
    float ay = py - dlgAnsScroll_;
    for (int i = 0; i < (int)ln.answers.size(); ++i) {
        DialogueAnswer& a = ln.answers[i];
        float cardH = 158;
        if (ay + cardH > py - 30 && ay < py + areg.height) {
            ui::panel({ px - 2, ay, pw2 + 4, cardH - 6 }, ui::kPanel);
            float ix = px + 6, iw = pw2 - 12, iy = ay + 6;
            Rectangle af = { ix, iy, iw - 34, 24 }; int fid = 100 + i;
            if (ui::mouseIn(af) && lclick) dlgFocus_ = fid; else if (lclick && !ui::mouseIn(af) && dlgFocus_ == fid) dlgFocus_ = -1;
            ui::textField(af, a.text, dlgFocus_ == fid, 60);
            if (ui::button({ ix + iw - 30, iy, 30, 24 }, "x")) { ln.answers.erase(ln.answers.begin()+i); p.save(); EndScissorMode(); return; }
            iy += 28;
            optionButton({ ix, iy, 150, 24 }, "대응",
                { kDlgRespNames[0],kDlgRespNames[1],kDlgRespNames[2],kDlgRespNames[3],kDlgRespNames[4],kDlgRespNames[5],kDlgRespNames[6] },
                {}, a.respType, 3000 + i);
            ui::intStepper({ ix + 158, iy, iw - 158, 24 }, "→라인", a.gotoLine, 1, -1, 99); iy += 28;
            if (a.respType == DR_Reward) {
                ui::intStepper({ ix, iy, iw/2-4, 24 }, "골드", a.rewardGold, 10, 0, 99999);
                ui::intStepper({ ix+iw/2+4, iy, iw/2-4, 24 }, "경험", a.rewardExp, 5, 0, 99999); iy += 28;
                entityButton({ ix, iy, iw/2-4, 24 }, "아이템", a.rewardItemId, ENT_Item, 3100+i);
                ui::intStepper({ ix+iw/2+4, iy, iw/2-4, 24 }, "개수", a.rewardItemCount, 1, 1, 999); iy += 28;
            } else if (a.respType == DR_SpawnMob) {
                entityButton({ ix, iy, iw, 24 }, "몹", a.mobId, ENT_Mob, 3200+i); iy += 28;
            } else if (a.respType == DR_NpcHostile || a.respType == DR_NpcFriendly || a.respType == DR_NpcFollow) {
                entityButton({ ix, iy, iw/2-4, 24 }, "NPC", a.npcCharId, ENT_Mob, 3300+i);
                int ms = (int)(a.durationSecs*1000);
                if (ui::intStepper({ ix+iw/2+4, iy, iw/2-4, 24 }, "시간(s·0=무제한)", ms, 1000, 0, 600000)) a.durationSecs = ms/1000.0f;
                iy += 28;
                if (a.respType == DR_NpcFollow)
                    if (ui::button({ ix, iy, iw, 24 }, a.dismissFollowers ? "추종 해제 대답: 켜짐" : "추종 해제 대답: 꺼짐", a.dismissFollowers)) { a.dismissFollowers = !a.dismissFollowers; p.save(); }
            } else if (a.respType == DR_Scene) {
                entityButton({ ix, iy, iw, 24 }, "시나리오", a.sceneId, ENT_Scene, 3400+i); iy += 28;
            }
        }
        ay += cardH;
    }
    EndScissorMode();
}

// ============================ 스토리 시나리오 (RTS-style, big map) ============================
void Editor::drawScenarioTab() {
    float W = (float)screenW(), H = (float)screenH();
    DrawRectangleRec({ 0, kToolbarH, W, H - kToolbarH }, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    auto& list = db.scenes;
    ui::label("시나리오 — 맵에서 NPC/이펙트를 끌어 배치하고, 발동 지점·NPC를 지정 (RTS식)", 14, (int)kToolbarH + 8, 16, ui::kAccent);
    float top = kToolbarH + 34, botY = H - 10, panelH = botY - top;
    float leftW = 198, leftX = 8;
    float ctrlW = 262, ctrlX = W - ctrlW - 8;
    float mapX = leftX + leftW + 8, mapW = ctrlX - mapX - 8;
    ui::panel({ leftX, top, leftW, panelH }, ui::kPanel);
    ui::panel({ ctrlX, top, ctrlW, panelH }, ui::kPanel);

    // ---- LEFT panel: 제목(그룹)별 장면 목록 ----
    //   장면 = 클릭선택·▶재생 · 드래그→다른 제목으로 이동
    //   제목 = 클릭선택 · ▶전체 미리보기 · 드래그→맵에 그 제목의 발동지점 등록
    auto inGroups = [&](const std::string& g){ return std::find(db.sceneGroups.begin(), db.sceneGroups.end(), g) != db.sceneGroups.end(); };
    {
        float lx = leftX + 8, lw = leftW - 16, ly = top + 8;
        DrawTextU("시나리오 제목 / 장면", (int)lx, (int)ly, 13, ui::kAccent); ly += 18;
        if (ui::button({ lx, ly, lw, 20 }, "+ 새 제목(그룹)")) {
            int n = (int)db.sceneGroups.size() + 1; std::string nm;
            do { nm = "제목 " + std::to_string(n++); } while (inGroups(nm));
            db.sceneGroups.push_back(nm); scnGroupSel_ = nm; p.save();
        }
        ly += 24;
        DrawTextU("장면 드래그→제목이동 · 제목 드래그→맵 발동", (int)lx, (int)ly, 9, ui::kTextDim); ly += 14;
        Rectangle reg = { leftX, ly, leftW, top + panelH - ly - 8 };
        uiScissor((int)leftX, (int)ly, (int)leftW, (int)reg.height);
        if (ui::mouseIn(reg)) scnListScroll_ -= GetMouseWheelMove() * 28;
        if (scnListScroll_ < 0) scnListScroll_ = 0;
        float y = ly - scnListScroll_;
        std::vector<std::pair<Rectangle,std::string>> headerHits;   // 장면 재분류 드롭 대상
        std::vector<std::string> headers = db.sceneGroups; headers.push_back(std::string());  // "" = 미분류
        for (auto& g : headers) {
            bool bucket = g.empty();
            std::vector<int> ids;
            for (int i = 0; i < (int)list.size(); ++i) {
                bool member = bucket ? (list[i].group.empty() || !inGroups(list[i].group)) : (list[i].group == g);
                if (member) ids.push_back(i);
            }
            // '미분류'는 맨 아래에 따로 분리(간격 + 구분선) — 비어 있어도 항상 표시(드롭 대상)
            if (bucket) {
                y += 12;
                if (y + 2 > ly && y < ly + reg.height) DrawLine((int)lx, (int)y-4, (int)(lx+lw), (int)y-4, Fade(ui::kAccent, 0.45f));
            }
            bool selG = (!bucket && scnGroupSel_ == g);
            Rectangle hr = { lx, y, lw - 44, 22 };
            headerHits.push_back({ hr, g });
            if (y + 22 > ly && y < ly + reg.height) {
                DrawRectangleRec(hr, selG ? ui::kAccent : (bucket ? Color{40,42,50,255} : ui::kPanelHi));
                DrawTextU((bucket ? "미분류" : g).c_str(), (int)lx + 4, (int)y + 4, 12, selG ? BLACK : (bucket ? ui::kTextDim : ui::kAccentHi));
                if (scnSceneDragIdx_ >= 0 && scnLpDragging_ && ui::mouseIn(hr)) DrawRectangleLinesEx(hr, 2, WHITE);
                if (!bucket && ui::mouseIn(hr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    scnGroupSel_ = g; scnGroupTrigDrag_ = g; scnLpDragStart_ = GetMousePosition(); scnLpDragging_ = false;
                }
                if (!ids.empty() && ui::button({ lx + lw - 42, y, 42, 22 }, "▶전체")) {
                    std::vector<int> sids; for (int idx : ids) sids.push_back(list[idx].id);
                    engine_.startPlaytestScenes(sids); EndScissorMode(); return;   // 전체화면 시네마틱 재생
                }
            }
            y += 26;
            for (int idx : ids) {
                if (y + 22 > ly && y < ly + reg.height) {
                    Rectangle nameR = { lx + 12, y, lw - 52, 22 };
                    if (ui::button(nameR, list[idx].name, scnSel_ == idx)) { scnSel_ = idx; scnActSel_ = -1; scnObjSel_ = -1; scnGroupSel_ = list[idx].group; }
                    if (ui::mouseIn(nameR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        scnSceneDragIdx_ = idx; scnLpDragStart_ = GetMousePosition(); scnLpDragging_ = false;
                    }
                    // 우클릭 = 그 녹화본을 보면서 수정하는 '스튜디오'(전체화면 재생 후 F2로 편집 복귀)
                    if (ui::mouseIn(nameR) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                        scnSel_ = idx; scnActSel_ = -1; engine_.startPlaytestScene(list[idx].id); EndScissorMode(); return;
                    }
                    if (ui::button({ lx + lw - 38, y, 38, 22 }, "▶")) { engine_.startPlaytestScene(list[idx].id); EndScissorMode(); return; }
                }
                y += 24;
            }
        }
        EndScissorMode();
        if (list.empty() && db.sceneGroups.empty()) DrawTextU("'+ 새 제목'으로 시작", (int)lx, (int)ly + 4, 12, ui::kTextDim);

        // 장면을 끌어 제목 헤더에 놓으면 그 제목으로 이동(재분류)
        if (scnSceneDragIdx_ >= 0) {
            Vector2 mp = GetMousePosition();
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                if (!scnLpDragging_ && std::abs(mp.x-scnLpDragStart_.x)+std::abs(mp.y-scnLpDragStart_.y) > 6) scnLpDragging_ = true;
                if (scnLpDragging_ && scnSceneDragIdx_ < (int)list.size()) {
                    std::string nm = list[scnSceneDragIdx_].name; int tw = MeasureTextU(nm.c_str(),12)+12;
                    DrawRectangle((int)mp.x+8,(int)mp.y-10,tw,18,Fade(BLACK,0.85f));
                    DrawTextU(nm.c_str(),(int)mp.x+12,(int)mp.y-8,12,ui::kAccentHi);
                }
            } else {
                if (scnLpDragging_ && scnSceneDragIdx_ < (int)list.size())
                    for (auto& hh : headerHits) if (CheckCollisionPointRec(mp, hh.first)) {
                        list[scnSceneDragIdx_].group = hh.second; p.save();
                        setStatus(hh.second.empty()?"미분류로 이동":("제목 이동: "+hh.second)); break;
                    }
                scnSceneDragIdx_ = -1; scnLpDragging_ = false;
            }
        }
    }

    // ---- RIGHT control panel ----
    float cx = ctrlX + 10, cw = ctrlW - 20, cy = top + 8;
    // scene picker + new
    std::vector<std::string> sopt; std::vector<int> sval;
    for (int i = 0; i < (int)list.size(); ++i) { sopt.push_back(list[i].name); sval.push_back(i); }
    auto createScene = [&]{
        Scene s; s.id = 1; for (auto& e : list) if (e.id >= s.id) s.id = e.id + 1;   // 고유 id
        s.name = "장면" + std::to_string(s.id);
        s.group.clear();   // 새 장면/녹화본은 '제목없음'으로 들어감 → 드래그로 제목에 추가
        for (auto& mm : p.maps) if (mm->placed) { s.editMapId = mm->id; break; }
        if (s.editMapId < 0 && !p.maps.empty()) s.editMapId = p.maps.front()->id;
        list.push_back(s); scnSel_ = (int)list.size()-1; scnActSel_ = -1; scnObjSel_ = -1; scnAwaitDest_ = false; p.save();
    };
    if (!list.empty()) {
        if (scnSel_ < 0 || scnSel_ >= (int)list.size()) scnSel_ = 0;
        optionButton({ cx, cy, cw - 30, 26 }, "", sopt, sval, scnSel_, 4400);
    } else DrawTextU("장면(시나리오) 없음", (int)cx, (int)cy + 4, 13, ui::kTextDim);
    if (ui::button({ cx + cw - 26, cy, 26, 26 }, "+")) createScene();
    cy += 32;
    if (scnSel_ < 0 || scnSel_ >= (int)list.size()) {
        // ── empty state: never leave the tab blank — show the map + a big create button ──
        if (ui::button({ cx, cy, cw, 34 }, "+ 새 장면 만들기")) createScene();
        cy += 42;
        DrawTextU("새 장면을 만들면 왼쪽 큰 지도에서", (int)cx, (int)cy, 12, ui::kTextDim); cy += 16;
        DrawTextU("· NPC/몹/이펙트를 끌어 배치", (int)cx, (int)cy, 12, ui::kTextDim); cy += 16;
        DrawTextU("· '장면 녹화'로 장면을 이어 제작", (int)cx, (int)cy, 12, ui::kTextDim); cy += 16;
        DrawTextU("· 발동 지점/NPC도 지정", (int)cx, (int)cy, 12, ui::kTextDim);
        Rectangle canvas = { mapX, top, mapW, panelH };
        DrawRectangleRec(canvas, Color{ 18, 20, 26, 255 });
        std::shared_ptr<Map> bm; for (auto& mm : p.maps) if (mm->placed) { bm = mm; break; }
        if (!bm && !p.maps.empty()) bm = p.maps.front();
        const RenderTexture2D* bt = bm ? bestThumb(bm->id) : nullptr;
        if (bt && bm->tilemap.width() > 0) {
            float tw=(float)bt->texture.width, tht=(float)bt->texture.height;
            float s=std::min(canvas.width/tw, canvas.height/tht);
            float pw=tw*s, ph=tht*s, bx=canvas.x+(canvas.width-pw)/2, by=canvas.y+(canvas.height-ph)/2;
            DrawTexturePro(bt->texture, {0,0,tw,-tht}, {bx,by,pw,ph}, {0,0}, 0, Fade(WHITE,0.5f));
        }
        DrawTextU("← 오른쪽 위 '+ 새 장면 만들기'로 시작", (int)canvas.x+16, (int)(canvas.y+canvas.height/2), 18, ui::kAccentHi);
        return;
    }
    Scene& sc = list[scnSel_];
    if (sc.editMapId < 0 || !p.map(sc.editMapId)) {
        for (auto& mm : p.maps) if (mm->placed) { sc.editMapId = mm->id; break; }
        if (sc.editMapId < 0 && !p.maps.empty()) sc.editMapId = p.maps.front()->id;
    }
    if (scnLive_) { drawLiveRecorder(sc); return; }   // 라이브 RTS 녹화 화면(전체)
    { // name + delete
        Rectangle nf = { cx, cy, cw - 56, 24 };
        if (ui::mouseIn(nf) && lclick) scnFocus_ = 0; else if (lclick && !ui::mouseIn(nf) && scnFocus_ == 0) scnFocus_ = -1;
        ui::textField(nf, sc.name, scnFocus_ == 0, 40);
        if (ui::button({ cx + cw - 50, cy, 50, 24 }, "삭제")) { list.erase(list.begin()+scnSel_); scnSel_=-1; scnActSel_=-1; scnObjSel_=-1; scnAwaitDest_=false; p.save(); return; }
    }
    cy += 30;
    // ── 제목(그룹): 이 장면이 속한 제목 + 선택 제목 이름변경/삭제 ──
    {
        DrawTextU(("제목: " + (sc.group.empty()?std::string("미분류"):sc.group)).c_str(), (int)cx, (int)cy, 11, ui::kAccentHi);
        cy += 16;
        int gi = -1; for (int i=0;i<(int)db.sceneGroups.size();++i) if (db.sceneGroups[i]==scnGroupSel_) gi=i;
        if (gi >= 0) {
            Rectangle gf = { cx, cy, cw - 56, 22 };
            bool foc = (scnGroupRenameFocus_ == gi);
            if (ui::mouseIn(gf) && lclick) scnGroupRenameFocus_ = gi;
            else if (lclick && !ui::mouseIn(gf) && foc) scnGroupRenameFocus_ = -1;
            std::string old = db.sceneGroups[gi], edited = old;
            ui::textField(gf, edited, foc, 30);
            if (edited != old && !edited.empty()) {   // 이름 변경을 장면·발동이벤트에 전파
                for (auto& s2 : list) if (s2.group == old) s2.group = edited;
                for (auto& mm : p.maps) for (auto& e : mm->events) if (e.sceneGroup == old) e.sceneGroup = edited;
                db.sceneGroups[gi] = edited; scnGroupSel_ = edited; p.save();
            }
            if (ui::button({ cx + cw - 50, cy, 50, 22 }, "제목삭제")) {
                for (auto& s2 : list) if (s2.group == old) s2.group.clear();      // 장면은 미분류로
                for (auto& mm : p.maps) mm->events.erase(std::remove_if(mm->events.begin(), mm->events.end(),
                    [&](const Event& e){ return e.sceneGroup == old; }), mm->events.end());   // 발동 제거
                db.sceneGroups.erase(db.sceneGroups.begin()+gi); scnGroupSel_.clear(); p.save();
            }
            cy += 26;
        } else { DrawTextU("(좌측에서 제목을 클릭하면 이름변경)", (int)cx, (int)cy, 10, ui::kTextDim); cy += 16; }
    }
    // resolve a spawn's entity name (refId>=0 → 몹, refId<=-2 → 등록 캐릭터)
    auto spawnEntName = [&](int refId)->std::string {
        if (refId <= -2) { const CharacterDef* c = db.character(-refId-1); return c ? c->name : "캐릭터"; }
        const CharacterDef* md = db.mob(refId); return md ? md->name : "몹";
    };
    auto spawnTagLabel = [&](int tag)->std::string {
        for (auto& a : sc.actions) if (a.type == SA_Spawn && a.targetId == tag)
            return "#" + std::to_string(tag) + " " + spawnEntName(a.refId);
        return "#" + std::to_string(tag);
    };

    // ── simulate the "stage": each tag's position after all recorded actions ──
    auto m0 = p.map(sc.editMapId);
    std::unordered_map<int, Vector2> stage;
    int mcx = m0 ? m0->tilemap.width()/2 : 0, mcy = m0 ? m0->tilemap.height()/2 : 0;
    stage[0] = { (float)mcx, (float)mcy };                 // 플레이어(태그0) 기본 위치
    for (auto& a : sc.actions) {
        if (a.type == SA_Spawn || a.type == SA_MoveChar) stage[a.targetId] = { (float)a.x, (float)a.y };
        else if (a.type == SA_Remove) { for (int t : a.removeTags) stage.erase(t); if (a.targetId >= 0) stage.erase(a.targetId); }
    }

    // ── 시나리오 배치·녹화 진입: NPC/유닛을 배치하고 실시간으로 조종하며 장면을 녹화 ──
    scnRecordMode_ = false;   // (구) 스냅샷 배치모드 제거 — 시나리오 배치·녹화로 일원화
    if (ui::button({ cx, cy, cw, 30 }, "▶ RTS 녹화 시작")) {
        scnLive_ = true; scnLiveInit_ = false; scnRecording_ = false; scnRecClock_ = 0;
        liveUnits_.clear(); recCmds_.clear(); liveSel_.clear();
    }
    cy += 34;
    // ── 녹화본 사이에 끼울 '대화 장면' 추가 → 대화 탭에서 대사 작성 ──
    if (ui::button({ cx, cy, cw, 26 }, "+ 대화 장면 (녹화본 사이 대사)")) {
        DialogueScenario nd; nd.id=(int)db.dialogues.size()+1; while(db.dialogue(nd.id))++nd.id;
        nd.name="대화"+std::to_string(nd.id); nd.lines.push_back({ "", "...", -1, {} });
        db.dialogues.push_back(nd);
        Scene ds; ds.id=1; for(auto&e:list) if(e.id>=ds.id) ds.id=e.id+1;
        ds.name="대화 "+std::to_string(ds.id); ds.group.clear(); ds.editMapId=sc.editMapId;
        SceneAction a; a.type=SA_Dialogue; a.refId=nd.id; a.time=0; ds.actions.push_back(a);
        list.push_back(ds); scnSel_=(int)list.size()-1; scnActSel_=0;
        for (int k=0;k<(int)db.dialogues.size();++k) if (db.dialogues[k].id==nd.id) dlgSel_=k;
        dlgLineSel_=0; dlgPopupOpen_=true; tab_=Tab::Dialogue; p.save();
        setStatus("대화 장면 추가 — 대화 탭에서 대사 작성(미분류)"); return;
    }
    cy += 30;
    // ── 시점(카메라): 재생 시 화면이 어디를 어느 배율로 비출지 ──
    DrawTextU("시점(카메라):", (int)cx, (int)cy, 12, ui::kAccentHi); cy += 18;
    { std::vector<std::string> o = { "플레이어중심","전체맵","특정위치","특정유닛중심" };
      optionButton({ cx, cy, cw, 24 }, "", o, {}, sc.camMode, 4600); cy += 28; }
    if (sc.camMode != 1) {
        int z = (int)(sc.camZoom*100+0.5f); if (z<25) z=25; if (z>400) z=400;
        ui::intStepper({ cx, cy, cw, 24 }, "확대(%)", z, 25, 25, 400); sc.camZoom = z/100.0f; cy += 28;
    }
    if (sc.camMode == 2) {
        DrawTextU(TextFormat("위치: %d,%d", sc.camX, sc.camY), (int)cx, (int)cy, 11, ui::kTextDim);
        if (ui::button({ cx+cw-110, cy-2, 110, 20 }, "지도에서 지정", scnCamPick_)) scnCamPick_ = !scnCamPick_;
        cy += 22;
    }
    if (sc.camMode == 3) {
        std::vector<std::string> o = { "플레이어(0)" }; std::vector<int> v = { 0 };
        for (auto& a : sc.actions) if (a.type==SA_Spawn) { o.push_back(spawnTagLabel(a.targetId)); v.push_back(a.targetId); }
        optionButton({ cx, cy, cw, 24 }, "중심 유닛", o, v, sc.camTag, 4610); cy += 28;
    }
    if (scnRecordMode_) {
        DrawTextU("무대에서 토큰을 끌어 배치 → '장면 녹화'로 한 장면 기록", (int)cx, (int)cy, 11, ui::kAccentHi); cy += 16;
        // step duration 0.42~1.42
        { int csd = (int)(scnStepDur_*100+0.5f); if (csd<42) csd=42; if (csd>142) csd=142;
          ui::intStepper({ cx, cy, cw, 24 }, "장면시간(×0.01초)", csd, 10, 42, 142); scnStepDur_ = csd/100.0f; }
        cy += 28;
        // 무대에 몹 등장 추가(중앙)
        entityButton({ cx, cy, cw/2-4, 24 }, "몹", scnRecMob_, ENT_Mob, 4700);
        if (ui::button({ cx+cw/2+2, cy, cw/2-4, 24 }, "+ 몹 등장")) {
            int tag = 1; for (auto& a : sc.actions) if (a.type==SA_Spawn && a.targetId>=tag) tag = a.targetId+1;
            SceneAction sp; sp.type = SA_Spawn; sp.targetId = tag; sp.refId = scnRecMob_; sp.x = mcx; sp.y = mcy;
            sc.actions.push_back(sp); p.save();
        }
        cy += 28;
        // 등록된 캐릭터에서 골라 맵에 끌어다 등장 (윈도우 탐색기식 브라우저)
        if (ui::button({ cx, cy, cw, 24 }, scnRecPlaceChar_>=0 ? "맵에 클릭해서 등장 배치…" : "등록 캐릭터에서 등장(목록·검색)", scnRecPlaceChar_>=0)) {
            if (scnRecPlaceChar_ >= 0) scnRecPlaceChar_ = -1;     // 토글 취소
            else openCharBrowser([this](int cid){ scnRecPlaceChar_ = cid; });  // 고른 뒤 맵 클릭으로 배치
        }
        cy += 28;
        // 이펙트 뿌리기: 오른쪽 목록에서 선택 후 맵 클릭
        assetButton({ cx, cy, cw/2-4, 24 }, "이펙트", scnRecEffect_, 4710);
        ui::intStepper({ cx+cw/2+2, cy, cw/2-4, 24 }, "반경", scnRecRadius_, 1, 1, 30); cy += 28;
        DrawTextU(TextFormat("대기 이펙트 %d개 (맵 클릭=뿌리기)", (int)scnPendingFx_.size()), (int)cx, (int)cy, 11, ui::kTextDim); cy += 16;
        // 녹화 / 취소
        if (ui::button({ cx, cy, cw, 28 }, "★ 장면 녹화 (현재 배치 기록)")) {
            int rec = 0;
            for (auto& kv : stage) {
                auto it = scnDraft_.find(kv.first);
                if (it != scnDraft_.end() && (it->second.x != kv.second.x || it->second.y != kv.second.y)) {
                    SceneAction mv; mv.type = SA_MoveChar; mv.targetId = kv.first;
                    mv.x = (int)it->second.x; mv.y = (int)it->second.y; mv.time = scnStepDur_;
                    sc.actions.push_back(mv); ++rec;
                }
            }
            for (auto& fx : scnPendingFx_) { fx.time = scnStepDur_; sc.actions.push_back(fx); ++rec; }
            scnPendingFx_.clear(); scnDraft_.clear();
            p.save(); setStatus(TextFormat("장면 녹화됨 (+%d 동작)", rec));
        }
        cy += 32;
        if (ui::button({ cx, cy, cw, 22 }, "마지막 녹화 취소")) {
            // 직전 녹화로 추가된 연속 이동/이펙트 블록을 통째로 제거
            while (!sc.actions.empty() &&
                   (sc.actions.back().type == SA_MoveChar || sc.actions.back().type == SA_Effect))
                sc.actions.pop_back();
            scnActSel_ = -1; scnDraft_.clear(); p.save();
        }
        cy += 28;
        DrawLine((int)cx, (int)cy, (int)(cx+cw), (int)cy, ui::kPanelHi); cy += 6;
    } else { scnDraft_.clear(); scnPendingFx_.clear(); }

    // 선택 캐릭터가 제거되어 사라졌으면 선택 해제(장면 편집은 스냅샷 배치모드/라이브 녹화로)
    if (scnObjSel_ >= 0 && !stage.count(scnObjSel_)) { scnObjSel_ = -1; scnAwaitDest_ = false; }
    // action sequence chips (numbered) — click to select, reorder, delete
    DrawTextU(TextFormat("동작 순서 (%d) — 선택 후 맵에서 편집", (int)sc.actions.size()), (int)cx, (int)cy, 12, ui::kAccentHi); cy += 18;
    Rectangle chreg = { cx, cy, cw, 96 };
    uiScissor((int)chreg.x, (int)chreg.y, (int)chreg.width, (int)chreg.height);
    if (ui::mouseIn(chreg)) scnActScroll_ -= GetMouseWheelMove() * 30;
    if (scnActScroll_ < 0) scnActScroll_ = 0;
    float chy = cy - scnActScroll_;
    for (int i = 0; i < (int)sc.actions.size(); ++i) {
        if (chy + 24 > cy && chy < cy + chreg.height) {
            SceneAction& a = sc.actions[i];
            std::string lbl = std::to_string(i+1) + ". " + kSceneActNames[std::max(0,std::min(6,a.type))];
            if (ui::button({ cx, chy, cw - 78, 22 }, lbl, scnActSel_ == i)) {
                scnActSel_ = (scnActSel_==i)?-1:i; scnAwaitDest_ = false;
                // 동작이 가리키는 캐릭터를 선택 상태로 동기화(있으면)
                if (scnActSel_>=0 && (a.type==SA_MoveChar||a.type==SA_Motion||a.type==SA_Spawn)) scnObjSel_ = a.targetId;
            }
            if (ui::button({ cx + cw - 76, chy, 22, 22 }, "위")) { if (i>0) { std::swap(sc.actions[i],sc.actions[i-1]); p.save(); } }
            if (ui::button({ cx + cw - 52, chy, 26, 22 }, "아래")) { if (i+1<(int)sc.actions.size()) { std::swap(sc.actions[i],sc.actions[i+1]); p.save(); } }
            if (ui::button({ cx + cw - 24, chy, 24, 22 }, "x")) { sc.actions.erase(sc.actions.begin()+i); if (scnActSel_>=(int)sc.actions.size()) scnActSel_=-1; p.save(); EndScissorMode(); return; }
        }
        chy += 26;
    }
    EndScissorMode();
    cy += chreg.height + 6;
    DrawLine((int)cx, (int)cy, (int)(cx+cw), (int)cy, ui::kPanelHi); cy += 6;

    // duration slider 0.42 ~ 1.42s (장면별 표시시간)
    auto durControl = [&](float& t) {
        int cs = (int)(t*100 + 0.5f); if (cs < 42) cs = 42; if (cs > 142) cs = 142;
        if (ui::intStepper({ cx, cy, cw, 24 }, "표시(×0.01초)", cs, 10, 42, 142)) {}
        t = cs / 100.0f;
        DrawTextU(TextFormat("= %.2f초 (0.42~1.42)", t), (int)cx, (int)cy + 26, 11, ui::kTextDim);
        cy += 44;
    };
    // selected action params
    if (scnActSel_ >= 0 && scnActSel_ < (int)sc.actions.size()) {
        SceneAction& a = sc.actions[scnActSel_];
        DrawTextU(TextFormat("선택 동작 %d: %s", scnActSel_+1, kSceneActNames[std::max(0,std::min(6,a.type))]), (int)cx, (int)cy, 13, ui::kAccent); cy += 20;
        if (a.type == SA_MoveChar) {
            std::vector<std::string> o = { "플레이어(0)" }; std::vector<int> v = { 0 };
            for (auto& s2 : sc.actions) if (s2.type==SA_Spawn) { o.push_back(spawnTagLabel(s2.targetId)); v.push_back(s2.targetId); }
            optionButton({ cx, cy, cw, 24 }, "이동 대상", o, v, a.targetId, 4150); cy += 28;
            DrawTextU(TextFormat("목적지: %d,%d", a.x, a.y), (int)cx, (int)cy, 11, ui::kTextDim);
            if (ui::button({ cx+cw-110, cy-2, 110, 20 }, "지도에서 지정", scnAwaitDest_)) scnAwaitDest_ = !scnAwaitDest_;
            cy += 22;
            durControl(a.time);
        } else if (a.type == SA_Dialogue) {
            entityButton({ cx, cy, cw, 24 }, "대화", a.refId, ENT_Dialogue, 4100); cy += 28;
            if (ui::button({ cx, cy, cw, 24 }, "대화 편집(대화 탭)")) {
                if (a.refId < 0 || !db.dialogue(a.refId)) {
                    DialogueScenario nd; nd.id=(int)db.dialogues.size()+1; while(db.dialogue(nd.id))++nd.id;
                    nd.name = sc.name + " 대사"; nd.lines.push_back({ "", "...", -1, {} });
                    db.dialogues.push_back(nd); a.refId = nd.id; p.save();
                }
                for (int k=0;k<(int)db.dialogues.size();++k) if (db.dialogues[k].id==a.refId) dlgSel_=k;
                dlgLineSel_=0; dlgPopupOpen_=true; tab_ = Tab::Dialogue; return;
            }
            cy += 28;
        } else if (a.type == SA_Effect) {
            assetButton({ cx, cy, cw, 24 }, "이펙트", a.refId, 4200); cy += 28;
            DrawTextU(TextFormat("위치: %d,%d · 반경 %d칸", a.x, a.y, a.radius), (int)cx, (int)cy, 11, ui::kTextDim);
            if (ui::button({ cx+cw-110, cy-2, 110, 20 }, "지도에서 지정", scnAwaitDest_)) scnAwaitDest_ = !scnAwaitDest_;
            cy += 22;
            durControl(a.time);
        } else if (a.type == SA_Spawn) {
            ui::intStepper({ cx, cy, cw, 24 }, "태그(번호)", a.targetId, 1, 1, 99); cy += 28;
            DrawTextU(("등장 대상: " + spawnEntName(a.refId)).c_str(), (int)cx, (int)cy, 11, ui::kAccentHi); cy += 16;
            if (ui::button({ cx, cy, cw/2-4, 24 }, "등록 캐릭터")) {   // 캐릭터로 지정(refId = -id-1)
                int idx = scnActSel_;
                openCharBrowser([this, idx](int cid){
                    if (cid < 0) return;
                    Database& d2 = engine_.project().database;
                    if (scnSel_<0 || scnSel_>=(int)d2.scenes.size()) return;
                    Scene& s2 = d2.scenes[scnSel_];
                    if (idx<0 || idx>=(int)s2.actions.size()) return;
                    s2.actions[idx].refId = -cid-1; engine_.project().save();
                });
            }
            entityButton({ cx+cw/2+2, cy, cw/2-4, 24 }, "몹", a.refId, ENT_Mob, 4300); cy += 28;   // 몹으로 지정(refId>=0)
            DrawTextU(TextFormat("위치: %d,%d", a.x, a.y), (int)cx, (int)cy, 11, ui::kTextDim);
            if (ui::button({ cx+cw-110, cy-2, 110, 20 }, "지도에서 지정", scnAwaitDest_)) scnAwaitDest_ = !scnAwaitDest_;
            cy += 22;
        } else if (a.type == SA_Remove) {
            std::string s2 = "제거: ";
            if (a.removeTags.empty()) s2 += "(맵에서 대상 클릭)";
            else for (int t : a.removeTags) s2 += "#" + std::to_string(t) + " ";
            DrawTextU(s2.c_str(), (int)cx, (int)cy, 11, a.removeTags.empty()?ui::kTextDim:ui::kAccentHi); cy += 18;
            if (ui::button({ cx, cy, cw, 22 }, "선택 비우기")) { a.removeTags.clear(); p.save(); } cy += 26;
        } else if (a.type == SA_Wait) {
            DrawTextU("지정 시간만큼 장면을 멈춥니다(연출 간격).", (int)cx, (int)cy, 11, ui::kTextDim); cy += 18;
            durControl(a.time);
        } else if (a.type == SA_Motion) {   // 동작 전환(죽음/공격 등)
            std::vector<std::string> o = { "플레이어(0)" }; std::vector<int> v = { 0 };
            for (auto& s2 : sc.actions) if (s2.type==SA_Spawn) { o.push_back(spawnTagLabel(s2.targetId)); v.push_back(s2.targetId); }
            optionButton({ cx, cy, cw, 24 }, "대상", o, v, a.targetId, 4180); cy += 28;
            std::vector<std::string> mo; for (int k=0;k<MO_COUNT;++k) mo.push_back(kMotionNames[k]);
            if (a.refId < 0) a.refId = MO_Attack;
            optionButton({ cx, cy, cw, 24 }, "동작", mo, {}, a.refId, 4181); cy += 28;
            durControl(a.time);
        }
    } else {
        DrawTextU("동작을 추가/선택하면 맵에서 편집합니다.", (int)cx, (int)cy, 12, ui::kTextDim); cy += 22;
    }
    DrawLine((int)cx, (int)cy, (int)(cx+cw), (int)cy, ui::kPanelHi); cy += 8;

    // ---- trigger: where on the map / which NPC's talk starts this scene ----
    auto m = p.map(sc.editMapId);
    Event* trigPoint = nullptr; int trigNpcId = -1;
    if (m) for (auto& e : m->events) if (e.sceneId == sc.id) { if (e.graphicAsset < 0) trigPoint = &e; else trigNpcId = e.id; }
    DrawTextU("발동(시작) 조건:", (int)cx, (int)cy, 12, ui::kAccentHi); cy += 18;
    if (ui::button({ cx, cy, cw, 24 }, scnTrigMode_==1 ? "발동지점: 맵 빈곳 클릭…" : (trigPoint?"발동지점 이동(맵 클릭)":"발동지점 지정(맵 클릭)"), scnTrigMode_==1))
        scnTrigMode_ = (scnTrigMode_==1)?0:1;
    cy += 28;
    if (ui::button({ cx, cy, cw, 24 }, scnTrigMode_==2 ? "발동 NPC: 맵에서 NPC 클릭…" : "발동 NPC 지정(맵 NPC 클릭)", scnTrigMode_==2))
        scnTrigMode_ = (scnTrigMode_==2)?0:2;
    cy += 26;
    {
        std::string ts = "현재: ";
        if (trigPoint) ts += TextFormat("지점(%d,%d) ", trigPoint->x, trigPoint->y);
        if (trigNpcId >= 0) { for (auto& e : m->events) if (e.id==trigNpcId) ts += "NPC '" + (e.speakerName.empty()?assetName(e.graphicAsset):e.speakerName) + "'"; }
        if (!trigPoint && trigNpcId < 0) ts += "없음";
        DrawTextU(ts.c_str(), (int)cx, (int)cy, 11, ui::kTextDim);
        cy += 18;
        if (trigPoint && ui::button({ cx, cy, cw, 22 }, "발동지점 제거")) { for (int i=0;i<(int)m->events.size();++i) if (&m->events[i]==trigPoint) { m->events.erase(m->events.begin()+i); break; } p.save(); }
    }
    cy += 28;
    if (ui::button({ cx, cy, cw, 26 }, "시나리오 테스트 (실행·F2로 복귀)")) { engine_.startPlaytestScene(sc.id); return; }

    // ---- BIG MAP (left): markers + drag(RTS) + place + radius + remove + triggers ----
    Rectangle canvas = { mapX, top, mapW, panelH };
    DrawRectangleRec(canvas, Color{ 18, 20, 26, 255 });
    const RenderTexture2D* th = m ? bestThumb(sc.editMapId) : nullptr;
    // map chooser overlaid top-left
    std::vector<std::string> mopts; std::vector<int> mvals;
    for (auto& mm : p.maps) { mopts.push_back(mm->name); mvals.push_back(mm->id); }
    if (!mopts.empty()) optionButton({ mapX + 4, top + 4, 240, 24 }, "맵", mopts, mvals, sc.editMapId, 4500);
    if (!th || !m || m->tilemap.width() <= 0) { DrawTextU("맵 미리보기 없음", (int)canvas.x+8, (int)canvas.y+34, 13, ui::kTextDim); return; }

    float tw = (float)th->texture.width, tht = (float)th->texture.height;
    float availH = canvas.height - 36;
    float s = std::min(canvas.width / tw, availH / tht);
    float pw = tw * s, ph = tht * s, bx = canvas.x + (canvas.width - pw)/2, by = canvas.y + 32 + (availH - ph)/2;
    Rectangle imgR = { bx, by, pw, ph };
    DrawTexturePro(th->texture, { 0,0,tw,-tht }, imgR, {0,0}, 0, WHITE);
    DrawRectangleLinesEx(imgR, 1, Fade(BLACK, 0.6f));
    int mwT = m->tilemap.width(), mhT = m->tilemap.height();
    Vector2 mouse = GetMousePosition();
    bool inMap = CheckCollisionPointRec(mouse, imgR);
    auto t2s = [&](float tx, float ty){ return Vector2{ bx + (tx+0.5f)/mwT*pw, by + (ty+0.5f)/mhT*ph }; };
    auto s2t = [&](int& tx, int& ty){ tx = std::max(0,std::min(mwT-1,(int)((mouse.x-bx)/pw*mwT))); ty = std::max(0,std::min(mhT-1,(int)((mouse.y-by)/ph*mhT))); };
    float tileSp = pw/mwT;

    // full context markers (이벤트·NPC·몹·오브젝트·몹스폰) like the world preview
    drawMapElementMarkers(*m, bx, by, pw, ph, true);
    // NPC click targets for 발동 NPC mode + current-trigger highlight
    float npcHit = std::max(11.0f, tileSp*0.7f);
    for (auto& e : m->events) {
        if (e.graphicAsset < 0 && e.charId < 0) continue;
        Vector2 sp = t2s((float)e.x, (float)e.y);
        if (e.id == trigNpcId) { DrawCircleLines((int)sp.x,(int)sp.y,(int)npcHit,ui::kGood); DrawTextU("발동", (int)sp.x+8,(int)sp.y-8,11,ui::kGood); }
        if (scnTrigMode_==2 && CheckCollisionPointCircle(mouse, sp, npcHit)) {
            DrawCircleLines((int)sp.x,(int)sp.y,(int)npcHit,WHITE);
            if (lclick) {
                if (!scnGroupSel_.empty()) { e.sceneGroup = scnGroupSel_; e.sceneId = -1; setStatus("이 NPC 대화 시 '"+scnGroupSel_+"' 발동"); }
                else { for (auto& e2 : m->events) if (e2.sceneId==sc.id && e2.graphicAsset>=0) e2.sceneId=-1; e.sceneId=sc.id; setStatus("이 NPC 대화 시 이 장면 발동"); }
                scnTrigMode_=0; p.save();
            }
        }
    }
    // ── 모든 장면의 실행지점(트리거)을 별 아이콘으로 표시 — 클릭/호버하면 제목이 옆에 ──
    auto sceneName = [&](int sid)->std::string {
        for (auto& sc2 : db.scenes) if (sc2.id == sid) return sc2.name;
        return "장면#" + std::to_string(sid);
    };
    for (auto& e : m->events) {
        bool isGroup = !e.sceneGroup.empty();
        if (!isGroup && e.sceneId < 0) continue;            // 트리거 아님
        if (e.graphicAsset >= 0 || e.charId >= 0) continue; // 지점 트리거만(NPC 트리거 제외)
        bool drag = (scnTrigDragId_ == e.id);
        Vector2 sp = drag ? mouse : t2s((float)e.x, (float)e.y);
        bool isCur = isGroup ? (!sc.group.empty() && e.sceneGroup == sc.group) : (e.sceneId == sc.id);
        Color col = isCur ? ui::kGood : ui::kAccentHi;
        DrawPoly(sp, 5, 9, 0, col); DrawPolyLines(sp, 5, 9, 0, WHITE);
        bool hov = CheckCollisionPointCircle(mouse, t2s((float)e.x,(float)e.y), 11);
        if (scnTrigSel_ == e.id || hov)
            DrawTextU((isGroup ? ("▶ " + e.sceneGroup + " (제목)") : ("▶ " + sceneName(e.sceneId))).c_str(), (int)sp.x+11, (int)sp.y-8, 12, col);
        // 우클릭 = 즉시 삭제(확인 없이) — 어떤 모드에서든 동작
        if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            int id = e.id;
            m->events.erase(std::remove_if(m->events.begin(), m->events.end(),
                            [id](const Event& x){ return x.id==id; }), m->events.end());
            if (scnTrigSel_==id) scnTrigSel_=-1;
            scnTrigMode_ = 0; p.save(); setStatus("실행지점 삭제됨(우클릭)"); break;
        }
        if (hov && scnTrigMode_==0 && scnDragIdx_<0 && scnTrigDragId_<0
            && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            scnTrigSel_ = e.id; scnTrigDragId_ = e.id;
        }
    }
    // 실행지점 드래그 이동/놓기
    if (scnTrigDragId_ >= 0) {
        Event* te = nullptr; for (auto& e : m->events) if (e.id==scnTrigDragId_) te=&e;
        if (te) { int tx, ty; s2t(tx, ty);
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { te->x=tx; te->y=ty; }
            else { p.save(); scnTrigDragId_=-1; }
        } else scnTrigDragId_=-1;
    }

    // ── 좌측에서 끌어온 '제목(그룹)'을 맵에 놓아 그 제목의 발동지점 등록 ──
    if (!scnGroupTrigDrag_.empty() && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        if (!scnLpDragging_ &&
            (std::abs(mouse.x-scnLpDragStart_.x)+std::abs(mouse.y-scnLpDragStart_.y) > 6))
            scnLpDragging_ = true;
        if (scnLpDragging_) {
            if (inMap) { int tx,ty; s2t(tx,ty); Vector2 g=t2s((float)tx,(float)ty);
                DrawPoly(g,5,9,0,Fade(ui::kGood,0.8f)); DrawCircleLines((int)g.x,(int)g.y,12,WHITE); }
            std::string nm = "발동: " + scnGroupTrigDrag_;
            int twd = MeasureTextU(nm.c_str(),12)+12;
            DrawRectangle((int)mouse.x+8,(int)mouse.y-10,twd,18,Fade(BLACK,0.85f));
            DrawTextU(nm.c_str(),(int)mouse.x+12,(int)mouse.y-8,12,ui::kAccentHi);
        }
    } else if (!scnGroupTrigDrag_.empty()) {   // 버튼을 뗌 → 드롭 처리
        if (scnLpDragging_ && inMap) {
            int tx,ty; s2t(tx,ty);
            Event ne; ne.id = m->nextEventId(); ne.type = EventType::Message; ne.trigger = TriggerType::PlayerTouch;
            ne.graphicAsset = -1; ne.sceneId = -1; ne.sceneGroup = scnGroupTrigDrag_;
            ne.label = "[시나리오 제목] " + scnGroupTrigDrag_; ne.x = tx; ne.y = ty;
            m->events.push_back(ne); p.save();
            setStatus("'" + scnGroupTrigDrag_ + "' 발동지점 등록");
        }
        scnGroupTrigDrag_.clear(); scnLpDragging_ = false;
    }

  if (!scnRecordMode_) {
    // 태그→대표 스프라이트(플레이어=등록 플레이어 캐릭터, 등장=그 spawn의 캐릭터/몹)
    auto tagThumb = [&](int tag)->int {
        if (tag==0) { const CharacterDef* c=db.character(p.playerCharId); return c?charThumbAsset(*c):-1; }
        for (auto& a2 : sc.actions) if (a2.type==SA_Spawn && a2.targetId==tag) {
            if (a2.refId<=-2) { const CharacterDef* c=db.character(-a2.refId-1); return c?charThumbAsset(*c):-1; }
            const CharacterDef* md=db.mob(a2.refId); return md?charThumbAsset(*md):-1;
        }
        return -1;
    };
    bool clickUsed = false;
    bool removeMode = (scnActSel_>=0 && scnActSel_<(int)sc.actions.size() && sc.actions[scnActSel_].type==SA_Remove);

    // (1) 위치 지정(armed): +이동/+이펙트/+등장 직후 또는 '지도에서 지정' 토글 시 — 지도 클릭으로 좌표 확정
    if (scnAwaitDest_ && scnActSel_>=0 && scnActSel_<(int)sc.actions.size()) {
        SceneAction& a = sc.actions[scnActSel_];
        if (a.type==SA_MoveChar||a.type==SA_Effect||a.type==SA_Spawn) {
            if (inMap) {
                int tx,ty; s2t(tx,ty); Vector2 g=t2s((float)tx,(float)ty);
                DrawCircleLines((int)g.x,(int)g.y,7,WHITE);
                if (a.type==SA_Effect) {
                    float wh=GetMouseWheelMove(); if(wh!=0){a.radius=std::max(1,std::min(30,a.radius+(int)wh));p.save();}
                    float rr=std::max(1,a.radius)*tileSp; DrawCircleLines((int)g.x,(int)g.y,(int)rr,Fade(Color{250,180,60,255},0.7f));
                }
                if (lclick) { a.x=tx; a.y=ty; scnAwaitDest_=false; clickUsed=true; p.save(); setStatus("위치 지정 완료"); }
            }
            if (IsKeyPressed(KEY_ESCAPE)||IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) scnAwaitDest_=false;
        } else scnAwaitDest_=false;
    }

    // (2) 캐릭터 토큰: 플레이어 + 등장 유닛을 실제 스프라이트로 그리고, 클릭=선택(제거모드면 대상 토글)
    for (auto& kv : stage) {
        int tag=kv.first; Vector2 sp=t2s(kv.second.x,kv.second.y);
        float sz=std::max(24.0f, tileSp*1.4f);
        int thumb=tagThumb(tag);
        if (thumb>=0) drawSpriteCentered(thumb, sp, sz);
        else DrawCircleV(sp, 8, tag==0?Color{120,170,250,255}:Color{120,200,120,255});
        bool seld=(scnObjSel_==tag);
        bool hov=CheckCollisionPointCircle(mouse, sp, sz/2+2);
        if (removeMode && tag!=0) {   // 제거 대상 선택 표시
            bool chosen=std::find(sc.actions[scnActSel_].removeTags.begin(),sc.actions[scnActSel_].removeTags.end(),tag)!=sc.actions[scnActSel_].removeTags.end();
            DrawCircleLines((int)sp.x,(int)sp.y,(int)(sz/2+4),chosen?ui::kDanger:Fade(WHITE,0.6f));
        } else if (seld) { DrawCircleLines((int)sp.x,(int)sp.y,(int)(sz/2+4),ui::kAccentHi); DrawCircleLines((int)sp.x,(int)sp.y,(int)(sz/2+6),ui::kAccentHi); }
        else if (hov) DrawCircleLines((int)sp.x,(int)sp.y,(int)(sz/2+3),WHITE);
        std::string nm = tag==0?"플레이어":spawnTagLabel(tag);
        if (seld||hov) DrawTextU(nm.c_str(), (int)sp.x+(int)(sz/2)+2, (int)sp.y-8, 12, seld?ui::kAccentHi:WHITE);
        if (!clickUsed && !scnAwaitDest_ && scnTrigMode_==0 && scnDragIdx_<0 && hov && lclick) {
            if (removeMode && tag!=0) {
                auto& rt=sc.actions[scnActSel_].removeTags;
                auto it=std::find(rt.begin(),rt.end(),tag);
                if (it!=rt.end()) rt.erase(it); else rt.push_back(tag);
                p.save();
            } else { scnObjSel_=tag; setStatus("선택: "+nm); }
            clickUsed=true;
        }
    }

    // (3) 선택 동작이 이동/이펙트/등장이면 위치 마커를 드래그로 미세조정
    if (!scnAwaitDest_ && scnActSel_>=0 && scnActSel_<(int)sc.actions.size()) {
        SceneAction& a=sc.actions[scnActSel_];
        if (a.type==SA_MoveChar||a.type==SA_Effect||a.type==SA_Spawn) {
            Vector2 sp = (scnDragIdx_==scnActSel_)?mouse:t2s((float)a.x,(float)a.y);
            DrawCircleLines((int)sp.x,(int)sp.y,6,Fade(WHITE,0.9f));
            if (!clickUsed && scnDragIdx_<0 && scnTrigMode_==0 && CheckCollisionPointCircle(mouse,sp,8) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { scnDragIdx_=scnActSel_; clickUsed=true; }
        }
    }
    if (scnDragIdx_>=0 && scnDragIdx_<(int)sc.actions.size()) {
        int tx,ty; s2t(tx,ty);
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { sc.actions[scnDragIdx_].x=tx; sc.actions[scnDragIdx_].y=ty; }
        else { p.save(); scnDragIdx_=-1; }
    }

    // (4) 발동지점 모드(맵 빈곳 클릭) — 제목 선택 시 그 제목(그룹) 전체, 아니면 이 장면
    if (scnTrigMode_==1 && inMap && lclick && scnDragIdx_<0 && !clickUsed) {
        int tx, ty; s2t(tx, ty);
        Event ne; ne.id = m->nextEventId(); ne.type = EventType::Message; ne.trigger = TriggerType::PlayerTouch;
        ne.graphicAsset = -1; ne.x = tx; ne.y = ty;
        if (!scnGroupSel_.empty()) { ne.sceneGroup = scnGroupSel_; ne.label = "[시나리오 제목] " + scnGroupSel_; setStatus("'"+scnGroupSel_+"' 발동지점 설정"); }
        else { ne.sceneId = sc.id; ne.label = "[시나리오 발동]"; setStatus("이 장면 발동지점 설정"); }
        m->events.push_back(ne);
        scnTrigMode_ = 0; p.save();
    }
    // (5) 시점=특정위치: 맵 클릭으로 카메라 위치 지정
    if (scnCamPick_) {
        if (inMap) { int tx,ty; s2t(tx,ty); Vector2 g=t2s((float)tx,(float)ty);
            DrawCircleLines((int)g.x,(int)g.y,8,ui::kAccentHi); DrawTextU("시점 위치", (int)g.x+9,(int)g.y-8,11,ui::kAccentHi);
            if (lclick) { sc.camX=tx; sc.camY=ty; scnCamPick_=false; p.save(); setStatus("시점 위치 지정됨"); } }
        if (IsKeyPressed(KEY_ESCAPE)||IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) scnCamPick_=false;
    }
  } else {
    // ── 녹화 모드 (RTS식): 마퀴 다중선택 · 그룹 드래그 이동 · 우클릭 전체 동작/삭제 ──
    for (auto& kv : stage) if (!scnDraft_.count(kv.first)) scnDraft_[kv.first] = kv.second;  // init draft
    auto selected = [&](int tag){ return std::find(scnSelTags_.begin(), scnSelTags_.end(), tag) != scnSelTags_.end(); };
    // token under cursor
    int hovTag = -1000;
    for (auto& kv : scnDraft_) if (CheckCollisionPointCircle(mouse, t2s(kv.second.x, kv.second.y), 11)) hovTag = kv.first;
    if (IsKeyPressed(KEY_ESCAPE)) {   // Esc: 메뉴 닫기 → 배치 취소 → 선택 해제 순
        if (scnCtxOpen_) scnCtxOpen_ = false;
        else if (scnRecPlaceChar_ >= 0) scnRecPlaceChar_ = -1;
        else scnSelTags_.clear();
    }

    // dropping a browsed character onto the map → new 등장(SA_Spawn) at the click tile
    if (scnRecPlaceChar_ >= 0) {
        if (inMap) {
            int tx, ty; s2t(tx, ty);
            Vector2 g = t2s((float)tx, (float)ty);
            DrawCircleLines((int)g.x,(int)g.y, 12, ui::kAccentHi);
            DrawTextU(("여기 등장: " + spawnEntName(-scnRecPlaceChar_-1)).c_str(), (int)g.x+12, (int)g.y-8, 12, ui::kAccentHi);
            if (lclick) {
                int tag = 1; for (auto& a : sc.actions) if (a.type==SA_Spawn && a.targetId>=tag) tag = a.targetId+1;
                SceneAction sp; sp.type = SA_Spawn; sp.targetId = tag; sp.refId = -scnRecPlaceChar_-1; sp.x = tx; sp.y = ty;
                sc.actions.push_back(sp); scnRecPlaceChar_ = -1; p.save(); setStatus("등록 캐릭터 등장 추가됨");
            }
        }
    } else if (!scnCtxOpen_) {
        // begin: press a token → (re)select + group drag; press empty → marquee (or effect drop)
        if (lclick && !scnGroupDrag_ && !scnMarquee_) {
            if (hovTag != -1000) {
                if (!selected(hovTag)) scnSelTags_ = { hovTag };   // single-select unless already in group
                int sx, sy; s2t(sx, sy); scnDragStartTile_ = { (float)sx, (float)sy };
                scnDragBase_.clear(); for (int t : scnSelTags_) if (scnDraft_.count(t)) scnDragBase_[t] = scnDraft_[t];
                scnGroupDrag_ = true;
            } else if (inMap && scnRecEffect_ < 0) {
                scnMarquee_ = true; scnMarqueeStart_ = mouse;     // empty drag → marquee
            }
        }
        // group drag move (all selected together)
        if (scnGroupDrag_) {
            int tx, ty; s2t(tx, ty);
            Vector2 d = { tx - scnDragStartTile_.x, ty - scnDragStartTile_.y };
            for (auto& kv : scnDragBase_) scnDraft_[kv.first] = { kv.second.x + d.x, kv.second.y + d.y };
            if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { scnGroupDrag_ = false; p.save(); }
        }
        // marquee select
        if (scnMarquee_) {
            Rectangle mr = { std::min(scnMarqueeStart_.x, mouse.x), std::min(scnMarqueeStart_.y, mouse.y),
                             std::abs(mouse.x-scnMarqueeStart_.x), std::abs(mouse.y-scnMarqueeStart_.y) };
            DrawRectangleRec(mr, Fade(ui::kAccent, 0.15f)); DrawRectangleLinesEx(mr, 1, ui::kAccent);
            if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                scnSelTags_.clear();
                for (auto& kv : scnDraft_) if (CheckCollisionPointRec(t2s(kv.second.x, kv.second.y), mr)) scnSelTags_.push_back(kv.first);
                scnMarquee_ = false;
            }
        }
        // right-click → open action menu for the whole selection
        if (hovTag != -1000 && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            if (!selected(hovTag)) scnSelTags_ = { hovTag };
            scnCtxOpen_ = true; scnCtxPos_ = mouse;
        }
        // effect drop (오른쪽 목록에서 이펙트 선택 후 빈 곳 클릭)
        if (scnRecEffect_ >= 0 && inMap && lclick && hovTag == -1000) {
            int tx, ty; s2t(tx, ty);
            SceneAction fx; fx.type = SA_Effect; fx.refId = scnRecEffect_; fx.x = tx; fx.y = ty; fx.radius = scnRecRadius_;
            scnPendingFx_.push_back(fx);
        }
    }
    // pending effects
    for (auto& fx : scnPendingFx_) {
        Vector2 sp = t2s((float)fx.x, (float)fx.y); float rr = std::max(1, fx.radius) * tileSp;
        DrawCircleLines((int)sp.x,(int)sp.y, rr, Color{250,180,60,255});
        DrawCircle((int)sp.x,(int)sp.y, rr, Fade(Color{250,180,60,255}, 0.12f));
        DrawCircleV(sp, 4, Color{250,180,60,255});
    }
    // resolve a tag's sprite asset (player=playerChar, spawn=its char/mob 썸네일)
    auto tagThumb = [&](int tag)->int {
        if (tag == 0) { const CharacterDef* c = db.character(p.playerCharId); return c ? charThumbAsset(*c) : -1; }
        for (auto& a2 : sc.actions) if (a2.type==SA_Spawn && a2.targetId==tag) {
            const CharacterDef* c = a2.refId <= -2 ? db.character(-a2.refId-1) : db.mob(a2.refId);
            return c ? charThumbAsset(*c) : -1;
        }
        return -1;
    };
    // tokens — draw the real sprite (readable RTS stage); selected = bright ring
    for (auto& kv : scnDraft_) {
        int tag = kv.first; Vector2 sp = t2s(kv.second.x, kv.second.y);
        Color col = (tag == 0) ? Color{120,170,250,255} : Color{120,200,120,255};
        if (selected(tag)) DrawCircleLines((int)sp.x,(int)sp.y, 15, ui::kAccentHi);
        int aid = tagThumb(tag);
        if (aid >= 0) {
            const Texture2D& tex = engine_.assetTexture(aid);
            if (tex.id) { float fw = tex.width>=tex.height*2 ? tex.width/4.0f : (float)tex.width;
                float r=12, scl=std::min(r*2/fw, r*2/(float)tex.height);
                DrawCircleV(sp, r+2, Fade(col,0.5f));
                DrawTexturePro(tex, {0,0,fw,(float)tex.height}, {sp.x-fw*scl/2, sp.y-tex.height*scl/2, fw*scl, tex.height*scl}, {0,0}, 0, WHITE);
            } else { DrawCircleV(sp,10,Fade(BLACK,0.6f)); DrawCircleV(sp,8,col); }
        } else { DrawCircleV(sp,10,Fade(BLACK,0.6f)); DrawCircleV(sp,8,col); }
        DrawTextU((tag==0 ? std::string("플레이어") : spawnTagLabel(tag)).c_str(), (int)sp.x+13, (int)sp.y-8, 12, WHITE);
    }
    // right-click context menu — applies to ALL selected tokens
    if (scnCtxOpen_) {
        const char* items[MO_COUNT+1];
        for (int k=0;k<MO_COUNT;++k) items[k]=kMotionNames[k];
        items[MO_COUNT]="유닛 삭제";
        float mw=130, mh=(MO_COUNT+1)*22+24;
        Rectangle box={ scnCtxPos_.x, scnCtxPos_.y, mw, mh };
        if (box.x+mw > canvas.x+canvas.width) box.x = canvas.x+canvas.width-mw;
        if (box.y+mh > canvas.y+canvas.height) box.y = canvas.y+canvas.height-mh;
        DrawRectangleRec(box, ui::kPanelHi); DrawRectangleLinesEx(box, 2, ui::kAccent);
        DrawTextU(TextFormat("선택 %d개 전체", (int)scnSelTags_.size()), (int)box.x+6, (int)box.y+4, 11, ui::kAccentHi);
        for (int k=0;k<=MO_COUNT;++k) {
            Rectangle b={ box.x+4, box.y+20+k*22, mw-8, 20 };
            bool del = (k==MO_COUNT);
            if (ui::button(b, items[k], false)) {
                for (int tag : scnSelTags_) {
                    if (del) { if (tag==0) continue;            // 플레이어는 삭제 불가
                        SceneAction rm; rm.type=SA_Remove; rm.targetId=tag; sc.actions.push_back(rm); scnDraft_.erase(tag);
                    } else {
                        SceneAction mo; mo.type=SA_Motion; mo.targetId=tag; mo.refId=k; mo.time=scnStepDur_; sc.actions.push_back(mo);
                    }
                }
                if (del) scnSelTags_.clear();
                scnCtxOpen_=false; p.save(); setStatus(del?"선택 유닛 삭제 기록됨":"선택 전체 동작 기록됨");
            }
        }
        if (lclick && !CheckCollisionPointRec(mouse, box)) scnCtxOpen_=false;
    }
  }
    // hint line
    const char* hint = scnRecordMode_ ? "녹화: 빈곳 드래그=다중선택 · 선택 드래그=그룹이동 · 우클릭=전체 동작/삭제 → '장면 녹화'"
                     : scnTrigMode_==1 ? "발동지점: 맵 빈곳을 클릭" : scnTrigMode_==2 ? "발동 NPC: 맵의 NPC를 클릭"
                     : scnAwaitDest_ ? "위치를 지도에서 클릭 (Esc/우클릭 취소 · 이펙트는 휠로 반경)"
                     : "캐릭터 클릭=선택 → 오른쪽 명령 버튼으로 이동/동작/제거 · 위치 마커 드래그=미세조정";
    DrawTextU(hint, (int)canvas.x+8, (int)(canvas.y+canvas.height-22), 13, ui::kAccentHi);
}

// ── 장면 미리보기 PiP: 좌측 하단에 작게 재생, 클릭하면 중앙 확대, X로 닫기 ──
void Editor::drawScenePreviewOverlay() {
    const Texture2D& tex = engine_.scenePreviewTexture();
    if (tex.id == 0) return;
    { static bool envBig=false; if (!envBig) { envBig=true; if (getenv("TSUKURU_PREVIEW_BIG")) scnPrevBig_=true; } } // debug
    float W = (float)screenW(), H = (float)screenH();
    float tw = (float)tex.width, th = (float)tex.height;     // 640×360
    Rectangle box;
    if (scnPrevBig_) {                                       // 중앙 큰 화면
        float bw = W * 0.66f, bh = bw * th / tw;
        if (bh > H * 0.78f) { bh = H * 0.78f; bw = bh * tw / th; }
        box = { (W - bw) / 2, (H - bh) / 2 + 6, bw, bh };
    } else {                                                 // 좌측 하단 작게(크기에 맞게)
        float bw = std::min(360.0f, W * 0.42f), bh = bw * th / tw;
        box = { 10, H - bh - 44, bw, bh };
    }
    // 확대(큰 화면)면 키보드/조작이 게임에 전달되도록 상호작용 모드 ON.
    engine_.setScenePreviewInteractive(scnPrevBig_);
    // 제목줄(영역 위) + 외곽 프레임. scnPrevBox_ 는 입력 가림 판정에 쓰이므로 제목줄 포함.
    Rectangle frame = { box.x - 3, box.y - 26, box.width + 6, box.height + 29 };
    scnPrevBox_ = frame;
    if (scnPrevBig_) DrawRectangle(0, 0, (int)W, (int)H, Fade(BLACK, 0.55f));   // 확대 시 뒤 어둡게
    DrawRectangleRec(frame, Fade(Color{ 10, 12, 18, 255 }, 0.96f));
    DrawRectangleLinesEx(frame, 2, ui::kAccent);
    std::string title = "장면 미리보기";
    if (!engine_.scenePreviewName().empty()) title += " — " + engine_.scenePreviewName();
    DrawTextU(title.c_str(), (int)box.x + 4, (int)box.y - 22, 14, ui::kAccentHi);
    Vector2 m = GetMousePosition();
    // X 닫기 / (확대 시) 작게 버튼 — 확대 모드에선 화면 클릭이 게임으로 가도록 토글은 버튼으로만.
    if (ui::button({ box.x + box.width - 24, box.y - 25, 22, 22 }, "x")) {
        engine_.stopScenePreview(); scnPrevBig_ = false; return;
    }
    if (scnPrevBig_) {
        if (ui::button({ box.x + box.width - 78, box.y - 25, 50, 22 }, "작게")) { scnPrevBig_ = false; return; }
    }
    // 장면 화면(상하 반전)
    DrawTexturePro(tex, { 0, 0, tw, -th }, box, { 0, 0 }, 0, WHITE);
    DrawRectangleLinesEx(box, 1, Fade(BLACK, 0.6f));
    // 작은 화면일 때만 화면 클릭으로 확대(확대 상태에선 클릭이 게임 조작으로 전달됨)
    if (!scnPrevBig_ && CheckCollisionPointRec(m, box) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        scnPrevBig_ = true;
    // 안내
    const char* hint = scnPrevBig_ ? "키보드로 조작 (방향키 이동·Enter 대화·Esc 건너뛰기) · '작게'/X"
                                   : "클릭=크게(키 조작) · X=닫기";
    int hw = MeasureTextU(hint, 11);
    DrawRectangle((int)box.x + 4, (int)(box.y + box.height - 16), hw + 6, 14, Fade(BLACK, 0.6f));
    DrawTextU(hint, (int)box.x + 6, (int)(box.y + box.height - 15), 11, Fade(ui::kText, 0.95f));
}

} // namespace tsukuru
