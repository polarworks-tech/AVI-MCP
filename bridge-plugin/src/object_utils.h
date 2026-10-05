#pragma once
// object_id(位置に依存しない不変の識別子)を扱うための共通処理。
// 詳細はdocs/design/00_COMMON.mdを参照。
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>  // plugin2.hがLPCWSTR等のWindows型を前提にしているため先にinclude

#include "json_types.h"
#include "plugin2.h"

// object_idからOBJECT_HANDLEを解決する。
// SDKにID直接取得のAPIが無いため、全レイヤーを走査して一致するオブジェクトを探す。
// 呼び出しは1回のコールバック内で完結させ、ハンドルを外に持ち出さないこと。
// 注意: EDIT_SECTION::infoは「call_read_section利用不可」なので、layer_maxは
// 呼び出し元がcall_edit_section_param/call_read_section_paramの外で
// 事前にget_edit_infoから取得し、引数で渡すこと。
OBJECT_HANDLE ResolveObjectById(EDIT_SECTION* edit, int64_t target_id, int layer_max);

// object(解決済みのOBJECT_HANDLE)が持つエフェクトの中から、effect_idが一致するものを探す。
// object_idと同様、SDKにID直接取得のAPIが無いため、get_effect_listで全件取得して
// get_effect_idを比較する。呼び出しは1回のコールバック内で完結させること。
EFFECT_HANDLE ResolveEffectById(EDIT_SECTION* edit, OBJECT_HANDLE object, int64_t target_effect_id);

// オブジェクト一覧系コマンドの戻り値要素
struct ObjectEntry {
    int64_t object_id;
    int layer, start, end;
};

json ObjectEntryToJson(const ObjectEntry& e);
json ObjectEntriesToJsonArray(const std::vector<ObjectEntry>& entries);

// エフェクト一覧系コマンドの戻り値要素
struct EffectEntry {
    int64_t effect_id;
    std::wstring name;
};

json EffectEntryToJson(const EffectEntry& e);
