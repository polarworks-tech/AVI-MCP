#include "commands_rendering.h"

#include <cstdint>
#include <cstring>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(x)  // AviUtl2本体を落とさないため、assertでプロセスを止めない
#include "stb_image_write.h"

#include "globals.h"
#include "plugin2.h"
#include "string_utils.h"

namespace {

// PIXEL_RGBA(unsigned char r,g,b,a; filter2.h参照)のピクセル配列をPNGにエンコードする。
std::vector<uint8_t> EncodePng(const std::vector<uint8_t>& pixels, int width, int height) {
    std::vector<uint8_t> png;
    stbi_write_png_to_func(
        [](void* context, void* data, int size) {
            auto* out = static_cast<std::vector<uint8_t>*>(context);
            const auto* bytes = static_cast<uint8_t*>(data);
            out->insert(out->end(), bytes, bytes + size);
        },
        &png, width, height, /*comp=*/4, pixels.data(), width * 4);
    return png;
}

// rendering_scene_video/wait_rendering_taskはメインスレッド(BridgeWndProc経由)から
// 呼ぶ前提で実装している(docs/design/03_RENDERING.md参照、検証済み)。
bool HandleRenderSceneFrame(const json& req, json& res) {
    struct Param {
        int frame = 0;
        bool done = false;
        int width = 0;
        int height = 0;
        std::vector<uint8_t> pixels;  // width*height*4 (RGBA, 詰め直し済み)
    } param;
    param.frame = req.value("frame", 0);

    const bool requested = g_edit->rendering_scene_video(
        param.frame, &param,
        [](void* p, int frame, const void* buffer, int width, int height, int pitch) {
            auto* param = static_cast<Param*>(p);
            param->width = width;
            param->height = height;
            param->pixels.resize(static_cast<size_t>(width) * height * 4);
            const auto* src = static_cast<const uint8_t*>(buffer);
            for (int y = 0; y < height; ++y) {
                std::memcpy(
                    param->pixels.data() + static_cast<size_t>(y) * width * 4,
                    src + static_cast<size_t>(y) * pitch,
                    static_cast<size_t>(width) * 4);
            }
            param->done = true;
        });

    if (!requested) {
        res["ok"] = false;
        res["error"] = "rendering_scene_video failed (invalid frame or busy)";
        return true;
    }

    // レンダリングタスクの完了を待つ(非同期コールバックの同期化)
    g_edit->wait_rendering_task();

    if (!param.done) {
        res["ok"] = false;
        res["error"] = "rendering callback did not complete";
        return true;
    }

    const std::vector<uint8_t> png = EncodePng(param.pixels, param.width, param.height);
    if (png.empty()) {
        res["ok"] = false;
        res["error"] = "PNG encoding failed";
        return true;
    }

    res["ok"] = true;
    res["result"] = {
        {"width", param.width},
        {"height", param.height},
        {"png_base64", Base64Encode(png)},
    };
    return true;
}

}  // namespace

bool TryHandleRenderingCommand(const std::string& cmd, const json& req, json& res) {
    if (cmd == "render_scene_frame") {
        return HandleRenderSceneFrame(req, res);
    }
    return false;
}
