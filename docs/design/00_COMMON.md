# カテゴリ共通設計

各カテゴリの詳細設計から参照される、横断的な設計事項。

## object_id → OBJECT_HANDLE 解決戦略

### 制約

[DESIGN_POLICY.md](../DESIGN_POLICY.md) で「`object_id`を主キーにする」方針を決めたが、
SDK(`plugin2.h`)には **`object_id`からハンドルを直接取得するAPIが存在しない**。
ハンドルを得る手段は以下の3つのみ:

- `find_object(layer, frame)` — 指定レイヤー上、指定フレーム以降の最初のオブジェクト
- `get_selected_object(index)` / `get_selected_object_num()` — 選択中のオブジェクトのみ
- `get_focus_object()` — フォーカス中のオブジェクトのみ

また `OBJECT_HANDLE` 自体の有効期間はヘッダ上明記されていない
(`EFFECT_HANDLE`には「エフェクトが破棄されるかコールバック処理の終了まで有効」と明記があるが、
`OBJECT_HANDLE`には同等の記述がない)。安全側に倒し、**ハンドルはコールバックをまたいで
保持しない**(呼び出しごとに再取得する)方針とする。

### 注意: `EDIT_SECTION::info`は`call_read_section`系では使えない

`plugin2.h`のコメントに「編集情報 (call_read_section利用不可)」と明記されている通り、
`EDIT_SECTION::info`(layer_maxの取得に使いたくなるメンバ)は`call_read_section`/
`call_read_section_param`のコールバック内では無効(アクセスするとクラッシュする)。
**実装時にこれを見落としてクラッシュさせた実例がある**(`get_object_alias`実装時、
詳細は[SDK_NOTES.md](../SDK_NOTES.md))。

`layer_max`等の編集情報が必要な場合は、`call_read_section_param`を呼ぶ**前**に
`EDIT_HANDLE::get_edit_info`で取得し、値をパラメータ経由でコールバックに渡すこと。

### 解決方針: 全レイヤー走査

`object_id`を指定された操作は、ブリッジプラグイン内の共通ヘルパーで
以下の手順でハンドルを解決する。

```
ResolveObjectById(EDIT_SECTION* edit, int64_t target_id, int layer_max) -> OBJECT_HANDLE:
    # layer_maxは呼び出し元がcall_*_section_paramの外でget_edit_infoから取得して渡す
    for layer in 0..layer_max:
        frame = 0
        loop:
            obj = edit->find_object(layer, frame)
            if obj == nullptr: break (次のレイヤーへ)
            if edit->get_object_id(obj) == target_id: return obj
            pos = edit->get_object_layer_frame(obj)
            frame = pos.end + 1  # 次のオブジェクトを検索
    return nullptr
```

- 1回のコールバック(`call_edit_section_param`/`call_read_section_param`)内で解決から
  操作までを完結させる(ハンドルを呼び出しの外に持ち出さない)。
- プロジェクト規模が大きい場合に線形探索のコストがかかるが、まずはこれで実装し、
  パフォーマンス上の問題が実際に出てから最適化(キャッシュ等)を検討する
  (早すぎる最適化を避ける)。

### 全オブジェクト一覧化にも同じ走査を使う

カテゴリA要件にある「オブジェクト一覧」的な機能も、SDKに専用APIがないため、
上記と同じ全レイヤー走査で実装する(`ResolveObjectById`の収集版)。

```
ListAllObjects(EDIT_SECTION* edit) -> vector<{object_id, layer, start, end}>:
    (ResolveObjectByIdと同じループで、一致判定の代わりに全件収集する)
```

## エラーハンドリングの共通規約

- ブリッジプラグインのレスポンスは常に `{"ok": bool, "result"?: ..., "error"?: string}`。
- `object_id`解決に失敗した場合は `{"ok": false, "error": "object not found: <id>"}` を返す。
- `call_edit_section_param`/`call_read_section_param`自体が失敗した場合
  (出力中・レンダリング中等)は `{"ok": false, "error": "edit section busy"}` を返す。
- MCPサーバー側は `res.ok === false` を `isError: true` のツール結果に変換する
  (実装済み: `mcp-server/src/index.js` の `runBridgeTool`)。

## ブリッジコマンドの命名規約

- ブリッジのJSON Linesコマンド名はSDK関数名をそのまま使う(例: `cmd: "move_object"`)
- MCPツール名は `aviutl2_<snake_case>` とする(例: `aviutl2_move_object`)
- 複数形・一覧系は `list_` 接頭辞を使う(例: `list_objects`、SDKに対応する単一関数がないため)
