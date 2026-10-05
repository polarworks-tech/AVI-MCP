#include "object_utils.h"

#include <vector>

#include "string_utils.h"

OBJECT_HANDLE ResolveObjectById(EDIT_SECTION* edit, int64_t target_id, int layer_max) {
    for (int layer = 0; layer <= layer_max; ++layer) {
        int frame = 0;
        while (true) {
            OBJECT_HANDLE obj = edit->find_object(layer, frame);
            if (!obj) {
                break;
            }
            if (edit->get_object_id(obj) == target_id) {
                return obj;
            }
            const OBJECT_LAYER_FRAME pos = edit->get_object_layer_frame(obj);
            frame = pos.end + 1;
        }
    }
    return nullptr;
}

EFFECT_HANDLE ResolveEffectById(EDIT_SECTION* edit, OBJECT_HANDLE object, int64_t target_effect_id) {
    const int num = edit->get_effect_list(object, nullptr, 0);
    if (num <= 0) {
        return nullptr;
    }
    std::vector<EFFECT_HANDLE> effects(num);
    const int got = edit->get_effect_list(object, effects.data(), num);
    for (int i = 0; i < got; ++i) {
        if (edit->get_effect_id(effects[i]) == target_effect_id) {
            return effects[i];
        }
    }
    return nullptr;
}

json ObjectEntryToJson(const ObjectEntry& e) {
    return {{"object_id", e.object_id}, {"layer", e.layer}, {"start", e.start}, {"end", e.end}};
}

json ObjectEntriesToJsonArray(const std::vector<ObjectEntry>& entries) {
    json arr = json::array();
    for (const auto& e : entries) {
        arr.push_back(ObjectEntryToJson(e));
    }
    return arr;
}

json EffectEntryToJson(const EffectEntry& e) {
    return {{"effect_id", e.effect_id}, {"name", WideToUtf8(e.name)}};
}
