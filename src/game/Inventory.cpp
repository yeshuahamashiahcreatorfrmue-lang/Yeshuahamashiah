#include "game/Inventory.h"

using nlohmann::json;

namespace tsukuru {

void Inventory::addItem(int itemId, int count) {
    if (count <= 0) return;
    items_[itemId] += count;
}

bool Inventory::removeItem(int itemId, int count) {
    auto it = items_.find(itemId);
    if (it == items_.end() || it->second < count) return false;
    it->second -= count;
    if (it->second <= 0) items_.erase(it);
    return true;
}

int Inventory::count(int itemId) const {
    auto it = items_.find(itemId);
    return it == items_.end() ? 0 : it->second;
}

std::vector<std::pair<int,int>> Inventory::list() const {
    std::vector<std::pair<int,int>> out;
    for (const auto& kv : items_) out.emplace_back(kv.first, kv.second);
    return out;
}

json Inventory::toJson() const {
    json arr = json::array();
    for (const auto& kv : items_) arr.push_back({{"id", kv.first}, {"count", kv.second}});
    return {{"gold", gold}, {"items", arr}};
}

void Inventory::fromJson(const json& j) {
    items_.clear();
    gold = j.value("gold", 0);
    for (const auto& it : j.value("items", json::array()))
        items_[it.value("id", -1)] = it.value("count", 0);
}

} // namespace tsukuru
