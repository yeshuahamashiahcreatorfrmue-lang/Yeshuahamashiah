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

    // ---- LEFT: scenario list ----
    {
        float x = lx + 8, w = lw - 16, y = top + 8;
        if (ui::button({ x, y, w, 28 }, "+ 새 대화")) {
            DialogueScenario d; d.id = (int)list.size() + 1; d.name = "대화" + std::to_string(d.id);
            d.lines.push_back({ "", "...", {} });
            list.push_back(d); dlgSel_ = (int)list.size() - 1; dlgLineSel_ = 0; p.save();
        }
        y += 34;
        for (int i = 0; i < (int)list.size(); ++i) {
            if (ui::button({ x, y, w, 26 }, list[i].name, dlgSel_ == i)) { dlgSel_ = i; dlgLineSel_ = 0; dlgFocus_ = -1; }
            y += 28;
        }
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
                if (ui::button({ x, ly, w-30, 26 }, lbl, dlgLineSel_ == i)) { dlgLineSel_ = i; dlgFocus_ = -1; }
                if (ui::button({ x+w-28, ly, 28, 26 }, "x")) { d.lines.erase(d.lines.begin()+i); if(dlgLineSel_>=(int)d.lines.size())dlgLineSel_=(int)d.lines.size()-1; p.save(); EndScissorMode(); return; }
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
                ui::intStepper({ ix, iy, iw/2-4, 24 }, "아이템ID", a.rewardItemId, 1, -1, 9999);
                ui::intStepper({ ix+iw/2+4, iy, iw/2-4, 24 }, "개수", a.rewardItemCount, 1, 1, 999); iy += 28;
            } else if (a.respType == DR_SpawnMob) {
                ui::intStepper({ ix, iy, iw, 24 }, "몹ID(db.mobs)", a.mobId, 1, -1, 9999); iy += 28;
            } else if (a.respType == DR_NpcHostile || a.respType == DR_NpcFriendly || a.respType == DR_NpcFollow) {
                ui::intStepper({ ix, iy, iw/2-4, 24 }, "NPC ID", a.npcCharId, 1, -1, 9999);
                int ms = (int)(a.durationSecs*1000);
                if (ui::intStepper({ ix+iw/2+4, iy, iw/2-4, 24 }, "시간(s·0=무제한)", ms, 1000, 0, 600000)) a.durationSecs = ms/1000.0f;
                iy += 28;
                if (a.respType == DR_NpcFollow)
                    if (ui::button({ ix, iy, iw, 24 }, a.dismissFollowers ? "추종 해제 대답: 켜짐" : "추종 해제 대답: 꺼짐", a.dismissFollowers)) { a.dismissFollowers = !a.dismissFollowers; p.save(); }
            } else if (a.respType == DR_Scene) {
                ui::intStepper({ ix, iy, iw, 24 }, "시나리오ID(시나리오 탭)", a.sceneId, 1, -1, 9999); iy += 28;
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
    ui::label("스토리 시나리오 — 장면별 동작(이동·대화·이펙트·등장·제거·대기)을 순서대로", 14, (int)kToolbarH + 8, 18, ui::kAccent);

    float top = kToolbarH + 36, botY = H - 10, panelH = botY - top;
    float lw = 250, pad = 10;
    float lx = 10, rx = lx + lw + pad, rw = W - rx - 10;
    ui::panel({ lx, top, lw, panelH }, ui::kPanel);
    ui::panel({ rx, top, rw, panelH }, ui::kPanel);
    if (scnSel_ < 0 && !list.empty()) scnSel_ = 0;

    // ---- LEFT: scene list ----
    {
        float x = lx + 8, w = lw - 16, y = top + 8;
        if (ui::button({ x, y, w, 28 }, "+ 새 장면")) {
            Scene s; s.id = (int)list.size()+1; s.name = "장면" + std::to_string(s.id);
            list.push_back(s); scnSel_ = (int)list.size()-1; p.save();
        }
        y += 34;
        for (int i = 0; i < (int)list.size(); ++i) {
            if (ui::button({ x, y, w, 26 }, list[i].name, scnSel_ == i)) { scnSel_ = i; scnFocus_ = -1; }
            y += 28;
        }
        float by = top + panelH - 70;
        if (scnSel_ >= 0 && scnSel_ < (int)list.size()) {
            DrawTextU("이름", (int)x, (int)by, 12, ui::kTextDim); by += 16;
            Rectangle nf = { x, by, w, 26 };
            if (ui::mouseIn(nf) && lclick) scnFocus_ = 0; else if (lclick && !ui::mouseIn(nf) && scnFocus_==0) scnFocus_=-1;
            ui::textField(nf, list[scnSel_].name, scnFocus_ == 0, 40); by += 30;
            if (ui::button({ x, by, w, 24 }, "장면 삭제")) { list.erase(list.begin()+scnSel_); scnSel_=-1; p.save(); return; }
        }
    }
    if (scnSel_ < 0 || scnSel_ >= (int)list.size()) return;
    Scene& sc = list[scnSel_];

    // ---- RIGHT: action sequence ----
    float x = rx + 10, w = rw - 20, y = top + 10;
    DrawTextU(TextFormat("동작 순서 (%d)", (int)sc.actions.size()), (int)x, (int)y, 15, ui::kAccent); y += 22;
    if (ui::button({ x, y, 150, 26 }, "+ 동작 추가")) { sc.actions.push_back({}); p.save(); }
    y += 32;
    Rectangle reg = { rx, y, rw, top + panelH - y - 8 };
    uiScissor((int)rx, (int)y, (int)rw, (int)reg.height);
    if (ui::mouseIn(reg)) scnActScroll_ -= GetMouseWheelMove()*36; if (scnActScroll_<0) scnActScroll_=0;
    float ay = y - scnActScroll_;
    for (int i = 0; i < (int)sc.actions.size(); ++i) {
        SceneAction& a = sc.actions[i];
        float cardH = 96;
        if (ay + cardH > y - 30 && ay < y + reg.height) {
            ui::panel({ x-2, ay, w+4, cardH-6 }, ui::kPanelHi);
            float ix = x + 6, iw = w - 12, iy = ay + 6;
            DrawTextU(TextFormat("%d.", i+1), (int)ix, (int)iy+4, 14, ui::kAccentHi);
            optionButton({ ix + 28, iy, 130, 24 }, "종류",
                { kSceneActNames[0],kSceneActNames[1],kSceneActNames[2],kSceneActNames[3],kSceneActNames[4],kSceneActNames[5] },
                {}, a.type, 4000 + i);
            if (ui::button({ ix + iw - 60, iy, 28, 24 }, "위로")) { if(i>0){ std::swap(sc.actions[i],sc.actions[i-1]); p.save(); } }
            if (ui::button({ ix + iw - 30, iy, 30, 24 }, "x")) { sc.actions.erase(sc.actions.begin()+i); p.save(); EndScissorMode(); return; }
            iy += 28;
            // params per type
            if (a.type == SA_MoveChar) {
                ui::intStepper({ ix, iy, iw/3-4, 24 }, "대상태그", a.targetId, 1, 0, 99);
                ui::intStepper({ ix+iw/3+2, iy, iw/3-4, 24 }, "X", a.x, 1, 0, 1742);
                ui::intStepper({ ix+2*iw/3+4, iy, iw/3-4, 24 }, "Y", a.y, 1, 0, 1742); iy += 28;
                int ms=(int)(a.time*1000); if (ui::intStepper({ ix, iy, iw, 24 }, "이동시간(ms)", ms, 100, 0, 60000)) a.time=ms/1000.0f;
            } else if (a.type == SA_Dialogue) {
                ui::intStepper({ ix, iy, iw, 24 }, "대화ID(db.dialogues)", a.refId, 1, -1, 9999);
            } else if (a.type == SA_Effect) {
                ui::intStepper({ ix, iy, iw/3-4, 24 }, "이펙트에셋", a.refId, 1, -1, 9999);
                ui::intStepper({ ix+iw/3+2, iy, iw/3-4, 24 }, "X", a.x, 1, 0, 1742);
                ui::intStepper({ ix+2*iw/3+4, iy, iw/3-4, 24 }, "Y", a.y, 1, 0, 1742); iy += 28;
                int ms=(int)(a.time*1000); if (ui::intStepper({ ix, iy, iw, 24 }, "지속(ms)", ms, 100, 0, 60000)) a.time=ms/1000.0f;
            } else if (a.type == SA_Spawn) {
                ui::intStepper({ ix, iy, iw/4-4, 24 }, "태그", a.targetId, 1, 0, 99);
                ui::intStepper({ ix+iw/4+2, iy, iw/4-4, 24 }, "몹ID", a.refId, 1, -1, 9999);
                ui::intStepper({ ix+2*iw/4+2, iy, iw/4-4, 24 }, "X", a.x, 1, 0, 1742);
                ui::intStepper({ ix+3*iw/4+4, iy, iw/4-4, 24 }, "Y", a.y, 1, 0, 1742);
            } else if (a.type == SA_Remove) {
                ui::intStepper({ ix, iy, iw, 24 }, "제거할 태그", a.targetId, 1, 0, 99);
            } else { // SA_Wait
                int ms=(int)(a.time*1000); if (ui::intStepper({ ix, iy, iw, 24 }, "대기(ms)", ms, 100, 0, 60000)) a.time=ms/1000.0f;
            }
        }
        ay += cardH;
    }
    EndScissorMode();
}

} // namespace tsukuru
