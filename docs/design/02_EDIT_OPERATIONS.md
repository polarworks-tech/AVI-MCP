# カテゴリB: オブジェクト編集系 設計書

対応する要件: [REQUIREMENTS.md#カテゴリb-オブジェクト編集系](../REQUIREMENTS.md)
共通設計: [00_COMMON.md](00_COMMON.md)(`object_id`解決戦略、エラー規約)

全てのコマンドは `call_edit_section_param` 経由で実装する(Undo対象)。

## ツール一覧

| MCPツール名 | 対応SDK API | 状態 |
|---|---|---|
| `aviutl2_create_object_from_alias` | `create_object_from_alias` | ✅実装済 |
| `aviutl2_move_object` | `move_object` | ✅実装済 |
| `aviutl2_delete_object` | `delete_object` | ✅実装済 |
| `aviutl2_set_item_value` | `set_object_item_value` | ✅実装済(`set_effect_item_value`は将来) |
| `aviutl2_create_object_from_media_file` | `create_object_from_media_file` | ✅実装済 |
| `aviutl2_split_clip` | 複合操作(下記参照) | ✅実装済(カット編集) |
| `aviutl2_list_object_effects` | `get_effect_list` + `get_effect_id` + `get_effect_name` | ✅実装済 |
| `aviutl2_create_effect` | `create_effect` | ✅実装済 |
| `aviutl2_delete_effect` | `delete_effect` | ✅実装済 |
| `aviutl2_create_object` | `create_object` | 将来 |
| `aviutl2_move_effect` | `move_effect` | 将来 |
| `aviutl2_rename_object` / `aviutl2_rename_layer` | `set_object_name` / `set_layer_name` | 将来 |
| `aviutl2_set_layer_state` | `set_layer_enable` / `set_layer_lock` | 将来 |
| 区間操作一式 | `create_object_section` / `delete_object_section` / `move_object_section` | 見送り(下記参照) |

## 共通パターン: object_id指定の編集操作

カテゴリBの操作は共通して以下の形になる(`move_object`を例に):

```cpp
struct Param {
    int64_t object_id;
    int layer, frame;       // 操作固有のパラメータ
    bool success = false;
    std::string error;
};
call_edit_section_param(&param, [](void* p, EDIT_SECTION* edit) {
    auto* param = static_cast<Param*>(p);
    OBJECT_HANDLE obj = ResolveObjectById(edit, param->object_id);  // 00_COMMON.md
    if (!obj) { param->error = "object not found"; return; }
    param->success = edit->move_object(obj, param->layer, param->frame);
    if (!param->success) param->error = "move_object failed (target occupied?)";
});
```

この定型処理(ID解決 → 失敗時はエラー格納 → SDK呼び出し → 結果判定)を
ヘルパー関数化してコードの重複を避ける(実装時に `WithResolvedObject<T>(id, fn)`
のようなテンプレート/ラムダヘルパーを用意する想定)。

## 詳細設計

### aviutl2_move_object(✅実装済)

- **入力**: `object_id` (int64), `layer` (int), `frame` (int)
- **ブリッジコマンド**: `{"cmd": "move_object", "object_id": 1, "layer": 2, "frame": 30}`
- **出力(成功)**: `{"ok": true, "result": {"layer": 2, "start": 30, "end": 129}}`
  (移動後の位置を`get_object_layer_frame`で取得して返す)
- **出力(失敗)**: `{"ok": false, "error": "move_object failed (target occupied?)"}`
- **注意**: SDK仕様上、移動先に既にオブジェクトがあると失敗する。
  「空いている場所を探してから移動する」は呼び出し側(エージェント)の責務とし、
  ツール自体は単純な移動のみを提供する。

### aviutl2_delete_object(✅実装済)

- **入力**: `object_id` (int64)
- **ブリッジコマンド**: `{"cmd": "delete_object", "object_id": 1}`
- **出力**: `{"ok": true, "result": {"deleted_object_id": 1}}`
- **注意**: `delete_object`自体はSDK上戻り値`void`(失敗を通知する手段がない)。
  削除前に`ResolveObjectById`で存在確認をしてから呼ぶことで、
  「見つからないのに削除成功と返す」事態を避ける。

### aviutl2_set_item_value(✅実装済)

オブジェクトの設定項目(エフェクトのパラメータ)の値を変更する。
`set_object_item_value`は「オブジェクト直下のエフェクト名+項目名」を指定する形であり、
`set_effect_item_value`は「エフェクトハンドル+項目名」を指定する形。
まずは`object_id`から辿れる`set_object_item_value`相当のみを提供する
(`EFFECT_HANDLE`を外部にIDとして公開する設計は、エフェクト側にも`effect_id`が
あるため将来的に追加できるが、最初はオブジェクト単位のシンプルな形にする)。

- **入力**: `object_id` (int64), `effect` (string, 例: "テキスト"または"テキスト:1"), `item` (string, 例: "サイズ"), `value` (string)
- **ブリッジコマンド**: `{"cmd": "set_item_value", "object_id": 1, "effect": "テキスト", "item": "サイズ", "value": "200.00"}`
- **出力**: `{"ok": true}` / `{"ok": false, "error": "set_object_item_value failed (effect/item not found)"}`
- **注意**: `effect`/`item`名は日本語のエフェクト内部名そのもの。
  有効な名称一覧はカテゴリAの`aviutl2_list_available_effects`や
  `aviutl2_get_object_alias`(既存オブジェクトのダンプ)から得る想定。
  エージェントに対しては、まず対象オブジェクトを`get_object_alias`で読んでから
  値を書き換える、というワークフローを前提にツールの説明文で案内する。

### aviutl2_create_object_from_media_file(✅実装済)

- **入力**: `file` (string, 絶対パス), `layer` (int), `frame` (int), `length` (int, 0で自動調整)
- **ブリッジコマンド**: `{"cmd": "create_object_from_media_file", "file": "...", "layer": 0, "frame": 0, "length": 0}`
- **出力**: `create_object_from_alias`と同形式(`object_id`/`layer`/`start`/`end`)
- 当初「将来」としていたが、`aviutl2_split_clip`の実装・検証のため先行実装した。

### aviutl2_list_object_effects(✅実装済)

オブジェクトが持つエフェクトの一覧を`effect_id`付きで取得する。`effect_id`は
`object_id`と同じ考え方(SDKの`get_effect_id`が返す、位置に依存しない識別子)で、
`aviutl2_delete_effect`で対象を指定する際に使う。

- **入力**: `object_id` (int64)
- **ブリッジコマンド**: `{"cmd": "list_object_effects", "object_id": 1}`
- **出力**: `{"ok": true, "result": {"effects": [{"effect_id": 161, "name": "テキスト"}, {"effect_id": 162, "name": "標準描画"}]}}`
- **実装**: `object_id`解決後、`get_effect_list(obj, nullptr, 0)`で件数を取得→
  同じ関数を実バッファで呼び出し→各ハンドルの`get_effect_id`/`get_effect_name`を取得

### aviutl2_create_effect(✅実装済)

- **入力**: `object_id` (int64), `effect` (string, 追加するエフェクト名)
- **ブリッジコマンド**: `{"cmd": "create_effect", "object_id": 1, "effect": "ぼかし"}`
- **出力(成功)**: `{"ok": true, "result": {"effect_id": 163}}`
- **出力(失敗)**: `{"ok": false, "error": "create_effect failed (unknown effect name, or object does not accept it)"}`
- **検証結果**: 実機でテキストオブジェクトに「ぼかし」を追加し、`get_object_alias`で
  `[Object.2] effect.name=ぼかし 範囲=5 ...`として反映されることを確認済み。
  不明なエフェクト名は正しく失敗する。

### aviutl2_delete_effect(✅実装済)

- **入力**: `object_id` (int64), `effect_id` (int64)
- **ブリッジコマンド**: `{"cmd": "delete_effect", "object_id": 1, "effect_id": 163}`
- **出力**: `{"ok": true, "result": {"deleted_effect_id": 163}}`
- **実装**: `object_id`解決 → `ResolveEffectById`(`get_effect_list`+`get_effect_id`の
  走査、`object_utils.cpp`)でエフェクトハンドルを解決 → `delete_effect`
- **重要な実機検証結果**: SDKコメントには出力系エフェクトの有効/無効切り替え
  (`set_effect_enable`)が制限される旨の記載があるが、**`delete_effect`自体は
  入力・出力系エフェクト(「テキスト」「標準描画」等、素材そのもの)も削除できて
  しまう**ことを確認した。出力系を削除するとオブジェクトは何も描画されなくなるが、
  クラッシュはしない(レンダリングして確認済み)。ツールの説明文で
  「意図せず削除しないよう注意」を明記している。

### aviutl2_split_clip(✅実装済、カット編集)

既存オブジェクトを`split_frame`で前半・後半の2つのオブジェクトに分割する。
SDKには直接の分割APIが存在しないため、実機検証で確立した以下の手順で実現している。

1. 対象オブジェクトの`alias`・位置(`layer`/`start`/`end`)を取得
2. `split_frame`が`(start, end]`の範囲内か検証
3. 元のオブジェクトを`delete_object`
4. 前半: 元の`alias`(後述のメタデータ除去後)で`create_object_from_alias(layer, start, split_frame - start)`
5. 後半: 動画/音声ファイルなら`再生位置`をオフセットした`alias`で
   `create_object_from_alias(layer, split_frame, end - split_frame + 1)`

**実装時に発見した2つの落とし穴**:

- **`frame=開始,終了`メタデータの除去が必須**: `get_object_alias`が返すテキストには
  `[Object]`の直後に`frame=0,80`のような絶対位置メタデータが含まれる。これを
  そのまま`create_object_from_alias`に渡すと、引数のlayer/frame/lengthと内容が
  矛盾し**作成に失敗する**(前半は成功するが後半だけ失敗する、という分かりにくい
  症状で顕在化した)。再利用前に`StripFrameMetadata`で必ず取り除く。
- **動画/音声の「素材内の再生開始位置」のオフセット**: メディアオブジェクトの
  alias内に`再生位置=開始秒,終了秒,モード,フラグ`という項目がある
  (`effect.name=動画ファイル`/`音声ファイル`のみ)。後半オブジェクトは
  `(split_frame - start) / fps`秒だけこの開始秒をずらす必要がある。ずらさないと
  後半も素材の先頭から再生され、見た目上「分割」にならない。
  `OffsetPlaybackPosition`で実装。実機で前半(黒画面)・後半(オフセット後の絵)が
  異なるフレームになることをレンダリング画像で確認済み。
- テキスト等、メディアを持たないオブジェクトは`再生位置`を持たないため、
  単純に同じ内容のまま前後に分けるだけでよい(`effect.name`で判定)。

**安全策**: 手順4または5が失敗した場合(作成できた方があれば削除した上で)、
元の`alias`・位置で**ロールバックを試みる**。ロールバックも失敗した場合は、
エラーメッセージに元の`alias`全文を含めて返す(手動復元できるように)。

- **入力**: `object_id` (int64), `split_frame` (int, タイムライン上の絶対フレーム)
- **ブリッジコマンド**: `{"cmd": "split_clip", "object_id": 1, "split_frame": 40}`
- **出力(成功)**:
  ```json
  {"ok": true, "result": {
    "is_media": true,
    "front": {"object_id": 2, "layer": 0, "start": 0, "end": 39},
    "back": {"object_id": 3, "layer": 0, "start": 40, "end": 80}
  }}
  ```
- **出力(失敗、範囲外)**: `{"ok": false, "error": "split_frame must satisfy start < split_frame <= end (start=0, end=80)"}`
- **出力(失敗、ロールバック成功)**: `{"ok": false, "error": "split_clip failed; original object was restored unchanged"}`

### 将来実装分(概要のみ)

| ツール | 入力 | 備考 |
|---|---|---|
| `aviutl2_create_object` | `effect`, `layer`, `frame`, `length` | エイリアス不要の簡易オブジェクト作成 |
| `aviutl2_move_effect` | `object_id`, `effect_id`, `index` | エフェクトの並び替え |
| `aviutl2_rename_object` / `aviutl2_rename_layer` | `object_id`/`layer`, `name` | 名称変更。破壊性は低いので早めに実装してよい |
| `aviutl2_set_layer_state` | `layer`, `enable`/`lock` | レイヤー単位の表示・ロック切替 |

### 区間(中間点)操作は見送り

`create_object_section`/`delete_object_section`/`move_object_section`は
AviUtl(1)由来の「同一オブジェクト内のパラメータ補間区切り」機能であり、
当初想定していた「クリップを物理的に分割する」用途には使えないことが分かった
(カット編集は`aviutl2_split_clip`で実現済み)。この3APIを使う具体的な需要が
出るまで実装は見送る。
