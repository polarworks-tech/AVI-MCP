#pragma once
// ローカルTCP(127.0.0.1限定)でJSON Lines形式のコマンドを受け取るサーバー。
// 通信とスレッドの橋渡しのみを担当し、コマンドの中身(何を処理するか)は知らない
// (commands_read.cpp / commands_edit.cpp 側の責務)。

// 非表示ウィンドウの作成とTCPサーバースレッドの起動を行う。RegisterPlugin()から呼ぶ。
void StartBridgeServer();

// サーバースレッドを停止し、終了を待つ。UninitializePlugin()から呼ぶ。
void StopBridgeServer();
