/**
 * cmd_ui.cpp
 * ----------
 * CLI force-open commands for the VSEPR desktop.
 * Uses a raw TCP socket to send a one-line JSON command to the IPC server.
 */
#include "cli/cmd_ui.hpp"
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   using sock_t = SOCKET;
   static constexpr sock_t kBadSock = INVALID_SOCKET;
   static void sock_init()  { WSADATA w; WSAStartup(MAKEWORD(2,2),&w); }
   static void sock_close(sock_t s) { closesocket(s); }
   static int  sock_err()   { return WSAGetLastError(); }
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
   using sock_t = int;
   static constexpr sock_t kBadSock = -1;
   static void sock_init()  {}
   static void sock_close(sock_t s) { close(s); }
   static int  sock_err()   { return errno; }
#endif

namespace vsepr {
namespace cli {

int UiCommand::ipcPort()
{
    const char* env = std::getenv("VSEPR_IPC_PORT");
    if (env) { int p = std::atoi(env); if (p > 0 && p < 65536) return p; }
    return 47215;
}

bool UiCommand::sendIpc(const std::string& json) const
{
    sock_init();
    int port = ipcPort();

    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kBadSock) {
        std::cerr << "[ui] socket error: " << sock_err() << "\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        sock_close(s);
        return false;
    }

    std::string payload = json + "\n";
    send(s, payload.c_str(), static_cast<int>(payload.size()), 0);

    char buf[256]{};
    int n = recv(s, buf, sizeof(buf)-1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::string reply(buf);
        while (!reply.empty() && (reply.back()=='\n' || reply.back()=='\r'))
            reply.pop_back();
        std::cout << "[ui] desktop: " << reply << "\n";
    }
    sock_close(s);
    return true;
}

std::string UiCommand::Help() const
{
    return
        "Usage: vsepr ui <sub-command> [arg]\n\n"
        "Sub-commands:\n"
        "  showroom              Enter showroom / admin debug mode\n"
        "  open-all              Raise every dock panel\n"
        "  open-panel  <name>    Raise the named dock panel (partial match)\n"
        "  open-vsim   <path>    Open a .vsim script in the editor\n"
        "  open-dynx   <path>    Open a .dynx replay archive\n"
        "  open-xyz    <path>    Import a .xyz / .xyzFull file\n"
        "  ping                  Check whether a desktop instance is listening\n\n"
        "IPC port: 47215 (override: VSEPR_IPC_PORT env var)\n";
}

int UiCommand::Execute(const std::vector<std::string>& args)
{
    if (args.empty()) { std::cout << Help(); return 0; }

    const std::string& sub = args[0];
    auto nextArg = [&](size_t idx) -> std::string {
        return (idx < args.size()) ? args[idx] : std::string{};
    };

    std::string json;

    if (sub == "showroom") {
        json = R"({ "cmd": "showroom" })";
    } else if (sub == "open-all") {
        json = R"({ "cmd": "open-all" })";
    } else if (sub == "ping") {
        json = R"({ "cmd": "ping" })";
    } else if (sub == "open-panel") {
        std::string panel = nextArg(1);
        if (panel.empty()) { std::cerr << "[ui] open-panel requires a name\n"; return 1; }
        for (auto& c : panel) if (c == '"') c = '\'';
        json = std::string(R"({ "cmd": "open-panel", "panel": ")") + panel + "\" }";
    } else if (sub == "open-vsim") {
        std::string path = nextArg(1);
        if (path.empty()) { std::cerr << "[ui] open-vsim requires a path\n"; return 1; }
        for (auto& c : path) if (c == '\\') c = '/';
        json = std::string(R"({ "cmd": "open-vsim", "path": ")") + path + "\" }";
    } else if (sub == "open-dynx") {
        std::string path = nextArg(1);
        if (path.empty()) { std::cerr << "[ui] open-dynx requires a path\n"; return 1; }
        for (auto& c : path) if (c == '\\') c = '/';
        json = std::string(R"({ "cmd": "open-dynx", "path": ")") + path + "\" }";
    } else if (sub == "open-xyz") {
        std::string path = nextArg(1);
        if (path.empty()) { std::cerr << "[ui] open-xyz requires a path\n"; return 1; }
        for (auto& c : path) if (c == '\\') c = '/';
        json = std::string(R"({ "cmd": "open-xyz", "path": ")") + path + "\" }";
    } else {
        std::cerr << "[ui] unknown sub-command: " << sub << "\n";
        std::cout << Help();
        return 1;
    }

    std::cout << "[ui] sending to the archived desktop IPC route (port " << ipcPort() << "): " << json << "\n";

    if (!sendIpc(json)) {
        std::cerr << "[ui] no active live-viewer IPC endpoint is available on port " << ipcPort() << "\n";
        if (sub == "showroom")
            std::cout << "[ui] hint: launch the live viewer with:  vsepr-view\n";
        else if (sub == "open-vsim" && args.size() > 1)
            std::cout << "[ui] hint: vsepr-view --artifact \"" << args[1] << "\"\n";
        else if (sub == "open-dynx" && args.size() > 1)
            std::cout << "[ui] hint: vsepr-view --artifact \"" << args[1] << "\"\n";
        else if (sub == "open-xyz" && args.size() > 1)
            std::cout << "[ui] hint: vsepr-view --artifact \"" << args[1] << "\"\n";
        else if (sub == "open-panel" && args.size() > 1)
            std::cout << "[ui] hint: vsepr-view\n";
        return 1;
    }
    return 0;
}

}} // namespace vsepr::cli
