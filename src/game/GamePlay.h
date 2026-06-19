#pragma once
// GamePlay: the runtime that plays a project as a game. Handles grid movement
// with smooth interpolation, collision, event interaction, teleports, random
// encounters (-> Battle), the message box, and the in-game menu.
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "world/Map.h"
#include "core/Types.h"

namespace tsukuru {

class Engine;
class Battle;
class Menu;

class GamePlay {
public:
    explicit GamePlay(Engine& engine);
    ~GamePlay();

    void onEnter();      // called when switching into play mode
    void update(float dt);
    void draw();

private:
    enum class Phase { Field, Message, Battle, Menu, GameOver };

    void loadMap(int id);
    void updateField(float dt);
    void tryMove(Direction d);
    void interact();
    void runEvent(Event& e);
    void startBattle(const std::vector<int>& enemyIds);
    void checkEncounter();
    void updateBattle(float dt);
    void drawField();
    void drawMessage();
    void drawBattle();
    void drawCharacter(int assetId, int dir, int frame, float px, float py);

    Engine& engine_;
    std::shared_ptr<Map> map_;
    Camera2D cam_{};

    Phase phase_ = Phase::Field;

    // Player smooth movement (grid -> pixel interpolation)
    float pxX_ = 0, pxY_ = 0;     // current pixel position (top-left of tile)
    int   destX_ = 0, destY_ = 0; // grid destination
    bool  moving_ = false;
    int   dir_ = 0;               // Direction
    float animTime_ = 0;
    int   frame_ = 0;
    int   stepsSinceEncounter_ = 0;

    // message box
    std::string message_;
    Event* pendingEvent_ = nullptr;

    std::unique_ptr<Battle> battle_;
    std::unique_ptr<Menu>   menu_;
    int battleSelection_ = 0;   // 0 attack,1 skill,2 item,3 flee
    int battleSubSelection_ = 0;
    bool battleSubMenu_ = false;
};

} // namespace tsukuru
