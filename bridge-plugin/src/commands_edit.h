#pragma once
// カテゴリB: オブジェクト編集系コマンド(docs/design/02_EDIT_OPERATIONS.md)
#include <string>

#include "json_types.h"

// TryHandleReadCommandと同じ規約(未知のコマンドならfalseを返す)。
bool TryHandleEditCommand(const std::string& cmd, const json& req, json& res);
