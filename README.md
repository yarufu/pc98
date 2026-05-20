## 見出し

# ADV98.EXE

NEC PC-9801 / PC-9821 シリーズ向け  
16色ADVゲームエンジン（開発中）

PC-98風ADVゲームを作成することを目的とした、
スクリプト型ADVエンジンです。

## 動画

<a href="https://www.youtube.com/watch?v=tbKbpCH4YyY">
  <img src="docs/youtube_thumb.png" width="480">
</a>

ChatGPT と Codex を使って開発したPC-98 ADVゲームエンジンの実機動作動画です。

## スクリーンショット

（画像を貼る）

## 機能

- 背景表示
- 立ち絵表示
- メッセージ表示
- 選択肢
- 分岐
- save/load
- PMD BGM対応

## 動作確認環境

### 実機
- PC-9821Ra43
- PC-9821V13

### エミュレータ
- T98-NEXT

## 開発環境

- ia16-elf-gcc
- WSL Ubuntu
- VS Code
- ChatGPT + Codex

## About

このプロジェクトは、
ChatGPT と Codex の支援を受けながら
PokuG が開発しています。

AI は：

- コード生成
- デバッグ
- リファクタリング
- 実装支援

などに利用しています。

全体設計、統合、素材制作、
最終判断は作者が行っています。

## 必須ファイル

- ADV98.EXE
- script.txt

## Optional

### PMD.COM
PMD.COM 常駐時のみ BGM が有効になります。

未常駐時：
- #bgm
- #bgmstart
- #bgmstop
- #bgmfade

は無視されます。

### マウスドライバ
未常駐でも動作します。  
その場合はキーボード操作のみになります。

## debug.txt

ファイル読み込み失敗時などの情報を
debug.txt に出力します。

## 現在の状態

開発中です。  
仕様は変更される可能性があります。

## Blog

開発記録：
https://pokug.net/

## License

MIT License
