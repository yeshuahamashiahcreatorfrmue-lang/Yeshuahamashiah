// EditorStory: the 대화로그 (branching dialogue) and 스토리 시나리오 (scene
// sequencer) editor tabs. Both author data on Database (dialogues / scenes).
#include "editor/Editor.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include <algorithm>

namespace tsukuru {

// ============================ 대화로그 (Dialogue) ============================
void Editor::drawDialogueTab() {
    float W = (float)screenW(), H = (float)screenH();
    DrawRectangleRec({ 0, kToolbarH, W, H - kToolbarH }, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    auto& list = db.dialogues;
    ui::label("대화로그 시나리오 — 라인별로 대사와 대답·대응을 짠다", 14, (int)kToolbarH + 8, 18, ui::kAccent);

    float top = kToolbarH + 36, botY = H - 10, panelH = botY - top;
    float lw = 240, mw = 300, pad = 10;
    float lx = 10, mx = lx + lw + pad, rx = mx + mw + pad, rw = W - rx - 10;
    ui::panel({ lx, top, lw, panelH }, ui::kPanel);
    ui::panel({ mx, top, mw, panelH }, ui::kPanel);
    ui::panel({ rx, top, rw, panelH }, ui::kPanel);
    if (dlgSel_ < 0 && !list.empty()) dlgSel_ = 0;

    // ---- LEFT: scenario list (검색 + 스크롤) ----
    {
        float x = lx + 8, w = lw - 16, y = top + 8;
        if (ui::button({ x, y, w, 28 }, "+ 새 대화")) {
            DialogueScenario d; d.id = (int)list.size() + 1; d.name = "대화" + std::to_string(d.id);
            d.lines.push_back({ "", "...", {} });
            list.push_back(d); dlgSel_ = (int)list.size() - 1; dlgLineSel_ = 0; p.save();
        }
        y += 32;
        searchBox({ x, y, w, 24 }, dlgSearch_, 9001); y += 28;
        Rectangle reg = { lx, y, lw, top + panelH - y - 104 };
        uiScissor((int)lx, (int)y, (int)lw, (int)reg.height);
        float ly = y - dlgListScroll_; int shown = 0;
        for (int i = 0; i < (int)list.size(); ++i) {
            if (!nameMatch(list[i].name, dlgSearch_)) continue;
            if (ly + 26 > y && ly < y + reg.height)
                if (ui::button({ x, ly, w - 12, 26 }, list[i].name, dlgSel_ == i)) { dlgSel_ = i; dlgLineSel_ = 0; dlgFocus_ = -1; }
            ly += 28; ++shown;
        }
        EndScissorMode();
        scrollbar(reg, dlgListScroll_, shown * 28.0f + 4);
    }
    if (dlgSel_ < 0 || dlgSel_ >= (int)list.size()) return;
    DialogueScenario& d = list[dlgSel_];

    // ---- LEFT bottom: name + dup/delete ----
    {
        float x = lx + 8, w = lw - 16, y = top + panelH - 96;
        DrawTextU("이름", (int)x, (int)y, 12, ui::kTextDim); y += 16;
        Rectangle nf = { x, y, w, 26 };
        if (ui::mouseIn(nf) && lclick) dlgFocus_ = 0; else if (lclick && !ui::mouseIn(nf) && dlgFocus_==0) dlgFocus_ = -1;
        ui::textField(nf, d.name, dlgFocus_ == 0, 40); y += 32;
        if (ui::button({ x, y, w/2-2, 26 }, "복제")) { DialogueScenario c=d; c.id=(int)list.size()+1; c.name=d.name+" 사본"; list.push_back(c); dlgSel_=(int)list.size()-1; p.save(); return; }
        if (ui::button({ x+w/2+2, y, w/2-2, 26 }, "삭제")) { list.erase(list.begin()+dlgSel_); dlgSel_=-1; dlgLineSel_=-1; p.save(); return; }
    }

    // ---- MIDDLE: line list (1,2,3,...) ----
    {
        float x = mx + 8, w = mw - 16, y = top + 8;
        DrawTextU(TextFormat("대사 라인 (%d) — 클릭하여 편집", (int)d.lines.size()), (int)x, (int)y, 13, ui::kAccentHi); y += 22;
        if (ui::button({ x, y, w, 26 }, "+ 라인 추가")) { d.lines.push_back({ "", "...", {} }); dlgLineSel_=(int)d.lines.size()-1; p.save(); }
        y += 32;
        Rectangle reg = { mx, y, mw, top + panelH - y - 8 };
        uiScissor((int)mx, (int)y, (int)mw, (int)reg.height);
        if (ui::mouseIn(reg)) dlgLineScroll_ -= GetMouseWheelMove()*36; if (dlgLineScroll_<0) dlgLineScroll_=0;
        float ly = y - dlgLineScroll_;
        for (int i = 0; i < (int)d.lines.size(); ++i) {
            if (ly + 28 > y && ly < y + reg.height) {
                std::string lbl = std::to_string(i+1) + ". " + (d.lines[i].speaker.empty()?"":("["+d.lines[i].speaker+"] ")) + d.lines[i].text;
                if ((int)lbl.size() > 40) lbl = lbl.substr(0, 40) + "..";
                if (ui::button({ x, ly, w-88, 26 }, lbl, dlgLineSel_ == i)) { dlgLineSel_ = i; dlgFocus_ = -1; }
                if (ui::button({ x+w-86, ly, 26, 26 }, "위")) { if(i>0){ std::swap(d.lines[i],d.lines[i-1]); if(dlgLineSel_==i)dlgLineSel_=i-1; else if(dlgLineSel_==i-1)dlgLineSel_=i; p.save(); } }
                if (ui::button({ x+w-58, ly, 26, 26 }, "아래")) { if(i+1<(int)d.lines.size()){ std::swap(d.lines[i],d.lines[i+1]); if(dlgLineSel_==i)dlgLineSel_=i+1; else if(dlgLineSel_==i+1)dlgLineSel_=i; p.save(); } }
                if (ui::button({ x+w-30, ly, 28, 26 }, "x")) { d.lines.erase(d.lines.begin()+i); if(dlgLineSel_>=(int)d.lines.size())dlgLineSel_=(int)d.lines.size()-1; p.save(); EndScissorMode(); return; }
            }
            ly += 30;
        }
        EndScissorMode();
    }

    // ---- RIGHT: selected line editor (speaker/text + answers) ----
    if (dlgLineSel_ < 0 || dlgLineSel_ >= (int)d.lines.size()) return;
    DialogueLine& ln = d.lines[dlgLineSel_];
    float x = rx + 10, w = rw - 20, y = top + 10;
    DrawTextU(TextFormat("라인 %d 편집", dlgLineSel_+1), (int)x, (int)y, 16, ui::kAccent); y += 24;
    DrawTextU("말하는 이", (int)x, (int)y, 12, ui::kTextDim); y += 16;
    Rectangle sf = { x, y, w, 26 };
    if (ui::mouseIn(sf) && lclick) dlgFocus_ = 1; else if (lclick && !ui::mouseIn(sf) && dlgFocus_==1) dlgFocus_=-1;
    ui::textField(sf, ln.speaker, dlgFocus_ == 1, 30); y += 32;
    DrawTextU("대사", (int)x, (int)y, 12, ui::kTextDim); y += 16;
    Rectangle tf = { x, y, w, 26 };
    if (ui::mouseIn(tf) && lclick) dlgFocus_ = 2; else if (lclick && !ui::mouseIn(tf) && dlgFocus_==2) dlgFocus_=-1;
    ui::textField(tf, ln.text, dlgFocus_ == 2, 200); y += 36;

    DrawTextU(TextFormat("대답 (%d) — 비우면 그냥 다음 라인", (int)ln.answers.size()), (int)x, (int)y, 13, ui::kAccentHi); y += 20;
    if (ui::button({ x, y, 160, 26 }, "+ 대답 추가")) { ln.answers.push_back({}); p.save(); }
    y += 32;
    Rectangle areg = { rx, y, rw, top + panelH - y - 8 };
    uiScissor((int)rx, (int)y, (int)rw, (int)areg.height);
    if (ui::mouseIn(areg)) dlgAnsScroll_ -= GetMouseWheelMove()*36; if (dlgAnsScroll_<0) dlgAnsScroll_=0;
    float ay = y - dlgAnsScroll_;
    for (int i = 0; i < (int)ln.answers.size(); ++i) {
        DialogueAnswer& a = ln.answers[i];
        float cardH = 158;
        if (ay + cardH > y - 30 && ay < y + areg.height) {
            ui::panel({ x-2, ay, w+4, cardH-6 }, ui::kPanelHi);
            float ix = x + 6, iw = w - 12, iy = ay + 6;
            // answer text
            Rectangle af = { ix, iy, iw - 34, 24 };
            int fid = 100 + i;
            if (ui::mouseIn(af) && lclick) dlgFocus_ = fid; else if (lclick && !ui::mouseIn(af) && dlgFocus_==fid) dlgFocus_=-1;
            ui::textField(af, a.text, dlgFocus_ == fid, 60);
            if (ui::button({ ix + iw - 30, iy, 30, 24 }, "x")) { ln.answers.erase(ln.answers.begin()+i); p.save(); EndScissorMode(); return; }
            iy += 28;
            // response type picker
            optionButton({ ix, iy, 150, 24 }, "대응",
                { kDlgRespNames[0],kDlgRespNames[1],kDlgRespNames[2],kDlgRespNames[3],kDlgRespNames[4],kDlgRespNames[5],kDlgRespNames[6] },
                {}, a.respType, 3000 + i);
            // goto line stepper
            ui::intStepper({ ix + 158, iy, iw - 158, 24 }, "→라인", a.gotoLine, 1, -1, 99); iy += 28;
            // type-specific params
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

// ============================ 스토리 시나리오 ============================
void Editor::drawScenarioTab() {
    float W = (float)screenW(), H = (float)screenH();
    DrawRectangleRec({ 0, kToolbarH, W, H - kToolbarH }, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    auto& list = db.scenes;
    ui::label("스토리 시나리오 — 동작을 고르고 오른쪽 맵에서 위치·반경·대상을 지정", 14, (int)kToolbarH + 8, 18, ui::kAccent);

    float top = kToolbarH + 36, botY = H - 10, panelH = botY - top;
    float pad = 10, lw = 170, mw = 330;
    float lx = 10, mx = lx + lw + pad, rx = mx + mw + pad, rw = W - rx - 10;
    ui::panel({ lx, top, lw, panelH }, ui::kPanel);
    ui::panel({ mx, top, mw, panelH }, ui::kPanel);
    ui::panel({ rx, top, rw, panelH }, ui::kPanel);
    if (scnSel_ < 0 && !list.empty()) scnSel_ = 0;

    // ---- LEFT: scene list + name/delete ----
    {
        float x = lx + 8, w = lw - 16, y = top + 8;
        if (ui::button({ x, y, w, 28 }, "+ 새 장면")) {
            Scene s; s.id = (int)list.size()+1; s.name = "장면" + std::to_string(s.id);
            list.push_back(s); scnSel_ = (int)list.size()-1; scnActSel_ = -1; p.save();
        }
        y += 32;
        searchBox({ x, y, w, 24 }, scnSearch_, 9101); y += 28;
        Rectangle reg = { lx, y, lw, top + panelH - y - 80 };
        uiScissor((int)lx, (int)y, (int)lw, (int)reg.height);
        float ly = y - scnListScroll_; int shown = 0;
        for (int i = 0; i < (int)list.size(); ++i) {
            if (!nameMatch(list[i].name, scnSearch_)) continue;
            if (ly + 26 > y && ly < y + reg.height)
                if (ui::button({ x, ly, w - 12, 26 }, list[i].name, scnSel_ == i)) { scnSel_ = i; scnFocus_ = -1; scnActSel_ = -1; }
            ly += 28; ++shown;
        }
        EndScissorMode();
        scrollbar(reg, scnListScroll_, shown * 28.0f + 4);
        float by = top + panelH - 70;
        if (scnSel_ >= 0 && scnSel_ < (int)list.size()) {
            DrawTextU("이름", (int)x, (int)by, 12, ui::kTextDim); by += 16;
            Rectangle nf = { x, by, w, 26 };
            if (ui::mouseIn(nf) && lclick) scnFocus_ = 0; else if (lclick && !ui::mouseIn(nf) && scnFocus_==0) scnFocus_=-1;
            ui::textField(nf, list[scnSel_].name, scnFocus_ == 0, 40); by += 30;
            if (ui::button({ x, by, w, 24 }, "장면 삭제")) { list.erase(list.begin()+scnSel_); scnSel_=-1; scnActSel_=-1; p.save(); return; }
        }
    }
    if (scnSel_ < 0 || scnSel_ >= (int)list.size()) return;
    Scene& sc = list[scnSel_];
    // default the editing map to the first placed map
    if (sc.editMapId < 0 || !p.map(sc.editMapId)) {
        for (auto& m : p.maps) if (m->placed) { sc.editMapId = m->id; break; }
        if (sc.editMapId < 0 && !p.maps.empty()) sc.editMapId = p.maps.front()->id;
    }
    // tags defined by 등장 actions earlier in the sequence (for 이동/제거 대상 선택)
    auto spawnTagLabel = [&](int tag)->std::string {
        for (auto& a : sc.actions) if (a.type == SA_Spawn && a.targetId == tag) {
            const CharacterDef* md = db.mob(a.refId);
            return "#" + std::to_string(tag) + " " + (md ? md->name : "몹");
        }
        return "#" + std::to_string(tag);
    };

    // ---- MIDDLE: action sequence (select a card → edit it on the map) ----
    {
        float x = mx + 10, w = mw - 20, y = top + 10;
        DrawTextU(TextFormat("동작 순서 (%d) — 카드를 눌러 선택", (int)sc.actions.size()), (int)x, (int)y, 14, ui::kAccent); y += 22;
        if (ui::button({ x, y, 150, 26 }, "+ 동작 추가")) { sc.actions.push_back({}); scnActSel_ = (int)sc.actions.size()-1; p.save(); }
        y += 32;
        Rectangle reg = { mx, y, mw, top + panelH - y - 8 };
        uiScissor((int)mx, (int)y, (int)mw, (int)reg.height);
        float contentH = 4;
        for (auto& a : sc.actions) contentH += (a.type==SA_Spawn||a.type==SA_MoveChar) ? 124 : a.type==SA_Effect ? 120 : a.type==SA_Remove ? 96 : 92;
        float ay = y - scnActScroll_;
        for (int i = 0; i < (int)sc.actions.size(); ++i) {
            SceneAction& a = sc.actions[i];
            float cardH = (a.type==SA_Spawn||a.type==SA_MoveChar) ? 124 : a.type==SA_Effect ? 120 : a.type==SA_Remove ? 96 : 92;
            if (ay + cardH > y - 30 && ay < y + reg.height) {
                bool selCard = (scnActSel_ == i);
                ui::panel({ x-2, ay, w+4, cardH-6 }, selCard ? Color{ 38, 52, 70, 255 } : ui::kPanelHi);
                if (selCard) DrawRectangleLinesEx({ x-2, ay, w+4, cardH-6 }, 2, ui::kAccent);
                float ix = x + 6, iw = w - 12, iy = ay + 6;
                if (ui::button({ ix, iy, 26, 24 }, std::to_string(i+1), selCard)) scnActSel_ = selCard ? -1 : i;
                optionButton({ ix + 30, iy, 120, 24 }, "",
                    { kSceneActNames[0],kSceneActNames[1],kSceneActNames[2],kSceneActNames[3],kSceneActNames[4],kSceneActNames[5] },
                    {}, a.type, 4000 + i);
                if (ui::button({ ix + iw - 92, iy, 28, 24 }, "위")) { if(i>0){ std::swap(sc.actions[i],sc.actions[i-1]); p.save(); } }
                if (ui::button({ ix + iw - 62, iy, 28, 24 }, "아래")) { if(i+1<(int)sc.actions.size()){ std::swap(sc.actions[i],sc.actions[i+1]); p.save(); } }
                if (ui::button({ ix + iw - 30, iy, 30, 24 }, "x")) { sc.actions.erase(sc.actions.begin()+i); if(scnActSel_>=(int)sc.actions.size())scnActSel_=-1; p.save(); EndScissorMode(); return; }
                iy += 28;
                if (a.type == SA_MoveChar) {
                    std::vector<std::string> opts = { "플레이어(0)" }; std::vector<int> vals = { 0 };
                    for (auto& s2 : sc.actions) if (s2.type==SA_Spawn) { opts.push_back(spawnTagLabel(s2.targetId)); vals.push_back(s2.targetId); }
                    optionButton({ ix, iy, iw, 24 }, "이동대상", opts, vals, a.targetId, 4150+i); iy += 28;
                    DrawTextU(TextFormat("목적지 칸: %d, %d  (맵 클릭으로 지정)", a.x, a.y), (int)ix, (int)iy, 12, ui::kTextDim); iy += 20;
                    int ms=(int)(a.time*1000); if (ui::intStepper({ ix, iy, iw, 24 }, "이동시간(ms)", ms, 100, 0, 60000)) a.time=ms/1000.0f;
                } else if (a.type == SA_Dialogue) {
                    entityButton({ ix, iy, iw, 24 }, "대화", a.refId, ENT_Dialogue, 4100+i);
                } else if (a.type == SA_Effect) {
                    assetButton({ ix, iy, iw, 24 }, "이펙트", a.refId, 4200+i); iy += 28;
                    DrawTextU(TextFormat("발생 칸: %d, %d · 반경 %d칸  (맵에서 지정/휠)", a.x, a.y, a.radius), (int)ix, (int)iy, 12, ui::kTextDim); iy += 20;
                    int ms=(int)(a.time*1000); if (ui::intStepper({ ix, iy, iw, 24 }, "지속(ms)", ms, 100, 0, 60000)) a.time=ms/1000.0f;
                } else if (a.type == SA_Spawn) {
                    ui::intStepper({ ix, iy, iw/2-4, 24 }, "태그", a.targetId, 1, 1, 99);
                    entityButton({ ix+iw/2+2, iy, iw/2-4, 24 }, "몹", a.refId, ENT_Mob, 4300+i); iy += 28;
                    DrawTextU(TextFormat("등장 칸: %d, %d  (맵 클릭으로 지정)", a.x, a.y), (int)ix, (int)iy, 12, ui::kTextDim);
                } else if (a.type == SA_Remove) {
                    std::string s2 = "제거 대상: ";
                    if (a.removeTags.empty()) s2 += "(맵에서 클릭 선택)";
                    else for (int t : a.removeTags) s2 += "#" + std::to_string(t) + " ";
                    DrawTextU(s2.c_str(), (int)ix, (int)iy, 12, a.removeTags.empty()?ui::kTextDim:ui::kAccentHi); iy += 22;
                    if (ui::button({ ix, iy, 110, 22 }, "선택 비우기")) { a.removeTags.clear(); p.save(); }
                } else { // SA_Wait
                    int ms=(int)(a.time*1000); if (ui::intStepper({ ix, iy, iw/2-4, 24 }, "대기(ms)", ms, 100, 0, 60000)) a.time=ms/1000.0f;
                    std::vector<std::string> opts = { "전체" }; std::vector<int> vals = { -1 };
                    for (auto& s2 : sc.actions) if (s2.type==SA_Spawn) { opts.push_back(spawnTagLabel(s2.targetId)); vals.push_back(s2.targetId); }
                    optionButton({ ix+iw/2+2, iy, iw/2-4, 24 }, "대기대상", opts, vals, a.targetId, 4160+i);
                }
            }
            ay += cardH;
        }
        EndScissorMode();
        scrollbar(reg, scnActScroll_, contentH);
    }

    // ---- RIGHT: MAP — place positions / radius / remove-selection visually ----
    {
        float x = rx + 10, w = rw - 20, y = top + 10;
        // map chooser
        std::vector<std::string> mopts; std::vector<int> mvals;
        for (auto& m : p.maps) { mopts.push_back(m->name); mvals.push_back(m->id); }
        if (mopts.empty()) { DrawTextU("맵이 없습니다. 먼저 맵을 만드세요.", (int)x, (int)y, 14, ui::kTextDim); return; }
        optionButton({ x, y, w, 24 }, "배경 맵", mopts, mvals, sc.editMapId, 4500); y += 30;

        auto m = p.map(sc.editMapId);
        if (!m) return;
        const RenderTexture2D* th = mapThumb(m->id);  // built in ensureThumbsForTab()
        Rectangle canvas = { x, y, w, top + panelH - y - 10 };
        DrawRectangleRec(canvas, Color{ 18, 20, 26, 255 });
        if (!th || m->tilemap.width() <= 0) { DrawTextU("맵 미리보기를 만들 수 없음", (int)x+8, (int)y+8, 13, ui::kTextDim); return; }

        float tw = (float)th->texture.width, tht = (float)th->texture.height;
        float s = std::min(canvas.width / tw, canvas.height / tht);
        float pw = tw * s, ph = tht * s;
        float bx = canvas.x + (canvas.width - pw)/2, by = canvas.y + (canvas.height - ph)/2;
        Rectangle imgR = { bx, by, pw, ph };
        DrawTexturePro(th->texture, { 0,0,tw,-tht }, imgR, {0,0}, 0, WHITE);
        DrawRectangleLinesEx(imgR, 1, Fade(BLACK, 0.6f));

        int mwT = m->tilemap.width(), mhT = m->tilemap.height();
        Vector2 mouse = GetMousePosition();
        bool inMap = CheckCollisionPointRec(mouse, imgR);
        auto t2s = [&](float tx, float ty){ return Vector2{ bx + (tx+0.5f)/mwT*pw, by + (ty+0.5f)/mhT*ph }; };
        float tileSp = pw/mwT;   // one tile in screen px

        // draw every action's marker; selected one is bright
        auto drawMarker = [&](int i) {
            SceneAction& a = sc.actions[i];
            bool sel = (scnActSel_ == i);
            Vector2 sp = t2s((float)a.x, (float)a.y);
            Color col = a.type==SA_Effect ? Color{250,180,60,255}
                      : a.type==SA_Spawn  ? Color{120,200,120,255}
                      : a.type==SA_MoveChar ? Color{120,170,250,255} : ui::kTextDim;
            if (a.type==SA_Effect) {
                float rr = std::max(1, a.radius) * tileSp;
                DrawCircleLines((int)sp.x, (int)sp.y, rr, Fade(col, sel?0.9f:0.4f));
                DrawCircle((int)sp.x, (int)sp.y, sel?rr:rr, Fade(col, sel?0.18f:0.08f));
            }
            DrawCircleV(sp, sel?7:5, Fade(BLACK,0.6f));
            DrawCircleV(sp, sel?6:4, col);
            DrawTextU(std::to_string(i+1).c_str(), (int)sp.x-3, (int)sp.y-7, 13, WHITE);
        };
        for (int i = 0; i < (int)sc.actions.size(); ++i) {
            int t = sc.actions[i].type;
            if (t==SA_Effect||t==SA_Spawn||t==SA_MoveChar) drawMarker(i);
        }

        // interaction depends on the selected action
        if (scnActSel_ >= 0 && scnActSel_ < (int)sc.actions.size()) {
            SceneAction& a = sc.actions[scnActSel_];
            int tx = (int)((mouse.x - bx)/pw*mwT), ty = (int)((mouse.y - by)/ph*mhT);
            tx = std::max(0, std::min(mwT-1, tx)); ty = std::max(0, std::min(mhT-1, ty));
            if (a.type==SA_Effect || a.type==SA_Spawn || a.type==SA_MoveChar) {
                if (inMap) {
                    DrawCircleLines((int)t2s((float)tx,(float)ty).x, (int)t2s((float)tx,(float)ty).y, 5, Fade(WHITE,0.7f));
                    if (lclick) { a.x = tx; a.y = ty; p.save(); }
                }
                if (a.type==SA_Effect && inMap) { float wh=GetMouseWheelMove(); if(wh!=0){ a.radius=std::max(1,std::min(30,a.radius+(int)wh)); p.save(); } }
                DrawTextU(a.type==SA_Effect? "맵 클릭=중심, 휠=반경" : "맵 클릭=위치 지정",
                          (int)canvas.x+6, (int)(canvas.y+canvas.height-20), 13, ui::kAccentHi);
            } else if (a.type==SA_Remove) {
                // click a 등장(spawn) marker to toggle its tag in removeTags
                DrawTextU("등장한 대상(초록)을 클릭=제거 목록에 추가/취소", (int)canvas.x+6, (int)(canvas.y+canvas.height-20), 13, ui::kAccentHi);
                for (int j = 0; j < (int)sc.actions.size(); ++j) {
                    if (sc.actions[j].type != SA_Spawn) continue;
                    SceneAction& sp2 = sc.actions[j];
                    Vector2 mp = t2s((float)sp2.x, (float)sp2.y);
                    bool chosen = std::find(a.removeTags.begin(), a.removeTags.end(), sp2.targetId) != a.removeTags.end();
                    DrawCircleLines((int)mp.x, (int)mp.y, 10, chosen?ui::kDanger:Fade(WHITE,0.5f));
                    if (CheckCollisionPointCircle(mouse, mp, 10)) {
                        DrawTextU(spawnTagLabel(sp2.targetId).c_str(), (int)mp.x+12, (int)mp.y-8, 13, WHITE);
                        if (lclick) {
                            if (chosen) a.removeTags.erase(std::find(a.removeTags.begin(), a.removeTags.end(), sp2.targetId));
                            else a.removeTags.push_back(sp2.targetId);
                            p.save();
                        }
                    }
                }
            } else {
                DrawTextU("이 동작은 맵 위치가 필요 없습니다.", (int)canvas.x+6, (int)(canvas.y+canvas.height-20), 13, ui::kTextDim);
            }
        } else {
            DrawTextU("가운데에서 동작을 선택하면 여기 맵에서 편집합니다.", (int)canvas.x+6, (int)(canvas.y+canvas.height-20), 13, ui::kTextDim);
        }
    }
}

} // namespace tsukuru
