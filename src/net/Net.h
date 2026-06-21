#pragma once
// Net: minimal cross-platform TCP networking for MMO-style co-op. One process
// HOSTs (authoritative relay, up to maxPlayers slots) and others JOIN. Players
// are synced per-zone (map): a client only receives the players standing in the
// same map, so the world scales by zone-loading. Single-threaded + non-blocking
// (polled once per frame), so there are no locks and it never stalls the game.
#include <string>
#include <vector>
#include <unordered_map>

namespace tsukuru {

// A networked player's last-known state (sent ~15x/sec).
struct NetPlayer {
    int id    = -1;
    int mapId = -1;
    int x = 0, y = 0, dir = 0;
    int charId = -1;   // playerCharId so remotes draw the right sprite
};

class Net {
public:
    enum class Mode { Off, Host, Client };
    ~Net();

    bool startHost(int port, int maxPlayers = 42);
    bool startClient(const std::string& host, int port);
    void stop();

    Mode mode() const { return mode_; }
    bool active() const { return mode_ != Mode::Off; }
    int  myId() const { return myId_; }
    int  maxPlayers() const { return maxPlayers_; }
    // Host: connected clients + 1 (self). Client: known remotes + 1 (self).
    int  playerCount() const;
    std::string status() const;

    // Poll the socket(s); push the local player's state out. Call once per frame.
    void update(float dt, const NetPlayer& local);

    // Remote players currently in `mapId` (never includes the local player).
    std::vector<NetPlayer> remotesInMap(int mapId) const;

    // --- chat ---
    void sendChat(const std::string& text);                 // broadcast a chat line
    std::vector<std::pair<int,std::string>> takeChats();    // drain received (senderId, text)

private:
    struct Conn {
        long long sock = -1;
        std::string rbuf;     // accumulated received bytes (line-parsed)
        NetPlayer state;      // last state received from this client
        bool got = false;     // has sent at least one state
        bool alive = true;
    };

    void hostPoll();
    void clientPoll();
    void sendLine(long long sock, const std::string& s);

    Mode mode_ = Mode::Off;
    long long listen_ = -1;          // host listening socket
    long long client_ = -1;          // client's socket to the host
    std::vector<Conn> conns_;        // host: connected clients
    std::unordered_map<int, NetPlayer> remotes_; // id -> state (both sides)
    std::string crbuf_;              // client receive buffer
    NetPlayer local_;                // last local state we sent
    int  myId_ = 0;                  // host is 0; clients get assigned ids
    int  nextId_ = 1;                // host: next client id to hand out
    int  maxPlayers_ = 42;
    float tick_ = 0;                 // send throttle
    std::vector<std::pair<int,std::string>> chatIn_;   // received chat (senderId, text)
    std::string status_ = "오프라인";
};

} // namespace tsukuru
