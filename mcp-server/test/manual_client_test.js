// AviUtl2が起動し、AviUtl2McpBridge.aux2が読み込まれている状態で実行する動作確認スクリプト。
//
//   cd mcp-server
//   node test/manual_client_test.js
//
// 注意: 編集中のプロジェクトに検証用のテキストオブジェクトを1つ作成し、終了時に削除する。
//       作業中のプロジェクトではなく、空のプロジェクトで実行すること。
import { mkdtempSync, rmSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const serverDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

const transport = new StdioClientTransport({
  command: "node",
  args: ["src/index.js"],
  cwd: serverDir,
});

const client = new Client({ name: "test-client", version: "0.0.1" });
await client.connect(transport);

function textOf(res) {
  return res.content[0].text;
}

const tools = await client.listTools();
console.log("TOOLS (count=" + tools.tools.length + "):", tools.tools.map((t) => t.name));

// query_objects: scope=all (空のプロジェクトならオブジェクト無し)
const queryAllEmpty = await client.callTool({ name: "aviutl2_query_objects", arguments: {} });
console.log("QUERY_ALL(empty):", textOf(queryAllEmpty));

// オブジェクト作成
const created = await client.callTool({
  name: "aviutl2_create_object_from_alias",
  arguments: {
    alias: "[Object]\n[Object.0]\neffect.name=テキスト\nテキスト=manual client test\n[Object.1]\neffect.name=標準描画\n",
    layer: 2,
    frame: 5,
    length: 30,
  },
});
console.log("CREATE:", textOf(created));
if (created.isError) {
  console.log("オブジェクトを作成できなかったため中断する(空のプロジェクトで実行しているか確認すること)");
  await client.close();
  process.exit(1);
}
const objectId = JSON.parse(textOf(created)).object_id;

// query_objects: scope=all
const queryAll = await client.callTool({ name: "aviutl2_query_objects", arguments: { scope: "all" } });
console.log("QUERY_ALL:", textOf(queryAll));

// query_objects: scope=at_position (作成した位置)
const queryAtPos = await client.callTool({
  name: "aviutl2_query_objects",
  arguments: { scope: "at_position", layer: 2, frame: 0 },
});
console.log("QUERY_AT_POSITION:", textOf(queryAtPos));

// query_objects: scope=at_position (何もない位置)
const queryAtPosEmpty = await client.callTool({
  name: "aviutl2_query_objects",
  arguments: { scope: "at_position", layer: 9, frame: 0 },
});
console.log("QUERY_AT_POSITION(empty):", textOf(queryAtPosEmpty));

// query_objects: scope=selected (何も選択していないはず)
const querySelected = await client.callTool({ name: "aviutl2_query_objects", arguments: { scope: "selected" } });
console.log("QUERY_SELECTED:", textOf(querySelected));

// output_file: 不正なplugin名を渡し、エラーメッセージに有効なプラグイン名の一覧が出ることを確認する
const testDir = mkdtempSync(path.join(os.tmpdir(), "aviutl2-mcp-test-"));
const badPluginRes = await client.callTool({
  name: "aviutl2_output_file",
  arguments: { path: path.join(testDir, "out.mp4"), output_plugin: "存在しないプラグイン" },
});
console.log("BAD_PLUGIN(should list available):", textOf(badPluginRes), badPluginRes.isError);

// 後片付け: 作成した検証用オブジェクトと一時ディレクトリを削除する
const deleted = await client.callTool({ name: "aviutl2_delete_object", arguments: { object_id: objectId } });
console.log("CLEANUP:", textOf(deleted));

await client.close();
rmSync(testDir, { recursive: true, force: true });
process.exit(0);
