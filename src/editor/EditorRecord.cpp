// EditorRecord: 라이브 RTS 녹화 — 실시간으로 유닛을 조종(드래그 선택·우클릭 길찾기
// 이동·공격/죽음 동작)하며 동영상처럼 타임라인을 기록하고, '녹화 완료' 시 그 연기를
// 스토리 시나리오(Scene) 동작들로 변환한다. 재생(테스트)으로 바로 확인.
#include "editor/Editor.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "database/Database.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace tsukuru {

// BFS 4방향 길찾기: 시작→목표 타일 경로(시작 제외, 목표 포함). 막히면 빈 경로.
std::vector<Vector2> Editor::findPath(Map& m, int sx, int sy, int tx, int ty) {
    int W = m.tilemap.width(), H = m.tilemap.height();
    std::vector<Vector2> out;
    if (W <= 0 || H <= 0) return out;
    if (tx < 0 || ty < 0 || tx >= W || ty >= H) return out;
    if (m.tilemap.blocked(tx, ty) || (sx == tx && sy == ty)) return out;
    std::vector<int> prev(W * H, -2);
    std::queue<int> q; int s = sy * W + sx, goal = ty * W + tx;
    prev[s] = -1; q.push(s);
    const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    bool found = false; int iter = 0;
    while (!q.empty() && iter++ < 60000) {
        int c = q.front(); q.pop();
        if (c == goal) { found = true; break; }
        int cx = c % W, cy = c / W;
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int n = ny * W + nx;
            if (prev[n] != -2 || m.tilemap.blocked(nx, ny)) continue;
            prev[n] = c; q.push(n);
        }
    }
    if (!found) return out;
    std::vector<Vector2> rev; int c = goal;
    while (c != -1) { rev.push_back({ (float)(c % W), (float)(c / W) }); c = prev[c]; }
    for (int i = (int)rev.size() - 2; i >= 0; --i) out.push_back(rev[i]);
    return out;
}

void Editor::liveInitUnits(Scene& sc, Map& m) {
    liveUnits_.clear(); liveSel_.clear();
    LiveUnit pl; pl.tag = 0; pl.charId = engine_.project().playerCharId; pl.isMob = false;
    pl.tx = m.tilemap.width() / 2.0f; pl.ty = m.tilemap.height() / 2.0f;
    liveUnits_.push_back(pl);
    for (auto& a : sc.actions) if (a.type == SA_Spawn) {
        LiveUnit u; u.tag = a.targetId; u.isMob = (a.refId >= 0);
        u.charId = a.refId >= 0 ? a.refId : (-a.refId - 1);
        u.tx = (float)a.x; u.ty = (float)a.y; liveUnits_.push_back(u);
    }
    scnLiveInit_ = true;
}

// 기록된 타임라인을 장면 동작들로 변환(동시 명령은 한 순간으로 묶고, 간격은 대기로).
void Editor::liveBuildScene(Scene& sc, Map& m) {
    std::sort(recCmds_.begin(), recCmds_.end(), [](const RecCmd& a, const RecCmd& b){ return a.t < b.t; });
    sc.actions.clear();
    std::unordered_map<int, Vector2> cur;     // 태그별 현재 타일(이동시간 계산용)
    cur[0] = { m.tilemap.width() / 2.0f, m.tilemap.height() / 2.0f };
    const float speed = 5.0f;
    float cursor = 0; size_t i = 0;
    while (i < recCmds_.size()) {
        float t = recCmds_[i].t;
        if (t - cursor > 0.06f) { SceneAction w; w.type = SA_Wait; w.time = t - cursor; sc.actions.push_back(w); cursor = t; }
        while (i < recCmds_.size() && recCmds_[i].t <= t + 0.06f) {
            const RecCmd& c = recCmds_[i];
            if (c.type == 2) {                                  // 등장
                SceneAction a; a.type = SA_Spawn; a.targetId = c.tag;
                a.refId = c.isMob ? c.charId : (-c.charId - 1); a.x = c.x; a.y = c.y;
                sc.actions.push_back(a); cur[c.tag] = { (float)c.x, (float)c.y };
            } else if (c.type == 0) {                           // 이동
                Vector2 pcur = cur.count(c.tag) ? cur[c.tag] : Vector2{ (float)c.x, (float)c.y };
                float dist = std::hypot(c.x - pcur.x, c.y - pcur.y);
                SceneAction a; a.type = SA_MoveChar; a.targetId = c.tag; a.x = c.x; a.y = c.y;
                a.time = std::max(0.42f, dist / speed);
                sc.actions.push_back(a); cur[c.tag] = { (float)c.x, (float)c.y };
            } else if (c.type == 1) {                           // 동작(공격/죽음 등)
                SceneAction a; a.type = SA_Motion; a.targetId = c.tag; a.refId = c.motion; a.time = 0.8f;
                sc.actions.push_back(a);
            } else if (c.type == 3) {                           // 제거
                SceneAction a; a.type = SA_Remove; a.targetId = c.tag; sc.actions.push_back(a);
            } else if (c.type == 4) {                           // 이펙트
                SceneAction a; a.type = SA_Effect; a.refId = c.charId; a.x = c.x; a.y = c.y;
                a.radius = std::max(1, c.motion); a.time = 0.8f; sc.actions.push_back(a);
            }
            ++i;
        }
    }
    engine_.project().save();
}

void Editor::drawLiveRecorder(Scene& sc) {
    Project& p = engine_.project();
    Database& db = p.database;
    float W = (float)screenW(), H = (float)screenH();
    DrawRectangleRec({ 0, kToolbarH, W, H - kToolbarH }, Color{ 16, 18, 24, 255 });
    auto m = p.map(sc.editMapId);
    if (!m) { scnLive_ = false; return; }
    if (!scnLiveInit_) liveInitUnits(sc, *m);
    float dt = GetFrameTime(); if (dt > 0.05f) dt = 0.05f;
    float simDt = scnPaused_ ? 0.0f : dt;       // 일시정지면 시간·이동 정지(상황 세팅용)
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool rclick = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
    Vector2 mouse = GetMousePosition();
    auto unitDef = [&](const LiveUnit& u)->const CharacterDef* {
        return u.charId < 0 ? nullptr : (u.isMob ? db.mob(u.charId) : db.character(u.charId));
    };
    auto selected = [&](int tag){ return std::find(liveSel_.begin(), liveSel_.end(), tag) != liveSel_.end(); };

    // ---- top control bar (2 rows) ----
    float by = kToolbarH + 6, bx = 8;
    auto tbtn = [&](const char* t, float w, bool on=false)->bool { bool r = ui::button({ bx, by, w, 28 }, t, on); bx += w + 4; return r; };
    if (tbtn(scnRecording_ ? "녹화 완료(저장)" : "녹화 시작", 120, scnRecording_)) {
        if (!scnRecording_) {                       // 시작: 초기화 + 등장 기록(t=0)
            liveInitUnits(sc, *m); recCmds_.clear(); scnRecClock_ = 0; scnRecording_ = true; scnPaused_ = false;
            for (auto& u : liveUnits_) if (u.tag != 0)
                recCmds_.push_back({ 0, 2, u.tag, (int)std::lround(u.tx), (int)std::lround(u.ty), u.charId, 0, u.isMob });
        } else { scnRecording_ = false; scnPaused_ = false; liveBuildScene(sc, *m); setStatus("녹화 완료 → 장면 저장됨"); }
    }
    if (tbtn(scnPaused_ ? "▶ 재개 (P)" : "일시정지 (P)", 130, scnPaused_) || IsKeyPressed(KEY_P)) scnPaused_ = !scnPaused_;
    if (tbtn("▶ 재생(테스트)", 120)) { engine_.startPlaytestScene(sc.id); scnLive_ = false; return; }
    if (tbtn("닫기", 64)) { scnLive_ = false; scnRecording_ = false; scnPaused_ = false; liveFxMode_ = false; return; }
    if (scnRecording_) { if (!scnPaused_) scnRecClock_ += dt;
        DrawTextU(TextFormat(scnPaused_ ? "일시정지  %.1f초  (명령 %d)" : "REC  %.1f초  (명령 %d)", scnRecClock_, (int)recCmds_.size()),
                  (int)bx + 6, (int)by + 6, 15, scnPaused_ ? ui::kAccentHi : ui::kDanger); }
    // row 2
    by += 32; bx = 8;
    if (tbtn("+ 유닛", 80)) openCharBrowser([this](int cid){ livePlaceChar_ = cid; });
    auto applyMotion = [&](int mo){
        for (auto& u : liveUnits_) if (selected(u.tag)) { u.motion = mo; u.motionT = 0.8f;
            if (scnRecording_) recCmds_.push_back({ scnRecClock_, 1, u.tag, 0,0, 0, mo, false }); }
    };
    if (tbtn("공격", 64)) applyMotion(MO_Attack);
    if (tbtn("죽음", 64)) applyMotion(MO_Death);
    if (tbtn("제거", 64)) {
        for (auto& u : liveUnits_) if (selected(u.tag) && u.tag != 0) {
            u.dead = true; if (scnRecording_) recCmds_.push_back({ scnRecClock_, 3, u.tag, 0,0,0,0,false }); }
        liveUnits_.erase(std::remove_if(liveUnits_.begin(), liveUnits_.end(), [](const LiveUnit& u){ return u.dead; }), liveUnits_.end());
        liveSel_.clear();
    }
    if (tbtn(liveFxMode_ ? "이펙트: 맵클릭" : "이펙트 뿌리기", 110, liveFxMode_)) liveFxMode_ = !liveFxMode_;
    assetButton({ bx, by, 130, 28 }, "", scnRecEffect_, 4810); bx += 134;
    ui::intStepper({ bx, by, 96, 28 }, "반경", scnRecRadius_, 1, 1, 20); bx += 100;

    // ---- map canvas ----
    Rectangle canvas = { 8, by + 38, W - 16, H - (by + 38) - 10 };
    DrawRectangleRec(canvas, Color{ 18, 20, 26, 255 });
    const RenderTexture2D* th = bestThumb(sc.editMapId);
    if (!th || m->tilemap.width() <= 0) { DrawTextU("맵 미리보기 없음", (int)canvas.x+10, (int)canvas.y+10, 14, ui::kTextDim); return; }
    float tw = (float)th->texture.width, tht = (float)th->texture.height;
    float s = std::min(canvas.width / tw, canvas.height / tht);
    float pw = tw * s, ph = tht * s, ox = canvas.x + (canvas.width - pw)/2, oy = canvas.y + (canvas.height - ph)/2;
    Rectangle imgR = { ox, oy, pw, ph };
    DrawTexturePro(th->texture, { 0,0,tw,-tht }, imgR, {0,0}, 0, WHITE);
    DrawRectangleLinesEx(imgR, 1, Fade(BLACK, 0.6f));
    int mwT = m->tilemap.width(), mhT = m->tilemap.height();
    bool inMap = CheckCollisionPointRec(mouse, imgR);
    auto t2s = [&](float tx, float ty){ return Vector2{ ox + (tx+0.5f)/mwT*pw, oy + (ty+0.5f)/mhT*ph }; };
    auto s2t = [&](int& tx, int& ty){ tx = std::max(0,std::min(mwT-1,(int)((mouse.x-ox)/pw*mwT))); ty = std::max(0,std::min(mhT-1,(int)((mouse.y-oy)/ph*mhT))); };
    float unitR = std::max(9.0f, std::min(pw/mwT, ph/mhT) * 0.55f);

    // update + draw units
    int hov = -1;
    for (int i = 0; i < (int)liveUnits_.size(); ++i) if (CheckCollisionPointCircle(mouse, t2s(liveUnits_[i].tx, liveUnits_[i].ty), unitR+2)) hov = i;
    for (auto& u : liveUnits_) {
        // movement along path (frozen while paused: simDt == 0)
        if (!u.path.empty() && simDt > 0) {
            Vector2 w = u.path.front();
            float ddx = w.x - u.tx, ddy = w.y - u.ty, dist = std::sqrt(ddx*ddx + ddy*ddy);
            float step = 5.0f * simDt;
            if (dist <= step) { u.tx = w.x; u.ty = w.y; u.path.erase(u.path.begin()); }
            else { u.tx += ddx/dist*step; u.ty += ddy/dist*step; }
            u.dir = std::fabs(ddx) > std::fabs(ddy) ? (ddx >= 0 ? 2 : 1) : (ddy >= 0 ? 0 : 3);
            u.animT += simDt; if (u.animT > 0.12f) { u.animT = 0; u.frame = (u.frame+1)%4; }
        }
        if (u.motionT > 0 && simDt > 0) { u.motionT -= simDt; u.animT += simDt; if (u.animT > 0.1f) { u.animT = 0; u.frame = (u.frame+1)%4; } if (u.motionT <= 0) u.motion = MO_Walk; }
        // draw sprite
        Vector2 sp = t2s(u.tx, u.ty);
        if (selected(u.tag)) { DrawCircleLines((int)sp.x,(int)sp.y, unitR+4, ui::kAccentHi); DrawCircleLines((int)sp.x,(int)sp.y, unitR+5, ui::kAccentHi); }
        const CharacterDef* cd = unitDef(u);
        int mo = (u.motionT > 0) ? u.motion : MO_Walk;
        int asset = -1;
        if (cd) { const auto& fr = cd->motions[mo].dirFrames(u.dir).empty() ? cd->motions[MO_Walk].dirFrames(u.dir) : cd->motions[mo].dirFrames(u.dir);
                  if (!fr.empty()) asset = fr[u.frame % (int)fr.size()]; }
        Color col = (u.tag==0) ? Color{120,170,250,255} : Color{120,200,120,255};
        if (asset >= 0) { const Texture2D& tex = engine_.assetTexture(asset);
            if (tex.id) { float fw = tex.width>=tex.height*2 ? tex.width/4.0f : (float)tex.width;
                float scl = std::min(unitR*2/fw, unitR*2/(float)tex.height);
                DrawCircleV(sp, unitR+1, Fade(col,0.45f));
                DrawTexturePro(tex, {0,0,fw,(float)tex.height}, {sp.x-fw*scl/2, sp.y-tex.height*scl/2, fw*scl, tex.height*scl}, {0,0}, 0, WHITE);
            } else { DrawCircleV(sp, unitR, col); }
        } else { DrawCircleV(sp, unitR, col); }
        DrawTextU((u.tag==0?"플레이어":("#"+std::to_string(u.tag))).c_str(), (int)sp.x+unitR, (int)sp.y-8, 12, WHITE);
        // path preview
        Vector2 prev = sp;
        for (auto& wp : u.path) { Vector2 q = t2s(wp.x, wp.y); DrawLineEx(prev, q, 1.5f, Fade(col,0.5f)); prev = q; }
    }

    // ---- place a browsed unit on map ----
    if (livePlaceChar_ >= 0) {
        if (inMap) { int tx, ty; s2t(tx, ty);
            DrawCircleLines((int)t2s((float)tx,(float)ty).x,(int)t2s((float)tx,(float)ty).y, unitR, ui::kAccentHi);
            DrawTextU("여기 유닛 배치", (int)mouse.x+12, (int)mouse.y-8, 12, ui::kAccentHi);
            if (lclick) {
                int tag = 1; for (auto& u : liveUnits_) if (u.tag >= tag) tag = u.tag + 1;
                LiveUnit nu; nu.tag = tag; nu.charId = livePlaceChar_; nu.isMob = false; nu.tx = tx; nu.ty = ty;
                liveUnits_.push_back(nu);
                if (scnRecording_) recCmds_.push_back({ scnRecClock_, 2, tag, tx, ty, livePlaceChar_, 0, false });
                livePlaceChar_ = -1;
            }
        }
        return;   // placement mode consumes input
    }

    // recorded effects: show rings (배치한 이펙트 위치)
    for (auto& c : recCmds_) if (c.type == 4) {
        Vector2 fp = t2s((float)c.x, (float)c.y); float rr = std::max(1, c.motion) * (pw/mwT);
        DrawCircleLines((int)fp.x,(int)fp.y, rr, Fade(Color{250,180,60,255}, 0.8f));
        DrawCircle((int)fp.x,(int)fp.y, rr, Fade(Color{250,180,60,255}, 0.10f));
    }

    // ---- effect placement mode: 맵 클릭 = 이펙트 기록(현재 시계 시각) ----
    if (liveFxMode_) {
        if (inMap) {
            int tx, ty; s2t(tx, ty);
            Vector2 g = t2s((float)tx,(float)ty); float rr = std::max(1, scnRecRadius_) * (pw/mwT);
            DrawCircleLines((int)g.x,(int)g.y, rr, ui::kAccentHi);
            if (lclick && scnRecEffect_ >= 0) {
                recCmds_.push_back({ scnRecClock_, 4, 0, tx, ty, scnRecEffect_, scnRecRadius_, false });
                setStatus(scnRecording_ ? "이펙트 기록됨" : "녹화 중이 아닙니다(이펙트는 녹화 중에만 기록)");
            }
        }
        DrawTextU("이펙트 모드: 맵 클릭=뿌리기(녹화 중 기록) · 오른쪽 위 목록에서 이펙트 선택",
                  (int)canvas.x+8, (int)(canvas.y+canvas.height-22), 13, ui::kAccentHi);
        return;   // effect mode consumes input
    }

    // ---- selection + commands ----
    if (lclick && inMap && !liveMarquee_) {
        if (hov >= 0) liveSel_ = { liveUnits_[hov].tag };       // click a unit = select it
        else { liveMarquee_ = true; liveMarqueeStart_ = mouse; }
    }
    if (liveMarquee_) {
        Rectangle mr = { std::min(liveMarqueeStart_.x,mouse.x), std::min(liveMarqueeStart_.y,mouse.y),
                         std::fabs(mouse.x-liveMarqueeStart_.x), std::fabs(mouse.y-liveMarqueeStart_.y) };
        DrawRectangleRec(mr, Fade(ui::kAccent,0.15f)); DrawRectangleLinesEx(mr, 1, ui::kAccent);
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            liveSel_.clear();
            for (auto& u : liveUnits_) if (CheckCollisionPointRec(t2s(u.tx,u.ty), mr)) liveSel_.push_back(u.tag);
            liveMarquee_ = false;
        }
    }
    // right-click = move selected (pathfind)
    if (rclick && inMap && !liveSel_.empty()) {
        int tx, ty; s2t(tx, ty);
        for (auto& u : liveUnits_) if (selected(u.tag)) {
            u.path = findPath(*m, (int)std::lround(u.tx), (int)std::lround(u.ty), tx, ty);
            if (u.path.empty()) u.path.push_back({ (float)tx, (float)ty });   // 직선 폴백
            if (scnRecording_) recCmds_.push_back({ scnRecClock_, 0, u.tag, tx, ty, 0, 0, false });
        }
    }
    DrawTextU(TextFormat("유닛 %d · 선택 %d", (int)liveUnits_.size(), (int)liveSel_.size()),
              (int)canvas.x+8, (int)(canvas.y+canvas.height-22), 13, ui::kTextDim);
}

} // namespace tsukuru
