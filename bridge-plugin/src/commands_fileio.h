#pragma once
// カテゴリF: ファイルI/O・プロジェクト保存系コマンド(docs/design/06_FILE_IO.md)
#include <string>

#include "json_types.h"

// TryHandleReadCommandと同じ規約(未知のコマンドならfalseを返す)。
bool TryHandleFileIoCommand(const std::string& cmd, const json& req, json& res);
