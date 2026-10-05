# AviUtl2 SDK 調査メモ

調査に使ったソース:
- `C:\Program Files\AviUtl2\aviutl2.txt` (本体の同梱ドキュメント, v2.1.3)
- https://github.com/aviutl2/aviutl2_sdk_mirror (非公式ミラー, MIT License, リポジトリ直下の`sdk_mirror/`にcloneして使用)
- https://zenn.dev/mnonamer/articles/aviutl2-plugins-howto (プラグイン作成記事)

## 重要な前提

- **AviUtl2本体はクローズドソース**(フリーソフトだがソース非公開、作者「ＫＥＮくん」)
- **Plugin SDKはMIT Licenseで公式公開**されている。ヘッダファイルのみ(実装はDLL内に隠蔽)なので、
  挙動の細部(プロジェクトファイルの内部フォーマット等)はリバースが必要だが、
  「外部から操作する」ための正式なフックは用意されている。
- 「OSS」と呼べるのはSDK(ヘッダ+サンプル)の部分。本体は対象外。

## プラグインの種類と拡張子

| 種類 | 拡張子 | 用途 |
|---|---|---|
| 入力プラグイン | `.aui2` | 他ファイル形式の読み込み |
| 出力プラグイン | `.auo2` | 他ファイル形式への出力 |
| フィルタプラグイン | `.auf2` | メディアオブジェクト/フィルタ効果の追加 |
| スクリプトモジュール | `.mod2` | Luaスクリプトから呼べる関数を追加 |
| **汎用プラグイン** | **`.aux2`** | **独自ウィンドウ、インポート/エクスポートメニュー、プロジェクト編集操作の追加** ← MCPブリッジはこれ |

配置先:
- グローバル: `C:\ProgramData\aviutl2\Plugin\`(1階層下も対象)
- ポータブル: 実行ファイルと同階層の `data\Plugin\`

## 汎用プラグインのエントリポイント (DLLエクスポート関数)

```cpp
// 必須
EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host);

// 任意
EXTERN_C __declspec(dllexport) DWORD RequiredVersion();               // 必要バージョン
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version);  // 初期化
EXTERN_C __declspec(dllexport) void UninitializePlugin();             // 終了処理
EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE*);    // ログ
EXTERN_C __declspec(dllexport) void InitializeConfig(CONFIG_HANDLE*); // 設定
EXTERN_C __declspec(dllexport) void InitializeCache(CACHE_HANDLE*);   // キャッシュ
EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable();
```

## MCPブリッジとして使う核心API (`plugin2.h`)

1. `RegisterPlugin(HOST_APP_TABLE* host)` 内で `host->create_edit_handle()` を呼び、
   グローバルに保持した `EDIT_HANDLE*` を他スレッドからも使えるようにする。
2. 編集操作は必ず `edit_handle->call_edit_section_param(param, callback)` 経由で
   メインスレッド上の `EDIT_SECTION*` を介して行う(排他制御されている)。
   参照のみなら `call_read_section` も使える(こちらは更新系関数が使えない制約あり)。
3. `EDIT_SECTION` でできること(抜粋):
   - `create_object_from_alias` / `create_object_from_media_file` / `create_object` — オブジェクト作成
   - `move_object` / `delete_object` / `set_object_item_value` / `get_object_item_value` — 編集
   - `create_effect` / `delete_effect` / `set_effect_item_value` / `get_effect_item_value` — エフェクト操作
   - `get_selected_object` / `get_focus_object` / `set_focus_object` — 選択状態
   - `set_scene_size` / `set_scene_frame_rate` / `create_scene` / `select_scene` — シーン操作
4. `EDIT_HANDLE` でできること(抜粋):
   - `get_edit_info` — 現在の編集状態(解像度、フレームレート、カーソル位置等)取得
   - `open_project_file` / `save_project_file` / `create_project` — プロジェクトの開く/保存/新規作成
   - `output_file` — 出力プラグインでファイル出力開始
   - `rendering_scene_video` / `rendering_object_video` — 指定フレームの画像を非同期レンダリングしてコールバックで受け取る(≒スクリーンショット取得に相当)
   - `enum_effect_name` / `enum_effect_item` / `enum_module_info` — 利用可能なエフェクト/モジュール一覧の列挙(MCPのツール定義を動的生成する際に有用)

これだけでMCPのツールとして「オブジェクト作成・移動・削除」「エフェクト値の読み書き」
「プロジェクトを開く/保存する」「指定フレームをレンダリングして画像を取得する」が一通り実現できる。

## オブジェクトエイリアス(`.object`)フォーマット

`create_object_from_alias` に渡す文字列(UTF-8)はINI的な独自形式。
`WindowClient.cpp` のサンプルより:

```ini
[Object]
[Object.0]
effect.name=テキスト
サイズ=150.00
文字色=3bcb48
テキスト=さんぷる
[Object.1]
effect.name=標準描画
[Object.2]
effect.name=縁取り
サイズ=10
縁色=ffffff
```

- `[Object.N]` が1つのエフェクト(入力/フィルタ/出力)に対応し、`effect.name` でエフェクト種別名(日本語)を指定
- それ以外のキーはそのエフェクトの設定項目名(日本語/UTF-8)
- `get_object_alias(object)` で既存オブジェクトからこの形式の文字列を逆に取得できる
  → **既存プロジェクトのオブジェクトを読んでテンプレート化するのが現実的な調査方法**

## ファイルパス変数

プラグインやエイリアス/プリセットのファイルパスで使える変数展開:
`%AppInstall%` `%AppData%` `%Plugin%` `%Script%` `%Default%` `%Alias%` `%Preset%`
`%Figure%` `%Transition%` `%Font%` `%Language%` `%Backup%` `%Log%` `%Package%` `%Batch%`

## プロジェクトファイル

拡張子は `.aup2`。内部フォーマットは今回未解析(本体のソース非公開、SDKにも直接の記載なし)。
`PROJECT_FILE` 構造体(`plugin2.h`)経由でプラグイン独自のキー値をプロジェクトに保存/取得できるが、
これはプラグイン用の保存領域であり、プロジェクト本体のオブジェクト配置等はこの構造体では直接読めない。
→ オブジェクト一覧の取得は `EDIT_SECTION::find_object` / `get_object_alias` を使う(ファイルを直接パースしない)。

## 未調査・今後の課題

- `.aup2` のバイナリ/テキスト構造(直接パースする必要が出た場合)
- `filter2.h` / `input2.h` / `output2.h` (今回は汎用プラグインのみで十分なため未読)
- 複数クライアントからの同時アクセス、長時間接続時の安定性

## 実装して分かった重要な落とし穴(ブリッジプラグイン実装時に遭遇)

### 1. EDIT_HANDLE/EDIT_SECTIONの操作はメインスレッドから呼ぶ必要がある

TCP受信スレッド(ワーカースレッド)から直接 `call_edit_section_param` / `call_read_section_param` を
呼び出すと **AviUtl2本体がクラッシュする**(アクセス違反, 0xC0000005)。
`aviutl2.ini`配下のログに `params->callback() structured exception in
Plugin::CommonPluginService::callEditSection()` という形で記録される。

ドキュメントの「コールバック関数はメインスレッドから呼ばれます」は、
“内部で自動的にメインスレッドにディスパッチしてくれる”という意味ではなく、
**呼び出し元自体がメインスレッドであることが前提**という意味だった。
(SDKサンプルのWindowClient.cppも、ウィンドウプロシージャ(=メインスレッド)内からのみ呼んでいる)

**対策**: プラグイン内に非表示ウィンドウ(`WS_POPUP`, `CreateWindowExW`)を作成し、
ワーカースレッドからは`SendMessage`でそのウィンドウにカスタムメッセージを送って、
ウィンドウプロシージャ(メインスレッド上で実行される)内でSDK呼び出しを行う。
`SendMessage`は呼び出し元をブロックしメインスレッドのメッセージループで処理されるため、
同期的に結果を受け取れる。

### 2. SDKヘッダのバージョンと本体のバージョンが食い違うとクラッシュする

`sdk_mirror`はGitHub Actionsで自動追従しているため**常に最新のSDK**だが、
手元のAviUtl2本体は過去にインストールしたバージョンで固定されている。

発生当時の本体バージョンは **2.1.3**(2026/8/2)で、`EDIT_SECTION::get_object_id()` /
`get_effect_id()`(CHANGELOG記載: 2026/9/19にSDKへ追加)を呼び出したところ、
関数自体は実行されず未定義领域にジャンプしてクラッシュした
(`create_object_from_alias`自体は成功し有効なハンドルを返していたので、
原因の特定には「どのAPI呼び出しの後でクラッシュしたか」を1行ずつログで確認するのが有効だった)。

**対策・教訓**:
- 新しめの関数(CHANGELOGで追加時期を確認できる)を使う前に、本体のバージョンとの対応を疑う
- 可能ならまず`RequiredVersion()`で明示的にバージョンチェックする(今回は未実装のまま)
- 機能が足りる限り、より古くからあるAPI(例: `get_object_layer_frame`でオブジェクトを識別する等)
  で代替する方が互換性が高い
- **本体を最新版(2.1.11a, 2026/9/30)に更新したところ、`get_object_id`/`get_effect_id`は
  クラッシュせず正常に動作することを確認した**。新しいAPIを安定して使いたい場合は、
  代替APIで回避するより本体を更新する方が結局シンプル。
  現在のブリッジプラグインは`get_object_id`を正式に採用し、`find_object` /
  `create_object_from_alias`の結果に`object_id`(レイヤー・フレーム位置が変わっても
  不変の識別子)を含めている。
- 本体更新は `AviUtl2_v2.1.x_setup.exe -F -S -I "C:\Program Files\AviUtl2"` で
  既存のインストール先に上書きすれば、`ProgramData\aviutl2`配下の設定やプラグイン配置は
  保持されたまま更新できる(確認済み)。管理者権限が必要なため、
  コマンドライン実行では昇格(UAC)に失敗することがあり、その場合はインストーラを
  手動でダブルクリックして進めるのが確実だった。

### 3. `call_read_section`系コールバック内で`EDIT_SECTION::info`にアクセスするとクラッシュする

`get_object_alias`(`object_id`解決のための全レイヤー走査、[00_COMMON.md](design/00_COMMON.md)参照)
実装時に、`call_read_section_param`のコールバック内で`edit->info->layer_max`を
読んだところクラッシュした(アクセス違反 0xC0000005、ログには
`funcProcReadSection() structured exception in ...call_read_section_param()`、
クラッシュ箇所は自プラグイン内のコードアドレスと記録された)。

**原因**: `plugin2.h`の`EDIT_SECTION::info`のコメントに
「編集情報 (call_read_section利用不可)」と明記されている通り、このメンバは
`call_read_section`/`call_read_section_param`のコールバック内では無効だった。
`call_edit_section_param`では使えていた(`create_object_from_alias`等)ため、
「前に動いたから今回も大丈夫」と思い込んでコメントの注記を見落とした。

**対策**: `layer_max`等の編集情報が必要な読み取り処理は、
`call_read_section_param`を呼ぶ**前**に`EDIT_HANDLE::get_edit_info`で取得し、
値をパラメータ経由でコールバックに渡す。

**教訓**: SDKのコメントにある `(call_read_section利用不可)` のような注記は、
関数だけでなくメンバ変数にも付いている。使う前に毎回確認する。

### 4. デバッグ手法

- AviUtl2の`LOG_HANDLE`は `error()` レベルのみ `ProgramData\aviutl2\Log\*.log` に残り、
  `info()` 以下は恐らくUI上のログ表示のみでファイルには残らない
- 詳細なトレースが必要な場合は独自に直接ファイル書き込みするデバッグログを仕込むのが手堅い
  (原因特定後は削除する)
- クラッシュ時は `Get-WinEvent -FilterHashtable @{LogName='Application';
  ProviderName='Application Error'}` でWindowsのクラッシュ記録(例外コード、オフセット)を確認できる
- プラグインの動作確認は `Get-Process`でウィンドウハンドルを取得し`CopyFromScreen`で
  スクリーンショットを撮ると、実際にプレビューに反映されたかを視覚的に確認できる
