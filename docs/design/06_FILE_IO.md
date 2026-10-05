# カテゴリF: ファイルI/O・プロジェクト保存系 設計書

対応する要件: [REQUIREMENTS.md#カテゴリf-ファイルioプロジェクト保存系](../REQUIREMENTS.md)
共通設計: [00_COMMON.md](00_COMMON.md)(エラー規約)

## 設計方針(確定事項)

1. **上書き保存は既定で禁止**。保存先に既にファイルが存在する場合は常にエラーとする(オプトアウト不可)
2. **実行フローは2段階**(`confirm`フラグ方式、下記参照)
3. **ツールは他カテゴリと同様、最初から有効**(オプトイン機構は設けない)

## confirmフラグ方式

別々のツールに分けず、1つのツールに`confirm`(bool, 既定false)を持たせる。

- `confirm: false`(既定): 実際には実行せず、パスの検証結果(存在確認、保存先の妥当性)と
  「このまま`confirm: true`で呼べば実行される」旨を返す。**ドライラン**。
- `confirm: true`: 実際に操作を実行する。このときも上書き禁止チェックは必ず行う
  (ドライラン後に別プロセスがファイルを作成した場合等に備え、実行直前に再チェックする)。

この方式を採用した理由:
- MCPツールの数を増やさずに済む
- 「まずconfirm:falseで内容を確認させ、エージェントがユーザーに提示してから、
  ユーザーの了承を得てconfirm:trueで呼び直す」という自然な会話フローになる
- ドライランと実実行で同じレスポンス形式を使えるため実装が単純になる

## output_fileの論点解決

当初2つの論点(進捗/キャンセル、既存プラグインとの干渉)を未解決としていたが、
以下のSDK APIで解決した。

1. **進捗確認**: `EDIT_HANDLE::get_edit_state()`が`EDIT_STATE_EDIT`/`PLAY`/`SAVE`を返す。
   出力中は`SAVE`になるため、エージェントが`aviutl2_get_output_status`をポーリングし、
   `save`以外に変わったら完了とみなせる(詳細な%進捗は得られないが、完了検知は可能)。
2. **既存プラグインとの干渉**: `EDIT_HANDLE::enum_module_info()`で
   `MODULE_INFO::TYPE_PLUGIN_OUTPUT`のものだけ列挙すると、インストール済みの
   出力プラグイン名(`output_file`の`output_plugin`引数にそのまま渡せる文字列)を
   取得できる。開発環境で実際に「MP4 Exporter (by えすご/Esugo)」
   「拡張 x264 出力(GUI) Ex」の2つが取得できることを確認済み。
   当初はこれを`aviutl2_list_output_plugins`という独立ツールにしていたが、
   [DESIGN_POLICY.md](../DESIGN_POLICY.md)の「ツール設計の原則」(補助的な情報取得は
   可能なら本体操作に畳み込む)に基づき、`output_file`内部の検証処理に統合した。
   `output_plugin`に不明な名前を渡すと、ドライラン・実行のいずれでも
   エラーメッセージに有効なプラグイン名一覧が返る。
3. **キャンセル機能**: SDKに出力を中断するAPIが見当たらないため、**提供しない**
   (スコープ外とする)。

## ツール一覧

| MCPツール名 | 対応SDK API | 状態 |
|---|---|---|
| `aviutl2_save_project_file` | `save_project_file` | ✅実装済 |
| `aviutl2_open_project_file` | `open_project_file` | ✅実装済 |
| `aviutl2_create_project` | `create_project` | ✅実装済 |
| `aviutl2_get_output_status` | `get_edit_state` | ✅実装済 |
| `aviutl2_output_file` | `output_file` + `enum_module_info`(検証用、内部利用のみ) | ✅実装済 |

## 詳細設計

いずれも`EDIT_HANDLE`の直接メンバ呼び出しであり、`call_edit_section_param`等の
コールバックは不要(SDKコメントに「参照ロック、編集ロック状態では利用出来ません」と
あるため、むしろコールバック内から呼んではならない)。メインスレッド
(`BridgeWndProc`経由)から直接呼ぶ。

### aviutl2_save_project_file(✅実装済)

- **入力**: `path` (string, 絶対パス), `confirm` (bool, 既定false)
- **ブリッジコマンド**: `{"cmd": "save_project_file", "path": "D:\\...\\out.aup2", "confirm": false}`
- **検証内容**(confirmの値によらず毎回実施):
  - `path`が空でないこと
  - 保存先に既にファイルが存在しないこと(存在すれば常にエラー)
  - 親ディレクトリが存在すること
- **出力(confirm:false, 検証OK)**: `{"ok": true, "result": {"confirmed": false, "would_save_to": "..."}}`
- **出力(confirm:true, 成功)**: `{"ok": true, "result": {"confirmed": true, "saved_to": "..."}}`
- **出力(失敗)**: `{"ok": false, "error": "file already exists (overwrite is not allowed): ..."}`

### aviutl2_open_project_file(✅実装済)

- **入力**: `path` (string), `confirm` (bool, 既定false)
- **検証内容**: `path`が存在すること
- **出力(confirm:false)**: `{"ok": true, "result": {"confirmed": false, "would_open": "...", "warning": "現在の編集内容は保存されていない場合失われます"}}`
- **出力(confirm:true)**: `open_project_file(path, show_confirm=false)`を呼ぶ。
  `show_confirm`はAviUtl2自身の確認ダイアログの表示有無。MCP層で既に確認済みのため`false`を渡し、
  操作不能なネイティブダイアログが表示される事態を避ける。

### aviutl2_create_project(✅実装済)

- **入力**: `width`, `height`, `rate`, `scale`, `sample_rate`(いずれも省略可、省略時はAviUtl2の
  一般的な初期値を既定とする), `confirm` (bool, 既定false)
- **検証内容**: 特になし(新規作成なのでパスの衝突等は発生しない)
- **出力(confirm:false)**: `{"ok": true, "result": {"confirmed": false, "would_create": {...}, "warning": "現在の編集内容は保存されていない場合失われます"}}`
- **出力(confirm:true)**: `create_project(..., show_confirm=false)`を呼ぶ(理由は上記と同様)。

### aviutl2_get_output_status(✅実装済)

- **入力**: なし
- **出力**: `{"ok": true, "result": {"state": "edit" | "play" | "save"}}`
- `aviutl2_output_file`実行後、`state`が`save`から変わるまでポーリングすることで
  エンコード完了を検知する。

### aviutl2_output_file(✅実装済)

- **入力**: `path` (string), `output_plugin` (string, 有効な名前は後述のエラー応答で確認できる),
  `confirm` (bool, 既定false)
- **検証内容**(confirmの値によらず毎回実施):
  - `path`/`output_plugin`が空でないこと
  - 出力先に既にファイルが存在しないこと(上書き禁止)
  - 親ディレクトリが存在すること
  - `output_plugin`が`enum_module_info`(`TYPE_PLUGIN_OUTPUT`)で列挙される
    インストール済みプラグイン名のいずれかと一致すること
- **出力(confirm:false, 検証OK)**: `{"ok": true, "result": {"confirmed": false, "would_output_to": "...", "output_plugin": "..."}}`
- **出力(confirm:true, 成功)**: `{"ok": true, "result": {"confirmed": true, "started": true, "output_to": "...", "note": "..."}}`
  (`output_file`自体はエンコード開始のみ行い完了を待たずに返るため、`started`は
  「開始できたか」を示す。完了は`aviutl2_get_output_status`で別途確認する)
- **出力(失敗、ファイル存在時)**: `{"ok": false, "error": "file already exists (overwrite is not allowed): ..."}`
- **出力(失敗、プラグイン名不正時)**: `{"ok": false, "error": "unknown output_plugin: \"...\". available plugins: [\"MP4 Exporter (by えすご/Esugo)\", \"拡張 x264 出力(GUI) Ex\"]"}`
  のように、有効なプラグイン名一覧を含めて返す(ドライラン・実行どちらでも、`enum_module_info`の
  結果と一致しない場合はこの形でエラーになる)。
- **検証結果**: 実際にMP4 Exporterで10フレームのテキストオブジェクトを出力し、
  `get_output_status`のポーリングで`save`→`edit`の遷移を検出できることを確認した。
  上書き禁止(同一パスへの再出力が`confirm:true`でも失敗すること)、
  不正なプラグイン名を渡した際に実機のプラグイン名一覧(日本語含む)が
  正しく返ることも確認済み。
- **制限**: キャンセル機能はない。
