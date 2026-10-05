#include "bridge_server.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <string>
#include <thread>

#include "commands_edit.h"
#include "commands_fileio.h"
#include "commands_read.h"
#include "commands_rendering.h"
#include "globals.h"
#include "json_types.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

constexpr int kPort = 23910;
constexpr wchar_t kWindowClassName[] = L"AviUtl2McpBridgeWindow";
constexpr UINT WM_MCP_COMMAND = WM_APP + 1;

std::atomic<bool> g_running{false};
SOCKET g_listen_sock = INVALID_SOCKET;
std::thread g_server_thread;
HWND g_hwnd = nullptr;

// SendMessageでメインスレッドに渡すコマンド実行コンテキスト
struct CommandContext {
    const json* req;
    json* res;
};

// コマンドを実行してレスポンスのJSONを返す。cmd名でカテゴリ別のハンドラに振り分ける。
json HandleCommand(const json& req) {
    json res;
    const std::string cmd = req.value("cmd", "");

    if (cmd == "ping") {
        res["ok"] = true;
        res["result"] = "pong";
        return res;
    }

    if (!g_edit) {
        res["ok"] = false;
        res["error"] = "edit handle not ready";
        return res;
    }

    if (TryHandleReadCommand(cmd, req, res)) {
        return res;
    }
    if (TryHandleEditCommand(cmd, req, res)) {
        return res;
    }
    if (TryHandleRenderingCommand(cmd, req, res)) {
        return res;
    }
    if (TryHandleFileIoCommand(cmd, req, res)) {
        return res;
    }

    res["ok"] = false;
    res["error"] = "unknown command: " + cmd;
    return res;
}

// 任意のスレッドから呼び出し、メインスレッド(ウィンドウプロシージャ)上でHandleCommandを実行させる
// SDKのEDIT_HANDLE/EDIT_SECTION系APIはメインスレッドからの呼び出しを前提としているため、
// TCPの受信スレッドから直接呼ぶとクラッシュする。SendMessageで同期ディスパッチする。
json DispatchCommand(const json& req) {
    json res;
    if (!g_hwnd) {
        res["ok"] = false;
        res["error"] = "bridge window not ready";
        return res;
    }
    CommandContext ctx{&req, &res};
    SendMessageW(g_hwnd, WM_MCP_COMMAND, 0, reinterpret_cast<LPARAM>(&ctx));
    return res;
}

LRESULT CALLBACK BridgeWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_MCP_COMMAND) {
        auto* ctx = reinterpret_cast<CommandContext*>(lparam);
        *ctx->res = HandleCommand(*ctx->req);
        return 0;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

void HandleClient(SOCKET client) {
    std::string buffer;
    char chunk[4096];
    while (g_running) {
        const int n = recv(client, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            break;
        }
        buffer.append(chunk, n);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (line.empty()) {
                continue;
            }

            json res;
            try {
                const json req = json::parse(line);
                res = DispatchCommand(req);
            } catch (const std::exception& e) {
                res = {{"ok", false}, {"error", std::string("parse error: ") + e.what()}};
            }

            const std::string out = res.dump() + "\n";
            send(client, out.c_str(), static_cast<int>(out.size()), 0);
        }
    }
    closesocket(client);
}

void ServerLoop() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return;
    }

    g_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_sock == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // ローカルのみ
    addr.sin_port = htons(kPort);

    if (bind(g_listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        LogError(L"AviUtl2McpBridge: bind failed (port " + std::to_wstring(kPort) + L" in use?)");
        closesocket(g_listen_sock);
        g_listen_sock = INVALID_SOCKET;
        WSACleanup();
        return;
    }
    listen(g_listen_sock, 4);

    LogInfo(L"AviUtl2McpBridge: listening on 127.0.0.1:" + std::to_wstring(kPort));

    while (g_running) {
        SOCKET client = accept(g_listen_sock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            break;
        }
        std::thread(HandleClient, client).detach();
    }

    closesocket(g_listen_sock);
    g_listen_sock = INVALID_SOCKET;
    WSACleanup();
}

}  // namespace

void StartBridgeServer() {
    // メインスレッドでAPI呼び出しをディスパッチするための非表示ウィンドウ
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(wcex);
    wcex.lpszClassName = kWindowClassName;
    wcex.lpfnWndProc = BridgeWndProc;
    wcex.hInstance = GetModuleHandle(nullptr);
    if (RegisterClassExW(&wcex)) {
        g_hwnd = CreateWindowExW(
            0, kWindowClassName, kWindowClassName, WS_POPUP,
            0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    }

    g_running = true;
    g_server_thread = std::thread(ServerLoop);
}

void StopBridgeServer() {
    g_running = false;
    if (g_listen_sock != INVALID_SOCKET) {
        closesocket(g_listen_sock);  // accept()を解除する
    }
    if (g_server_thread.joinable()) {
        g_server_thread.join();
    }
}
