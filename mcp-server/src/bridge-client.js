// AviUtl2ブリッジプラグイン(C++, 127.0.0.1:23910)とのJSON Lines通信クライアント
import net from "node:net";

const BRIDGE_HOST = "127.0.0.1";
const BRIDGE_PORT = 23910;
const DEFAULT_TIMEOUT_MS = 5000;

/**
 * ブリッジプラグインに1コマンドを送信し、1レスポンスを受け取る。
 * 接続は毎回使い捨て(コマンドごとに新規接続)。
 * @param {object} command - {cmd: string, ...} 形式のコマンド
 * @param {object} [options]
 * @param {number} [options.timeoutMs] - 既定5秒。レンダリング等、時間のかかる処理は
 *   呼び出し側で長めの値を指定すること。
 * @returns {Promise<object>} ブリッジからのレスポンス ({ok, result} または {ok: false, error})
 */
export function sendBridgeCommand(command, options = {}) {
  const timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  return new Promise((resolve, reject) => {
    const socket = net.createConnection({ host: BRIDGE_HOST, port: BRIDGE_PORT });
    // バイト列(Buffer)のまま蓄積する。チャンクごとに toString("utf8") すると、
    // マルチバイト文字(日本語等)がちょうどチャンク境界で分割された場合に
    // デコードが壊れる(文字化けする)ため、改行が見つかるまではデコードしない。
    let buffer = Buffer.alloc(0);
    let settled = false;

    const finish = (fn, value) => {
      if (settled) return;
      settled = true;
      socket.destroy();
      fn(value);
    };

    socket.setTimeout(timeoutMs, () => {
      finish(reject, new Error(`AviUtl2ブリッジへの応答がタイムアウトしました(${timeoutMs}ms)。AviUtl2が起動しているか確認してください。`));
    });

    socket.on("connect", () => {
      socket.write(JSON.stringify(command) + "\n");
    });

    socket.on("data", (chunk) => {
      buffer = Buffer.concat([buffer, chunk]);
      const idx = buffer.indexOf(0x0a); // '\n'
      if (idx !== -1) {
        const line = buffer.subarray(0, idx).toString("utf8");
        try {
          finish(resolve, JSON.parse(line));
        } catch (e) {
          finish(reject, new Error(`ブリッジからの応答をJSONとして解釈できませんでした: ${e.message}`));
        }
      }
    });

    socket.on("error", (err) => {
      finish(
        reject,
        new Error(
          `AviUtl2ブリッジ(127.0.0.1:${BRIDGE_PORT})に接続できません(${err.message})。` +
            "AviUtl2が起動していて、AviUtl2McpBridge.aux2プラグインが読み込まれているか確認してください。"
        )
      );
    });
  });
}
