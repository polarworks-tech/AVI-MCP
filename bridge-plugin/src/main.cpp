//----------------------------------------------------------------------------------
//  AviUtl2 MCP Bridge - 汎用プラグイン(.aux2)
//  AviUtl2に対する実際の操作(コマンドの中身)はcommands_read.cpp / commands_edit.cppに、
//  TCP通信とスレッドの橋渡しはbridge_server.cppに分かれている。
//  このファイルはAviUtl2から呼ばれるプラグインのエントリポイントのみを持つ。
//----------------------------------------------------------------------------------
#include "bridge_server.h"
#include "globals.h"
#include "plugin2.h"

namespace {

COMMON_PLUGIN_TABLE g_common_plugin_table = {
    L"AviUtl2 MCP Bridge",
    L"AviUtl2 MCP Bridge version 0.1 - localhost:23910 JSON bridge",
};

}  // namespace

EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE* handle) {
    g_logger = handle;
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) {
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    StopBridgeServer();
}

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &g_common_plugin_table;
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    g_edit = host->create_edit_handle();
    StartBridgeServer();
}
