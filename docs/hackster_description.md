# 応募用説明文 — M5Stack Global Innovation Contest 2026

Hackster.io 投稿用の説明文です。英語版(グローバル審査向け・推奨)と日本語版の両方を用意しています。
画像は `docs/images/` のものをそのまま使えます (実機と同じカラーテーブルから生成したモックアップ)。

---

## タイトル / Title

**EN:** iroirotokei — A Color Clock That Paints Time
**JA:** 色時計 (iroirotokei) — 時間を色で描く時計

## 一言紹介 / Elevator pitch

**EN:** A round AMOLED clock that slowly shifts through the colors of the sky — from dawn to dusk — over 24 hours. Mix your own color from 16.7M candidates, hear every color as a note, and tilt the device to blend paints like watercolor.
**JA:** 24時間かけて空の色 (朝焼け→昼→夕焼け→夕闇) をゆっくりと移ろう円形AMOLEDの時計。1,677万色から自分だけの色を混ぜて作れて、その色は「音」にもなる。傾ければ水彩絵の具のようににじんで混ざる。

---

## 本文 (English)

### The idea

We usually read time as numbers. But before clocks, people felt time through the color of the sky.
**iroirotokei** ("iro" = color, "tokei" = clock in Japanese) brings that feeling back: the entire 466×466 round AMOLED display of the M5Stack StopWatch becomes a single, slowly changing color that follows the sky of a real day — deep midnight navy, pre-dawn purple, sunrise coral, daylight blue, golden hour, sunset red, and twilight violet.

The color is recomputed **every second** by linearly interpolating between 14 key colors placed across 24 hours, so the change is imperceptibly smooth — like the real sky. A gradient ring around the bezel shows the whole day's palette at a glance, with a white marker at "now". Internally colors are computed in 24-bit (16.7M colors).

Two ways to play with time itself:

- **Time travel:** the dial is 24 hours, so just touch the screen and drag around the circle — the display jumps to that hour's color, playing a note for every hour you cross. Release to snap back to now.
- **Day replay:** long-press the right button and the whole 24-hour journey replays in about 48 seconds, sounding each hour's color as a note — the day becomes a color-and-melody timelapse. Perfect for demos.

### Make your own color — and hear it

Long-press the left button to enter **Color Maker** mode:

- Left button: pick a pigment from a 10-color palette
- Right button: mix it in — the mix is a running average, so it behaves like real paint: the more you add, the subtler each drop becomes
- Every mix plays **the sound of the color you just made**: hue is mapped onto a two-octave pentatonic scale (red = low, violet = high; grays follow brightness). You can literally search for the sound of your favorite color.
- Stuck? **Shake the device** and a completely random color out of 16.7 million appears — then refine it by mixing.

### Voice Color — what does your voice look like?

Switch to **Voice Color** mode and just talk: the microphone turns your voice into color in real time — pitch becomes hue (low voices are red, high voices are violet) and loudness becomes brightness. When you stop talking, the device answers by playing *the sound of your voice's color*. Singing makes the colors dance. (The mic and speaker share the audio codec, so the firmware swaps between them automatically.)

### Tilt Paint — watercolor by gravity

Using the built-in BMI270 IMU, **Tilt Paint** mode turns the whole screen into wet watercolor paper. Pick a paint pot for each button (left and right), then simply tilt the device: paint flows toward the low side and pools there — strong where it gathers, faint where it thins out. A per-frame diffusion pass makes neighboring colors bleed into each other exactly like wet-on-wet watercolor. While you tilt, high pentatonic notes twinkle randomly like stardust — the harder you tilt, the louder and faster they sparkle. A long press wipes the paper clean.

### Fill the room — ambient light with Unit HEX

Plug an M5Stack **Unit HEX** (37× SK6812 LEDs) into Port A and the panel becomes ambient light: all 37 LEDs mirror the "current color" of every mode — the sky color of the clock, the color you just mixed, the average of your watercolor painting, even your voice's color. LED on/off, fine brightness (2–100%) and speaker volume are all adjusted on-device with circular gauge screens, and every setting persists in flash. The unit is optional — the firmware runs perfectly without it.

### Random mode

A mode for letting go: the clock keeps ticking while the background drifts to a new random color every few seconds, and the HEX panel becomes a starfield of 37 independently twinkling random colors.

### Always on time — Wi-Fi + NTP

Set up Wi-Fi once from your phone (hold the right button at power-on; the device becomes an access point with a captive portal). From then on the clock syncs to internet time at every boot and daily at 3 AM, writes it to the battery-backed RTC, then switches Wi-Fi off to save power.

### Sounds of stars, sky and earth

All feedback sounds are designed around a celestial theme: a star-twinkle for mode switching, a distant-bell arpeggio at the top of every hour, and a single low "earth" tone when muting. Sound can be toggled with one click and the setting persists in flash.

### And it is still a stopwatch

True to the device's name, a lap-capable 1/100s stopwatch is one button away.

### How it works

- **Hardware:** M5Stack StopWatch (ESP32-S3R8, 1.75" round AMOLED 466×466 / CO5300, BMI270 IMU, RX8130CE RTC, ES8311 codec + 1W speaker, two buttons)
- **Firmware:** PlatformIO + Arduino framework + M5Unified/M5GFX
- **Color engine:** 14 keyframes → per-second linear interpolation over 86,400 seconds
- **Color mixing:** cumulative average in floating point
- **Watercolor sim:** 58×58 float grid; gravity-weighted paint deposit + diffusion at ~15 fps, rendered via a zoomed sprite
- **Voice-to-color:** 16 kHz mic frames → RMS (loudness→brightness) + zero-crossing rate (pitch→hue), with automatic mic/speaker codec swapping
- **Color-to-sound:** RGB → hue → 10-step pentatonic scale (C5–A6), played as note + octave sparkle
- **Ambient light:** FastLED over Grove Port A (Unit HEX, SK6812×37), power-limited to 5 V / 500 mA, cold-boot-safe data line handling
- **Timekeeping:** Wi-Fi NTP auto-sync (smartphone captive-portal setup, resync daily at 3 AM) + battery-backed RTC + serial/PowerShell fallback
- Full source code, color table and build instructions: **https://github.com/aokko2000/iroirotokei**

### Build steps

1. Flash the firmware: open the repo with VSCode + PlatformIO, hold the power button to enter download mode, and hit Upload — all libraries are fetched automatically.
2. Plug the Unit HEX into Port A (optional).
3. Set up Wi-Fi once from your phone (hold the right button at power-on) — the clock syncs itself from then on.
4. The 3D-printed desk stand holds the StopWatch upright with the Unit HEX embedded below, so the panel glows through from under the clock.

### Why it matters

iroirotokei turns a utilitarian gadget into ambient art: you learn to *feel* what 6 AM looks like. The color-mixing mode doubles as a playful color-theory toy for kids — additive mixing, hue, brightness and even synesthesia (what does teal sound like?) — all on a palm-sized device.

---

## 本文 (日本語)

### アイデア

私たちは普段、時間を「数字」で読んでいます。でも時計がなかった頃、人は空の色で時間を感じていました。
**色時計 (iroirotokei)** はその感覚を取り戻す試みです。M5Stack StopWatch の円形AMOLED (466×466) 全体がひとつの「色」になり、実際の1日の空 — 深夜の紺、未明の紫、朝焼け、昼の青空、黄金の時間、夕焼け、夕闇 — をなぞって24時間かけてゆっくりと変化します。

色は24時間に配置した14個のキーカラーを**毎秒**線形補間して作るので、本物の空のように気づかないほど滑らかに移ろいます。画面の縁には1日分のパレットがリングとして表示され、白いマーカーが「いま」を指します。色の計算は内部24bit (1,677万色) です。

時間そのものと遊ぶ仕掛けも2つ:

- **タイムトラベル:** 文字盤=24時間なので、画面にタッチして円をなぞるだけで、その角度の時刻の色へ瞬間移動。「時」を越えるたびに音が鳴り、指を離せば現在に戻ります。
- **1日の再生:** 右ボタン長押しで、24時間の色の旅を約48秒でタイムラプス再生。1時間ごとにその色の音が鳴り、1日が色とメロディで流れます。デモに最適。

### 自分の色を作る — そして聴く

左ボタン長押しで「色づくり」モードへ:

- 左ボタン: 10色のパレットから絵の具を選ぶ
- 右ボタン: 混ぜる — 累積平均なので本物の絵の具のように、混ぜるほど1滴の影響が繊細になります
- 混ぜるたびに、**できた色そのものの音**が鳴ります。色相を2オクターブのペンタトニック音階に対応させました (赤は低く、紫は高く。無彩色は明るさで決まる)。「好きな色の音」を探す遊びができます。
- 迷ったら**本体を振る**。1,677万色から完全ランダムな色が現れ、そこから混ぜて調整できます。

### 声の色 — あなたの声は何色?

「声の色」モードに切り替えて話しかけるだけ。マイクが声をリアルタイムに色へ変換します — 声の高さが色相に (低い声は赤、高い声は紫)、大きさが明るさに。話し終わると、デバイスが「あなたの声の色の音」を鳴らして返してくれます。歌うと色が踊ります。(マイクとスピーカーはコーデックを共有しているため、ファームウェアが自動で切り替えます)

### 傾き絵の具 — 重力で描く水彩

内蔵IMU (BMI270) を使った「傾き絵の具」モードでは、画面全体が濡れた水彩紙になります。左右のボタンでそれぞれの絵の具つぼの色を選び、あとは本体を傾けるだけ。絵の具は低い側へ流れてそこに溜まります — 集まる場所は濃く、離れた場所は淡く。毎フレームの拡散処理で、隣りあう色が「にじみ」のようにじわっと混ざります。傾けている間は星屑のような高音のペンタトニックがランダムにまたたき、強く傾けるほど大きく・速く鳴ります。長押しで紙を真っさらに。

### 部屋ごと染める — Unit HEXの環境光

PortAにM5Stack **Unit HEX** (SK6812 LED×37) をつなぐと、六角形のパネルが環境光になります。37個のLEDが全モードの「いまの色」と連動 — 時計なら空の色、色づくりなら混ぜた色、傾き絵の具なら画面の平均色、声の色ならあなたの声の色。LEDのオン/オフ・明るさ (2〜100%)・スピーカー音量は本体の円形ゲージ画面で細かく調整でき、設定はすべて保存されます。Unitはオプションで、未接続でも全機能が動きます。

### 色ランダムモード

時計は秒まで動いたまま、背景色が数秒ごとにランダムな色へゆっくり漂いつづけるモード。HEXパネルは37個がバラバラにきらめく星空になります。

### いつでも正確 — Wi-Fi + NTP

Wi-Fi設定はスマホから一度だけ (右ボタンを押しながら電源オンでデバイスがアクセスポイントになり、設定画面が開きます)。以降は起動のたびと毎日3時にインターネット時刻へ自動同期し、電池バックアップ付きRTCにも書き込んで、同期後はWi-Fiを切って省電力に。

### 星・空・地球の音

効果音はすべて天体をテーマにデザインしました。モード切替は星のまたたき、毎正時は遠くの鐘のようなアルペジオ、消音時は大地を思わせる低い一音。音はワンクリックでオン/オフでき、設定はフラッシュに保存されます。

### ストップウォッチも健在

デバイス名に恥じない、ラップ機能付き1/100秒ストップウォッチもボタンひとつで使えます。

### 技術構成

- **ハードウェア:** M5Stack StopWatch (ESP32-S3R8、1.75インチ円形AMOLED 466×466 / CO5300、BMI270 IMU、RX8130CE RTC、ES8311コーデック+1Wスピーカー、ボタン2個)
- **ファームウェア:** PlatformIO + Arduinoフレームワーク + M5Unified/M5GFX
- **カラーエンジン:** 14キーフレーム → 86,400秒を毎秒線形補間
- **混色:** 浮動小数点の累積平均
- **水彩シミュレーション:** 58×58の浮動小数点格子。重力方向に重み付けした流し込み+にじみ拡散を約15fpsで計算し、スプライト拡大描画
- **声→色:** 16kHzのマイクフレームからRMS (大きさ→明るさ) とゼロ交差率 (高さ→色相) を算出。マイク/スピーカーはコーデック共有のため自動交代
- **色→音:** RGB → 色相 → 10段階ペンタトニック音階 (C5–A6)、基音+オクターブのきらめき
- **環境光:** Grove PortA経由のFastLED制御 (Unit HEX / SK6812×37)。5V/500mAの電力制限、コールドスタート対策済み
- **時刻:** Wi-Fi NTP自動同期 (スマホから設定、毎日3時に再同期) + 電池バックアップ付きRTC + シリアル/PowerShellの手動手段も併備
- ソースコード・カラーテーブル・ビルド手順は **https://github.com/aokko2000/iroirotokei** に全公開。

### つくりかた

1. ファームウェア書き込み: リポジトリをVSCode+PlatformIOで開き、電源ボタン長押しでダウンロードモードにしてUpload (ライブラリは自動取得)
2. Unit HEXをPortAに接続 (オプション)
3. Wi-Fi設定をスマホから一度だけ (右ボタンを押しながら電源オン) — 以降は時刻が自動で合う
4. 3Dプリント製の卓上スタンドにStopWatchを立て、下部にUnit HEXを組み込み — 時計の足元からパネルの光がにじむ構成

### この作品の意味

色時計は実用ガジェットをアンビエントアートに変えます。使ううちに「朝6時の色」を体で覚えていきます。色づくりモードは子ども向けの色彩教育おもちゃにもなります — 混色、色相、明度、さらには共感覚 (ティール色はどんな音?) まで、手のひらの上で。

---

## ハードウェア一覧 / Things used

- M5Stack StopWatch × 1
- M5Stack Unit Hex (SK6812×37) × 1 — オプション、PortAに接続
- 3Dプリント製 卓上スタンド (自作、Unit Hex組み込み) × 1
- USB Type-C cable × 1 (書き込み用)

## 応募メモ

- 締切: 2026年8月7日 23:59 (PST)
- 手順: ① Hackster.io にプロジェクト公開 (この説明文+写真/動画+GitHubリポジトリ) → ② 公式Googleフォームで応募
- 動画デモ推奨: 「朝焼け→昼への変化 (時刻を5:20に設定して10分早回し)」「色づくりで混ぜて音が変わる様子」「毎正時の鐘」を撮ると審査基準 (創造性/完成度/見せ方/インパクト) に効きます
- GitHubリポジトリ: https://github.com/aokko2000/iroirotokei (公開済み・最新)
- 動画の追加おすすめ: 「傾き絵の具で傾けて混ぜる」「声の色で歌う」「Unit HEXが空の色で光る様子」「色ランダムの星空きらめき」
