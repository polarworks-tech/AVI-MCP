# カテゴリC: カーソル・表示状態の操作 設計書

対応する要件: [REQUIREMENTS.md#カテゴリc-カーソル表示状態の操作](../REQUIREMENTS.md)

優先度は低いため、骨格のみ定義する。着手時に本書を詳細化する。

## ツール一覧(案)

| MCPツール名 | 対応SDK API | 状態 |
|---|---|---|
| `aviutl2_set_cursor` | `set_cursor_layer_frame` | 未着手 |
| `aviutl2_set_display_position` | `set_display_layer_frame` | 未着手 |
| `aviutl2_set_select_range` | `set_select_range` | 未着手 |
| `aviutl2_focus_object` | `set_focus_object` | 未着手 |

## 設計メモ

- いずれも`call_edit_section_param`経由(UI状態の変更もプロジェクト編集の一種として
  扱われるため)。
- 実際の用途は「エージェントが操作した結果をユーザーに見せる」(カーソルを合わせる、
  選択状態にする)こと。カテゴリB(オブジェクト編集)の操作とセットで提供すると
  効果的(例: オブジェクト作成後に自動でそこへカーソルを合わせる)。
- 単独でのツール化の優先度は低い。カテゴリB/Dの実装が進んだ段階で、
  「ついでに付ける」形で検討するのが現実的。
