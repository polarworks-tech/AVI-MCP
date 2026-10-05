#include "commands_fileio.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

#include "globals.h"
#include "plugin2.h"
#include "string_utils.h"

namespace fs = std::filesystem;

namespace {

// pathが絶対パスでなければエラーを設定してfalseを返す。相対パスはAviUtl2プロセスの
// カレントディレクトリ基準で解決されてしまい、意図しない場所にファイルが作られる
// 事故につながるため、ファイルI/O系コマンドでは常に絶対パスを要求する。
bool RequireAbsolutePath(const fs::path& path, const std::string& path_utf8, json& res) {
    if (!path.is_absolute()) {
        res["ok"] = false;
        res["error"] = "path must be an absolute path: " + path_utf8;
        return false;
    }
    return true;
}

bool HandleSaveProjectFile(const json& req, json& res) {
    const std::string path_utf8 = req.value("path", "");
    const bool confirm = req.value("confirm", false);

    if (path_utf8.empty()) {
        res["ok"] = false;
        res["error"] = "path is required";
        return true;
    }

    const fs::path path(Utf8ToWide(path_utf8));
    if (!RequireAbsolutePath(path, path_utf8, res)) {
        return true;
    }
    std::error_code ec;

    // 上書き禁止: confirmの値によらず、既存ファイルへの保存は常に拒否する
    if (fs::exists(path, ec)) {
        res["ok"] = false;
        res["error"] = "file already exists (overwrite is not allowed): " + path_utf8;
        return true;
    }

    const fs::path parent = path.parent_path();
    if (!parent.empty() && !fs::exists(parent, ec)) {
        res["ok"] = false;
        res["error"] = "parent directory does not exist: " + path_utf8;
        return true;
    }

    if (!confirm) {
        res["ok"] = true;
        res["result"] = {
            {"confirmed", false},
            {"would_save_to", path_utf8},
        };
        return true;
    }

    const bool success = g_edit->save_project_file(path.c_str());
    if (!success) {
        res["ok"] = false;
        res["error"] = "save_project_file failed (output in progress?)";
    } else {
        res["ok"] = true;
        res["result"] = {{"confirmed", true}, {"saved_to", path_utf8}};
    }
    return true;
}

bool HandleOpenProjectFile(const json& req, json& res) {
    const std::string path_utf8 = req.value("path", "");
    const bool confirm = req.value("confirm", false);

    if (path_utf8.empty()) {
        res["ok"] = false;
        res["error"] = "path is required";
        return true;
    }

    const fs::path path(Utf8ToWide(path_utf8));
    if (!RequireAbsolutePath(path, path_utf8, res)) {
        return true;
    }
    std::error_code ec;

    if (!fs::exists(path, ec)) {
        res["ok"] = false;
        res["error"] = "file not found: " + path_utf8;
        return true;
    }

    if (!confirm) {
        res["ok"] = true;
        res["result"] = {
            {"confirmed", false},
            {"would_open", path_utf8},
            {"warning", "current unsaved edits will be discarded"},
        };
        return true;
    }

    // show_confirm=false: AviUtl2自身の確認ダイアログは出さない(MCP層のconfirmで確認済みのため)
    const bool success = g_edit->open_project_file(path.c_str(), /*show_confirm=*/false);
    if (!success) {
        res["ok"] = false;
        res["error"] = "open_project_file failed (output in progress?)";
    } else {
        res["ok"] = true;
        res["result"] = {{"confirmed", true}, {"opened", path_utf8}};
    }
    return true;
}

bool HandleCreateProject(const json& req, json& res) {
    const int width = req.value("width", 1920);
    const int height = req.value("height", 1080);
    const int rate = req.value("rate", 30);
    const int scale = req.value("scale", 1);
    const int sample_rate = req.value("sample_rate", 44100);
    const bool confirm = req.value("confirm", false);

    if (!confirm) {
        res["ok"] = true;
        res["result"] = {
            {"confirmed", false},
            {"would_create",
             {{"width", width}, {"height", height}, {"rate", rate}, {"scale", scale}, {"sample_rate", sample_rate}}},
            {"warning", "current unsaved edits will be discarded"},
        };
        return true;
    }

    const EDIT_INFO::COLOR background{0, 0, 0, 255};
    const bool success =
        g_edit->create_project(width, height, rate, scale, sample_rate, background, /*show_confirm=*/false);
    if (!success) {
        res["ok"] = false;
        res["error"] = "create_project failed (output in progress?)";
    } else {
        res["ok"] = true;
        res["result"] = {{"confirmed", true}};
    }
    return true;
}

// インストールされている出力プラグインの名前一覧を取得する。
// aviutl2_list_output_pluginsという独立ツールにはせず(docs/DESIGN_POLICY.mdの
// 「ツール設計の原則」参照)、output_fileの検証・エラーメッセージからのみ利用する。
std::vector<std::wstring> ListOutputPluginNames() {
    std::vector<std::wstring> names;
    g_edit->enum_module_info(&names, [](void* p, MODULE_INFO* info) {
        if (info->type != MODULE_INFO::TYPE_PLUGIN_OUTPUT || !info->name) {
            return;
        }
        auto* names = static_cast<std::vector<std::wstring>*>(p);
        names->push_back(info->name);
    });
    return names;
}

std::string JoinOutputPluginNamesForError(const std::vector<std::wstring>& names) {
    std::string joined;
    for (const auto& name : names) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += "\"" + WideToUtf8(name) + "\"";
    }
    return joined;
}

bool HandleGetOutputStatus(const json& req, json& res) {
    const int state = g_edit->get_edit_state();
    std::string state_str = "unknown";
    if (state == EDIT_HANDLE::EDIT_STATE_EDIT) {
        state_str = "edit";
    } else if (state == EDIT_HANDLE::EDIT_STATE_PLAY) {
        state_str = "play";
    } else if (state == EDIT_HANDLE::EDIT_STATE_SAVE) {
        state_str = "save";
    }
    res["ok"] = true;
    res["result"] = {{"state", state_str}};
    return true;
}

bool HandleOutputFile(const json& req, json& res) {
    const std::string path_utf8 = req.value("path", "");
    const std::string output_plugin_utf8 = req.value("output_plugin", "");
    const bool confirm = req.value("confirm", false);

    if (path_utf8.empty() || output_plugin_utf8.empty()) {
        res["ok"] = false;
        res["error"] = "path and output_plugin are required";
        return true;
    }

    const fs::path path(Utf8ToWide(path_utf8));
    if (!RequireAbsolutePath(path, path_utf8, res)) {
        return true;
    }
    std::error_code ec;

    // 上書き禁止: confirmの値によらず、既存ファイルへの出力は常に拒否する
    if (fs::exists(path, ec)) {
        res["ok"] = false;
        res["error"] = "file already exists (overwrite is not allowed): " + path_utf8;
        return true;
    }

    const fs::path parent = path.parent_path();
    if (!parent.empty() && !fs::exists(parent, ec)) {
        res["ok"] = false;
        res["error"] = "parent directory does not exist: " + path_utf8;
        return true;
    }

    const std::vector<std::wstring> available_plugins = ListOutputPluginNames();
    const std::wstring output_plugin_wide = Utf8ToWide(output_plugin_utf8);
    const bool plugin_known =
        std::find(available_plugins.begin(), available_plugins.end(), output_plugin_wide) !=
        available_plugins.end();
    if (!plugin_known) {
        res["ok"] = false;
        res["error"] =
            "unknown output_plugin: \"" + output_plugin_utf8 + "\". available plugins: [" +
            JoinOutputPluginNamesForError(available_plugins) + "]";
        return true;
    }

    if (!confirm) {
        res["ok"] = true;
        res["result"] = {
            {"confirmed", false},
            {"would_output_to", path_utf8},
            {"output_plugin", output_plugin_utf8},
        };
        return true;
    }

    // output_fileはエンコード開始のみ行い、完了を待たずに返る(非同期)。
    // 完了確認はaviutl2_get_output_statusのポーリングで行う(docs/design/06_FILE_IO.md参照)。
    const bool started = g_edit->output_file(path.c_str(), output_plugin_wide.c_str(), nullptr, nullptr);
    if (!started) {
        res["ok"] = false;
        res["error"] = "output_file failed (busy?). available plugins: [" +
                        JoinOutputPluginNamesForError(available_plugins) + "]";
    } else {
        res["ok"] = true;
        res["result"] = {
            {"confirmed", true},
            {"started", true},
            {"output_to", path_utf8},
            {"note", "encoding started asynchronously; poll aviutl2_get_output_status to check completion"},
        };
    }
    return true;
}

}  // namespace

bool TryHandleFileIoCommand(const std::string& cmd, const json& req, json& res) {
    if (cmd == "save_project_file") {
        return HandleSaveProjectFile(req, res);
    }
    if (cmd == "open_project_file") {
        return HandleOpenProjectFile(req, res);
    }
    if (cmd == "create_project") {
        return HandleCreateProject(req, res);
    }
    if (cmd == "get_output_status") {
        return HandleGetOutputStatus(req, res);
    }
    if (cmd == "output_file") {
        return HandleOutputFile(req, res);
    }
    return false;
}
