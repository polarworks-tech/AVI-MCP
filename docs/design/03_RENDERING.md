# カテゴリD: レンダリング系 設計書

対応する要件: [REQUIREMENTS.md#カテゴリd-レンダリング系](../REQUIREMENTS.md)

## 技術的な核心: 非同期コールバックの同期化

`rendering_scene_video`等は「レンダリングのタスクを追加するのみで完了する」非同期APIで、
完了時に**イベント通知スレッド**からコールバックが呼ばれる(`EDIT_HANDLE`のコメント参照)。
MCPツール呼び出しは1回のリクエスト/レスポンスを期待するため、ブリッジプラグイン内で
非同期→同期の変換を行う。

```cpp
struct RenderResult {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    std::vector<uint8_t> png_bytes;  // エンコード後のPNGデータ
    int width = 0, height = 0;
};

// TCPワーカースレッド側(メインスレッドからの戻りを待つ)
auto result = std::make_shared<RenderResult>();
bool requested = g_edit->rendering_scene_video(frame, result.get(),
    [](void* p, int frame, const void* buffer, int width, int height, int pitch) {
        auto* r = static_cast<RenderResult*>(p);
        // buffer(PIXEL_RGBA, RGBA16bitFloat相当)をPNGにエンコードしてr->png_bytesに格納
        {
            std::lock_guard<std::mutex> lock(r->mtx);
            r->width = width; r->height = height;
            r->done = true;
        }
        r->cv.notify_all();
    });
if (!requested) { /* エラー応答 */ }
{
    std::unique_lock<std::mutex> lock(result->mtx);
    result->cv.wait_for(lock, std::chrono::seconds(30), [&]{ return result->done; });
}
```

**注意点**:
- `rendering_scene_video`自体もメインスレッド前提の可能性がある([00_COMMON.md](00_COMMON.md)の
  スレッド設計と同様、SendMessage経由のディスパッチが必要)。要検証。
- コールバックは「イベント通知スレッド」から呼ばれると明記されており、
  TCPワーカースレッドとも違う第3のスレッドになる。`std::condition_variable`での
  待機自体はスレッドをまたいで安全だが、実装前に小さな検証(ping的なテスト)を
  行ってから本実装に進むこと。
- 画像フォーマットはコメントに `PIXEL_RGBA` 形式とあるが、本体の内部フォーマットは
  `RGBA16bitFloat(乗算済みα)`(aviutl2.txt記載)。`PIXEL_RGBA`構造体の実際のビット幅を
  ヘッダで確認し、8bit RGBAへの変換(トーンマッピング含む)が必要か検証する。

## ツール一覧

| MCPツール名 | 対応SDK API | 状態 |
|---|---|---|
| `aviutl2_render_scene_frame` | `rendering_scene_video` + `wait_rendering_task` | ✅実装済 |
| `aviutl2_render_object_frame` | `rendering_object_video` | 将来 |
| 音声系 | `rendering_scene_audio` / `rendering_object_audio` | 将来・需要待ち |

## 詳細設計

### aviutl2_render_scene_frame(✅実装済)

- **入力**: `frame` (int)
- **ブリッジコマンド**: `{"cmd": "render_scene_frame", "frame": 0}`
- **出力**: `{"ok": true, "result": {"width": 1920, "height": 1080, "png_base64": "..."}}`
- **MCPツール結果**: `content: [{type: "image", data: <base64>, mimeType: "image/png"}]`
  (MCP SDKの`ImageContentSchema`に準拠。`mcp-server/src/index.js`の`imageResult`で生成)。
- **画像エンコード**: `stb_image_write.h`(単一ヘッダ, `bridge-plugin/third_party/`にvendor済み)の
  `stbi_write_png_to_func`でメモリ上にPNGを生成し、自作のBase64エンコーダ(`string_utils.cpp`)で
  文字列化している。
- **検証結果**: `PIXEL_RGBA`(`filter2.h`)は`unsigned char r,g,b,a`の単純な8bit構造体であり、
  想定していたRGBA16bitFloatからの変換は不要だった。
  `rendering_scene_video`/`wait_rendering_task`をメインスレッド(`BridgeWndProc`経由、
  すなわち既存のコマンドディスパッチの仕組みに乗せる形)から呼んでクラッシュしないことを確認済み。
  実際にテキストオブジェクトを作成しレンダリングし、PNGとして正しく出力されることを目視確認した。
- **未対応**: タイムアウト処理(現状`wait_rendering_task`が返るまで待ち続ける)。
  問題が実際に発生してから対応する。

### aviutl2_render_object_frame(将来)

- **入力**: `object_id`, `frame`, `apply_effect` (bool)
- 基本構造は`aviutl2_render_scene_frame`と同じ。`object_id`解決([00_COMMON.md](00_COMMON.md))が
  必要な点が異なる。

### 音声系(将来・需要待ち)

テキスト主体のエージェント利用では音声データの使い道が画像より少ないため、
明確な需要が出てから着手する。実装パターンは映像系と同様(PCM floatバッファを
WAV化してBase64で返す等)。

## 実装順序の提案

1. まず`aviutl2_render_scene_frame`の**スレッドモデルを検証する小さなプロトタイプ**を
   作る(PNGエンコードなしで、バッファのサイズだけ返す等)
2. スレッドモデルの疑問([00_COMMON.md](00_COMMON.md)同様の`SendMessage`ディスパッチが
   必要か)が解消したら、PNGエンコードを実装
3. `aviutl2_render_object_frame`は映像系が安定してから追加
