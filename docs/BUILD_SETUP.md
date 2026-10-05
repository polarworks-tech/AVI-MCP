# ビルド環境構築手順 (Windows / AviUtl2 Bridge Plugin)

AviUtl2のブリッジプラグイン(`.aux2`)はC++のDLLとしてビルドする。
必要なツールチェインのセットアップ手順をまとめる。

## 1. 必要なもの

- **Visual Studio 2022 Build Tools**(C++ワークロード) — MSVCコンパイラ(`cl.exe`)とWindows SDKを含む
- **CMake** — Build Toolsに同梱される(`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`)ので別途インストール不要
- VSCode自体はエディタなので、これだけではビルドできない(C++コンパイラが別途必要)

## 2. インストール (winget)

管理者権限不要でユーザースコープにインストールされる。

```bash
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact \
  --accept-package-agreements --accept-source-agreements \
  --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

- ダウンロードサイズは数GB、インストールに数分〜十数分かかる
- `--quiet --wait` を付けないとインストーラがバックグラウンドで非同期に終わってしまい、完了検知ができない
- 既にインストール済みの場合は `--force` を付けて再実行すると修復/コンポーネント追加になる

### インストール後の確認

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
```

パスが返ってくればC++ワークロードの導入は成功。

## 3. ビルド用シェルの起動

`cl.exe` や `cmake.exe` はデフォルトのPATHには入らない。
「Developer Command Prompt for VS 2022」相当の環境変数をロードしてから使う。

PowerShellの場合:

```powershell
$vsPath = & "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -DevCmdArguments "-arch=x64"
```

これで同じシェル内で `cl /?` や `cmake --version` が通るようになる。
Claude Code からビルドする際も、都度このVsDevShellを読み込んでから `cmake`/`msbuild` を呼ぶ。

## 4. プラグインのビルド (このリポジトリ内)

```powershell
cd bridge-plugin
cmake -B build -S . -A x64
cmake --build build --config Release
```

成果物の `.dll` を `AviUtl2Mcp.aux2` にリネームし、
`C:\ProgramData\aviutl2\Plugin\` 以下に配置するとAviUtl2から読み込まれる。

## 5. 既知のトラブル

- `winget install` が終了コード `1602` で失敗する場合、たいていはインストーラ実行中にキャンセル(中断)されたことを示す。再実行すれば良い。
- 1回のインストールプロセスは同時に1つしか走らせられない。既存の `vs_installer.exe` プロセスが残っていないか `tasklist` で確認する。
