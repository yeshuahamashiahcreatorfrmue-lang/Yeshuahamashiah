#include "battle/Battle.h"
#include <algorithm>
#include <cstdlib>

namespace tsukuru {

Battle::Battle(Database& db, GameState& state, const std::vector<int>& enemyIds)
    : db_(db), state_(state) {
    for (int id : enemyIds) {
        const EnemyDef* d = db_.enemy(id);
        if (!d) continue;
        BattleEnemy e;
        e.enemyId = d->id; e.name = d->name;
        e.hp = e.maxHp = d->maxHp; e.mp = d->maxMp;
        e.atk = d->atk; e.def = d->def; e.spd = d->spd;
        e.expReward = d->expReward; e.goldReward = d->goldReward;
        enemies_.push_back(e);
    }
    if (enemies_.empty()) { result_ = BattleResult::Victory; return; }
    advanceToNextActor();
}

int Battle::firstAliveEnemy() const {
    for (int i = 0; i < (int)enemies_.size(); ++i) if (enemies_[i].alive()) return i;
    return -1;
}

int Battle::computeDamage(int atk, int def, int power) const {
    int base = atk + power - def / 2;
    if (base < 1) base = 1;
    // +/- 15% variance
    int variance = base * 15 / 100;
    int delta = variance > 0 ? (std::rand() % (2 * variance + 1)) - variance : 0;
    int dmg = base + delta;
    return dmg < 1 ? 1 : dmg;
}

void Battle::advanceToNextActor() {
    for (int i = 0; i < (int)state_.party.size(); ++i) {
        if (state_.party[i].alive()) { actorTurn_ = i; return; }
    }
    actorTurn_ = -1;
}

void Battle::submit(const BattleAction& action) {
    if (result_ != BattleResult::Ongoing || actorTurn_ < 0) return;
    PartyMember& me = state_.party[actorTurn_];

    switch (action.kind) {
        case ActionKind::Attack: {
            int ti = (action.targetIndex >= 0 && action.targetIndex < (int)enemies_.size()
                      && enemies_[action.targetIndex].alive())
                     ? action.targetIndex : firstAliveEnemy();
            if (ti >= 0) {
                int dmg = computeDamage(me.totalAtk(db_), enemies_[ti].def, 0);
                enemies_[ti].hp -= dmg;
                log_.push_back(me.actorId >= 0 ? ("Hero attacks " + enemies_[ti].name +
                               " for " + std::to_string(dmg) + "!")
                               : "Attack!");
            }
            break;
        }
        case ActionKind::Skill: {
            const Skill* sk = db_.skill(action.id);
            if (sk && me.mp >= sk->mpCost) {
                me.mp -= sk->mpCost;
                if (sk->healing) {
                    me.hp = std::min(me.maxHp, me.hp + sk->power);
                    log_.push_back("Hero uses " + sk->name + " (heal " + std::to_string(sk->power) + ").");
                } else {
                    int ti = firstAliveEnemy();
                    if (ti >= 0) {
                        int dmg = computeDamage(me.totalAtk(db_), enemies_[ti].def, sk->power);
                        enemies_[ti].hp -= dmg;
                        log_.push_back("Hero uses " + sk->name + " on " + enemies_[ti].name +
                                       " for " + std::to_string(dmg) + "!");
                    }
                }
            } else {
                log_.push_back("Not enough MP!");
            }
            break;
        }
        case ActionKind::Item: {
            const Item* it = db_.item(action.id);
            if (it && state_.inventory.has(it->id)) {
                if (it->effect == ItemEffect::HealHP) me.hp = std::min(me.maxHp, me.hp + it->power);
                else if (it->effect == ItemEffect::HealMP) me.mp = std::min(me.maxMp, me.mp + it->power);
                else if (it->effect == ItemEffect::Damage) {
                    int ti = firstAliveEnemy();
                    if (ti >= 0) enemies_[ti].hp -= it->power;
                }
                if (it->consumable) state_.inventory.removeItem(it->id);
                log_.push_back("Hero uses " + it->name + ".");
            }
            break;
        }
        case ActionKind::Flee: {
            // 50% chance to flee
            if (std::rand() % 2 == 0) { result_ = BattleResult::Fled; log_.push_back("Got away safely!"); return; }
            log_.push_back("Couldn't escape!");
            break;
        }
    }

    checkEnd();
    if (result_ != BattleResult::Ongoing) return;

    // Next living party member, or enemy turn if party exhausted this round.
    int next = -1;
    for (int i = actorTurn_ + 1; i < (int)state_.party.size(); ++i)
        if (state_.party[i].alive()) { next = i; break; }

    if (next >= 0) {
        actorTurn_ = next;
    } else {
        enemyTurns();
        checkEnd();
        if (result_ == BattleResult::Ongoing) advanceToNextActor();
    }
}

void Battle::enemyTurns() {
    for (auto& e : enemies_) {
        if (!e.alive()) continue;
        // Target a random living party member.
        std::vector<int> alive;
        for (int i = 0; i < (int)state_.party.size(); ++i)
            if (state_.party[i].alive()) alive.push_back(i);
        if (alive.empty()) return;
        int ti = alive[std::rand() % alive.size()];
        PartyMember& target = state_.party[ti];
        int dmg = computeDamage(e.atk, target.totalDef(db_), 0);
        target.hp -= dmg;
        if (target.hp < 0) target.hp = 0;
        log_.push_back(e.name + " hits Hero for " + std::to_string(dmg) + "!");
    }
}

void Battle::checkEnd() {
    bool enemiesDead = true;
    for (const auto& e : enemies_) if (e.alive()) { enemiesDead = false; break; }
    if (enemiesDead) { result_ = BattleResult::Victory; grantRewards(); return; }
    if (state_.partyWiped()) result_ = BattleResult::Defeat;
}

void Battle::grantRewards() {
    int exp = 0, gold = 0;
    for (const auto& e : enemies_) { exp += e.expReward; gold += e.goldReward; }
    state_.inventory.gold += gold;
    for (auto& m : state_.party) if (m.alive()) m.gainExp(exp);
    log_.push_back("Victory! Gained " + std::to_string(exp) + " EXP and " +
                   std::to_string(gold) + " gold.");
}

} // namespace tsukuru
