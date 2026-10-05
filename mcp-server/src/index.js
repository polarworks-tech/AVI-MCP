#!/usr/bin/env node
// AviUtl2をMCP経由で操作するサーバー。
// 実際の操作はAviUtl2McpBridge.aux2プラグイン(127.0.0.1:23910)に委譲する。
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { sendBridgeCommand } from "./bridge-client.js";

const server = new McpServer({ name: "aviutl2-mcp", version: "0.1.0" });

function jsonResult(value) {
  return { content: [{ type: "text", text: JSON.stringify(value, null, 2) }] };
}

function errorResult(error) {
  return {
    content: [{ type: "text", text: error instanceof Error ? error.message : String(error) }],
    isError: true,
  };
}

function imageResult(base64Png) {
  return { content: [{ type: "image", data: base64Png, mimeType: "image/png" }] };
}

/** ブリッジへコマンドを送り、失敗はMCPのエラー結果に、成功はJSON結果に変換するヘルパー */
async function runBridgeTool(command, options) {
  try {
    const res = await sendBridgeCommand(command, options);
    if (res.ok === false) {
      return errorResult(new Error(res.error ?? "unknown bridge error"));
    }
    return jsonResult(res.result ?? res);
  } catch (err) {
    return errorResult(err);
  }
}

server.registerTool(
  "aviutl2_ping",
  {
    description: "AviUtl2ブリッジプラグインへの接続を確認する",
    inputSchema: {},
  },
  async () => runBridgeTool({ cmd: "ping" })
);

server.registerTool(
  "aviutl2_get_edit_info",
  {
    description:
      "現在のAviUtl2プロジェクトの編集状態を取得する(解像度、フレームレート、サンプリングレート、" +
      "カーソル位置、オブジェクトが存在する最大フレーム/レイヤー、シーンIDなど)",
    inputSchema: {},
  },
  async () => runBridgeTool({ cmd: "get_edit_info" })
);

server.registerTool(
  "aviutl2_create_object_from_alias",
  {
    description:
      "オブジェクトエイリアス形式(AviUtl2の.objectファイルと同じINI形式, UTF-8)のテキストから、" +
      "指定したレイヤー・フレーム位置にオブジェクトを作成する。\n" +
      "フォーマット例(テキストオブジェクト):\n" +
      "[Object]\n[Object.0]\neffect.name=テキスト\nサイズ=150.00\n文字色=3bcb48\nテキスト=サンプル\n" +
      "[Object.1]\neffect.name=標準描画\n" +
      "既に同じ位置にオブジェクトがある、またはeffect.nameが存在しない場合は失敗する。",
    inputSchema: {
      alias: z.string().describe("オブジェクトエイリアスのテキスト(UTF-8, INI形式)"),
      layer: z.number().int().min(0).describe("作成するレイヤー番号(0始まり)"),
      frame: z.number().int().min(0).describe("作成するフレーム番号(0始まり)"),
      length: z
        .number()
        .int()
        .min(0)
        .default(100)
        .describe("オブジェクトのフレーム数。0を指定すると長さや位置が自動調整される"),
    },
  },
  async ({ alias, layer, frame, length }) =>
    runBridgeTool({ cmd: "create_object_from_alias", alias, layer, frame, length })
);

server.registerTool(
  "aviutl2_create_object_from_media_file",
  {
    description:
      "動画・画像・音声ファイルから、指定したレイヤー・フレーム位置にメディアオブジェクトを作成する。" +
      "既に同じ位置にオブジェクトがある、またはファイル形式に対応していない場合は失敗する。",
    inputSchema: {
      file: z.string().describe("メディアファイルの絶対パス"),
      layer: z.number().int().min(0).describe("作成するレイヤー番号(0始まり)"),
      frame: z.number().int().min(0).describe("作成するフレーム番号(0始まり)"),
      length: z
        .number()
        .int()
        .min(0)
        .default(0)
        .describe("オブジェクトのフレーム数。0を指定すると素材の長さから自動設定される"),
    },
  },
  async ({ file, layer, frame, length }) =>
    runBridgeTool({ cmd: "create_object_from_media_file", file, layer, frame, length })
);

server.registerTool(
  "aviutl2_get_object_alias",
  {
    description:
      "既存オブジェクトの内容をエイリアス形式のテキスト(create_object_from_aliasと同じINI形式)で取得する。" +
      "object_idはaviutl2_find_objectやaviutl2_create_object_from_aliasの戻り値から得られる。" +
      "既存オブジェクトの設定を読んでから似たものを作る、といった用途に使う。",
    inputSchema: {
      object_id: z.number().int().describe("対象オブジェクトのID(不変の識別子)"),
    },
  },
  async ({ object_id }) => runBridgeTool({ cmd: "get_object_alias", object_id })
);

server.registerTool(
  "aviutl2_query_objects",
  {
    description:
      "オブジェクトを検索する。scopeで検索方法を切り替える統合ツール:\n" +
      "- scope=\"all\"(既定): シーン内の全オブジェクトを一覧取得する。プロジェクトの現状を" +
      "把握するための出発点として使う。\n" +
      "- scope=\"selected\": 現在AviUtl2のUI上で選択されているオブジェクトのみ取得する。" +
      "ユーザーが選んだものに対して操作する、といった協調編集の用途に使う。\n" +
      "- scope=\"at_position\": layer・frameで指定した位置以降にある最初のオブジェクトのみ取得する" +
      "(layer・frameが必須)。\n" +
      "いずれも結果はobjects配列({object_id, layer, start, end}の形)で返る。",
    inputSchema: {
      scope: z.enum(["all", "selected", "at_position"]).default("all").describe("検索方法"),
      layer: z.number().int().min(0).optional().describe("scope=\"at_position\"のとき必須: 検索対象のレイヤー番号(0始まり)"),
      frame: z.number().int().min(0).optional().describe("scope=\"at_position\"のとき必須: 検索を開始するフレーム番号(0始まり)"),
    },
  },
  async ({ scope, layer, frame }) => {
    // zodのinputSchemaはフラットなshapeのため、「scope次第で他の引数が必須になる」という
    // 条件付き必須をスキーマだけでは表現できない。省略時に静かに既定値(0)へ丸められ
    // 誤動作するのを防ぐため、ここで明示的にチェックする。
    if (scope === "at_position" && (layer === undefined || frame === undefined)) {
      return errorResult(new Error('scope="at_position"を指定する場合、layerとframeの両方が必須です'));
    }
    return runBridgeTool({ cmd: "query_objects", scope, layer, frame });
  }
);

server.registerTool(
  "aviutl2_move_object",
  {
    description:
      "既存オブジェクトを指定したレイヤー・フレーム位置へ移動する。移動先に既にオブジェクトがあると失敗する。" +
      "空いている場所を探すのは呼び出し側の責務(aviutl2_query_objects等で事前に確認する)。" +
      "通常のUndo(Ctrl+Z)で元に戻せる。",
    inputSchema: {
      object_id: z.number().int().describe("移動対象オブジェクトのID"),
      layer: z.number().int().min(0).describe("移動先のレイヤー番号(0始まり)"),
      frame: z.number().int().min(0).describe("移動先のフレーム番号(0始まり)"),
    },
  },
  async ({ object_id, layer, frame }) => runBridgeTool({ cmd: "move_object", object_id, layer, frame })
);

server.registerTool(
  "aviutl2_delete_object",
  {
    description: "既存オブジェクトを削除する。通常のUndo(Ctrl+Z)で元に戻せる。",
    inputSchema: {
      object_id: z.number().int().describe("削除対象オブジェクトのID"),
    },
  },
  async ({ object_id }) => runBridgeTool({ cmd: "delete_object", object_id })
);

server.registerTool(
  "aviutl2_set_item_value",
  {
    description:
      "既存オブジェクトのエフェクト設定項目の値を変更する。effect/itemはAviUtl2内部の日本語名そのもの" +
      "(例: effect=\"テキスト\", item=\"サイズ\", value=\"200.00\")。" +
      "同じエフェクトが複数ある場合は effect=\"テキスト:1\" のように\":n\"(0始まり)で指定する。" +
      "有効なeffect/item名が分からない場合は、先にaviutl2_get_object_aliasで対象オブジェクトの" +
      "現在の設定一覧を読んでから値を決めること。",
    inputSchema: {
      object_id: z.number().int().describe("対象オブジェクトのID"),
      effect: z.string().describe("エフェクト名(例: \"テキスト\")。同名が複数ある場合は\"名前:n\""),
      item: z.string().describe("設定項目名(例: \"サイズ\")"),
      value: z.string().describe("設定する値(文字列)"),
    },
  },
  async ({ object_id, effect, item, value }) =>
    runBridgeTool({ cmd: "set_item_value", object_id, effect, item, value })
);

server.registerTool(
  "aviutl2_list_object_effects",
  {
    description:
      "オブジェクトが持つエフェクトの一覧(effect_id・名称)を取得する。" +
      "aviutl2_delete_effectで対象を指定する際、事前にeffect_idを調べるために使う。",
    inputSchema: {
      object_id: z.number().int().describe("対象オブジェクトのID"),
    },
  },
  async ({ object_id }) => runBridgeTool({ cmd: "list_object_effects", object_id })
);

server.registerTool(
  "aviutl2_create_effect",
  {
    description:
      "既存オブジェクトにエフェクトを追加する(例: \"ぼかし\", \"縁取り\" 等のフィルタ効果)。" +
      "effectは追加したいエフェクトの日本語名。有効なエフェクト名が分からない場合は、" +
      "似た構成の既存オブジェクトをaviutl2_get_object_aliasで読んで参考にすること。" +
      "入力・出力系のエフェクト(「標準描画」等)を指定した場合は差し替えになる。" +
      "通常のUndo(Ctrl+Z)で元に戻せる。",
    inputSchema: {
      object_id: z.number().int().describe("追加先オブジェクトのID"),
      effect: z.string().describe("追加するエフェクト名(例: \"ぼかし\")"),
    },
  },
  async ({ object_id, effect }) => runBridgeTool({ cmd: "create_effect", object_id, effect })
);

server.registerTool(
  "aviutl2_delete_effect",
  {
    description:
      "既存オブジェクトからエフェクトを削除する。effect_idはaviutl2_list_object_effectsまたは" +
      "aviutl2_create_effectの戻り値から得られる。" +
      "注意: 入力・出力系のエフェクト(「テキスト」「標準描画」等、素材そのもの)も削除できてしまう。" +
      "出力系エフェクトを削除すると、そのオブジェクトは何も描画されなくなる" +
      "(クラッシュはしないが見た目上消える)。意図せず削除しないよう、削除前に" +
      "aviutl2_list_object_effectsで対象を確認すること。通常のUndo(Ctrl+Z)で元に戻せる。",
    inputSchema: {
      object_id: z.number().int().describe("対象オブジェクトのID"),
      effect_id: z.number().int().describe("削除するエフェクトのID"),
    },
  },
  async ({ object_id, effect_id }) => runBridgeTool({ cmd: "delete_effect", object_id, effect_id })
);

server.registerTool(
  "aviutl2_split_clip",
  {
    description:
      "既存オブジェクトを指定フレームで前半・後半の2つのオブジェクトに分割する(カット編集)。" +
      "SDKに直接の分割APIが無いため、内部では[元オブジェクトを削除→前半・後半を同じ内容で再作成]という" +
      "手順を1回の操作としてまとめて行う。動画・音声ファイルの場合は、後半オブジェクトの" +
      "「素材内の再生開始位置」を自動的にsplit_frame分オフセットするため、見た目上は" +
      "連続再生のまま2つに割れる(実機で動作確認済み)。テキスト等メディアを持たないオブジェクトの" +
      "場合はオフセット計算をせず単純に分割する。" +
      "split_frameは対象オブジェクトの(開始, 終了]の範囲内である必要がある。" +
      "作成に失敗した場合は、可能な限り元のオブジェクトを変更前の状態に復元してからエラーを返す。" +
      "通常のUndo(Ctrl+Z)でもまとめて元に戻せる。",
    inputSchema: {
      object_id: z.number().int().describe("分割対象オブジェクトのID"),
      split_frame: z.number().int().min(0).describe("分割するフレーム番号(タイムライン上の絶対位置、対象オブジェクトの開始より後・終了以下)"),
    },
  },
  async ({ object_id, split_frame }) => runBridgeTool({ cmd: "split_clip", object_id, split_frame })
);

server.registerTool(
  "aviutl2_render_scene_frame",
  {
    description:
      "指定したフレーム番号のシーン全体をレンダリングし、PNG画像として取得する。" +
      "編集結果を実際の見た目で確認したいときに使う。解像度が大きい場合、画像サイズも大きくなる。",
    inputSchema: {
      frame: z.number().int().min(0).describe("レンダリングするフレーム番号(0始まり)"),
    },
  },
  async ({ frame }) => {
    try {
      // レンダリングは重いシーンで数秒かかることがあるため、既定(5秒)より長めに待つ
      const res = await sendBridgeCommand({ cmd: "render_scene_frame", frame }, { timeoutMs: 30000 });
      if (res.ok === false) {
        return errorResult(new Error(res.error ?? "unknown bridge error"));
      }
      return imageResult(res.result.png_base64);
    } catch (err) {
      return errorResult(err);
    }
  }
);

const CONFIRM_FLOW_NOTE =
  "この操作はconfirm(既定false)による2段階フローを取る。confirm=falseで呼ぶと実際には実行せず、" +
  "検証結果(パスの妥当性、警告)のみ返す。内容をユーザーに確認してもらった上でconfirm=trueで" +
  "再度呼び出すと、実際に操作が実行される。";

server.registerTool(
  "aviutl2_save_project_file",
  {
    description:
      "現在の編集内容をプロジェクトファイル(.aup2)として保存する。" +
      "上書き保存は常に禁止されており、保存先に既にファイルが存在する場合は(confirmの値によらず)失敗する。" +
      CONFIRM_FLOW_NOTE,
    inputSchema: {
      path: z.string().describe("保存先の絶対パス(例: D:\\\\projects\\\\scene1.aup2)"),
      confirm: z.boolean().default(false).describe("true の場合のみ実際に保存を実行する"),
    },
  },
  async ({ path, confirm }) => runBridgeTool({ cmd: "save_project_file", path, confirm })
);

server.registerTool(
  "aviutl2_open_project_file",
  {
    description:
      "既存のプロジェクトファイル(.aup2)を開く。現在の編集内容で保存していないものは失われる。" +
      CONFIRM_FLOW_NOTE,
    inputSchema: {
      path: z.string().describe("開くプロジェクトファイルの絶対パス"),
      confirm: z.boolean().default(false).describe("true の場合のみ実際に開く"),
    },
  },
  async ({ path, confirm }) => runBridgeTool({ cmd: "open_project_file", path, confirm })
);

server.registerTool(
  "aviutl2_create_project",
  {
    description:
      "新規プロジェクトを作成する。現在の編集内容で保存していないものは失われる。" + CONFIRM_FLOW_NOTE,
    inputSchema: {
      width: z.number().int().min(1).default(1920).describe("シーンの横幅"),
      height: z.number().int().min(1).default(1080).describe("シーンの縦幅"),
      rate: z.number().int().min(1).default(30).describe("フレームレート(分子)"),
      scale: z.number().int().min(1).default(1).describe("フレームレート(分母)"),
      sample_rate: z.number().int().min(1).default(44100).describe("サンプリングレート"),
      confirm: z.boolean().default(false).describe("true の場合のみ実際に作成する"),
    },
  },
  async ({ width, height, rate, scale, sample_rate, confirm }) =>
    runBridgeTool({ cmd: "create_project", width, height, rate, scale, sample_rate, confirm })
);

server.registerTool(
  "aviutl2_get_output_status",
  {
    description:
      "現在の編集状態を取得する(edit=編集中, play=プレビュー再生中, save=ファイル出力中)。" +
      "aviutl2_output_fileでエンコードを開始した後、完了したかどうかを確認するために" +
      "このツールを定期的に呼び出す(ポーリングする)こと。stateがsave以外に変わったら完了とみなせる。",
    inputSchema: {},
  },
  async () => runBridgeTool({ cmd: "get_output_status" })
);

server.registerTool(
  "aviutl2_output_file",
  {
    description:
      "現在のシーンを指定した出力プラグインでファイルに書き出す(エンコード)。" +
      "上書き保存は常に禁止されており、出力先に既にファイルが存在する場合は失敗する。" +
      "この呼び出しはエンコードの開始のみを行い、完了を待たずに返る。完了確認は" +
      "aviutl2_get_output_statusをポーリングすること。output_pluginに誤った名前を渡すと、" +
      "エラーメッセージに有効なプラグイン名の一覧が返るので、それを見て再試行すればよい。" +
      "なお、このツールにキャンセル機能はない(SDKに中断APIが存在しないため)。" +
      CONFIRM_FLOW_NOTE,
    inputSchema: {
      path: z.string().describe("出力先の絶対パス"),
      output_plugin: z.string().describe("出力プラグイン名。不明な場合はダミー値で一度呼び出すとエラーで一覧が返る"),
      confirm: z.boolean().default(false).describe("true の場合のみ実際にエンコードを開始する"),
    },
  },
  async ({ path, output_plugin, confirm }) =>
    runBridgeTool({ cmd: "output_file", path, output_plugin, confirm })
);

const transport = new StdioServerTransport();
await server.connect(transport);
