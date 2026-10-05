#include "commands_read.h"

#include <vector>

#include "globals.h"
#include "object_utils.h"
#include "plugin2.h"

namespace {

bool HandleGetEditInfo(const json& req, json& res) {
    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    res["ok"] = true;
    res["result"] = {
        {"width", info.width},
        {"height", info.height},
        {"rate", info.rate},
        {"scale", info.scale},
        {"sample_rate", info.sample_rate},
        {"frame", info.frame},
        {"layer", info.layer},
        {"frame_max", info.frame_max},
        {"layer_max", info.layer_max},
        {"scene_id", info.scene_id},
    };
    return true;
}

bool HandleGetObjectAlias(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int layer_max = 0;
        bool found = false;
        std::string alias;
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_read_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (obj) {
            LPCSTR alias = edit->get_object_alias(obj);
            if (alias) {
                param->found = true;
                param->alias = alias;  // コールバック終了後は無効になるためここでコピーする
            }
        }
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.found) {
        res["ok"] = false;
        res["error"] = "object not found: " + std::to_string(param.object_id);
    } else {
        res["ok"] = true;
        res["result"] = {{"alias", param.alias}};
    }
    return true;
}

// aviutl2_list_object_effects: オブジェクトが持つエフェクトの一覧(effect_id/name)を取得する。
// aviutl2_delete_effectで対象を指定する際、effect_idを調べるために使う。
bool HandleListObjectEffects(const json& req, json& res) {
    struct Param {
        int64_t object_id = 0;
        int layer_max = 0;
        bool found = false;
        std::vector<EffectEntry> effects;
    } param;
    param.object_id = req.value("object_id", static_cast<int64_t>(0));

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_read_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id, param->layer_max);
        if (!obj) {
            return;
        }
        param->found = true;
        const int num = edit->get_effect_list(obj, nullptr, 0);
        if (num <= 0) {
            return;
        }
        std::vector<EFFECT_HANDLE> effects(num);
        const int got = edit->get_effect_list(obj, effects.data(), num);
        for (int i = 0; i < got; ++i) {
            LPCWSTR name = edit->get_effect_name(effects[i]);
            param->effects.push_back({edit->get_effect_id(effects[i]), name ? name : L""});
        }
    });

    if (!called) {
        res["ok"] = false;
        res["error"] = "edit section busy (rendering/output in progress)";
    } else if (!param.found) {
        res["ok"] = false;
        res["error"] = "object not found: " + std::to_string(param.object_id);
    } else {
        json arr = json::array();
        for (const auto& e : param.effects) {
            arr.push_back(EffectEntryToJson(e));
        }
        res["ok"] = true;
        res["result"] = {{"effects", arr}};
    }
    return true;
}

// aviutl2_query_objects: scope("all" | "selected" | "at_position")でオブジェクト検索の
// 3つの切り口(全件・選択中・位置指定)を1つのコマンドに統合している
// (docs/DESIGN_POLICY.mdの「ツール設計の原則」参照。旧find_object/list_objects/
// get_selected_objectsを統合した)。戻り値は常に{"objects": [...]}の配列形式で統一する。
bool HandleQueryObjects(const json& req, json& res) {
    const std::string scope = req.value("scope", "all");

    if (scope == "selected") {
        struct Param {
            std::vector<ObjectEntry> objects;
        } param;

        const bool called = g_edit->call_read_section_param(&param, [](void* p, EDIT_SECTION* edit) {
            auto* param = static_cast<Param*>(p);
            const int num = edit->get_selected_object_num();
            for (int i = 0; i < num; ++i) {
                OBJECT_HANDLE obj = edit->get_selected_object(i);
                if (!obj) {
                    continue;
                }
                const OBJECT_LAYER_FRAME pos = edit->get_object_layer_frame(obj);
                param->objects.push_back({edit->get_object_id(obj), pos.layer, pos.start, pos.end});
            }
        });

        res["ok"] = called;
        if (!called) {
            res["error"] = "edit section busy (rendering/output in progress)";
        } else {
            res["result"] = {{"objects", ObjectEntriesToJsonArray(param.objects)}};
        }
        return true;
    }

    if (scope == "at_position") {
        struct Param {
            int layer = 0;
            int frame = 0;
            std::vector<ObjectEntry> objects;
        } param;
        param.layer = req.value("layer", 0);
        param.frame = req.value("frame", 0);

        const bool called = g_edit->call_read_section_param(&param, [](void* p, EDIT_SECTION* edit) {
            auto* param = static_cast<Param*>(p);
            OBJECT_HANDLE obj = edit->find_object(param->layer, param->frame);
            if (obj) {
                const OBJECT_LAYER_FRAME pos = edit->get_object_layer_frame(obj);
                param->objects.push_back({edit->get_object_id(obj), pos.layer, pos.start, pos.end});
            }
        });

        res["ok"] = called;
        if (!called) {
            res["error"] = "edit section busy (rendering/output in progress)";
        } else {
            res["result"] = {{"objects", ObjectEntriesToJsonArray(param.objects)}};
        }
        return true;
    }

    // scope == "all" (既定): 全レイヤーを走査して全オブジェクトを収集する
    struct Param {
        int layer_max = 0;
        std::vector<ObjectEntry> objects;
    } param;

    EDIT_INFO info{};
    g_edit->get_edit_info(&info, sizeof(info));
    param.layer_max = info.layer_max;

    const bool called = g_edit->call_read_section_param(&param, [](void* p, EDIT_SECTION* edit) {
        auto* param = static_cast<Param*>(p);
        for (int layer = 0; layer <= param->layer_max; ++layer) {
            int frame = 0;
            while (true) {
                OBJECT_HANDLE obj = edit->find_object(layer, frame);
                if (!obj) {
                    break;
                }
                const OBJECT_LAYER_FRAME pos = edit->get_object_layer_frame(obj);
                param->objects.push_back({edit->get_object_id(obj), pos.layer, pos.start, pos.end});
                frame = pos.end + 1;
            }
        }
    });

    res["ok"] = called;
    if (!called) {
        res["error"] = "edit section busy (rendering/output in progress)";
    } else {
        res["result"] = {{"objects", ObjectEntriesToJsonArray(param.objects)}};
    }
    return true;
}

}  // namespace

bool TryHandleReadCommand(const std::string& cmd, const json& req, json& res) {
    if (cmd == "get_edit_info") {
        return HandleGetEditInfo(req, res);
    }
    if (cmd == "get_object_alias") {
        return HandleGetObjectAlias(req, res);
    }
    if (cmd == "list_object_effects") {
        return HandleListObjectEffects(req, res);
    }
    if (cmd == "query_objects") {
        return HandleQueryObjects(req, res);
    }
    return false;
}
