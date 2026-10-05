# 要件定義

MCPツールとして公開する機能の要件。SDK(`plugin2.h`)の`EDIT_SECTION`/`EDIT_HANDLE` API全量
(約140関数、`register_*`系のプラグイン初期化用フックを除く)をカテゴリ分けし、
カテゴリごとに「何を満たすべきか」を定義する。実装の進め方・優先順位の考え方は
[DESIGN_POLICY.md](DESIGN_POLICY.md) を参照。

## 目的

LLMエージェントがMCP経由でAviUtl2の編集内容を読み取り・変更できるようにする。

## カテゴリA: 読み取り系

現在の編集状態・オブジェクト・エフェクトの情報を取得する機能群。

| 対応API | 要件 |
|---|---|
| `get_edit_info` | 解像度・フレームレート・サンプリングレート・カーソル位置・シーンIDを取得できること |
| `find_object` | 指定レイヤー・フレーム以降の最初のオブジェクトを検索でき、`object_id`を返すこと |
| `get_selected_object` / `get_selected_object_num` | 現在選択中のオブジェクト一覧を取得できること |
| `get_object_alias` | オブジェクトの内容をエイリアス形式のテキストとして取得できること(編集内容の読み出し) |
| `get_object_layer_frame` | オブジェクトの現在位置を取得できること |
| `get_object_name` / `get_layer_name` / `get_scene_name` | 各種名称を取得できること |
| `get_object_item_value` / `get_effect_item_value` | 個別設定項目の値を取得できること |
| `get_object_track_value` / `get_object_track_info` | トラックバーの値・情報を取得できること |
| `get_effect_list` / `find_effect` / `get_effect_name` | オブジェクトが持つエフェクトの一覧・検索ができること |
| `get_object_id` / `get_effect_id` | 位置に依存しない不変の識別子を取得できること |
| `enum_effect_name` / `enum_module_info` / `enum_effect_item` | 利用可能なエフェクト/モジュールの一覧を取得できること |
| `enum_scene_name` | シーン一覧を取得できること |
| `get_mark_frame_list` / `get_mark_frame_memo` | マークされたフレームの一覧・メモを取得できること |
| `is_support_media_file` / `get_media_info` | メディアファイルの対応可否・情報を取得できること |

**制約**: すべて副作用を持たないこと(`call_read_section`系での実装を必須とする)。

## カテゴリB: オブジェクト編集系

既存オブジェクトを変更する。AviUtl2の通常のUndo(Ctrl+Z)の対象になること。

| 対応API | 要件 |
|---|---|
| `create_object_from_alias` | エイリアスからオブジェクトを作成でき、`object_id`を返すこと |
| `create_object_from_media_file` | メディアファイルからオブジェクトを作成できること |
| `create_object` | エフェクト名指定のみでオブジェクトを作成できること |
| `move_object` | `object_id`を指定してオブジェクトを別のレイヤー・フレームへ移動できること |
| `delete_object` | `object_id`を指定してオブジェクトを削除できること |
| `set_object_item_value` / `set_effect_item_value` | `object_id`を指定して設定項目の値を変更できること |
| `create_effect` / `delete_effect` / `move_effect` | エフェクトの追加・削除・並び替えができること |
| `set_object_name` / `set_layer_name` | 名称を変更できること |
| `create_object_section` / `delete_object_section` / `move_object_section` | 中間点(区間)を操作できること |
| `set_layer_enable` / `set_layer_lock` | レイヤーの表示・ロック状態を切り替えられること |

**制約**:
- すべて`object_id`を主キーとして対象を指定すること(レイヤー・フレーム位置の直接指定に依存しない)
- 存在しない`object_id`を指定した場合は明確なエラーを返すこと

## カテゴリC: カーソル・表示状態の操作

編集内容ではなくUIの状態を変える。

| 対応API | 要件 |
|---|---|
| `set_cursor_layer_frame` | 編集カーソルを移動できること |
| `set_display_layer_frame` | 表示開始位置を変更できること |
| `set_select_range` | フレーム範囲選択ができること |
| `set_focus_object` | オブジェクトの選択状態を変更できること |

## カテゴリD: レンダリング系

画像・音声データを取得する。

| 対応API | 要件 |
|---|---|
| `rendering_scene_video` | 指定フレームのシーン全体を画像として取得できること |
| `rendering_object_video` | 指定オブジェクト単体を画像として取得できること |
| `rendering_scene_audio` / `rendering_object_audio` | 音声データを取得できること |
| `wait_rendering_task` | レンダリング完了を待機できること |

**制約**: 非同期コールバックAPIを同期的なMCPツール呼び出しに変換し、
画像データをMCPクライアントが扱える形式(Base64等)で返すこと。

## カテゴリE: シーン・プロジェクト構造操作

| 対応API | 要件 |
|---|---|
| `create_scene` / `select_scene` | シーンの作成・切り替えができること |
| `set_scene_size` / `set_scene_frame_rate` / `set_scene_sample_rate` | シーン設定を変更できること |

**制約**: AviUtl2本体がUndoに対応していないことをツールの説明に明記すること。

## カテゴリF: ファイルI/O・プロジェクト保存系

| 対応API | 要件 |
|---|---|
| `open_project_file` | プロジェクトファイルを開けること |
| `save_project_file` | プロジェクトファイルへ保存できること |
| `create_project` | 新規プロジェクトを作成できること |
| `output_file` | 出力プラグインでファイル書き出しができること |

**制約(最低要件、着手時に個別設計で再検討)**:
- 既定で上書き保存不可。別名保存または常にタイムスタンプ付きの新規ファイルにすること
- 実行前に現在の編集内容のパス・保存先パスを確認できる手段を提供すること
- `output_file`は長時間処理のため、進捗確認・キャンセル手段を提供すること
- ツール自体を既定で無効化し、明示的なオプトインを要求すること

## スコープ外

| 対象 | 理由 |
|---|---|
| `restart_host_app` | アプリケーション全体の再起動。破壊的すぎる |
| `register_*`系一式 | プラグイン初期化時のフック登録。実行時にMCP経由で呼ぶ対象ではない |
| `get_palette_info` / `set_palette_info` / `get_font` / `get_grid_bpm_list`等 | UI設定系。需要が出るまで対象外 |
