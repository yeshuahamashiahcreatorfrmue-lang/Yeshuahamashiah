#pragma once
// Battle: a self-contained turn-based combat resolver. It operates on the live
// party (GameState) and a troop of enemies built from EnemyDefs. The graphical
// layer drives it one action at a time; the logic here is fully headless-testable.
#include <string>
#include <vector>
#include "database/Database.h"
#include "game/GameState.h"

namespace tsukuru {

struct BattleEnemy {
    int enemyId = -1;
    std::string name;
    int hp = 0, maxHp = 0, mp = 0;
    int atk = 0, def = 0, spd = 0;
    int expReward = 0, goldReward = 0;
    bool alive() const { return hp > 0; }
};

enum class BattleResult { Ongoing, Victory, Defeat, Fled };

enum class ActionKind { Attack, Skill, Item, Flee };

struct BattleAction {
    ActionKind kind = ActionKind::Attack;
    int id = -1;        // skill id or item id
    int targetIndex = 0; // enemy index for offensive, party index for support
};

class Battle {
public:
    Battle(Database& db, GameState& state, const std::vector<int>& enemyIds);

    const std::vector<BattleEnemy>& enemies() const { return enemies_; }
    BattleResult result() const { return result_; }
    const std::vector<std::string>& log() const { return log_; }
    void clearLog() { log_.clear(); }

    // Whose turn (party member index). -1 when not the party's turn.
    int currentActor() const { return actorTurn_; }
    bool actorReady() const { return result_ == BattleResult::Ongoing && actorTurn_ >= 0; }

    // Apply the chosen action for the current party member, then run enemy turns.
    void submit(const BattleAction& action);

    int firstAliveEnemy() const;

private:
    void advanceToNextActor();
    void enemyTurns();
    int  computeDamage(int atk, int def, int power) const;
    void checkEnd();
    void grantRewards();

    Database&   db_;
    GameState&  state_;
    std::vector<BattleEnemy> enemies_;
    std::vector<std::string> log_;
    BattleResult result_ = BattleResult::Ongoing;
    int actorTurn_ = -1;
};

} // namespace tsukuru
