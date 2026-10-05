#pragma once
// カテゴリA: 読み取り系コマンド(docs/design/01_READ_OPERATIONS.md)
#include <string>

#include "json_types.h"

// cmdがこのカテゴリの既知のコマンドであれば処理してresに結果を書き込み、trueを返す。
// 未知のコマンドならresに触れずfalseを返す(呼び出し元が他のカテゴリに回す)。
// 呼び出し前提: g_edit が初期化済みであること(呼び出し元のHandleCommandで保証)。
bool TryHandleReadCommand(const std::string& cmd, const json& req, json& res);
