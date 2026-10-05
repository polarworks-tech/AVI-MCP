#pragma once
// プラグイン全体で共有するグローバル状態とログ出力ヘルパー。
// 実体はglobals.cppに定義する。
#include <string>

#include <windows.h>  // plugin2.h/logger2.hがLPCWSTR等のWindows型を前提にしているため先にinclude

#include "plugin2.h"
#include "logger2.h"

// AviUtl2のプロジェクトを編集・参照するためのハンドル。RegisterPlugin()で初期化される。
extern EDIT_HANDLE* g_edit;

// AviUtl2のログ機能へのハンドル。InitializeLogger()で初期化される。
extern LOG_HANDLE* g_logger;

void LogInfo(const std::wstring& message);
void LogError(const std::wstring& message);
