#include "commands_edit.h"

#include <iomanip>
#include <sstream>
#include <string>

#include "globals.h"
#include "object_utils.h"
#include "plugin2.h"
#include "string_utils.h"

namespace {

// エイリアステキスト先頭付近にある「frame=開始,終了」という絶対位置メタデータ行を取り除く。
// get_object_aliasで取得したエイリアスにはこの行が含まれるが、create_object_from_aliasへ
// 渡す際にlayer/frame/length引数と内容が矛盾すると作成に失敗することが実機検証で
// 判明したため(aviutl2_split_clip実装時に発見)、再利用時は必ず取り除く。
std::string StripFrameMetadata(const std::string& alias) {
    std::istringstream input(alias);
    std::ostringstream output;
    std::string line;
    while (std::getline(input, line)) {
        const bool has_cr = !line.empty() && line.back() == '\r';
        const std::string content = has_cr ? line.substr(0, line.size() - 1) : line;
        if (content.rfind("frame=", 0) == 0) {
            continue;
        }
        output << content << "\r\n";
    }
    return output.str();
}

// エイリアステキスト中の「再生位置=開始秒,終了秒,モード,フラグ」の開始秒をoffset_seconds分
// 加算する。動画ファイル/音声ファイルのエフェクトにのみ存在する項目で、このキーが
// 見つからない場合は何もせず元のテキストをそのまま返す(docs/design/02_EDIT_OPERATIONS.md
// のaviutl2_split_clip参照)。
std::string OffsetPlaybackPosition(const std::string& alias, double offset_seconds) {
    static const std::string kKey = "再生位置=";

    std::istringstream input(alias);
    std::ostringstream output;
    std::string line;
    while (std::getline(input, line)) {
        const bool has_cr = !line.empty() && line.back() == '\r';
        std::string content = has_cr ? line.substr(0, line.size() - 1) : line;

        if (content.rfind(kKey, 0) == 0) {
            const std::string value = content.substr(kKey.size());
            const size_t comma = value.find(',');
            if (comma != std::string::npos) {
                try {
                    double start_seconds = std::stod(value.substr(0, comma));
                    start_seconds += offset_seconds;
                    if (start_seconds < 0.0) {
                        start_seconds = 0.0;
                    }
                    std::ostringstream formatted;
                    formatted << std::fixed << std::setprecision(3) << start_seconds;
                    content = kKey + formatted.str() + value.substr(comma);
                } catch (const std::exception&) {
                    // 値がパース出来ない形式なら変更せず元のまま使う
                }
            }
        }

        output << content << "\r\n";
    }
    return output.str();
}

bool HandleCreateObjectFromMediaFile(const json& req, json& res) {
    struct Param {
        std::wstring file;
        int layer = 0;
        int frame = 0;
        int length = 0;
        bool success = false;
        OBJECT_LAYER_FRAME position{};
        int64_t object_id = 0;
    } param;
    param.file = Utf8ToWide(req.value("file", ""));
    param.layer = req.value("layer", 0);
    param.frame = req.value("frame", 0);
    param.length = req.value("length", 0);

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = edit->create_object_from_media_file(param->file.c_str(), param->layer, param->frame, param->length);
        if (obj) {
            param->success = true;
            param->position = edit->get_object_layer_frame(obj);
            param->object_id = edit->get_object_id(obj);
        }
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.success) {
        res["ok"] = false;
        res["error"] = "create_object_from_media_file failed (unsupported file or position occupied)";
    } else {
        res["ok"] = true;
        res["result"] = {
            {"object_id", param.object_id},
            {"layer", param.position.layer},
            {"start", param.position.start},
            {"end", param.position.end},
        };
    }
    return true;
}

bool HandleCreateObjectFromAlias(const json& req, json& res) {
    struct Param {
        std::string alias;
        int layer = 0;
        int frame = 0;
        int length = 0;
        bool success = false;
        OBJECT_LAYER_FRAME position{};
        int64_t object_id = 0;
    } param;
    param.alias = req.value("alias", "");
    param.layer = req.value("layer", 0);
    param.frame = req.value("frame", 0);
    param.length = req.value("length", 0);

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = edit->create_object_from_alias(param->alias.c_str(), param->layer, param->frame, param->length);
        if (obj) {
            param->success = true;
            param->position = edit->get_object_layer_frame(obj);
            param->object_id = edit->get_object_id(obj);
        }
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.success) {
        res["ok"] = false;
        res["error"] = "create_object_from_alias failed (invalid alias or position occupied)";
    } else {
        res["ok"] = true;
        res["result"] = {
            {"object_id", param.object_id},
            {"layer", param.position.layer},
            {"start", param.position.start},
            {"end", param.position.end},
        };
    }
    return true;
}

bool HandleMoveObject(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int layer_max = 0;
        int layer = 0;
        int frame = 0;
        bool found = false;
        bool success = false;
        OBJECT_LAYER_FRAME position{};
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));
    param.layer = req.value("layer", 0);
    param.frame = req.value("frame", 0);

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (!obj) {
            return;
        }
        param->found = true;
        param->success = edit->move_object(obj, param->layer, param->frame);
        if (param->success) {
            param->position = edit->get_object_layer_frame(obj);
        }
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.found) {
        res["ok"] = false;
        res["error"] = "object not found: " + std::to_string(param.object_id);
    } else if (!param.success) {
        res["ok"] = false;
        res["error"] = "move_object failed (target position occupied?)";
    } else {
        res["ok"] = true;
        res["result"] = {
            {"layer", param.position.layer},
            {"start", param.position.start},
            {"end", param.position.end},
        };
    }
    return true;
}

bool HandleDeleteObject(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int layer_max = 0;
        bool found = false;
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (!obj) {
            return;
        }
        param->found = true;
        edit->delete_object(obj);
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.found) {
        res["ok"] = false;
        res["error"] = "object not found: " + std::to_string(param.object_id);
    } else {
        res["ok"] = true;
        res["result"] = {{"deleted_object_id", param.object_id}};
    }
    return true;
}

bool HandleSetItemValue(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int layer_max = 0;
        std::wstring effect;
        std::wstring item;
        std::string value;
        bool found = false;
        bool success = false;
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));
    param.effect = Utf8ToWide(req.value("effect", ""));
    param.item = Utf8ToWide(req.value("item", ""));
    param.value = req.value("value", "");

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (!obj) {
            return;
        }
        param->found = true;
        param->success = edit->set_object_item_value(obj, param->effect.c_str(), param->item.c_str(), param->value.c_str());
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.found) {
        res["ok"] = false;
        res["error"] = "object not found: " + std::to_string(param.object_id);
    } else if (!param.success) {
        res["ok"] = false;
        res["error"] = "set_object_item_value failed (effect/item not found)";
    } else {
        res["ok"] = true;
    }
    return true;
}

bool HandleCreateEffect(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int layer_max = 0;
        std::wstring effect;
        bool found = false;
        bool success = false;
        int64_t effect_id = 0;
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));
    param.effect = Utf8ToWide(req.value("effect", ""));

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (!obj) {
            return;
        }
        param->found = true;
        EFFECT_HANDLE effect = edit->create_effect(obj, param->effect.c_str());
        if (effect) {
            param->success = true;
            param->effect_id = edit->get_effect_id(effect);
        }
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.found) {
        res["ok"] = false;
        res["error"] = "object not found: " + std::to_string(param.object_id);
    } else if (!param.success) {
        res["ok"] = false;
        res["error"] = "create_effect failed (unknown effect name, or object does not accept it)";
    } else {
        res["ok"] = true;
        res["result"] = {{"effect_id", param.effect_id}};
    }
    return true;
}

bool HandleDeleteEffect(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int64_t effect_id = 0;
        int layer_max = 0;
        bool object_found = false;
        bool effect_found = false;
        bool success = false;
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));
    param.effect_id = req.value("effect_id", static_cast<int64_t>(0));

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (!obj) {
            return;
        }
        param->object_found = true;
        EFFECT_HANDLE effect = ResolveEffectById(edit, obj, param->effect_id);
        if (!effect) {
            return;
        }
        param->effect_found = true;
        param->success = edit->delete_effect(obj, effect);
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.object_found) {
        res["ok"] = false;
        res["error"] = "object not found: " + std::to_string(param.object_id);
    } else if (!param.effect_found) {
        res["ok"] = false;
        res["error"] = "effect not found: " + std::to_string(param.effect_id);
    } else if (!param.success) {
        res["ok"] = false;
        res["error"] = "delete_effect failed (cannot remove an input/output effect?)";
    } else {
        res["ok"] = true;
        res["result"] = {{"deleted_effect_id", param.effect_id}};
    }
    return true;
}

// aviutl2_split_clip: 既存オブジェクトをsplit_frameで前半・後半の2つのオブジェクトに分割する。
// SDKに「分割」の直接APIが無いため、[削除 → 前半を元のエイリアスで再作成 →
// 後半を(メディアなら再生位置をオフセットした)エイリアスで再作成]という手順で実現する
// (docs/design/02_EDIT_OPERATIONS.mdの実機検証で確認済みの手法)。
// 途中で前半/後半の作成に失敗した場合は、可能な限り元のオブジェクトを復元してから
// エラーを返す(データ消失を避けるベストエフォートのロールバック)。
bool HandleSplitClip(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int split_frame = 0;
        int layer_max = 0;
        double fps = 30.0;

        bool success = false;
        std::string error;
        bool is_media = false;
        bool rolled_back = false;
        OBJECT_LAYER_FRAME front_pos{};
        OBJECT_LAYER_FRAME back_pos{};
        int64_t front_id = 0;
        int64_t back_id = 0;
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));
    param.split_frame = req.value("split_frame", 0);

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;
    param.fps = info.scale > 0 ? static_cast<double>(info.rate) / info.scale : 30.0;

    const bool called = g_edit->call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (!obj) {
            param->error = "object not found: " + std::to_string(param->object_id);
            return;
        }

        const OBJECT_LAYER_FRAME pos = edit->get_object_layer_frame(obj);
        if (param->split_frame <= pos.start || param->split_frame > pos.end) {
            param->error = "split_frame must satisfy start < split_frame <= end (start=" +
                            std::to_string(pos.start) + ", end=" + std::to_string(pos.end) + ")";
            return;
        }

        LPCSTR alias_cstr = edit->get_object_alias(obj);
        if (!alias_cstr) {
            param->error = "failed to read object alias";
            return;
        }
        const std::string alias = alias_cstr;  // コールバック終了後は無効になるためコピーする

        edit->delete_object(obj);

        param->is_media = alias.find("effect.name=動画ファイル") != std::string::npos ||
                          alias.find("effect.name=音声ファイル") != std::string::npos;
        // 元のalias中の「frame=開始,終了」メタデータは新しい位置・長さと矛盾し作成失敗の
        // 原因になるため、再利用する分(front/back)では必ず取り除く。ロールバック時は
        // 元の位置にそのまま戻すだけなので、取り除く前のaliasを使って問題ない。
        const std::string reusable_alias = StripFrameMetadata(alias);
        const double offset_seconds = (param->split_frame - pos.start) / param->fps;
        const std::string back_alias =
            param->is_media ? OffsetPlaybackPosition(reusable_alias, offset_seconds) : reusable_alias;

        OBJECT_HANDLE front = edit->create_object_from_alias(
            reusable_alias.c_str(), pos.layer, pos.start, param->split_frame - pos.start);
        OBJECT_HANDLE back = edit->create_object_from_alias(
            back_alias.c_str(), pos.layer, param->split_frame, pos.end - param->split_frame + 1);

        if (!front || !back) {
            // ロールバック: 作れた方は削除し、元の内容・位置で復元を試みる
            if (front) {
                edit->delete_object(front);
            }
            if (back) {
                edit->delete_object(back);
            }
            OBJECT_HANDLE restored =
                edit->create_object_from_alias(alias.c_str(), pos.layer, pos.start, pos.end - pos.start + 1);
            param->rolled_back = (restored != nullptr);
            param->error = param->rolled_back
                                ? "split_clip failed; original object was restored unchanged"
                                : "split_clip failed and rollback also failed; original alias follows: " + alias;
            return;
        }

        param->success = true;
        param->front_pos = edit->get_object_layer_frame(front);
        param->back_pos = edit->get_object_layer_frame(back);
        param->front_id = edit->get_object_id(front);
        param->back_id = edit->get_object_id(back);
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.success) {
        res["ok"] = false;
        res["error"] = param.error.empty() ? "split_clip failed" : param.error;
    } else {
        res["ok"] = true;
        res["result"] = {
            {"is_media", param.is_media},
            {"front",
             {{"object_id", param.front_id},
              {"layer", param.front_pos.layer},
              {"start", param.front_pos.start},
              {"end", param.front_pos.end}}},
            {"back",
             {{"object_id", param.back_id},
              {"layer", param.back_pos.layer},
              {"start", param.back_pos.start},
              {"end", param.back_pos.end}}},
        };
    }
    return true;
}

}  // namespace

bool TryHandleEditCommand(const std::string& cmd, const json& req, json& res) {
    if (cmd == "create_object_from_alias") {
        return HandleCreateObjectFromAlias(req, res);
    }
    if (cmd == "create_object_from_media_file") {
        return HandleCreateObjectFromMediaFile(req, res);
    }
    if (cmd == "move_object") {
        return HandleMoveObject(req, res);
    }
    if (cmd == "delete_object") {
        return HandleDeleteObject(req, res);
    }
    if (cmd == "set_item_value") {
        return HandleSetItemValue(req, res);
    }
    if (cmd == "create_effect") {
        return HandleCreateEffect(req, res);
    }
    if (cmd == "delete_effect") {
        return HandleDeleteEffect(req, res);
    }
    if (cmd == "split_clip") {
        return HandleSplitClip(req, res);
    }
    return false;
}
