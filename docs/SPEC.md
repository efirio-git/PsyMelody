# PsyMelody 仕様書

対象バージョン: v0.1.2 開発版（v0.1.1 + D&Dエクスポート / ツールチップ / 生成エンジン強化）
最終更新: 2026-08-07

---

## 1. 概要

PsyMelody は、サイケデリックトランス（Goa / Full-On / Dark Psy / Progressive Psy）向けの
MIDI メロディ／ベースライン／コードボイシング生成プラグイン。

| 項目 | 内容 |
|---|---|
| フレームワーク | JUCE（C++17、CMake 3.22+） |
| フォーマット | Standalone / VST3 / AU |
| プラグイン種別 | `IS_SYNTH=TRUE`、MIDI 出力あり（`producesMidi`）、MIDI 入力なし |
| バス構成 | ステレオ出力のみ（入力なし） |
| 開発元 | EDEN（manufacturer code `PsyM` / plugin code `PsMl`） |
| ライセンス | AGPLv3 |
| ウィンドウ | 960 × 720 固定（リサイズ不可） |
| フォント | Orbitron（Bold/Regular）、Inter（Regular/Medium/Bold）をバイナリ内蔵 |

DAW 上では音源トラックとして動作し、生成した MIDI をトランスポート同期でループ出力しつつ、
内蔵 PreviewSynth によるプレビュー音声をステレオバッファに出力する。

## 2. アーキテクチャ

### 2.1 データフロー

```
UI (PsyMelodyEditor)
  │ syncToParams()
  ▼
GeneratorParams（POD構造体、Processor が所有）
  │
  ├─ Melody       → GoaMelodyGenerator
  ├─ Bassline     → BasslineGenerator
  └─ ChordVoicing → ChordVoicingGenerator
  │
  ▼
std::vector<NoteEvent> currentPhrase（編集・Undo/Redo・プリセット・MIDIインポートの対象）
  │ loadPhrase()
  ▼
MidiPatternEngine（PPQ 同期ループ再生）──► MIDI 出力（DAW へ）
  │                                          │
  └──────────────────────────────────────────┴─► PreviewSynth ─► オーディオ出力
```

### 2.2 NoteEvent

| フィールド | 型 | 範囲 | 意味 |
|---|---|---|---|
| `noteNumber` | int | 0–127 | MIDI ノート番号 |
| `velocity` | float | 0.0–1.0 | ベロシティ |
| `pan` | float | −1.0(L)〜1.0(R) | パン（CC10 として出力） |
| `startBeat` | double | ≥0 | フレーズ先頭からの拍位置 |
| `duration` | double | ≥0.0625 | 拍単位の長さ |
| `pitchBend` | int | −8192〜8191 | ピッチベンド |
| `accent` | bool | — | TB-303 風アクセント |
| `slide` | bool | — | TB-303 風スライド |
| `isGraceNote` | bool | — | 装飾音フラグ |

### 2.3 スレッドモデル

| 共有データ | 保護機構 |
|---|---|
| `MidiPatternEngine::currentPhrase` / `activeNotes` / `flushActiveNotes` / `lastPanCC` | `juce::SpinLock`。メッセージスレッドの `loadPhrase()` はロック外でコピー構築し、ロック内は `swap` のみ。オーディオスレッドは try-lock（失敗時はそのブロックをスキップ） |
| `phraseLengthBeats` | `std::atomic<double>` |
| BPM フォールバック | `PsyMelodyProcessor::fallbackBpm`（atomic float）。BPM の書き込みは必ず `setBpm()` 経由 |
| `playbackPosition` / `playing` / `dawBpm` / `stateVersion` | atomic |
| PreviewSynth の `isEnabled` / `volume` / `waveType` / `killRequested` | atomic（processBlock 冒頭で一度スナップショット） |
| `currentPhrase`（Processor 側）、Undo 履歴、`genParams`（bpm 以外） | メッセージスレッド専用 |

- `loadPhrase()` は鳴動中ノートを直接クリアせず `flushActiveNotes` フラグを立て、
  次のオーディオブロック冒頭で note-off（+ ベンドリセット）を送出してからクリアする
  （DAW にノートオンが取り残されない）。
- `PreviewSynth::allNotesOff()` は `killRequested` フラグのセットのみ。ボイスのゼロ化は
  オーディオスレッドが次ブロックで行う。
- `activeNotes` はコンストラクタで 128 要素 reserve 済み（オーディオスレッドでの再確保回避）。
- `processBlock` 冒頭で `juce::ScopedNoDenormals` を適用。

## 3. パラメータ仕様

### 3.1 GeneratorParams（保存対象・全14項目）

| パラメータ | 型 | デフォルト | 範囲 | UI |
|---|---|---|---|---|
| `rootNote` | int | 0 (C) | 0–11 | ROOT ComboBox |
| `scaleIndex` | int | 0 | 0–6 | SCALE ComboBox |
| `bpm` | float | 145 | 60–200 (step 1) | BPM スライダー（DAW BPM 検出時は自動追従） |
| `phraseLengthBars` | int | 4 | 1–16 | PHRASE スライダー（表示 "N bars"） |
| `baseOctave` | int | 4 | 2–6 | OCTAVE スライダー |
| `patternCategory` | int | 1 | 0–3 | PATTERN ComboBox（Acid Arp / Lead Melody / Slow Melody / Ambient） |
| `progression` | int | 0 | 0–8 | CHORDS ComboBox |
| `subgenre` | int | 0 | 0–3 | GOA / FULL-ON / DARK PSY / PROGRESSIVE ピルボタン |
| `density` | float | 0.6 | 0–1 | DENSITY ノブ |
| `acidAmount` | float | 0.5 | 0–1 | ACID ノブ |
| `ornamentAmount` | float | 0.3 | 0–1 | ORNAMENT ノブ |
| `graceAmount` | float | 0.0 (OFF) | 0–1 | GRACE ノブ |
| `rhythmVariation` | float | 0.4 | 0–1 | RHYTHM VAR ノブ |
| `pitchRange` | float | 0.5 | 0–1 | PITCH RNG ノブ |
| `humanize` | float | 0.0 | 0–1 | HUMANIZE ノブ（タイミング±0.03拍・ベロシティ±15%の揺らぎ） |
| `swing` | float | 0.0 | 0–1 | SWING ノブ（16分裏拍を最大トリプレット位置=+1/12拍まで遅延） |

構造体末尾への追記のみ許可（`Presets.h` が位置指定集成体初期化のため、途中挿入は全ファクトリープリセットを黙って壊す）。

**シード**（GeneratorParams外・Processor所有）: `currentSeed`（0〜999999）と `seedLocked`。
生成アクションのたびに未ロックなら新シードを採番し、3ジェネレータ全てに `setSeed()`。
ロック中は同シード再利用 → GENERATE が完全に同一フレーズを再現する。
VARIATION と部分再生成は再シードしない（押すたびに変化するのが仕様）。

### 3.2 UI 専用（DAW プロジェクトに保存されない）

| 項目 | デフォルト | 選択肢 |
|---|---|---|
| GenMode | Melody | Melody / Bassline / Chord Voicing |
| BassStyle | Rolling | Rolling / Offbeat / Acid 303 / Minimal / Dark Groove |
| VoicingStyle | Pad | Pad / Stab / Arp / Pluck |
| Preview ON/OFF | OFF | — |
| Preview 波形 | Saw | Saw / Square / Sine / Triangle |
| Preview 音量 | 0.15 | 0–1 |
| レーン種別 | Velocity | Velocity / Pan / Pitch |
| 言語 | English | English / Japanese |

### 3.3 ホストオートメーション

**非対応。** APVTS / `AudioProcessorParameter` を使用しておらず、ホストのパラメータ
リストは空。パラメータ操作はプラグイン UI からのみ行う（設計上の既知制約）。

## 4. 生成アルゴリズム仕様

### 4.1 Melody（GoaMelodyGenerator）

**リズムパターン辞書: 56 種 × 16 ステップ（16分音符グリッド）**

| インデックス | 用途 |
|---|---|
| 0–7 | DENSE（Acid Arp、Goa 用） |
| 8–19 | MID（Lead Melody、Goa 用） |
| 20–27 | SPARSE（Slow Melody、Goa 用） |
| 28–31 | VERY SPARSE（Ambient、Goa 用） |
| 32–39 | Full-On 専用 |
| 40–47 | Dark Psy 専用 |
| 48–55 | Progressive Psy 専用 |

- サブジャンルが Goa(0) のときのみ `patternCategory`（0〜3）が有効。他サブジャンルは専用帯域に固定。
- **Euclidean（patternCategory=4）は例外的に全サブジャンルで有効**。Bjorklundアルゴリズムで
  16ステップに `k = round(density×15)+1` 個の打点を均等配置（回転はモチーフごとにランダム）。
  既存の間引き/ゴースト処理はそのまま適用されるため Rhythm Var も効く。
- 小節ごとに `density` とランダム変位（±2）でパターンを選択。`rhythmVariation` が高いほど
  ヒットの間引き＋まれな追加ヒットが発生。

**音程遷移**: サブジャンル別の遷移重みテーブル（Goa: フリジアン的半音進行 / Full-On:
広い跳躍とユニゾン / Dark Psy: 半音階+トライトーン / Progressive: 順次進行・高反復）。
`pitchRange` で跳躍幅、コードトーン倍率（Goa/Full-On 1.8、Dark Psy 0.8、Progressive 1.4）で
和声整合を制御し、最後にスケールへ量子化。

**コード進行: 9 種**

| idx | 名称 |
|---|---|
| 0 | Drone |
| 1 | i - bII |
| 2 | i - bVII |
| 3 | i - bVI |
| 4 | i - iv |
| 5 | i - bII - bVII - i |
| 6 | i - iv - bVI - bVII（Full-On） |
| 7 | Chromatic Drone（Dark Psy） |
| 8 | i - bVII - bVI - v（Progressive） |

**生成パイプライン**（`generatePhrase`）:

1. モチーフ生成（サブジャンル別モチーフ長、拍位置ベースのベロシティ + ヒューマナイズ）
2. セクションごとのモチーフ発展（Progressive は控えめ、Dark Psy は大胆）
3. Full-On のみ: 終盤 2 小節クレッシェンド
4. 強拍強調（75% 地点までビルド）
5. コール & レスポンス（後半で反転 / 移調エコー）
6. フレーズ終止処理（終端をルート / 5度 / 導音アプローチへ）
7. アシッドアーティキュレーション（`acidAmount` によるアクセント / スライド、
   Dark Psy はスライドにランダムベンド付加）
8. 装飾（`graceAmount` グレースノート、`ornamentAmount` トリル / ベンド）

**VARIATION**: 既存フレーズの各ノートを `variationAmount=0.3` 基準で音程 / タイミング /
ベロシティ変異させ、終止処理を再適用。フレーズ未生成時は新規生成にフォールバック。

**グルーヴ段（Humanize / Swing）**: 生成直後にプロセッサが全モード共通で適用
（VARIATION には再適用しない）。オフセットは**16分スロット単位で共有**するため、和音は
まとまって動き、装飾音は親ノートに追従する。スライドノートの duration はグルーヴ後の
隣接ノート位置に対して再計算される（レガート維持）。装飾音は対象外。

**部分再生成（Melodyモードのみ、GENERATE の ▾ メニュー）**:
- **音程のみ再生成**: 位置・長さ・ベロシティ・アーティキュレーションを保持し、
  音程だけをサブジャンル別重みテーブル + コード進行で歩き直す。コール&レスポンスと
  終止処理を再適用。装飾音は新しい親から再導出
- **リズムのみ再生成**: 新フレーズを生成し、旧フレーズの (時刻, 音程) を
  時間ベースで対応付けて音程を移植（「いつどの音が鳴るか」を保持）。
  トリル長のノートは装飾性維持のため対象外
- どちらも常にフリー乱数（シードロック中でも押すたびに変わる）

**シード**: `GoaMelodyGenerator` / `BasslineGenerator` / `ChordVoicingGenerator` すべて
`std::mt19937` + `setSeed()` 対応。SEED表示とロックトグルは §3.1 参照。

### 4.2 Bassline（BasslineGenerator）

- 音域はオクターブ 2 固定（`rootNote + 24`）。
- 5 スタイル: Rolling（16分ローリング）/ Offbeat（オフビート固定）/ Acid 303（固定パターン
  3 種からランダム）/ Minimal / Dark Groove（不規則 2 種）。
- `density` でヒット数、`variation` で音程変化（ルート / −5 / +7 → スケール量子化）、
  `acidAmount` でアクセント / スライドを制御。

### 4.3 Chord Voicing（ChordVoicingGenerator）

- コード進行は §4.1 と同じ **9 種すべてに対応**。
- 声部数 2–5（デフォルト 3）。不足分は既存音の +12 重ねで補う。
- 4 スタイル:

| スタイル | 内容 |
|---|---|
| Pad | 小節頭から 4 拍（density<0.5 で 8 拍）サステイン、pan ±0.3 ランダム |
| Stab | density に応じた 4 パターンの短打、拍頭アクセント |
| Arp | 8–16 ステップの上行→下行サイクル、4 ステップごとアクセント |
| Pluck | 声部ごと 0.02 拍のストラムディレイ付き短音 |

### 4.4 スケール定義（7 種）

| idx | 名称 | インターバル |
|---|---|---|
| 0 | Phrygian Dominant | 0 1 4 5 7 8 10 |
| 1 | Double Harmonic | 0 1 4 5 7 8 11 |
| 2 | Harmonic Minor | 0 2 3 5 7 8 11 |
| 3 | Natural Minor | 0 2 3 5 7 8 10 |
| 4 | Hirajoshi | 0 2 3 7 8 |
| 5 | Hungarian Minor | 0 2 3 6 7 8 11 |
| 6 | Phrygian | 0 1 3 5 7 8 10 |

`quantizeToScale` は各インターバルの ±12 隣接オクターブ候補も距離評価する
（例: B は root C のスケール音 0 に対し「1 半音上の C」へ量子化される）。結果は 0–127 に
クランプ。

## 5. プリセット仕様

### 5.1 ファクトリープリセット（35 個・8 カテゴリ）

Acid(5) / Melodic(6) / Eastern(4) / Epic(4) / Ambient(4) / Full-On(4) / Dark Psy(4) /
Progressive(4)。パラメータのみを持ち、ロード時に新規生成が走る。
プリセット選択時は GenMode が Melody にリセットされる。

### 5.2 ユーザープリセット

- 保存先: `~/Documents/EDEN/PsyMelody/Presets/<name>.xml`（スペース・`/`・`\` は `_` に置換）
- ルートタグ `PsyMelodyPreset`。**全 16 パラメータ**（`bpm` / `subgenre` / `graceAmount` /
  `humanize` / `swing` を含む。シードは含まない）+ `Sequence > Note`
  （属性 `nn, vel, pan, start, dur, pb, acc, sld, grace`）を保存。
- 旧形式ファイル（bpm 等なし）はデフォルト値で読み込まれる（後方互換）。
- 起動時に走査され、カテゴリ "User" として一覧末尾に表示。

## 6. MIDI 入出力仕様

### 6.1 リアルタイム MIDI 出力（MidiPatternEngine）

- DAW の PPQ 位置に同期し、フレーズ（最終ノート終端を 4 拍単位に切り上げた長さ）を
  ループ再生。出力チャンネルは 1 固定。
- **ピッチベンド**: `pitchBend != 0` のノートは note-on 直前にベンド送出、note-off 直後に
  センター（8192）へリセット。停止時 / フレーズ差し替え時のフラッシュでも同様にリセット。
- **パン (CC10)**: note-on 直前に `(pan+1)/2×127` を送出。値が変化したときのみ送る
  （パンさせたノートの後にセンターのノートが来れば自動で再センタリングされる）。
  ※ チャンネル単位の CC のため、同時発音するノート同士で異なるパンは表現できない。
- フレーズ終端ちょうどで終わるノートは、次ループ先頭の note-on と衝突しないよう
  終端直前（`phraseLen − 1e-4` 拍）で note-off する。
- 再生停止時は全アクティブノートに note-off を送出。
- ホストが PlayHead を提供しない場合、MIDI / 音声とも出力しない（既知制約 §9）。

### 6.2 MIDI エクスポート

- `MidiExport::exportToFile`: ticksPerBeat=480、テンポ + 拍子（4/4）メタイベント付き、
  単一トラック SMF。DAW BPM 検出時はそちらを優先。
- ピッチベンドはノート前送出 + ノート後リセット。**CC10 パンも on-change で書き出す。**
- EXPORT MIDI ボタン（分割ボタン）: 左クリック → FileChooser（デフォルト `~/Desktop/PsyMelody.mid`）。
  **右端グリップ（⁙）をドラッグ → 一時 .mid を生成してOSファイルドラッグ開始**、DAWへ直接
  ドロップできる。FL Studio ではピアノロール / チャンネルラックへのドロップに対応
  （プレイリストは FL 側の制約で不可。また FL のインポートはノート+ベロシティのみで
  CC10 / ピッチホイールは取り込まれない）。
- QUICK SAVE: 初回のみフォルダ選択、以降ワンクリック保存。`PsyMelody.mid` →
  `PsyMelody_1.mid` … と自動採番（上書きなし）。`[...]` でフォルダ変更。

### 6.3 MIDI インポート

`*.mid;*.midi` を全トラック走査。note-on/off ペアリング（duration < 0.01 拍は 0.25 に補正）、
CC10 → `pan`、pitch wheel → `pitchBend` を取り込み。小節数は自動算出（最大 16 にクランプ）。

## 7. UI 仕様

### 7.1 レイアウト

ヘッダ（タイトル + プリセット選択/保存）、左サイドバー（MELODY / BASSLINE / CHORD /
SETTINGS + UNDO / REDO）、コントロールストリップ（ROOT/SCALE、BPM、**SEED（値表示 +
ロックトグル）**、PHRASE/OCTAVE、PATTERN/CHORDS）、**8 ノブ列**（DENSITY / ACID /
ORNAMENT / GRACE / RHYTHM / PITCH RNG / HUMANIZE / SWING）+ サブジャンルピル +
スタイル選択、ピアノロール、レーンエディタ（VELOCITY / PAN / PITCH BEND タブ）、フッタ
（GENERATE（分割ボタン、▾で部分再生成メニュー）/ VARIATION / EXPORT（分割ボタン、
グリップでD&D）/ IMPORT / PREVIEW 系 / QUICK SAVE）。

**ツールチップ**: 全コントロールに搭載。言語設定（英/日）に連動して切り替わる。

デザインは "Neon Architect"（`PsyMelodyLookAndFeel`）: surface `#0e0e13`、
primary シアン `#81ecff`、secondary マゼンタ `#ff59e3`、tertiary パープル `#ba84ff`、
角丸なし・1px 境界線なし（背景色シフトで区画）。

### 7.2 ピアノロール操作

| 操作 | 挙動 |
|---|---|
| **空白をダブルクリック** | **ノート追加**（16分グリッドに floor スナップ、duration 0.25 拍、velocity 0.8、追加後に単独選択） |
| ノートをドラッグ | 移動（16分スナップ、範囲クランプ） |
| ノート右端 6px をドラッグ | 長さ変更（最小 0.0625 拍） |
| ノートを右クリック | 削除（選択中なら選択全体） |
| Shift+クリック | 選択トグル |
| 空白をドラッグ | ラバーバンド矩形選択（Shift で追加選択） |
| 複数選択をドラッグ | まとめて移動 |
| Ctrl/Cmd+A / Escape | 全選択 / 選択解除 |
| Ctrl+C / Ctrl+V | コピー / 現在表示位置へペースト |
| Delete / Backspace | 選択削除 |
| Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y | Undo / Redo |
| ↑↓ / Shift+↑↓ | ±1 半音 / ±1 オクターブ |
| ホイール / トラックパッド横 | 縦 / 横スクロール |
| Ctrl+ホイール / Shift+ホイール | 横ズーム（0.5–8x）/ 縦ズーム（0.5–4x） |
| 右上ボタン | H±/V± ズーム、Fit |

ノート色: 通常シアン / アクセント マゼンタ / スライド グリーン / グレース パープル /
選択 イエロー。30Hz タイマーで再生ヘッド追従描画。

### 7.3 Undo / Redo

- 履歴上限 50。**実際に編集が発生する操作の直前にのみ**スナップショットを取る
  （ノート移動 / リサイズのドラッグ開始時、削除、追加、ペースト、レーン編集開始時、
  生成系ボタン）。選択のみのクリックでは履歴を消費しない。

### 7.4 レーンエディタ

Velocity（0.05–1.0）/ Pan（−1〜1）/ Pitch（±8192）をクリック / ドラッグで編集。
ピアノロールの横ズーム / スクロールに追従。

### 7.5 設定ページ・ローカライズ

- 言語: English / Japanese（Settings ページと GENERATE / VARIATION / EXPORT MIDI /
  QUICK SAVE ボタンに適用。メイン画面のパラメータ名は意図的に英語のまま）。
- Manual / About タブ（スクロール可能）。日本語表示にはシステムの日本語フォントを
  自動探索（Hiragino → Yu Gothic → Meiryo → Noto Sans CJK 等）。

## 8. 状態保存仕様（DAW プロジェクト）

`getStateInformation` / `setStateInformation` は `ValueTree("PsyMelodyState")` の
バイナリ形式で以下を保存・復元する:

1. **全 16 GeneratorParams**（読み込み時に §3.1 の範囲へクランプ検証）+ `seed`
   （juce::int64 経由）+ `seedLocked`
2. **編集済みシーケンス**: `Sequence > Note` 子ツリー
   （属性はユーザープリセットと同一: `nn, vel, pan, start, dur, pb, acc, sld, grace`）

復元時は `patternEngine.loadPhrase()` まで実行され、`stateVersion`（atomic カウンタ）が
インクリメントされる。開いている Editor は 10Hz タイマーで `stateVersion` を監視し、
変化を検知すると UI とピアノロールを再同期する。

旧バージョン（Sequence なし）の状態データはパラメータのみ復元される。

**保存されないもの**: GenMode / BassStyle / VoicingStyle、Preview 設定、言語、
Quick Save フォルダ、ズーム / スクロール位置、Undo 履歴。

## 9. 既知の制約

| 制約 | 内容 |
|---|---|
| ホストオートメーション不可 | APVTS 不使用（§3.3）。 |
| PlayHead 必須 | PlayHead を提供しないホストでは出力なし。プレビュー音もトランスポート再生中のみ。 |
| ウィンドウ固定 | 960×720、リサイズ・スケーリング非対応。 |
| シードの再現範囲 | シードが再現するのは GENERATE 結果のみ。VARIATION / 部分再生成 / 手編集後のフレーズはシードから再現不可（フレーズ自体は状態保存で残る）。Undo はシード値を復元しない。 |
| MIDI チャンネル 1 固定 | 出力・エクスポートとも。パン CC10 はチャンネル単位のため同時発音内での個別パンは不可。 |
| フレーズ跨ぎノート | ノートはフレーズ長を超えられない（生成時にフレーズ長が終端まで切り上がるため実質発生しない）。終端ちょうどのノートはループ直前で note-off。 |
| Preview フィルタ固定 | カットオフ 2.6kHz 相当・レゾナンス 0.2（UI 非公開）。 |
| try-lock スキップ | フレーズ差し替えと衝突したオーディオブロックは MIDI 生成を 1 ブロック分スキップする（実用上不可知）。 |
| 未使用 API | `MidiExport::toMidiSequence()`（クリップボード用）は未使用。 |

## 10. ビルド

```
cmake -B build   # JUCE は ../JUCE に配置
cmake --build build -j8
```

成果物: `build/PsyMelody_artefacts/Release/`（Standalone / VST3 / AU）。
`JUCE_WEB_BROWSER=0`、`JUCE_USE_CURL=0`、スプラッシュ非表示、推奨警告 + LTO 有効。
