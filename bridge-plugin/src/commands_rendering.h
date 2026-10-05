#pragma once
// カテゴリD: レンダリング系コマンド(docs/design/03_RENDERING.md)
#include <string>

#include "json_types.h"

// TryHandleReadCommandと同じ規約(未知のコマンドならfalseを返す)。
bool TryHandleRenderingCommand(const std::string& cmd, const json& req, json& res);
