# AVI-MCP

AviUtl2 (AviUtl ExEdit2) を [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) 経由で操作するためのツールです。
LLMエージェントから、オブジェクトの作成・編集・カット分割・エフェクトの追加削除・レンダリング画像による確認・プロジェクトの保存や書き出しを行えます。

> **非公式プロジェクトです。** AviUtl2の作者(ＫＥＮくん)とは無関係です。AviUtl2本体は本リポジトリに含まれません。

## 特徴

- 19個のMCPツール(下記一覧)
- `object_id`による、位置に依存しない対象指定
- カット編集(`aviutl2_split_clip`)
- 編集結果をPNG画像で取得して確認できる(`aviutl2_render_scene_frame`)
- ファイルを扱う操作は上書きを常に拒否し、`confirm`による2段階フローで実行する

## 構成

```
LLMエージェント / MCPクライアント
        │  MCP (stdio)
        ▼
mcp-server (Node.js)            MCPツールの公開
        │  TCP + JSON Lines (127.0.0.1:23910 のみ)
        ▼
bridge-plugin (C++, .aux2)      AviUtl2内で動作する汎用プラグイン
        │  AviUtl2 Plugin SDK
        ▼
AviUtl2 本体
```

詳細は [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) を参照してください。

## 動作環境

- Windows 11 x64(開発・動作確認環境。Windows 10での動作は未確認)
- AviUtl2 **v2.1.11a**(動作確認バージョン。比較的新しいSDKのAPIを使用しているため、古いバージョンでは動作しない場合があります)

## ビルド済み版を使う場合

[Releases](https://github.com/polarworks-tech/AVI-MCP/releases) から `AVI-MCP-v*-win64.zip` をダウンロードし、同梱の `README.txt` の手順に従ってください(Node.js 20以上が必要です)。

zipにはSHA256ハッシュを併記しています。ダウンロード後に、値が一致することを確認してください(ファイル名は取得したものに合わせます)。

```powershell
Get-FileHash .\AVI-MCP-v0.1.0-win64.zip -Algorithm SHA256
```

> バイナリには署名がなく、Windowsの警告が表示される場合があります。出所に不安がある場合は、次の手順でソースからビルドできます。

## セットアップ(ソースからビルド)

### 前提

- Visual Studio 2022 Build Tools(C++ワークロード)と CMake 3.20以上([docs/BUILD_SETUP.md](docs/BUILD_SETUP.md))
- Node.js 20以上
- AviUtl2 本体(配布サイトから各自で入手: <https://spring-fragrance.mints.ne.jp/aviutl/>)

### 1. AviUtl2 Plugin SDKの取得

このリポジトリをcloneしたディレクトリで、SDK(非公式ミラー、MIT License)を `sdk_mirror/` として取得します。

```bash
git clone https://github.com/aviutl2/aviutl2_sdk_mirror.git sdk_mirror
```

### 2. ブリッジプラグインのビルドと配置

「Developer PowerShell for VS 2022」で実行します。

```powershell
cd bridge-plugin
cmake -B build -S . -A x64
cmake --build build --config Release
Copy-Item build\Release\AviUtl2McpBridge.aux2 "$env:ProgramData\aviutl2\Plugin\" -Force
```

AviUtl2を終了した状態で配置し、起動し直すとプラグインが読み込まれ、`127.0.0.1:23910` で待ち受けを開始します
(通常のインストールでの配置先。ポータブル運用など環境によってプラグインフォルダは異なります)。

### 3. MCPサーバーの準備

```bash
cd mcp-server
npm install
```

### 4. MCPクライアントへの登録

**Claude Code**: リポジトリ直下の `.mcp.json.example` を `.mcp.json` にコピーし、`cwd` を `mcp-server` ディレクトリの絶対パスに書き換えます。

**その他のMCPクライアント**: `node <絶対パス>/mcp-server/src/index.js` をstdioサーバーとして登録します。

### 5. 動作確認

AviUtl2を起動した状態で、**空のプロジェクト**に対して実行します(検証用オブジェクトを1つ作成し、終了時に削除します)。

```bash
cd mcp-server
node test/manual_client_test.js
```

## MCPツール一覧

| ツール | 内容 |
|---|---|
| `aviutl2_ping` | ブリッジプラグインへの接続確認 |
| `aviutl2_get_edit_info` | プロジェクトの解像度・フレームレート・カーソル位置等を取得 |
| `aviutl2_create_object_from_alias` | エイリアス(.object形式)のテキストからオブジェクトを作成(`object_id`も返す) |
| `aviutl2_create_object_from_media_file` | 動画・画像・音声ファイルからオブジェクトを作成 |
| `aviutl2_get_object_alias` | `object_id`を指定して既存オブジェクトの内容をエイリアス形式で読み取る |
| `aviutl2_query_objects` | オブジェクト検索(`scope`: all / selected / at_position) |
| `aviutl2_move_object` | オブジェクトを別のレイヤー・フレームへ移動 |
| `aviutl2_delete_object` | オブジェクトを削除 |
| `aviutl2_set_item_value` | エフェクトの設定項目の値を変更 |
| `aviutl2_split_clip` | オブジェクトを指定フレームで前半・後半に分割(カット編集) |
| `aviutl2_list_object_effects` | オブジェクトが持つエフェクトを`effect_id`付きで一覧取得 |
| `aviutl2_create_effect` | オブジェクトにエフェクトを追加 |
| `aviutl2_delete_effect` | `effect_id`を指定してエフェクトを削除 |
| `aviutl2_render_scene_frame` | 指定フレームのシーン全体をPNG画像でレンダリング・取得 |
| `aviutl2_save_project_file` | プロジェクトを保存(`confirm`による2段階フロー、上書き禁止) |
| `aviutl2_open_project_file` | プロジェクトを開く(`confirm`による2段階フロー) |
| `aviutl2_create_project` | 新規プロジェクトを作成(`confirm`による2段階フロー) |
| `aviutl2_get_output_status` | 編集/再生/出力中のどの状態かを取得(出力完了の確認に使用) |
| `aviutl2_output_file` | 出力プラグインでファイルを書き出し(`confirm`による2段階フロー、上書き禁止) |

## 注意事項

### データの保護

- **無保証です**(MIT License)。作業中のプロジェクトは、事前に保存・バックアップしてください。
- 編集操作はSDKの仕様上、AviUtl2のUndoに登録されるとされていますが、全操作での実機検証は行っていません。
- `aviutl2_delete_object`・`aviutl2_delete_effect`・`aviutl2_split_clip` などはプロジェクトを直接変更します。
- 保存・書き出し系は、既存ファイルへの上書きを常に拒否します。

### セキュリティ

- ブリッジは `127.0.0.1:23910` でのみ待ち受け、外部ネットワークには公開されません。
- **認証はありません。** 同じPC上のプロセスは誰でも接続でき、プロジェクトの編集と、任意の絶対パスへの新規ファイル作成(上書きは拒否)が可能です。共有PCや、信頼できないソフトウェアが動作する環境では使用しないでください。
- LLMに読ませる素材やドキュメントに悪意ある指示が含まれていると、エージェントがツールを意図しない形で使う可能性があります(プロンプトインジェクション)。信頼できない入力を扱う場合は、MCPクライアント側のツール実行の承認機能を有効にしてください。
- ポート番号(23910)は固定です。他のアプリケーションと競合した場合は待ち受けできず、AviUtl2のログにエラーが記録されます。

### 既知の制限

- 出力(エンコード)の途中キャンセルはできません(SDKに中断のAPIがないため)
- シーン操作、カーソル・表示状態の操作、区間(中間点)操作は未対応です

## ドキュメント

| ドキュメント | 内容 |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 全体構成と設計意図(最初に読むことを推奨) |
| [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md) | 要件。カテゴリ別のSDK対応表と満たすべき条件 |
| [docs/DESIGN_POLICY.md](docs/DESIGN_POLICY.md) | 設計方針。実装順序・リスク判断基準・安全性への姿勢 |
| [docs/design/](docs/design/) | カテゴリごとの詳細設計書(`00_COMMON.md`〜`06_FILE_IO.md`) |
| [docs/SDK_NOTES.md](docs/SDK_NOTES.md) | SDK調査メモ・実装時に判明した制約や落とし穴 |
| [docs/BUILD_SETUP.md](docs/BUILD_SETUP.md) | ビルド環境の構築手順 |

## 設計方針(要点)

- 読み取り系を優先して広げる(副作用がなく安全)
- 編集系はUndo可能な範囲で広げる
- ファイル保存・新規作成・書き出しは、安全策(上書き禁止・2段階確認)を組み込んで実装する
- シーン操作・アプリ再起動などの破壊的操作は原則として扱わない

## ライセンス

[MIT License](LICENSE)。同梱・利用している第三者ソフトウェアのライセンスは [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) を参照してください。

「AviUtl」「AviUtl ExEdit2」の名称および関連する権利は、それぞれの権利者に帰属します。
