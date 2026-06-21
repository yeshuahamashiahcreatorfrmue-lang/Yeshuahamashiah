#include "net/Net.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  typedef SOCKET socket_t;
  static const socket_t kBad = INVALID_SOCKET;
  static int  sockErr() { return WSAGetLastError(); }
  static bool wouldBlock() { int e = WSAGetLastError(); return e == WSAEWOULDBLOCK; }
  static void closeSock(socket_t s) { closesocket(s); }
  static void setNonBlock(socket_t s) { u_long m = 1; ioctlsocket(s, FIONBIO, &m); }
  static bool netInit() { static bool done=false; if(!done){ WSADATA w; done = WSAStartup(MAKEWORD(2,2), &w)==0; } return done; }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int socket_t;
  static const socket_t kBad = -1;
  static int  sockErr() { return errno; }
  static bool wouldBlock() { return errno == EWOULDBLOCK || errno == EAGAIN; }
  static void closeSock(socket_t s) { ::close(s); }
  static void setNonBlock(socket_t s) { int f = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, f | O_NONBLOCK); }
  static bool netInit() { return true; }
#endif

namespace tsukuru {

static void noDelay(socket_t s) { int one = 1; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one)); }

Net::~Net() { stop(); }

bool Net::startHost(int port, int maxPlayers) {
    stop();
    if (!netInit()) { status_ = "네트워크 초기화 실패"; return false; }
    maxPlayers_ = maxPlayers < 1 ? 1 : maxPlayers;
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kBad) { status_ = "소켓 생성 실패"; return false; }
    int yes = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons((unsigned short)port);
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) { closeSock(s); status_ = "포트 바인드 실패"; return false; }
    if (listen(s, 64) != 0) { closeSock(s); status_ = "리슨 실패"; return false; }
    setNonBlock(s);
    listen_ = (long long)s;
    mode_ = Mode::Host; myId_ = 0; nextId_ = 1;
    status_ = std::string("호스트 중 (포트 ") + std::to_string(port) + ", 최대 " + std::to_string(maxPlayers_) + "명)";
    return true;
}

bool Net::startClient(const std::string& host, int port) {
    stop();
    if (!netInit()) { status_ = "네트워크 초기화 실패"; return false; }
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kBad) { status_ = "소켓 생성 실패"; return false; }
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        // resolve a hostname
        addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
            closeSock(s); status_ = "주소 확인 실패: " + host; return false;
        }
        addr = *(sockaddr_in*)res->ai_addr; freeaddrinfo(res);
    }
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) { closeSock(s); status_ = "접속 실패: " + host; return false; }
    noDelay(s); setNonBlock(s);
    client_ = (long long)s;
    mode_ = Mode::Client; myId_ = -1;   // assigned by the host
    status_ = "접속됨: " + host;
    return true;
}

void Net::stop() {
    if (listen_ != -1) { closeSock((socket_t)listen_); listen_ = -1; }
    if (client_ != -1) { closeSock((socket_t)client_); client_ = -1; }
    for (auto& c : conns_) if (c.sock != -1) closeSock((socket_t)c.sock);
    conns_.clear(); remotes_.clear(); crbuf_.clear();
    mode_ = Mode::Off; myId_ = 0; status_ = "오프라인";
}

void Net::sendLine(long long sock, const std::string& s) {
    if (sock == -1) return;
    ::send((socket_t)sock, s.data(), (int)s.size(), 0);   // best-effort (non-blocking)
}

int Net::playerCount() const {
    if (mode_ == Mode::Host) {
        int n = 1; for (auto& c : conns_) if (c.alive) ++n; return n;
    }
    if (mode_ == Mode::Client) return (int)remotes_.size() + 1;
    return 0;
}

std::string Net::status() const { return status_; }

void Net::update(float dt, const NetPlayer& local) {
    local_ = local;
    if (mode_ == Mode::Off) return;
    tick_ += dt;
    bool send = tick_ >= 0.066f;   // ~15 Hz
    if (send) tick_ = 0;
    if (mode_ == Mode::Host) {
        // accept new connections
        for (;;) {
            if ((int)conns_.size() >= maxPlayers_) break;      // up to maxPlayers_ external clients
            socket_t cs = accept((socket_t)listen_, nullptr, nullptr);
            if (cs == kBad) break;
            noDelay(cs); setNonBlock(cs);
            Conn c; c.sock = (long long)cs; c.state.id = nextId_++;
            sendLine(c.sock, "ID " + std::to_string(c.state.id) + "\n");
            conns_.push_back(std::move(c));
        }
        local_.id = 0;
        hostPoll();
        if (send) {
            // relay every player's state to every client that shares its map
            for (auto& dst : conns_) {
                if (!dst.alive) continue;
                std::string out;
                // the host's own avatar
                if (local_.mapId == dst.state.mapId)
                    out += "U 0 " + std::to_string(local_.mapId) + " " + std::to_string(local_.x) + " " +
                           std::to_string(local_.y) + " " + std::to_string(local_.dir) + " " + std::to_string(local_.charId) + "\n";
                for (auto& src : conns_) {
                    if (!src.alive || &src == &dst || !src.got) continue;
                    if (src.state.mapId != dst.state.mapId) continue;
                    out += "U " + std::to_string(src.state.id) + " " + std::to_string(src.state.mapId) + " " +
                           std::to_string(src.state.x) + " " + std::to_string(src.state.y) + " " +
                           std::to_string(src.state.dir) + " " + std::to_string(src.state.charId) + "\n";
                }
                if (!out.empty()) sendLine(dst.sock, out);
            }
        }
        // host's own view of remotes (for rendering): all connected clients
        remotes_.clear();
        for (auto& c : conns_) if (c.alive && c.got) remotes_[c.state.id] = c.state;
    } else { // Client
        if (send && client_ != -1) {
            std::string s = "S " + std::to_string(local_.mapId) + " " + std::to_string(local_.x) + " " +
                            std::to_string(local_.y) + " " + std::to_string(local_.dir) + " " + std::to_string(local_.charId) + "\n";
            sendLine(client_, s);
        }
        clientPoll();
    }
}

void Net::hostPoll() {
    char buf[2048];
    for (auto& c : conns_) {
        if (!c.alive) continue;
        for (;;) {
            int n = (int)recv((socket_t)c.sock, buf, sizeof(buf), 0);
            if (n > 0) { c.rbuf.append(buf, n); continue; }
            if (n == 0) { c.alive = false; }            // peer closed
            break;                                       // n<0: would-block or error
        }
        // parse complete lines
        size_t pos;
        while ((pos = c.rbuf.find('\n')) != std::string::npos) {
            std::string line = c.rbuf.substr(0, pos); c.rbuf.erase(0, pos + 1);
            if (line.size() > 1 && line[0] == 'S') {
                NetPlayer p = c.state; // keep id
                std::sscanf(line.c_str() + 1, "%d %d %d %d %d", &p.mapId, &p.x, &p.y, &p.dir, &p.charId);
                p.id = c.state.id; c.state = p; c.got = true;
            } else if (line.rfind("C ", 0) == 0) {              // chat from this client
                std::string text = line.substr(2);
                chatIn_.push_back({ c.state.id, text });        // host displays it
                std::string relay = "M " + std::to_string(c.state.id) + " " + text + "\n";
                for (auto& o : conns_) if (o.alive && &o != &c) sendLine(o.sock, relay); // to others
            }
        }
    }
    // notify of disconnects, then reap
    bool any = false; for (auto& c : conns_) if (!c.alive) any = true;
    if (any) {
        for (auto& d : conns_) {
            if (!d.alive) continue;
            for (auto& g : conns_) if (!g.alive) sendLine(d.sock, "D " + std::to_string(g.state.id) + "\n");
        }
        for (auto& c : conns_) if (!c.alive && c.sock != -1) { closeSock((socket_t)c.sock); c.sock = -1; }
        conns_.erase(std::remove_if(conns_.begin(), conns_.end(),
                     [](const Conn& c){ return !c.alive; }), conns_.end());
    }
}

void Net::clientPoll() {
    if (client_ == -1) return;
    char buf[4096];
    for (;;) {
        int n = (int)recv((socket_t)client_, buf, sizeof(buf), 0);
        if (n > 0) { crbuf_.append(buf, n); continue; }
        if (n == 0) { stop(); return; }                  // host closed
        break;
    }
    size_t pos;
    while ((pos = crbuf_.find('\n')) != std::string::npos) {
        std::string line = crbuf_.substr(0, pos); crbuf_.erase(0, pos + 1);
        if (line.rfind("ID ", 0) == 0) { myId_ = std::atoi(line.c_str() + 3); }
        else if (line.size() > 1 && line[0] == 'U') {
            NetPlayer p; std::sscanf(line.c_str() + 1, "%d %d %d %d %d %d", &p.id, &p.mapId, &p.x, &p.y, &p.dir, &p.charId);
            if (p.id != myId_) remotes_[p.id] = p;
        } else if (line.size() > 1 && line[0] == 'D') {
            int id = std::atoi(line.c_str() + 1); remotes_.erase(id);
        } else if (line.rfind("M ", 0) == 0) {                 // chat relayed from host
            int id = std::atoi(line.c_str() + 2);
            size_t sp = line.find(' ', 2);
            std::string text = (sp != std::string::npos) ? line.substr(sp + 1) : "";
            chatIn_.push_back({ id, text });
        }
    }
}

// --- chat ---------------------------------------------------------------
void Net::sendChat(const std::string& raw) {
    if (mode_ == Mode::Off) return;
    std::string text = raw;
    for (char& c : text) if (c == '\n' || c == '\r') c = ' ';   // single line only
    if (text.size() > 200) text.resize(200);
    if (mode_ == Mode::Host) {
        std::string line = "M 0 " + text + "\n";                // host id = 0
        for (auto& c : conns_) if (c.alive) sendLine(c.sock, line);
    } else if (mode_ == Mode::Client) {
        sendLine(client_, "C " + text + "\n");                  // host will relay as M
    }
}

std::vector<std::pair<int,std::string>> Net::takeChats() {
    std::vector<std::pair<int,std::string>> out;
    out.swap(chatIn_);
    return out;
}

std::vector<NetPlayer> Net::remotesInMap(int mapId) const {
    std::vector<NetPlayer> v;
    for (auto& kv : remotes_) if (kv.second.mapId == mapId && kv.second.id != myId_) v.push_back(kv.second);
    return v;
}

} // namespace tsukuru
