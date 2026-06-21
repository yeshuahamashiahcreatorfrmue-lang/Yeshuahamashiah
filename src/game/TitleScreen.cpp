#include "game/TitleScreen.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "game/SaveMeta.h"
#include "core/Platform.h"
#include <vector>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace tsukuru {

TitleScreen::TitleScreen(Engine& engine) : engine_(engine) {}

// Newest existing save among slot1..3 + quick + auto (empty if none) —
// "이어하기" loads whichever was written most recently.
static fs::path latestSave(const std::string& projectDir) {
    fs::path dir = fs::path(projectDir) / "save";
    std::vector<fs::path> candidates = {
        dir / "slot1.json", dir / "slot2.json", dir / "slot3.json",
        dir / "quick.json", dir / "auto.json"
    };
    fs::path best; fs::file_time_type bestT{};
    for (const auto& f : candidates) {
        std::error_code ec;
        if (!fs::exists(f, ec)) continue;
        auto t = fs::last_write_time(f, ec);
        if (best.empty() || t > bestT) { best = f; bestT = t; }
    }
    return best;
}

// Re-read the most-recent save's headline fields into the cache (cheap to call
// on a timer; not every frame).
void TitleScreen::refreshContinueMeta() {
    SaveMeta m = readSaveMeta(latestSave(engine_.project().dir));
    continueExists_ = m.exists;
    continueLevel_ = m.level;
    continueSeconds_ = m.playSeconds;
}

void TitleScreen::startSingle() {
    Project& p = engine_.project();
    engine_.state().newGame(p.database, p.startActor, p.playerCharId, p.startMap, p.startX, p.startY,
                            p.startGold, p.startItems);
    engine_.setMode(Mode::Play);
}

void TitleScreen::update(float dt) {
    // refresh the continue-save cache immediately, then every 0.5s
    metaTimer_ -= dt;
    if (metaTimer_ <= 0) { refreshContinueMeta(); metaTimer_ = 0.5f; }

    // IME on only while typing the MMO connect IP; off otherwise so menu keys work.
    plat::setImeEnabled(ipFocus_);

    if (IsKeyPressed(KEY_ESCAPE)) { engine_.setMode(Mode::Editor); return; }

    const int count = 5;
    if (IsKeyPressed(KEY_DOWN)) selection_ = (selection_ + 1) % count;
    if (IsKeyPressed(KEY_UP))   selection_ = (selection_ + count - 1) % count;

    bool confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    int sw = screenW(), sh = screenH();
    int oy = sh / 2 + 24;
    Vector2 mp = GetMousePosition();
    for (int i = 0; i < count; ++i) {
        Rectangle r = { (float)sw / 2 - 200, (float)(oy + i * 40 - 4), 400, 34 };
        if (CheckCollisionPointRec(mp, r)) {
            selection_ = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) confirm = true;
        }
    }

    // IP text field (for MMO 접속)
    Rectangle ipR = { (float)sw / 2 - 200, (float)(oy + count * 40 + 10), 280, 30 };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) ipFocus_ = CheckCollisionPointRec(mp, ipR);
    ui::textField(ipR, ipText_, ipFocus_, 21);

    if (!confirm) return;

    if (selection_ == 0) startSingle();                       // 새 게임 (싱글)
    else if (selection_ == 1 && hasSave()) {                  // 이어하기 (가장 최근 슬롯)
        fs::path sv = latestSave(engine_.project().dir);
        std::ifstream f(sv.string());
        if (f) { json j; f >> j; engine_.state().fromJson(j); engine_.setMode(Mode::Play); }
    } else if (selection_ == 2) {                             // MMO 호스트 (최대 42명)
        engine_.net().startHost(7777, 42);
        startSingle();
    } else if (selection_ == 3) {                             // MMO 접속
        if (engine_.net().startClient(ipText_, 7777)) startSingle();
    } else if (selection_ == 4) {                             // 에디터로 나가기
        engine_.setMode(Mode::Editor);
    }
}

void TitleScreen::draw() {
    int sw = screenW(), sh = screenH();
    // Backdrop gradient
    DrawRectangleGradientV(0, 0, sw, sh, Color{ 20, 28, 48, 255 }, Color{ 8, 10, 18, 255 });

    const char* title = engine_.project().name.c_str();
    int ts = 64;
    int tw = MeasureTextU(title, ts);
    DrawTextU(title, (sw - tw) / 2, sh / 4, ts, ui::kText);
    const char* sub = "쯔꾸르 엔진으로 제작됨";
    int subw = MeasureTextU(sub, 18);
    DrawTextU(sub, (sw - subw) / 2, sh / 4 + ts + 8, 18, ui::kTextDim);

    const char* opts[5] = { "새 게임", "이어하기", "MMO 호스트 시작 (최대 42명)", "MMO 접속", "에디터로 나가기" };
    bool enabled[5] = { true, hasSave(), true, true, true };
    int oy = sh / 2 + 24;
    for (int i = 0; i < 5; ++i) {
        Color c = !enabled[i] ? ui::kTextDim : (i == selection_ ? ui::kAccentHi : ui::kText);
        std::string text = (i == selection_ ? "> " : "  ") + std::string(opts[i]);
        // annotate "이어하기" with the most recent save's headline (Lv / 플레이타임)
        if (i == 1 && continueExists_)
            text += "  (Lv " + std::to_string(continueLevel_) + " · " + formatPlayTime(continueSeconds_) + ")";
        int w = MeasureTextU(text.c_str(), 26);
        DrawTextU(text.c_str(), (sw - w) / 2, oy + i * 40, 26, c);
    }
    // IP field label + box for MMO 접속
    Rectangle ipR = { (float)sw / 2 - 200, (float)(oy + 5 * 40 + 10), 280, 30 };
    DrawTextU("접속 IP:", (int)ipR.x, (int)ipR.y - 20, 15, ui::kTextDim);
    ui::textField(ipR, ipText_, ipFocus_, 21);
    DrawTextU("(포트 7777) 호스트는 LAN/포트포워딩으로 외부 공개", (int)ipR.x + 290, (int)ipR.y + 6, 13, ui::kTextDim);
    if (engine_.net().active())
        DrawTextU(engine_.net().status().c_str(), (int)ipR.x, (int)ipR.y + 38, 14, ui::kGood);
    DrawTextU("위/아래: 선택   Enter: 확인   ESC: 에디터",
             20, sh - 30, 16, ui::kTextDim);
}

} // namespace tsukuru
