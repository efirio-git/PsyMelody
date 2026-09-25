# PsyMelody v0.1.3 - 導入手順書
## Psytrance Melody Generator by EDEN

---

## インストール

### macOS

#### インストーラーを使用（推奨）

1. `PsyMelody_v0.1.3.dmg` を開く
2. `PsyMelody_Installer.pkg` をダブルクリック
3. 画面の指示に従ってインストール
4. **カスタマイズ** をクリックしてインストールするフォーマットを選択:
   - **VST3 Plugin** → `/Library/Audio/Plug-Ins/VST3/`（FL Studio, Ableton, Cubase等）
   - **Audio Unit Plugin** → `/Library/Audio/Plug-Ins/Components/`（Logic Pro, GarageBand）
5. インストールをクリック
6. DAWを再起動

#### アンインストール

DMGに含まれる `Uninstall_PsyMelody.command` を実行してください。

#### 手動インストール

以下の場所にファイルをコピー:

| フォーマット | コピー先 |
|------------|---------|
| **PsyMelody.vst3** | `/Library/Audio/Plug-Ins/VST3/` |
| **PsyMelody.component** | `/Library/Audio/Plug-Ins/Components/` |

### Windows

**動作環境:** Windows 10 / 11（64 ビット）、VST3 対応の DAW（FL Studio、Ableton Live、Cubase、
Studio One、Bitwig、REAPER など）。Windows 版は VST3 のみで、Standalone アプリは含まれません。

#### インストーラーを使用（推奨）

1. `PsyMelody_v0.1.3_Windows.zip` を右クリックして **すべて展開** を選ぶ
2. 展開したフォルダの `Install_PsyMelody.bat` をダブルクリック
3. 管理者権限の確認が出たら「はい」を選ぶ
4. DAW を再起動し、プラグインを再スキャンする

インストール先: 64 ビット用の共通ファイルの VST3 フォルダ（通常は `C:\Program Files\Common Files\VST3\PsyMelody.vst3`）

#### 手動インストール

`PsyMelody.vst3` フォルダを `C:\Program Files\Common Files\VST3\` にコピーしてください。

#### セキュリティの警告について

Windows 版にはコード署名がありません。`Install_PsyMelody.bat` を実行すると「発行元を確認できま
せんでした」や「Windows によって PC が保護されました」と表示されることがあります。「実行」
（または「詳細情報」→「実行」）を選ぶと続行できます。ダウンロードしたファイルが改ざんされて
いないかは、GitHub のリリースページに載せている SHA256 と比べて確認できます
（PowerShell: `Get-FileHash <zip ファイル>`）。

#### アンインストール

`Uninstall_PsyMelody.bat` をダブルクリックします。プラグインの削除を確認します。プラグインを削除でき、
`Documents\EDEN\PsyMelody` がある場合は、プリセットとデータも削除するかを別に尋ねられます。
ドキュメントフォルダが OneDrive 上にある場合（例: `C:\Users\<user>\OneDrive\ドキュメント`）、空になった
`Documents\EDEN` フォルダが残ることがありますが、問題はありません。

---

## DAW別セットアップ

---

### FL Studio

#### プラグインスキャン
1. FL Studioを開く
2. **Options** > **Manage plugins**（またはF10）
3. **プラグインをスキャン開始** をクリック
4. スキャン完了を待つ
5. PsyMelodyが **Synth** タイプとして表示される

#### PsyMelodyの追加
1. **Channel Rack** を開く
2. 下部の **+** ボタンをクリック
3. プラグインリストから **PsyMelody** を選択

#### MIDIルーティング（外部シンセで音を出す場合）
1. Channel RackにPsyMelodyを追加
2. シンセ（Serum, Vital, TB-303等）を追加
3. PsyMelodyのチャンネル設定（歯車アイコン）> **MIDI** タブ > **Port** 出力を **1** に設定
4. シンセのチャンネル設定 > **MIDI** タブ > **Port** 入力を **1** に設定
5. PsyMelodyで **Generate** をクリック、Playを押す

#### 推奨ワークフロー
1. PsyMelodyでメロディを生成
2. **Export MIDI** または **Quick Save** をクリック
3. `.mid` ファイルをパターンのピアノロールにドラッグ＆ドロップ
4. パターンをお好みのシンセにルーティング
5. PsyMelodyを閉じてCPUリソースを節約

#### Windows
インストール後は上記と同じく **Options** > **Manage plugins** で再スキャンしてください。動作は
macOSと同じです（Windows 11のFL Studioで、高DPI表示スケーリングを含めて確認済み）。

---

### Ableton Live

#### プラグインスキャン
1. Ableton Liveを開く
2. **環境設定** > **Plug-Ins**
3. **Use VST3 Plug-In System Folder** が **On** であることを確認
4. 表示されない場合は **Rescan** をクリック

#### PsyMelodyの追加
1. 左側の **ブラウザ** から **Plug-Ins** > **VST3**
2. **PsyMelody** を見つける
3. **MIDIトラック** にドラッグ

#### MIDIルーティング
1. PsyMelodyのMIDIトラックを作成
2. シンセ用の別のMIDIトラックを作成
3. シンセトラックの **MIDI From** を **PsyMelody** に設定
4. Monitorを **In** に設定
5. シンセトラックをアーム

#### 推奨ワークフロー
1. PsyMelodyでメロディを生成
2. MIDIファイルとしてエクスポート
3. `.mid` を空のMIDIクリップスロットにドラッグ
4. お好みのシンセにルーティング

#### Windows
プラグインスキャンと設定手順は上記と同じです: **環境設定** > **Plug-Ins** で
**Use VST3 Plug-In System Folder** を有効にし、**Rescan** してください。

---

### Logic Pro（macOSのみ）

> **注意:** Logic Proは **Audio Unit (AU)** フォーマットを使用します（VST3は非対応）。
> Windowsでは利用できません。PsyMelody.componentがインストールされていることを確認してください。

#### プラグインスキャン
1. Logic Proを開く
2. **Logic Pro** > **設定** > **プラグインマネージャ**
3. 表示されない場合は **選択項目をリセットして再スキャン** をクリック
4. PsyMelodyが有効（チェック済み）であることを確認

#### PsyMelodyの追加
1. 新しい **ソフトウェア音源** トラックを作成
2. **音源** スロットをクリック
3. **AU Instruments** > **EDEN** > **PsyMelody** を選択

#### 推奨ワークフロー
1. PsyMelodyでメロディを生成
2. MIDIをエクスポート
3. 別のトラックに `.mid` をドラッグ
4. プロジェクトからPsyMelodyを削除してCPU節約

---

### Cubase / Nuendo

#### プラグインスキャン
1. Cubaseを開く
2. **スタジオ** > **VSTプラグインマネージャ**
3. **すべてを再スキャン** をクリック
4. **EDEN** の下にPsyMelodyが表示される

#### PsyMelodyの追加
1. 新しい **インストゥルメントトラック** を作成
2. VSTインストゥルメントリストから **PsyMelody** を選択

#### 推奨ワークフロー
1. PsyMelodyでメロディを生成
2. **Export MIDI** で保存
3. `.mid` ファイルをMIDI/インストゥルメントトラックにインポート
4. お好みのシンセに割り当て

---

### Studio One

#### プラグインスキャン
1. Studio Oneを開く
2. **Studio One** > **オプション** > **ロケーション** > **VSTプラグイン**
3. `/Library/Audio/Plug-Ins/VST3` がリストにあることを確認
4. **再スキャン** をクリック

#### PsyMelodyの追加
1. **ブラウザ** パネルを開く
2. **インストゥルメント** > **EDEN** > **PsyMelody**
3. 空のトラックにドラッグ

---

### Bitwig Studio

#### プラグインスキャン
1. Bitwig Studioを開く
2. **設定** > **プラグイン** > **ロケーション**
3. VST3システムパスが含まれていることを確認
4. **再スキャン** をクリック

#### PsyMelodyの追加
1. **+** をクリックしてデバイスを追加
2. **PsyMelody** を検索
3. インストゥルメントトラックに追加

---

### Windows（Cubase / Studio One / Bitwig Studio / REAPER）

Windowsでは、これらのDAWは標準でVST3システムフォルダ
（`C:\Program Files\Common Files\VST3\`）をスキャンするため、パスの追加設定は不要です。
インストール後は再スキャンするだけです:
- **Cubase:** **スタジオ** > **VSTプラグインマネージャ** > **すべてを再スキャン**
- **Studio One:** **Studio One** > **オプション** > **ロケーション** > **VSTプラグイン** > **再スキャン**
- **Bitwig Studio:** **設定** > **プラグイン** > **ロケーション** > **再スキャン**
- **REAPER:** **Options** > **Preferences** > **Plug-ins** > **VST** > **Re-scan**

---

## プレビューシンセ

外部シンセなしでメロディを試聴できる内蔵シンセ:

1. フッターバーの **▶ PREVIEW** トグルをクリック
2. **OSC** ドロップダウンからオシレーター波形を選択: **Saw** / **Square** / **Sine** / **Triangle**
3. **スピーカーアイコンのスライダー** で音量調整
4. DAWのPlayを押す

> **注意:** プレビューシンセは試聴用です。
> 制作時はMIDIをエクスポートして専用シンセを使用してください。

---

## トラブルシューティング

### インストール後にプラグインが表示されない
- DAWを再起動する
- DAWのプラグインマネージャで再スキャンする
- ファイルが正しいフォルダにあるか確認:
  - macOS VST3: `/Library/Audio/Plug-Ins/VST3/PsyMelody.vst3`
  - macOS AU: `/Library/Audio/Plug-Ins/Components/PsyMelody.component`
  - Windows VST3: `C:\Program Files\Common Files\VST3\PsyMelody.vst3`

### macOSで「開発元を確認できません」の警告が出る
- プラグインファイルを右クリック > **開く**
- または: **システム設定** > **プライバシーとセキュリティ** > **このまま開く** をクリック

### Windowsで「発行元を確認できません」/ SmartScreenの警告が出る
- Windows版にはコード署名がないため、正常な表示です
- 「実行」（または「詳細情報」→「実行」）を選んで続行してください
- ダウンロードのSHA256をGitHubリリースページの値と照合して確認できます

### インストーラーが「プラグインを置き換えられませんでした」と表示する（Windows）
- DAWが起動していて、プラグインが使用中のため置き換えられません
- DAWを完全に終了してから、`Install_PsyMelody.bat` を再度実行してください

### FL Studioで「error」と表示される
1. FL Studioを終了
2. キャッシュファイルを削除:
   ```
   ~/Documents/Image-Line/FL Studio/Presets/Plugin database/Installed/Generators/VST3/PsyMelody.nfo
   ~/Documents/Image-Line/FL Studio/Presets/Plugin database/Installed/Generators/VST3/PsyMelody.fst
   ```
3. FL Studioを再起動して再スキャン

### PsyMelodyから音が出ない
- PsyMelodyはMIDIを生成するプラグインです（オーディオは生成しません）
- MIDIをシンセにルーティングする必要があります（上記のDAW別手順を参照）
- または内蔵 **Preview** シンセを有効にしてください

### MIDIエクスポートが動作しない
- まずメロディを生成してください（**Generate** をクリック）
- エクスポート先フォルダに書き込み権限があるか確認

---

## システム要件

### macOS
- **OS:** macOS 10.15 (Catalina) 以降
- **アーキテクチャ:** Apple Silicon または Intel（Universal Binary）
- **フォーマット:** VST3, Audio Unit
- **DAW:** VST3またはAU対応の任意のDAW

### Windows
- **OS:** Windows 10 / 11（64ビット）
- **フォーマット:** VST3のみ（AU、Standaloneはなし）
- **DAW:** VST3対応の任意のDAW

---

## ファイルの場所

| 項目 | パス |
|------|------|
| macOS VST3プラグイン | `/Library/Audio/Plug-Ins/VST3/PsyMelody.vst3` |
| macOS AUプラグイン | `/Library/Audio/Plug-Ins/Components/PsyMelody.component` |
| macOS ユーザープリセット | `~/Documents/EDEN/PsyMelody/Presets/` |
| Windows VST3プラグイン | `C:\Program Files\Common Files\VST3\PsyMelody.vst3` |
| Windows ユーザープリセット | `Documents\EDEN\PsyMelody\Presets\`（ドキュメントフォルダ。OneDriveにリダイレクトされている場合あり） |

---

*PsyMelody v0.1.3 - Copyright (c) 2026 EDEN*
