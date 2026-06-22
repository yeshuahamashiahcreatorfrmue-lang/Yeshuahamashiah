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
    const RenderTexture2D* th = m ? mapThumb(dlgMapId_) : nullptr;
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
            if (e.graphicAsset < 0) continue;
            Vector2 sp = { bx + (e.x + 0.5f) / mwT * pw, by + (e.y + 0.5f) / mhT * ph };
            bool sel = (dlgNpcEventId_ == e.id);
            const Texture2D& tex = engine_.assetTexture(e.graphicAsset);
            if (tex.id) {
                float fw = tex.width >= tex.height * 2 ? tex.width / 4.0f : (float)tex.width;
                float sc = std::min(r * 2 / fw, r * 2 / (float)tex.height);
                DrawTexturePro(tex, { 0, 0, fw, (float)tex.height },
                               { sp.x - fw * sc / 2, sp.y - tex.height * sc / 2, fw * sc, tex.height * sc }, { 0, 0 }, 0, WHITE);
            } else DrawCircleV(sp, r, ui::factionColor((int)e.faction));
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
    float ctrlW = 272, ctrlX = W - ctrlW - 8;
    float mapX = 8, mapW = ctrlX - mapX - 8;
    ui::panel({ ctrlX, top, ctrlW, panelH }, ui::kPanel);

    // ---- RIGHT control panel ----
    float cx = ctrlX + 10, cw = ctrlW - 20, cy = top + 8;
    // scene picker + new
    std::vector<std::string> sopt; std::vector<int> sval;
    for (int i = 0; i < (int)list.size(); ++i) { sopt.push_back(list[i].name); sval.push_back(i); }
    if (!list.empty()) {
        if (scnSel_ < 0 || scnSel_ >= (int)list.size()) scnSel_ = 0;
        optionButton({ cx, cy, cw - 30, 26 }, "", sopt, sval, scnSel_, 4400);
    } else DrawTextU("장면이 없습니다", (int)cx, (int)cy + 4, 13, ui::kTextDim);
    if (ui::button({ cx + cw - 26, cy, 26, 26 }, "+")) {
        Scene s; s.id = (int)list.size()+1; s.name = "장면" + std::to_string(s.id);
        list.push_back(s); scnSel_ = (int)list.size()-1; scnActSel_ = -1; p.save();
    }
    cy += 32;
    if (scnSel_ < 0 || scnSel_ >= (int)list.size()) return;
    Scene& sc = list[scnSel_];
    if (sc.editMapId < 0 || !p.map(sc.editMapId)) {
        for (auto& mm : p.maps) if (mm->placed) { sc.editMapId = mm->id; break; }
        if (sc.editMapId < 0 && !p.maps.empty()) sc.editMapId = p.maps.front()->id;
    }
    { // name + delete
        Rectangle nf = { cx, cy, cw - 56, 24 };
        if (ui::mouseIn(nf) && lclick) scnFocus_ = 0; else if (lclick && !ui::mouseIn(nf) && scnFocus_ == 0) scnFocus_ = -1;
        ui::textField(nf, sc.name, scnFocus_ == 0, 40);
        if (ui::button({ cx + cw - 50, cy, 50, 24 }, "삭제")) { list.erase(list.begin()+scnSel_); scnSel_=-1; scnActSel_=-1; p.save(); return; }
    }
    cy += 30;
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

    // ── 장면녹화 모드 토글 + 컨트롤 ──
    if (ui::button({ cx, cy, cw, 26 }, scnRecordMode_ ? "장면녹화 모드: 켜짐 (끄기)" : "장면녹화 모드 켜기", scnRecordMode_)) {
        scnRecordMode_ = !scnRecordMode_; scnDraft_.clear(); scnPendingFx_.clear();
        scnSelTags_.clear(); scnGroupDrag_ = false; scnMarquee_ = false; scnCtxOpen_ = false; scnRecPlaceChar_ = -1;
    }
    cy += 30;
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
        if (ui::button({ cx, cy, cw, 28 }, "● 장면 녹화 (현재 배치 기록)")) {
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

    // add-action toolbar (7 types) — appends + selects
    DrawTextU("동작 추가:", (int)cx, (int)cy, 12, ui::kTextDim); cy += 16;
    for (int t = 0; t < 7; ++t) {
        Rectangle b = { cx + (t%3)*(cw/3), cy + (t/3)*28, cw/3 - 4, 26 };
        if (ui::button(b, std::string("+") + kSceneActNames[t])) {
            SceneAction na; na.type = t; na.time = 1.0f;
            sc.actions.push_back(na); scnActSel_ = (int)sc.actions.size()-1; p.save();
        }
    }
    cy += 88;
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
            if (ui::button({ cx, chy, cw - 78, 22 }, lbl, scnActSel_ == i)) scnActSel_ = (scnActSel_==i)?-1:i;
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
            optionButton({ cx, cy, cw, 24 }, "이동 NPC", o, v, a.targetId, 4150); cy += 28;
            DrawTextU(TextFormat("목적지: %d,%d (맵 클릭/드래그)", a.x, a.y), (int)cx, (int)cy, 11, ui::kTextDim); cy += 18;
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
            DrawTextU(TextFormat("위치: %d,%d · 반경 %d칸 (맵 클릭/휠)", a.x, a.y, a.radius), (int)cx, (int)cy, 11, ui::kTextDim); cy += 18;
            durControl(a.time);
        } else if (a.type == SA_Spawn) {
            ui::intStepper({ cx, cy, cw/2-4, 24 }, "태그", a.targetId, 1, 1, 99);
            entityButton({ cx+cw/2+2, cy, cw/2-4, 24 }, "몹", a.refId, ENT_Mob, 4300); cy += 28;
            DrawTextU(TextFormat("등장: %d,%d (맵 클릭/드래그)", a.x, a.y), (int)cx, (int)cy, 11, ui::kTextDim); cy += 18;
        } else if (a.type == SA_Remove) {
            std::string s2 = "제거: ";
            if (a.removeTags.empty()) s2 += "(맵에서 대상 클릭)";
            else for (int t : a.removeTags) s2 += "#" + std::to_string(t) + " ";
            DrawTextU(s2.c_str(), (int)cx, (int)cy, 11, a.removeTags.empty()?ui::kTextDim:ui::kAccentHi); cy += 18;
            if (ui::button({ cx, cy, cw, 22 }, "선택 비우기")) { a.removeTags.clear(); p.save(); } cy += 26;
        } else if (a.type == SA_Wait) {
            std::vector<std::string> o = { "전체" }; std::vector<int> v = { -1 };
            for (auto& s2 : sc.actions) if (s2.type==SA_Spawn) { o.push_back(spawnTagLabel(s2.targetId)); v.push_back(s2.targetId); }
            optionButton({ cx, cy, cw, 24 }, "대기대상", o, v, a.targetId, 4160); cy += 28;
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
    const RenderTexture2D* th = m ? mapThumb(sc.editMapId) : nullptr;
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
    for (auto& e : m->events) {
        if (e.graphicAsset < 0 && e.charId < 0) continue;
        Vector2 sp = t2s((float)e.x, (float)e.y);
        if (e.id == trigNpcId) { DrawCircleLines((int)sp.x,(int)sp.y,9,ui::kGood); DrawTextU("발동", (int)sp.x+8,(int)sp.y-8,11,ui::kGood); }
        if (scnTrigMode_==2 && CheckCollisionPointCircle(mouse, sp, 8)) {
            DrawCircleLines((int)sp.x,(int)sp.y,9,WHITE);
            if (lclick) { for (auto& e2 : m->events) if (e2.sceneId==sc.id && e2.graphicAsset>=0) e2.sceneId=-1; e.sceneId=sc.id; scnTrigMode_=0; p.save(); setStatus("이 NPC와 대화 시 시나리오 발동"); }
        }
    }
    // trigger point marker (star-ish), draggable
    if (trigPoint) {
        Vector2 sp = (scnDragIdx_==-2) ? mouse : t2s((float)trigPoint->x, (float)trigPoint->y);
        DrawPoly(sp, 5, 9, 0, ui::kGood); DrawPolyLines(sp, 5, 9, 0, WHITE);
        DrawTextU("발동지점", (int)sp.x+10, (int)sp.y-8, 11, ui::kGood);
        if (CheckCollisionPointCircle(mouse, t2s((float)trigPoint->x,(float)trigPoint->y), 11) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && scnTrigMode_==0)
            scnDragIdx_ = -2;
    }

  if (!scnRecordMode_) {
    // action markers (effect/spawn/move) — draggable (RTS)
    auto markerColor = [&](int t){ return t==SA_Effect?Color{250,180,60,255}:t==SA_Spawn?Color{120,200,120,255}:t==SA_MoveChar?Color{120,170,250,255}:ui::kTextDim; };
    for (int i = 0; i < (int)sc.actions.size(); ++i) {
        SceneAction& a = sc.actions[i];
        if (a.type!=SA_Effect && a.type!=SA_Spawn && a.type!=SA_MoveChar) continue;
        bool sel = (scnActSel_==i), drag = (scnDragIdx_==i);
        Vector2 sp = drag ? mouse : t2s((float)a.x, (float)a.y);
        if (a.type==SA_Effect) { float rr=std::max(1,a.radius)*tileSp; DrawCircleLines((int)sp.x,(int)sp.y,rr,Fade(markerColor(a.type),sel?0.9f:0.4f)); DrawCircle((int)sp.x,(int)sp.y,rr,Fade(markerColor(a.type),0.10f)); }
        DrawCircleV(sp, sel?7:5, Fade(BLACK,0.6f));
        DrawCircleV(sp, sel?6:4, markerColor(a.type));
        DrawTextU(std::to_string(i+1).c_str(), (int)sp.x-3, (int)sp.y-7, 13, WHITE);
        if (!drag && scnDragIdx_==-1 && scnTrigMode_==0 && CheckCollisionPointCircle(mouse, sp, 8) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            scnActSel_ = i; scnDragIdx_ = i;
        }
    }
    // dragging update / release
    if (scnDragIdx_ >= 0 && scnDragIdx_ < (int)sc.actions.size()) {
        int tx, ty; s2t(tx, ty);
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { sc.actions[scnDragIdx_].x = tx; sc.actions[scnDragIdx_].y = ty; }
        else { p.save(); scnDragIdx_ = -1; }
    } else if (scnDragIdx_ == -2 && trigPoint) {
        int tx, ty; s2t(tx, ty);
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { trigPoint->x = tx; trigPoint->y = ty; }
        else { p.save(); scnDragIdx_ = -1; }
    }

    // placing the trigger point (발동지점 mode): click empty map
    if (scnTrigMode_==1 && inMap && lclick && scnDragIdx_<0) {
        int tx, ty; s2t(tx, ty);
        if (!trigPoint) {
            Event ne; ne.id = m->nextEventId(); ne.type = EventType::Message; ne.trigger = TriggerType::PlayerTouch;
            ne.graphicAsset = -1; ne.sceneId = sc.id; ne.label = "[시나리오 발동]"; ne.x = tx; ne.y = ty;
            m->events.push_back(ne);
        } else { trigPoint->x = tx; trigPoint->y = ty; }
        scnTrigMode_ = 0; p.save(); setStatus("발동 지점 설정됨(밟으면 시작)");
    }

    // selected action: click empty map to place, wheel = effect radius, remove = pick spawns
    if (scnActSel_ >= 0 && scnActSel_ < (int)sc.actions.size() && scnDragIdx_ < 0 && scnTrigMode_==0) {
        SceneAction& a = sc.actions[scnActSel_];
        if (a.type==SA_Effect || a.type==SA_Spawn || a.type==SA_MoveChar) {
            if (inMap) {
                int tx, ty; s2t(tx, ty);
                DrawCircleLines((int)t2s((float)tx,(float)ty).x, (int)t2s((float)tx,(float)ty).y, 5, Fade(WHITE,0.7f));
                if (lclick) { a.x = tx; a.y = ty; p.save(); }
                if (a.type==SA_Effect) { float wh=GetMouseWheelMove(); if(wh!=0){ a.radius=std::max(1,std::min(30,a.radius+(int)wh)); p.save(); } }
            }
        } else if (a.type==SA_Remove) {
            for (int j = 0; j < (int)sc.actions.size(); ++j) {
                if (sc.actions[j].type != SA_Spawn) continue;
                SceneAction& sp2 = sc.actions[j];
                Vector2 mp = t2s((float)sp2.x, (float)sp2.y);
                bool chosen = std::find(a.removeTags.begin(), a.removeTags.end(), sp2.targetId) != a.removeTags.end();
                DrawCircleLines((int)mp.x, (int)mp.y, 10, chosen?ui::kDanger:Fade(WHITE,0.5f));
                if (CheckCollisionPointCircle(mouse, mp, 10) && lclick) {
                    if (chosen) a.removeTags.erase(std::find(a.removeTags.begin(), a.removeTags.end(), sp2.targetId));
                    else a.removeTags.push_back(sp2.targetId);
                    p.save();
                }
            }
        }
    }
  } else {
    // ── 녹화 모드 (RTS식): 마퀴 다중선택 · 그룹 드래그 이동 · 우클릭 전체 동작/삭제 ──
    for (auto& kv : stage) if (!scnDraft_.count(kv.first)) scnDraft_[kv.first] = kv.second;  // init draft
    auto selected = [&](int tag){ return std::find(scnSelTags_.begin(), scnSelTags_.end(), tag) != scnSelTags_.end(); };
    // token under cursor
    int hovTag = -1000;
    for (auto& kv : scnDraft_) if (CheckCollisionPointCircle(mouse, t2s(kv.second.x, kv.second.y), 11)) hovTag = kv.first;

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
    // tokens (selected = bright ring)
    for (auto& kv : scnDraft_) {
        int tag = kv.first; Vector2 sp = t2s(kv.second.x, kv.second.y);
        Color col = (tag == 0) ? Color{120,170,250,255} : Color{120,200,120,255};
        if (selected(tag)) DrawCircleLines((int)sp.x,(int)sp.y, 13, ui::kAccentHi);
        DrawCircleV(sp, 10, Fade(BLACK,0.6f)); DrawCircleV(sp, 8, col);
        DrawTextU((tag==0 ? std::string("플레이어") : spawnTagLabel(tag)).c_str(), (int)sp.x+11, (int)sp.y-8, 12, WHITE);
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
                     : "마커 드래그=이동 · 빈곳 클릭=배치 · 휠=이펙트 반경";
    DrawTextU(hint, (int)canvas.x+8, (int)(canvas.y+canvas.height-22), 13, ui::kAccentHi);
}

} // namespace tsukuru
