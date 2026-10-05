# Third-Party Licenses

本プロジェクトは以下の第三者ソフトウェアを同梱、またはビルド・実行時に利用しています。
本プロジェクト自体のライセンスは [LICENSE](LICENSE) (MIT License) です。

## 本リポジトリに同梱しているもの

### nlohmann/json 3.11.3

- 場所: `bridge-plugin/third_party/json.hpp`
- ライセンス: MIT License
- Copyright (c) 2013-2023 Niels Lohmann <https://nlohmann.me>
- 上流: <https://github.com/nlohmann/json>

### stb_image_write.h v1.16

- 場所: `bridge-plugin/third_party/stb_image_write.h`
- ライセンス: MIT License または Public Domain (Unlicense) の選択。本プロジェクトではMIT Licenseを選択して利用する
- Copyright (c) 2017 Sean Barrett
- 上流: <https://github.com/nothings/stb>
- 原文はファイル末尾に記載

## ビルド時・実行時に別途取得するもの(本リポジトリには含まれない)

### AviUtl2 Plugin SDK (aviutl2_sdk_mirror)

- 取得先: <https://github.com/aviutl2/aviutl2_sdk_mirror>(非公式ミラー)
- ライセンス: MIT License
- Copyright (c) 2025 Kenkun
- ビルド時にヘッダファイルを利用する。ビルドしたバイナリには、SDKのヘッダに由来する定義が含まれる。
  バイナリを配布する場合は、この著作権表示と許諾文(下記)を配布物に含めること。

### npm 依存パッケージ

- `@modelcontextprotocol/sdk`(MIT License)、`zod`(MIT License)、およびそれらの推移的依存
- `npm install` で取得され、各パッケージのライセンスに従う

### Node.js

- ライセンス: MIT License ほか(Node.js本体に同梱される各種依存のライセンス)
- Node.jsランタイムを含む形で配布物を作成する場合は、Node.jsのライセンスファイルを同梱すること

## MIT License(共通の許諾文)

上記のMIT Licenseの項目に共通する許諾文は以下のとおり。著作権表示は各項目に記載したものを用いる。

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
