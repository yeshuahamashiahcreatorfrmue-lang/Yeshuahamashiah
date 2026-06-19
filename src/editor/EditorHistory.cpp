// EditorHistory: undo/redo snapshots of the active map's tilemap.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "render/AssetGen.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {


// ---- undo / redo (snapshots of the active map's tilemap) ----
void Editor::pushUndo() {
    auto m = activeMap();
    if (!m) return;
    if (undoMap_ != m->id) { undo_.clear(); redo_.clear(); undoMap_ = m->id; }
    undo_.push_back(m->tilemap.toJson().dump());
    if (undo_.size() > kUndoLimit) undo_.erase(undo_.begin());
    redo_.clear();
}
void Editor::doUndo() {
    auto m = activeMap();
    if (!m || undoMap_ != m->id || undo_.empty()) return;
    redo_.push_back(m->tilemap.toJson().dump());
    m->tilemap.fromJson(nlohmann::json::parse(undo_.back()));
    undo_.pop_back();
    setStatus("실행 취소");
}
void Editor::doRedo() {
    auto m = activeMap();
    if (!m || undoMap_ != m->id || redo_.empty()) return;
    undo_.push_back(m->tilemap.toJson().dump());
    m->tilemap.fromJson(nlohmann::json::parse(redo_.back()));
    redo_.pop_back();
    setStatus("다시 실행");
}

} // namespace tsukuru
