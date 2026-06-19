#pragma once
// Inventory: the player's gold and owned items. Equipment management lives on
// the party actors (see GameState), but buy/sell/use flows go through here.
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace tsukuru {

class Inventory {
public:
    int gold = 0;

    void addItem(int itemId, int count = 1);
    bool removeItem(int itemId, int count = 1);
    int  count(int itemId) const;
    bool has(int itemId, int count = 1) const { return this->count(itemId) >= count; }

    // ordered list of (itemId, count) for menus
    std::vector<std::pair<int,int>> list() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::map<int,int> items_; // itemId -> count
};

} // namespace tsukuru
