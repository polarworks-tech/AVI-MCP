# カテゴリA: 読み取り系 設計書

対応する要件: [REQUIREMENTS.md#カテゴリa-読み取り系](../REQUIREMENTS.md)
共通設計: [00_COMMON.md](00_COMMON.md)(特に「全オブジェクト一覧化」の走査方式)

全てのコマンドは `call_read_section_param` 経由で実装する(副作用なし)。

## ツール一覧

| MCPツール名 | 対応SDK API | 状態 |
|---|---|---|
| `aviutl2_ping` | - (接続確認) | ✅実装済 |
| `aviutl2_get_edit_info` | `get_edit_info` | ✅実装済 |
| `aviutl2_query_objects` | `find_object` / 走査 / `get_selected_object(_num)` を`scope`で統合 | ✅実装済 |
| `aviutl2_get_object_alias` | `get_object_alias` | ✅実装済 |
| `aviutl2_get_object_detail` | `get_object_name` + `get_object_layer_frame` + `get_effect_list` + `get_effect_name` | 検討中(下記) |
| `aviutl2_get_item_value` | `get_object_item_value` / `get_effect_item_value` | 将来 |
| `aviutl2_list_available_effects` | `enum_effect_name` | 将来 |
| `aviutl2_list_scenes` | `enum_scene_name` | 将来 |
| `aviutl2_list_marks` | `get_mark_frame_list` / `get_mark_frame_memo` | 将来 |

## 詳細設計

### aviutl2_query_objects(✅実装済)

オブジェクト検索の統合ツール。当初`aviutl2_find_object`(位置指定検索)・
`aviutl2_list_objects`(全件)・`aviutl2_get_selected_objects`(選択中)の3ツールに
分かれていたが、「オブジェクトの集合を返す」という同じ概念の異なる切り口であるため
[DESIGN_POLICY.md](../DESIGN_POLICY.md)の「ツール設計の原則」に基づき1つに統合した。

- **入力**: `scope` ("all" | "selected" | "at_position", 既定"all")、
  `scope="at_position"`のときのみ`layer`/`frame`が必須
- **ブリッジコマンド**:
  - `{"cmd": "query_objects", "scope": "all"}`
  - `{"cmd": "query_objects", "scope": "selected"}`
  - `{"cmd": "query_objects", "scope": "at_position", "layer": 0, "frame": 0}`
- **出力**: scopeによらず常に同じ形式
  ```json
  {"ok": true, "result": {"objects": [
    {"object_id": 1, "layer": 0, "start": 0, "end": 99},
    {"object_id": 2, "layer": 1, "start": 0, "end": 59}
  ]}}
  ```
  (`at_position`は該当オブジェクトが無ければ空配列、あれば1件の配列)
- **実装**:
  - `scope="all"`: [00_COMMON.md](00_COMMON.md)の全レイヤー走査方式(SDKに専用APIがないため)
  - `scope="selected"`: `get_selected_object_num()`でループして`get_selected_object(i)`を呼ぶ
  - `scope="at_position"`: `find_object(layer, frame)`を1回呼ぶ
- **注意**: `scope="all"`はプロジェクトの規模(オブジェクト数×レイヤー数)に比例して
  走査コストがかかる。まず実装し、実測で問題が出たら最適化を検討する。

### aviutl2_get_object_alias(✅実装済)

既存オブジェクトの内容をエイリアス形式のテキストとして取得する。
`create_object_from_alias`の逆操作にあたり、「既存の編集内容を読んでから似たものを作る」
用途に使える。

- **入力**: `object_id` (int64)
- **ブリッジコマンド**: `{"cmd": "get_object_alias", "object_id": 1}`
- **処理**: [00_COMMON.md](00_COMMON.md)の`ResolveObjectById`でハンドルを解決し、
  `get_object_alias(obj)`を呼ぶ
- **出力**: `{"ok": true, "result": {"alias": "[Object]\n[Object.0]\n..."}}`
- **エラー**: 対象が見つからない場合 `{"ok": false, "error": "object not found: 1"}`

### aviutl2_get_object_detail(検討中)

`object_id`を受け取り、名称・位置・エフェクト一覧をまとめて返す複合ツール。
個別API(`get_object_name`, `get_effect_list`, `get_effect_name`)を1回のMCP呼び出しで
まとめて取得できると、エージェント側の往復回数が減らせる。

- **入力**: `object_id`
- **出力(案)**:
  ```json
  {
    "object_id": 1,
    "name": null,
    "layer": 0, "start": 0, "end": 99,
    "effects": [{"effect_id": 10, "name": "テキスト"}, {"effect_id": 11, "name": "標準描画"}]
  }
  ```
- **保留事項**: 1ツールに情報を集約するか、個別ツール(`get_object_name`,
  `list_object_effects`等)に分けるかは、実装時のMCPツール呼び出し回数と
  レスポンスサイズのバランスで決める。まずは個別ツールを素直に実装し、
  利用実態を見てから複合ツール化を検討する方が手堅い。

### 将来実装分(概要のみ)

| ツール | 入力 | 出力概要 |
|---|---|---|
| `aviutl2_get_item_value` | `object_id`, `effect`(名称), `item`(名称) | 設定項目の値(文字列) |
| `aviutl2_list_available_effects` | なし | インストール済みの全エフェクト名・種別・対応フラグ一覧 |
| `aviutl2_list_scenes` | なし | シーン名とID一覧 |
| `aviutl2_list_marks` | なし | マークされたフレームとメモの一覧 |

これらは実装パターンが`aviutl2_query_objects`や`aviutl2_get_object_alias`と大きく変わらないため、
詳細設計は着手時に本書を更新する形で追記する。
