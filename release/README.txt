AVI-MCP (非公式ツール)

AviUtl2 を MCP (Model Context Protocol) 経由で操作するためのツールです。
AviUtl2 の作者(ＫＥＮくん)とは無関係の非公式プロジェクトです。
AviUtl2 本体は含まれません。

■ 同梱物

  plugin\AviUtl2McpBridge.aux2   AviUtl2 用プラグイン
  server\                        MCPサーバー (Node.js)
  mcp.json.example               MCPクライアント(Claude Code 等)の設定例
  LICENSE, THIRD_PARTY_LICENSES.md

■ 動作環境

  Windows 11 x64 / AviUtl2 v2.1.11a / Node.js 20 以上

■ 導入手順

  1. AviUtl2 を終了する

  2. plugin\AviUtl2McpBridge.aux2 を次のフォルダにコピーする
       C:\ProgramData\aviutl2\Plugin\
     (ポータブル運用の場合は、AviUtl2 の data\Plugin フォルダ)

  3. server フォルダで PowerShell を開き、次を実行する(依存パッケージを取得します)
       npm ci --omit=dev

  4. MCPクライアントに登録する
     mcp.json.example を参考に、cwd を server フォルダの絶対パスに書き換える
     (Claude Code の場合は、書き換えたものを .mcp.json として保存)

  5. AviUtl2 を起動する
     起動すると 127.0.0.1:23910 で待ち受けを開始します

■ 動作確認 (空のプロジェクトで実行してください)

  server フォルダで次を実行する
       node test\manual_client_test.js

  検証用のオブジェクトを1つ作成し、終了時に削除します。

■ 注意

  - 無保証です。作業中のプロジェクトは、事前に保存・バックアップしてください。
  - 認証機構はありません。同じPC上のプロセスは誰でも接続でき、
    プロジェクトの編集と、任意の絶対パスへの新規ファイル作成(上書きは拒否)が可能です。
    共有PCや、信頼できないソフトウェアが動作する環境では使用しないでください。
  - このファイル群には署名がありません。Windows の警告が表示される場合があります。
    ダウンロードしたzipは、展開する前に右クリック → プロパティで
    「許可する」(ブロックの解除)を確認できます。
    配布ページ記載の SHA256 ハッシュと一致するかも確認してください。
  - 詳細・ソースコード: https://github.com/polarworks-tech/AVI-MCP
