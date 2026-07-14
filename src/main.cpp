// =====================================================================
// 色時計 (iroirotokei) for M5Stack StopWatch
//   - 色時計       : 24時間かけて朝焼け〜夕闇の色へゆっくり変化する時計
//   - 色づくり     : 左ボタンで色を選び、右ボタンで混ぜてオリジナル色を作る
//   - ストップウォッチ
//
// 操作方法:
//   左ボタン(KEYA/黄) 長押し ... モード切替 (色時計→色づくり→ストップウォッチ)
//   [色時計]    左短押し: LEDのオン/オフ
//               左ダブルクリック: LED明るさ調整画面 (左:暗く 右:明るく、3秒放置で決定)
//               右短押し: 音のオン/オフ (設定はすべて保存される)
//               右長押し: 1日の色を約48秒で再生 (タイムラプス)
//               画面をタッチしてなぞる: その角度の時刻の色を覗く (タイムトラベル)
//   [色づくり]  左短押し: パレットの色を選択 / 右短押し: 選んだ色を混ぜる
//               右長押し: 混ぜた色をリセット / 本体を振る: ランダム色
//   [傾き絵の具] 左短押し: 左の絵の具の色 / 右短押し: 右の絵の具の色
//               本体を傾けると傾けた側に絵の具が流れて水彩のように混ざる
//               右長押し: 紙をリセット
//   [声の色]    話しかけると声の高さ→色相、大きさ→明るさで色になる
//               話し終わると「声の色」の音が返ってくる / 右短押し: もう一度聴く
//   [ストップウォッチ] 左短押し: スタート/ストップ
//                      右短押し: ラップ(計測中) / リセット(停止中)
//
// 音: 星・空・地球をイメージしたきらめき系の音。
//     モード切替時 / 色を混ぜたとき(できた色が音になる) / 毎正時 に鳴る。
//
// 時刻合わせ:
//   [自動] 右ボタンを押しながら電源オン → Wi-Fi設定モード。
//          スマホでAP「iroirotokei」に接続し、自宅Wi-Fiを設定すると
//          以降は起動時と毎日3時にNTPで自動同期する。
//   [手動] シリアルモニタ(115200bps)から
//          T 12:34:56 / D 2026-07-13 12:34:56 / N (今すぐNTP同期)
// =====================================================================

#include <M5Unified.h>
#include <Preferences.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include <sys/time.h>

// 左右ボタンの割り当て (物理配置が逆だったらここを入れ替える)
static m5::Button_Class &btnL() { return M5.BtnA; }  // KEYA (黄)
static m5::Button_Class &btnR() { return M5.BtnB; }  // KEYB (青)

static constexpr int CX = 233;  // 画面中心 (466x466 円形)
static constexpr int CY = 233;
static constexpr int R  = 233;

// ---------------------------------------------------------------------
// モード
// ---------------------------------------------------------------------
enum Mode : uint8_t { MODE_CLOCK, MODE_MIXER, MODE_TILT, MODE_VOICE, MODE_STOPWATCH, MODE_COUNT };
static Mode mode = MODE_CLOCK;
static bool fullRedraw = true;

// ---------------------------------------------------------------------
// 24時間カラーテーブル (分, R, G, B) : 区間ごとに線形補間して滑らかに変化
// ---------------------------------------------------------------------
struct RGB8 { uint8_t r, g, b; };

struct ColorKey { uint16_t minute; RGB8 c; };
static const ColorKey DAY_COLORS[] = {
    {   0, {  8,  10,  35}},  // 0:00  深夜の紺
    { 180, { 12,  12,  48}},  // 3:00  未明
    { 270, { 45,  35,  90}},  // 4:30  薄明の紫
    { 330, {255,  94,  77}},  // 5:30  朝焼け
    { 390, {255, 170,  90}},  // 6:30  朝の金色
    { 480, {135, 206, 235}},  // 8:00  朝の空色
    { 720, {100, 180, 255}},  // 12:00 昼の青空
    { 900, {120, 190, 240}},  // 15:00 午後
    {1020, {255, 180,  80}},  // 17:00 黄金の時間
    {1110, {255,  90,  60}},  // 18:30 夕焼け
    {1170, {150,  60, 120}},  // 19:30 夕闇のマゼンタ
    {1230, { 60,  40, 110}},  // 20:30 宵の紫
    {1320, { 20,  20,  60}},  // 22:00 夜の青
    {1440, {  8,  10,  35}},  // 24:00 → 0:00 に戻る
};
static constexpr size_t NUM_KEYS = sizeof(DAY_COLORS) / sizeof(DAY_COLORS[0]);

// その日の経過秒 → 色 (区間線形補間)
static RGB8 colorAtSecond(uint32_t secOfDay)
{
    for (size_t i = 0; i + 1 < NUM_KEYS; ++i) {
        uint32_t t0 = DAY_COLORS[i].minute * 60u;
        uint32_t t1 = DAY_COLORS[i + 1].minute * 60u;
        if (secOfDay >= t0 && secOfDay <= t1) {
            float f = (t1 == t0) ? 0.f : float(secOfDay - t0) / float(t1 - t0);
            const RGB8 &a = DAY_COLORS[i].c;
            const RGB8 &b = DAY_COLORS[i + 1].c;
            return RGB8{
                uint8_t(a.r + (b.r - a.r) * f + 0.5f),
                uint8_t(a.g + (b.g - a.g) * f + 0.5f),
                uint8_t(a.b + (b.b - a.b) * f + 0.5f),
            };
        }
    }
    return DAY_COLORS[0].c;
}

static uint32_t rgb888(const RGB8 &c) { return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b; }

// 背景色に対して読みやすい文字色 (輝度で白黒を選ぶ)
static uint32_t contrastColor(const RGB8 &c)
{
    float luma = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
    return (luma > 145.f) ? 0x000000u : 0xFFFFFFu;
}

// ---------------------------------------------------------------------
// 音 (星・空・地球をイメージしたきらめき系サウンド)
//   ノンブロッキングの簡易シーケンサ: loop() から updateSound() を回す
// ---------------------------------------------------------------------
static Preferences prefs;
static bool soundOn = true;
static bool spkOk   = false;
static bool speakerActive = true;  // 声の色モード中はマイクと交代でオフになる

struct ToneStep { uint16_t freq; uint16_t durMs; uint16_t gapMs; };
static ToneStep sndSeq[6];
static int      sndLen = 0, sndPos = 0;
static uint32_t sndNextMs = 0;

static void playSeq(const ToneStep *steps, int n)
{
    if (!soundOn || !spkOk || !speakerActive) return;
    if (n > 6) n = 6;
    memcpy(sndSeq, steps, n * sizeof(ToneStep));
    sndLen = n;
    sndPos = 0;
    sndNextMs = millis();
}

static void updateSound()
{
    if (sndPos >= sndLen || millis() < sndNextMs) return;
    const ToneStep &s = sndSeq[sndPos];
    M5.Speaker.tone(s.freq, s.durMs);
    sndNextMs = millis() + s.durMs + s.gapMs;
    ++sndPos;
}

// 星のまたたき (モード切替)
static void soundModeSwitch()
{
    static const ToneStep s[] = {{1568, 50, 25}, {2093, 80, 0}};
    playSeq(s, 2);
}

// 小さなクリック (色の選択)
static void soundSelect()
{
    static const ToneStep s[] = {{1319, 20, 0}};
    playSeq(s, 1);
}

// 遠い鐘のような上昇アルペジオ (毎正時 / 空と宇宙のイメージ)
static void soundHourChime()
{
    static const ToneStep s[] = {{1047, 130, 70}, {1568, 130, 70}, {2093, 240, 0}};
    playSeq(s, 3);
}

static void soundToggleOn()
{
    static const ToneStep s[] = {{784, 45, 25}, {1568, 70, 0}};
    playSeq(s, 2);
}

static void soundToggleOff()  // オフにする直前に鳴らす低い一音 (地球=大地のイメージ)
{
    static const ToneStep s[] = {{392, 80, 0}};
    playSeq(s, 1);
}

// ペンタトニック(五音)音階 2オクターブ分。夜空のような浮遊感のある音階。
static const uint16_t PENTA[] = {523, 587, 659, 784, 880, 1047, 1175, 1319, 1568, 1760};

// 色 → ペンタトニックの音階番号 (色相→高さ、無彩色は明るさ→高さ)
static int colorNoteIndex(const RGB8 &c)
{
    float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    float mx = max(r, max(g, b)), mn = min(r, min(g, b));
    if (mx - mn < 0.04f) {
        return int(mx * 9.99f);  // 白黒グレーは明るいほど高い音
    }
    float hue;
    if (mx == r)      hue = 60.f * fmodf((g - b) / (mx - mn), 6.f);
    else if (mx == g) hue = 60.f * ((b - r) / (mx - mn) + 2.f);
    else              hue = 60.f * ((r - g) / (mx - mn) + 4.f);
    if (hue < 0) hue += 360.f;
    return int(hue / 360.f * 9.99f);
}

// 「作った色」を音に変える
static void soundOfColor(const RGB8 &c)
{
    int idx = colorNoteIndex(c);
    ToneStep s[2] = {{PENTA[idx], 90, 30}, {uint16_t(PENTA[idx] * 2), 60, 0}};
    playSeq(s, 2);
}

// タイムトラベル/タイムラプス中、「時」を越えるたびに鳴らす一音
static void soundHourNote(const RGB8 &c)
{
    if (!soundOn || !spkOk || !speakerActive) return;
    M5.Speaker.setChannelVolume(2, 140);
    M5.Speaker.tone(PENTA[colorNoteIndex(c)], 90, 2, true);
}

// ---------------------------------------------------------------------
// Unit Hex (PortA / G10, SK6812×37) — 画面の「いまの色」と連動するLED
// ---------------------------------------------------------------------
static constexpr int HEX_PIN = 10;
// 公称は37個。個体差や基板リビジョンで数が違っても全部灯るよう
// 余裕を持って送る (余ったデータは最後のLEDを素通りするだけで無害)
static constexpr int HEX_N   = 40;
static CRGB hexLeds[HEX_N];
static uint8_t  ledLevel = 96;    // 明るさ 4〜255 (NVSから復元)
static bool     ledOn    = true;  // オン/オフ (NVSから復元)
static uint32_t ledAdjLastInput = 0;
static bool     ledAdjDirty     = true;

// ---------------------------------------------------------------------
// 時刻管理 (RTC があれば RTC、無ければ内部クロック)
// ---------------------------------------------------------------------
static bool rtcOk = false;

static void systemTimeFromTm(struct tm &t)
{
    time_t epoch = mktime(&t);
    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
}

static void writeRtc(struct tm &t)
{
    if (!rtcOk) return;
    mktime(&t);  // tm_wday を正規化
    m5::rtc_datetime_t dt;
    dt.date.year    = t.tm_year + 1900;
    dt.date.month   = t.tm_mon + 1;
    dt.date.date    = t.tm_mday;
    dt.date.weekDay = t.tm_wday;
    dt.time.hours   = t.tm_hour;
    dt.time.minutes = t.tm_min;
    dt.time.seconds = t.tm_sec;
    M5.Rtc.setDateTime(dt);
}

// 現在時刻は常にESP32内部クロックから取る (確実に進む)。
// RTCは「起動時の読み込み元」と「時刻合わせ時の書き込み先」として使う。
static void getNow(struct tm &out)
{
    time_t now = time(nullptr);
    localtime_r(&now, &out);
}

// ビルド時刻 (__DATE__ = "Jul 13 2026") を tm に変換
static void buildTimeTm(struct tm &t)
{
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {};
    int day, year, hh, mm, ss;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
    t = {};
    const char *p = strstr(months, mon);
    t.tm_mon  = p ? int(p - months) / 3 : 0;
    t.tm_mday = day;
    t.tm_year = year - 1900;
    t.tm_hour = hh;
    t.tm_min  = mm;
    t.tm_sec  = ss;
    mktime(&t);
}

static void setupClock()
{
    setenv("TZ", "JST-9", 1);   // 日本時間 (NTP同期で使用)
    tzset();
    rtcOk = M5.Rtc.isEnabled();
    struct tm bt;
    buildTimeTm(bt);
    if (rtcOk) {
        auto dt = M5.Rtc.getDateTime();
        if (dt.date.year >= 2023) {
            // RTC の時刻を内部クロックへ同期
            struct tm t = {};
            t.tm_year = dt.date.year - 1900;
            t.tm_mon  = dt.date.month - 1;
            t.tm_mday = dt.date.date;
            t.tm_hour = dt.time.hours;
            t.tm_min  = dt.time.minutes;
            t.tm_sec  = dt.time.seconds;
            systemTimeFromTm(t);
        } else {
            writeRtc(bt);          // RTC が未設定ならビルド時刻を書く
            systemTimeFromTm(bt);
        }
    } else {
        systemTimeFromTm(bt);
    }
}

// --- Wi-Fi + NTP 自動時刻同期 ---
enum SyncState : uint8_t { SY_IDLE, SY_CONNECTING, SY_WAITTIME };
static SyncState syncState   = SY_IDLE;
static uint32_t  syncStartMs = 0;
static int       lastSyncDay = -1;

static void startNtpSync()
{
    if (syncState != SY_IDLE) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin();               // 保存済みのWi-Fi設定で接続
    syncState = SY_CONNECTING;
    syncStartMs = millis();
}

static void stopWifi()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    syncState = SY_IDLE;
}

static void updateNtpSync()
{
    if (syncState == SY_IDLE) return;
    if (syncState == SY_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            configTzTime("JST-9", "ntp.jst.mfeed.ad.jp", "pool.ntp.org", "time.google.com");
            syncState = SY_WAITTIME;
            syncStartMs = millis();
        } else if (millis() - syncStartMs > 15000) {
            stopWifi();          // Wi-Fi未設定 or 圏外: あきらめて続行
        }
        return;
    }
    // SY_WAITTIME: NTPの時刻が入るのを待つ
    struct tm t;
    getNow(t);
    if (t.tm_year + 1900 >= 2024) {
        writeRtc(t);             // RTCにも書いておく
        stopWifi();
        Serial.println("OK: NTP time synced");
    } else if (millis() - syncStartMs > 15000) {
        stopWifi();
    }
}

// 起動時に右ボタンが押されていたら: Wi-Fi設定モード (スマホから設定)
static void runWifiPortal()
{
    M5.Display.fillScreen(0x000000u);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::lgfxJapanGothic_24);
    M5.Display.setTextColor(0xFFFFFFu, 0x000000u);
    M5.Display.drawString("Wi-Fi設定モード", CX, 130);
    M5.Display.drawString("スマホのWi-Fi設定から", CX, 185);
    M5.Display.drawString("「iroirotokei」に接続して", CX, 220);
    M5.Display.drawString("自宅のWi-Fiを選んでください", CX, 255);
    M5.Display.drawString("(3分でタイムアウト)", CX, 310);

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    bool ok = wm.startConfigPortal("iroirotokei");

    M5.Display.fillScreen(0x000000u);
    M5.Display.drawString(ok ? "接続できました! 再起動します" : "設定されませんでした", CX, CY);
    delay(2000);
    ESP.restart();
}

// シリアルからの時刻合わせ  "T 12:34:56" / "D 2026-07-13 12:34:56" / "N"(NTP同期)
// 1文字ずつ溜めて、改行(\r か \n)が来たら1行として処理する
static void handleSerialTimeSet()
{
    static String line;
    bool complete = false;
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch == '\r' || ch == '\n') {
            if (line.length() > 0) { complete = true; break; }
        } else if (line.length() < 40) {
            line += ch;
        }
    }
    if (!complete) return;
    String work = line;
    line = "";
    work.trim();
    if (work.length() < 3) return;

    if (work[0] == 'N' || work[0] == 'n') {
        Serial.println("OK: starting NTP sync...");
        startNtpSync();
        return;
    }

    struct tm t;
    getNow(t);
    int y, mo, d, hh, mm, ss;
    bool ok = false;
    if ((work[0] == 'D' || work[0] == 'd') &&
        sscanf(work.c_str() + 1, " %d-%d-%d %d:%d:%d", &y, &mo, &d, &hh, &mm, &ss) == 6) {
        t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
        t.tm_hour = hh; t.tm_min = mm; t.tm_sec = ss;
        ok = true;
    } else if ((work[0] == 'T' || work[0] == 't') &&
               sscanf(work.c_str() + 1, " %d:%d:%d", &hh, &mm, &ss) == 3) {
        t.tm_hour = hh; t.tm_min = mm; t.tm_sec = ss;
        ok = true;
    }
    if (ok) {
        writeRtc(t);
        systemTimeFromTm(t);
        fullRedraw = true;
        Serial.println("OK: time set");
    } else {
        Serial.println("NG: use  T 12:34:56  or  D 2026-07-13 12:34:56");
    }
}

// ---------------------------------------------------------------------
// モード1: 色時計
// ---------------------------------------------------------------------
static RGB8 lastBg = {1, 1, 1};
static int lastRingMin = -1;
static int lastClockSec = -1;

// 画面の縁に「1日の色」のリングと現在時刻マーカーを描く
static void drawDayRing(uint32_t secOfDay)
{
    for (int i = 0; i < 144; ++i) {  // 10分刻み
        RGB8 c = colorAtSecond(i * 600u);
        float a0 = i * 2.5f - 90.f;   // 0:00 を真上に
        M5.Display.fillArc(CX, CY, R - 1, R - 26, a0, a0 + 2.6f, rgb888(c));
    }
    float a = secOfDay / 86400.f * 360.f - 90.f;
    M5.Display.fillArc(CX, CY, R - 1, R - 30, a - 1.5f, a + 1.5f, 0xFFFFFFu);
}

static const char *WDAY_JP[] = {"日", "月", "火", "水", "木", "金", "土"};

// --- 色時計のサブモード ---
//   CS_SCRUB : タッチで文字盤をなぞって好きな時刻の色を覗く (タイムトラベル)
//   CS_LAPSE : 右長押しで1日の色を約48秒で再生 (タイムラプス)
//   CS_LEDADJ: 左ダブルクリックでLEDの明るさ調整
enum ClockSub : uint8_t { CS_NORMAL, CS_SCRUB, CS_LAPSE, CS_LEDADJ };
static ClockSub clockSub      = CS_NORMAL;
static bool     touchOk       = false;
static uint32_t lapseStartMs  = 0;
static uint32_t lapseBaseSec  = 0;
static int      prevHourNote  = -1;
static RGB8      lastPrevBg    = {1, 1, 1};

static void clockResetDraw()  // 通常表示へ戻るときの再描画準備
{
    fullRedraw   = true;
    lastClockSec = -1;
    lastBg       = {1, 1, 1};
    lastRingMin  = -1;
}

// 指定時刻の色を全画面プレビュー (スクラブ/タイムラプス共用)
static void drawColorPreview(uint32_t sec, const char *label)
{
    RGB8 bg = colorAtSecond(sec);
    uint32_t bg24 = rgb888(bg), fg = contrastColor(bg);
    if (bg.r != lastPrevBg.r || bg.g != lastPrevBg.g || bg.b != lastPrevBg.b) {
        M5.Display.fillScreen(bg24);
        lastPrevBg = bg;
    }
    int hh = sec / 3600, mm = (sec / 60) % 60;

    char buf[16];
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(fg, bg24);
    M5.Display.setFont(&fonts::Font7);
    M5.Display.setTextSize(1.8f);
    snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
    M5.Display.drawString(buf, CX, CY - 20);

    M5.Display.setFont(&fonts::lgfxJapanGothic_24);
    M5.Display.setTextSize(1);
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", bg.r, bg.g, bg.b);
    M5.Display.drawString(buf, CX, CY + 70);
    M5.Display.drawString(label, CX, CY + 115);

    if (hh != prevHourNote) {  // 「時」を越えたらその色の音
        prevHourNote = hh;
        soundHourNote(bg);
    }
}

// --- LEDの明るさ調整画面 (左:暗く 右:明るく、3秒放置で決定) ---
static void drawLedAdjust()
{
    if (!ledAdjDirty && !fullRedraw) return;
    if (fullRedraw) {
        M5.Display.fillScreen(0x101010u);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setFont(&fonts::lgfxJapanGothic_28);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(0xFFFFFFu, 0x101010u);
        M5.Display.drawString("LEDの明るさ", CX, 70);
        M5.Display.setFont(&fonts::lgfxJapanGothic_24);
        M5.Display.setTextColor(0x808080u, 0x101010u);
        M5.Display.drawString("左: 暗く   右: 明るく", CX, 360);
        M5.Display.drawString("3秒そのままで決定", CX, 396);
        fullRedraw = false;
    }
    ledAdjDirty = false;

    float f = ledLevel / 255.f;
    M5.Display.fillArc(CX, CY, 214, 200, -90, -90 + f * 360, 0xF9C46Bu);
    M5.Display.fillArc(CX, CY, 214, 200, -90 + f * 360, 270, 0x303030u);

    char buf[8];
    snprintf(buf, sizeof(buf), "%3d%%", int(f * 100 + 0.5f));
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::lgfxJapanGothic_36);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(0xFFFFFFu, 0x101010u);
    M5.Display.fillRect(CX - 130, CY - 50, 260, 100, 0x101010u);
    M5.Display.drawString(buf, CX, CY);
    M5.Display.setTextSize(1);
}

static void ledAdjustLoop()
{
    static uint32_t nextRep = 0;
    int dir = 0;
    if (btnR().isPressed())      dir = +1;
    else if (btnL().isPressed()) dir = -1;
    if (dir) {
        uint32_t now = millis();
        bool first = btnR().wasPressed() || btnL().wasPressed();
        if (first || now >= nextRep) {
            nextRep = now + (first ? 350 : 45);   // 押しっぱなしで連続変化
            int v = ledLevel + dir * 6;
            ledLevel = uint8_t(v < 4 ? 4 : (v > 255 ? 255 : v));
            ledOn = true;
            ledAdjDirty = true;
            ledAdjLastInput = now;
        }
    }
    drawLedAdjust();

    if (millis() - ledAdjLastInput > 3000) {   // 決定して時計へ戻る
        prefs.putUChar("ledv", ledLevel);
        prefs.putBool("ledon", true);
        clockSub = CS_NORMAL;
        clockResetDraw();
    }
}

static void drawClock()
{
    struct tm t;
    getNow(t);
    if (t.tm_sec == lastClockSec && !fullRedraw) return;
    lastClockSec = t.tm_sec;

    uint32_t secOfDay = t.tm_hour * 3600u + t.tm_min * 60u + t.tm_sec;
    RGB8 bg = colorAtSecond(secOfDay);
    uint32_t bg24 = rgb888(bg);
    uint32_t fg   = contrastColor(bg);

    bool bgChanged = (bg.r != lastBg.r || bg.g != lastBg.g || bg.b != lastBg.b);
    if (fullRedraw || bgChanged) {
        M5.Display.fillScreen(bg24);
        lastBg = bg;
        lastRingMin = -1;
    }
    int nowMin = t.tm_hour * 60 + t.tm_min;
    if (nowMin != lastRingMin) {  // リングは1分ごとに描き直し (マーカー移動)
        drawDayRing(secOfDay);
        lastRingMin = nowMin;
    }

    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(fg, bg24);

    char buf[32];
    // 日付
    M5.Display.setFont(&fonts::lgfxJapanGothic_28);
    M5.Display.setTextSize(1);
    snprintf(buf, sizeof(buf), "%04d/%02d/%02d (%s)",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, WDAY_JP[t.tm_wday]);
    M5.Display.drawString(buf, CX, CY - 90);

    // 時刻 (7セグ風フォント)
    M5.Display.setFont(&fonts::Font7);
    M5.Display.setTextSize(1.4f);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    M5.Display.drawString(buf, CX, CY);

    // いまの色コード
    M5.Display.setFont(&fonts::lgfxJapanGothic_24);
    M5.Display.setTextSize(1);
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", bg.r, bg.g, bg.b);
    M5.Display.drawString(buf, CX, CY + 85);
    M5.Display.drawString("iroirotokei", CX, CY + 130);

    // 音とLEDの状態 (右短押し: 音 / 左短押し: LEDオンオフ / 左ダブル: 明るさ)
    char stat[32];
    char ledStr[8];
    if (ledOn) snprintf(ledStr, sizeof(ledStr), "%d%%", int(ledLevel / 255.f * 100 + 0.5f));
    else       strcpy(ledStr, "OFF");
    snprintf(stat, sizeof(stat), "音 %s   LED %s", soundOn ? "ON" : "OFF", ledStr);
    M5.Display.fillRect(CX - 110, CY + 150, 220, 30, bg24);
    M5.Display.drawString(stat, CX, CY + 165);

    fullRedraw = false;
}

// ---------------------------------------------------------------------
// モード2: 色づくり
// ---------------------------------------------------------------------
struct PaletteColor { const char *name; RGB8 c; };
static const PaletteColor PALETTE[] = {
    {"赤",       {255,   0,   0}},
    {"オレンジ", {255, 128,   0}},
    {"黄",       {255, 255,   0}},
    {"緑",       {  0, 200,   0}},
    {"シアン",   {  0, 220, 220}},
    {"青",       {  0,  64, 255}},
    {"紫",       {150,   0, 255}},
    {"マゼンタ", {255,   0, 180}},
    {"白",       {255, 255, 255}},
    {"黒",       {  0,   0,   0}},
};
static constexpr int NUM_PALETTE = sizeof(PALETTE) / sizeof(PALETTE[0]);

static int   selIdx  = 0;
static float mixR = 0, mixG = 0, mixB = 0;
static int   mixCount = 0;
static bool  mixerDirty = true;

static void mixerAddColor()
{
    const RGB8 &c = PALETTE[selIdx].c;
    // 累積平均: 混ぜるほど新しい色の影響が穏やかになる (絵の具を足すイメージ)
    mixR = (mixR * mixCount + c.r) / (mixCount + 1);
    mixG = (mixG * mixCount + c.g) / (mixCount + 1);
    mixB = (mixB * mixCount + c.b) / (mixCount + 1);
    ++mixCount;
}

static void drawMixer()
{
    if (!mixerDirty && !fullRedraw) return;

    if (fullRedraw) M5.Display.fillScreen(0x101010u);

    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::lgfxJapanGothic_28);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0xFFFFFFu, 0x101010u);
    M5.Display.drawString("色づくり", CX, 48);

    // 混ぜた色の大きな円
    RGB8 mix{uint8_t(mixR + 0.5f), uint8_t(mixG + 0.5f), uint8_t(mixB + 0.5f)};
    uint32_t mix24 = mixCount ? rgb888(mix) : 0x303030u;
    M5.Display.fillCircle(CX, 190, 100, mix24);
    M5.Display.drawCircle(CX, 190, 101, 0xFFFFFFu);
    M5.Display.drawCircle(CX, 190, 102, 0xFFFFFFu);

    char buf[48];
    M5.Display.setFont(&fonts::lgfxJapanGothic_24);
    if (mixCount) {
        snprintf(buf, sizeof(buf), "#%02X%02X%02X  (%d回)", mix.r, mix.g, mix.b, mixCount);
    } else {
        snprintf(buf, sizeof(buf), "右ボタンで色を混ぜる");
    }
    M5.Display.fillRect(CX - 180, 294, 360, 36, 0x101010u);  // 前の文字を消す
    M5.Display.drawString(buf, CX, 312);

    // パレット (選択中は白いリング)
    const int y = 366, spacing = 36, r = 14;
    for (int i = 0; i < NUM_PALETTE; ++i) {
        int x = CX + int((i - (NUM_PALETTE - 1) / 2.0f) * spacing);
        M5.Display.fillCircle(x, y, r + 4, (i == selIdx) ? 0xFFFFFFu : 0x101010u);
        M5.Display.fillCircle(x, y, r, rgb888(PALETTE[i].c));
        if (PALETTE[i].c.r + PALETTE[i].c.g + PALETTE[i].c.b < 60 && i != selIdx) {
            M5.Display.drawCircle(x, y, r, 0x606060u);  // 黒が見えるように枠
        }
    }
    // 選択中の色名 (円形画面の下端で切れないよう短めに)
    snprintf(buf, sizeof(buf), "選択: %s", PALETTE[selIdx].name);
    M5.Display.fillRect(CX - 150, 396, 300, 34, 0x101010u);  // 前の文字を消す
    M5.Display.drawString(buf, CX, 412);

    mixerDirty = false;
    fullRedraw = false;
}

// ---------------------------------------------------------------------
// モード3: 傾き絵の具 (IMUで水彩のように混色)
//   傾けた方向へ絵の具が流れ、低い側ほど濃く積もる。
//   毎フレームのにじみ拡散で、隣どうしの色がじわっと混ざる。
// ---------------------------------------------------------------------
static constexpr int   TG    = 58;                     // シミュレーション格子
static constexpr int   TCELL = 2;                      // キャンバス上のセル(px)
static constexpr float TZOOM = 466.f / (TG * TCELL);   // 画面へ拡大する倍率

static float    tiltPaint[TG][TG][3];
static float    tiltTemp[TG][TG][3];
static M5Canvas tiltCanvas(&M5.Display);
static bool     tiltReady     = false;
static int      tiltSelA      = 5;   // 左つぼ: 青
static int      tiltSelB      = 1;   // 右つぼ: オレンジ
static uint32_t tiltLastStep  = 0;
static uint32_t tiltLastSpark = 0;
static bool     imuOk         = false;

static const RGB8 PAPER = {250, 247, 240};  // 画用紙の色
static RGB8 tiltAvgColor = PAPER;           // 画面全体の平均色 (LED連動用)

static void tiltReset()
{
    for (int y = 0; y < TG; ++y)
        for (int x = 0; x < TG; ++x) {
            tiltPaint[y][x][0] = PAPER.r;
            tiltPaint[y][x][1] = PAPER.g;
            tiltPaint[y][x][2] = PAPER.b;
        }
}

static void tiltEnter()
{
    if (!tiltReady) {
        tiltCanvas.setColorDepth(16);
        tiltCanvas.createSprite(TG * TCELL, TG * TCELL);
        tiltReset();
        tiltReady = true;
    }
}

// キラキラ音: 傾きが強いほど大きく・速くまたたく
static void soundSparkle(float m)
{
    if (!soundOn || !spkOk || !speakerActive) return;
    uint32_t interval = 260 - uint32_t(m * 170);   // 90〜260ms
    if (millis() - tiltLastSpark < interval) return;
    tiltLastSpark = millis();
    M5.Speaker.setChannelVolume(1, uint8_t(50 + m * 205));  // 傾き→音量
    uint16_t f = PENTA[random(5, 10)] * 2;   // 高音域のペンタトニック
    if (random(0, 3) == 0) f *= 2;           // ときどきさらに1オクターブ上
    M5.Speaker.tone(f, 45, 1, true);
}

static void tiltStep()
{
    if (millis() - tiltLastStep < 66) return;   // 約15fps
    tiltLastStep = millis();

    // 傾きの取得 (実機で向きが逆に感じたら tx/ty の符号を反転する)
    float m = 0, nx = 0, ny = 0;
    if (imuOk) {
        M5.Imu.update();
        float ax, ay, az;
        M5.Imu.getAccel(&ax, &ay, &az);
        float tx = ax, ty = ay;
        m = sqrtf(tx * tx + ty * ty);
        if (m > 1.f) m = 1.f;
        if (m > 0.10f) { nx = tx / m; ny = ty / m; }
        else           { m = 0; }
    }

    // 1) 絵の具を注ぐ: 傾きの下流ほど濃く (色の強い場所と弱い場所)
    if (m > 0) {
        float bal = 0.5f + 0.5f * nx;   // 左に傾く=左つぼの色、右=右つぼの色
        const RGB8 &A = PALETTE[tiltSelA].c;
        const RGB8 &B = PALETTE[tiltSelB].c;
        float pr = A.r + (B.r - A.r) * bal;
        float pg = A.g + (B.g - A.g) * bal;
        float pb = A.b + (B.b - A.b) * bal;
        float wBase = (m - 0.10f) * 0.14f;
        for (int y = 0; y < TG; ++y) {
            float fy = (y * 2.f / (TG - 1)) - 1.f;
            for (int x = 0; x < TG; ++x) {
                float fx = (x * 2.f / (TG - 1)) - 1.f;
                float proj = 0.5f + 0.5f * (fx * nx + fy * ny);  // 下流=1, 上流=0
                float w = wBase * proj * proj;
                float *c = tiltPaint[y][x];
                c[0] += (pr - c[0]) * w;
                c[1] += (pg - c[1]) * w;
                c[2] += (pb - c[2]) * w;
            }
        }
        soundSparkle(m);
    }

    // 2) にじみ: 水彩のように隣とまざる
    const float d = 0.10f;
    for (int y = 0; y < TG; ++y)
        for (int x = 0; x < TG; ++x)
            for (int k = 0; k < 3; ++k) {
                float sum = 0;
                int n = 0;
                if (x > 0)      { sum += tiltPaint[y][x - 1][k]; ++n; }
                if (x < TG - 1) { sum += tiltPaint[y][x + 1][k]; ++n; }
                if (y > 0)      { sum += tiltPaint[y - 1][x][k]; ++n; }
                if (y < TG - 1) { sum += tiltPaint[y + 1][x][k]; ++n; }
                tiltTemp[y][x][k] = tiltPaint[y][x][k] * (1 - d) + (sum / n) * d;
            }
    memcpy(tiltPaint, tiltTemp, sizeof(tiltPaint));

    // 3) 描画 (低解像度キャンバス→拡大して画面へ) + 平均色の計算
    float sr = 0, sg = 0, sb = 0;
    for (int y = 0; y < TG; ++y)
        for (int x = 0; x < TG; ++x) {
            float *c = tiltPaint[y][x];
            sr += c[0]; sg += c[1]; sb += c[2];
            tiltCanvas.fillRect(x * TCELL, y * TCELL, TCELL, TCELL,
                tiltCanvas.color888(uint8_t(c[0]), uint8_t(c[1]), uint8_t(c[2])));
        }
    constexpr float NC = float(TG) * TG;
    tiltAvgColor = RGB8{uint8_t(sr / NC), uint8_t(sg / NC), uint8_t(sb / NC)};
    tiltCanvas.pushRotateZoom(CX, CY, 0, TZOOM, TZOOM);

    // 4) オーバーレイ: 左右の絵の具つぼ
    M5.Display.fillCircle(46, CY, 26, rgb888(PALETTE[tiltSelA].c));
    M5.Display.drawCircle(46, CY, 29, 0xFFFFFFu);
    M5.Display.drawCircle(46, CY, 30, 0x808080u);
    M5.Display.fillCircle(466 - 46, CY, 26, rgb888(PALETTE[tiltSelB].c));
    M5.Display.drawCircle(466 - 46, CY, 29, 0xFFFFFFu);
    M5.Display.drawCircle(466 - 46, CY, 30, 0x808080u);

    if (!imuOk) {
        M5.Display.setTextDatum(middle_center);
        M5.Display.setFont(&fonts::lgfxJapanGothic_24);
        M5.Display.setTextColor(0x000000u, rgb888(PAPER));
        M5.Display.drawString("IMUが見つかりません", CX, CY);
    }
}

// ---------------------------------------------------------------------
// モード4: 声の色 (マイクで声を色に変える)
//   声の高さ(ゼロ交差率)→色相、声の大きさ(RMS)→明るさ。
//   マイクとスピーカーは同時に使えないため交代で切り替える。
// ---------------------------------------------------------------------
static constexpr size_t VREC = 512;   // 16kHz × 512サンプル = 32ms
static int16_t voiceBuf[VREC];
static bool micOk = false;

enum VoiceState : uint8_t { VS_LISTEN, VS_PLAY };
static VoiceState voiceState      = VS_LISTEN;
static RGB8      voiceColor        = {80, 80, 80};
static bool     voiceHasColor     = false;
static float    voiceLevel        = 0;
static bool     voiceWasSpeaking  = false;
static uint32_t voiceQuietMs      = 0;

static void hsv2rgb(float h, float s, float v, RGB8 &out)
{
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h / 60.f, 2.f) - 1.f));
    float m = v - c;
    float r, g, b;
    if      (h <  60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    out = RGB8{uint8_t((r + m) * 255), uint8_t((g + m) * 255), uint8_t((b + m) * 255)};
}

static void voiceEnter()
{
    if (spkOk) { M5.Speaker.end(); speakerActive = false; }
    if (micOk) M5.Mic.begin();
    voiceState = VS_LISTEN;
    voiceWasSpeaking = false;
    voiceLevel = 0;
}

static void voiceExit()
{
    if (micOk) M5.Mic.end();
    if (spkOk) { M5.Speaker.begin(); speakerActive = true; }
}

// 「声の色」の音を鳴らす (マイク→スピーカーへ一時交代)
static void voicePlayback()
{
    if (!soundOn || !spkOk || !voiceHasColor) return;
    if (micOk) M5.Mic.end();
    M5.Speaker.begin();
    speakerActive = true;
    soundOfColor(voiceColor);
    voiceState = VS_PLAY;
}

static void drawVoice()
{
    static uint32_t lastDraw = 0;
    if (!fullRedraw && millis() - lastDraw < 50) return;
    lastDraw = millis();

    if (fullRedraw) {
        M5.Display.fillScreen(0x101010u);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setFont(&fonts::lgfxJapanGothic_28);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(0xFFFFFFu, 0x101010u);
        M5.Display.drawString("声の色", CX, 52);
        M5.Display.setFont(&fonts::lgfxJapanGothic_24);
        M5.Display.setTextColor(0x808080u, 0x101010u);
        M5.Display.drawString(micOk ? "話しかけてみて" : "マイクが見つかりません", CX, 398);
        fullRedraw = false;
    }

    // 声の色の円 (色コードは円の中に、読める色で)
    uint32_t c24 = voiceHasColor ? rgb888(voiceColor) : 0x303030u;
    M5.Display.fillCircle(CX, CY - 6, 110, c24);
    M5.Display.drawCircle(CX, CY - 6, 111, 0xFFFFFFu);
    if (voiceHasColor) {
        char buf[12];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", voiceColor.r, voiceColor.g, voiceColor.b);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setFont(&fonts::lgfxJapanGothic_24);
        M5.Display.setTextColor(contrastColor(voiceColor), c24);
        M5.Display.drawString(buf, CX, CY - 6);
    }

    // 音量メーター (外側の円弧)
    float lv = voiceLevel * 6.f;
    if (lv > 1.f) lv = 1.f;
    M5.Display.fillArc(CX, CY - 6, 158, 150, -90, -90 + lv * 360, 0xF9C46Bu);
    M5.Display.fillArc(CX, CY - 6, 158, 150, -90 + lv * 360, 270, 0x202020u);
}

static void voiceLoop()
{
    if (voiceState == VS_PLAY) {
        // 再生が終わったらマイクへ戻す
        if (sndPos >= sndLen && !M5.Speaker.isPlaying()) {
            M5.Speaker.end();
            speakerActive = false;
            if (micOk) M5.Mic.begin();
            voiceState = VS_LISTEN;
        }
        drawVoice();
        return;
    }

    if (micOk && M5.Mic.record(voiceBuf, VREC, 16000)) {
        while (M5.Mic.isRecording()) delay(1);

        float sum = 0;
        int zc = 0;
        for (size_t i = 0; i < VREC; ++i) {
            float s = voiceBuf[i] / 32768.f;
            sum += s * s;
            if (i && ((voiceBuf[i - 1] < 0) != (voiceBuf[i] < 0))) ++zc;
        }
        float rms = sqrtf(sum / VREC);
        voiceLevel = voiceLevel * 0.6f + rms * 0.4f;

        if (voiceLevel > 0.015f) {
            // 高さ→色相 (80〜1200Hzを対数で0〜300°へ)、大きさ→明るさ
            float freq = zc * 16000.f / (2.f * VREC);
            if (freq < 60.f) freq = 60.f;
            float t = (logf(freq) - logf(80.f)) / (logf(1200.f) - logf(80.f));
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            float v = 0.45f + 0.55f * fminf(voiceLevel * 6.f, 1.f);
            hsv2rgb(t * 300.f, 0.85f, v, voiceColor);
            voiceHasColor = true;
            voiceWasSpeaking = true;
            voiceQuietMs = millis();
        } else if (voiceWasSpeaking && millis() - voiceQuietMs > 500) {
            voiceWasSpeaking = false;
            voicePlayback();   // 話し終わったら「声の色」の音を返す
        }
    }
    drawVoice();
}

// ---------------------------------------------------------------------
// モード5: ストップウォッチ
// ---------------------------------------------------------------------
static bool     swRunning = false;
static uint32_t swAccumMs = 0;   // 停止までの累積
static uint32_t swStartMs = 0;   // 計測開始時の millis()
static uint32_t swLapMs   = 0;
static int      swLapNo   = 0;
static uint32_t lastSwDrawn = UINT32_MAX;

static uint32_t swElapsed() { return swAccumMs + (swRunning ? millis() - swStartMs : 0); }

static void formatSw(char *buf, size_t n, uint32_t ms)
{
    uint32_t cs = (ms / 10) % 100, s = (ms / 1000) % 60, m = ms / 60000;
    if (m >= 100) snprintf(buf, n, "%3lu:%02lu.%02lu", (unsigned long)m, (unsigned long)s, (unsigned long)cs);
    else          snprintf(buf, n, "%02lu:%02lu.%02lu", (unsigned long)m, (unsigned long)s, (unsigned long)cs);
}

static void drawStopwatch()
{
    uint32_t ms = swElapsed();
    if (!fullRedraw && ms / 10 == lastSwDrawn / 10) return;

    if (fullRedraw) {
        M5.Display.fillScreen(0x000000u);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setFont(&fonts::lgfxJapanGothic_28);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(0xFFFFFFu, 0x000000u);
        M5.Display.drawString("ストップウォッチ", CX, 60);
        M5.Display.setFont(&fonts::lgfxJapanGothic_24);
        M5.Display.setTextColor(0x808080u, 0x000000u);
        M5.Display.drawString("左:開始/停止", CX, 370);
        M5.Display.drawString("右:ラップ/リセット", CX, 400);
    }
    lastSwDrawn = ms;

    char buf[32];
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::Font7);
    M5.Display.setTextSize(1.4f);
    M5.Display.setTextColor(swRunning ? 0x00FF80u : 0xFFFFFFu, 0x000000u);
    formatSw(buf, sizeof(buf), ms);
    M5.Display.drawString(buf, CX, 200);

    M5.Display.setFont(&fonts::lgfxJapanGothic_24);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0xC0C0C0u, 0x000000u);
    if (swLapNo > 0) {
        char lapBuf[24];
        formatSw(lapBuf, sizeof(lapBuf), swLapMs);
        snprintf(buf, sizeof(buf), "LAP%02d  %s", swLapNo, lapBuf);
    } else {
        snprintf(buf, sizeof(buf), " ");
    }
    M5.Display.fillRect(CX - 150, 290, 300, 34, 0x000000u);
    M5.Display.drawString(buf, CX, 306);

    fullRedraw = false;
}

// ---------------------------------------------------------------------
// Unit Hex LED の更新: 各モードの「いまの色」をそのまま灯す
// ---------------------------------------------------------------------
static void updateLed()
{
    static uint32_t lastMs = 0;
    if (millis() - lastMs < 100) return;   // 10fpsで十分
    lastMs = millis();

    RGB8 c = {6, 6, 10};   // 出番がないときはほのかな夜色
    if (mode == MODE_CLOCK && (clockSub == CS_SCRUB || clockSub == CS_LAPSE)) {
        c = lastPrevBg;                            // タイムトラベル/ラプス中の色
    } else if (mode == MODE_CLOCK || mode == MODE_STOPWATCH) {
        struct tm t;                               // 1日の空の色
        getNow(t);
        c = colorAtSecond(t.tm_hour * 3600u + t.tm_min * 60u + t.tm_sec);
    } else if (mode == MODE_MIXER) {
        if (mixCount) c = RGB8{uint8_t(mixR + 0.5f), uint8_t(mixG + 0.5f), uint8_t(mixB + 0.5f)};
    } else if (mode == MODE_TILT) {
        c = tiltAvgColor;                          // 水彩の平均色
    } else if (mode == MODE_VOICE) {
        if (voiceHasColor) c = voiceColor;
    }

    static RGB8     lastC = {1, 2, 3};
    static uint8_t  lastB = 255;
    static uint32_t lastShow = 0;
    uint8_t b = ledOn ? ledLevel : 0;
    bool changed = !(c.r == lastC.r && c.g == lastC.g && c.b == lastC.b && b == lastB);
    // 変化がなくても1秒ごとに再送信する。送信タイミングの乱れ(Wi-Fi割り込み等)で
    // 化けた色を掴んだLEDがあっても、次の送信で正しい色に戻る。
    if (!changed && millis() - lastShow < 1000) return;
    lastC = c;
    lastB = b;
    lastShow = millis();
    FastLED.setBrightness(b);
    fill_solid(hexLeds, HEX_N, CRGB(c.r, c.g, c.b));
    FastLED.show();
}

// ---------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------
void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setBrightness(160);
    // 色深度はデフォルト(16bit/RGB8565)のまま使う。
    // 24bitを指定するとこのパネル(CO5300/QSPI)では色化けするため指定しない。
    btnL().setHoldThresh(600);
    btnR().setHoldThresh(600);

    // スピーカー (ES8311 コーデック) / マイク
    spkOk = M5.Speaker.isEnabled();
    if (spkOk) M5.Speaker.setVolume(110);
    speakerActive = spkOk;
    micOk = M5.Mic.isEnabled();

    // IMU (BMI270) — 傾き絵の具モードで使用
    imuOk = M5.Imu.isEnabled();

    // タッチパネル (CST820B) — タイムトラベルで使用
    touchOk = M5.Touch.isEnabled();

    // Unit Hex (PortA / G10)
    FastLED.addLeds<SK6812, HEX_PIN, GRB>(hexLeds, HEX_N);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);  // 電力保護 (5V/500mA)
    FastLED.clear(true);

    // 設定を復元 (音のオン/オフ、LEDの明るさ)
    prefs.begin("iroirotokei", false);
    soundOn = prefs.getBool("sound", true);
    ledLevel = prefs.getUChar("ledv", 96);
    if (ledLevel < 4) ledLevel = 4;
    ledOn = prefs.getBool("ledon", true);

    Serial.begin(115200);
    setupClock();

    // 右ボタンを押しながら起動 → Wi-Fi設定モード (戻らずに再起動する)
    M5.update();
    if (btnR().isPressed()) runWifiPortal();

    // 通常起動: 保存済みWi-FiがあればNTPで時刻を自動同期
    startNtpSync();

    M5.Display.fillScreen(0x000000u);
}

void loop()
{
    M5.update();
    handleSerialTimeSet();
    updateSound();
    updateLed();
    updateNtpSync();

    // 毎日3:00にNTPで自動再同期 (Wi-Fi設定済みのときだけ成功する)
    if (syncState == SY_IDLE) {
        struct tm t;
        getNow(t);
        if (t.tm_hour == 3 && t.tm_min == 0 && t.tm_mday != lastSyncDay) {
            lastSyncDay = t.tm_mday;
            startNtpSync();
        }
    }

    // 毎正時の時報 (どのモードでも鳴る)
    static uint32_t lastTimeCheck = 0;
    static int      lastChimeKey  = -1;
    if (millis() - lastTimeCheck >= 500) {
        lastTimeCheck = millis();
        struct tm t;
        getNow(t);
        int key = t.tm_yday * 24 + t.tm_hour;
        if (lastChimeKey < 0)          lastChimeKey = key;  // 起動直後は鳴らさない
        else if (key != lastChimeKey) { lastChimeKey = key; soundHourChime(); }
    }

    // 左長押し: モード切替 (LED調整画面中は「暗く」の連続押しに使うため除外)
    if (!(mode == MODE_CLOCK && clockSub == CS_LEDADJ) && btnL().wasHold()) {
        Mode prev = mode;
        mode = Mode((mode + 1) % MODE_COUNT);
        fullRedraw = true;
        mixerDirty = true;
        lastClockSec = -1;
        lastSwDrawn = UINT32_MAX;
        clockSub = CS_NORMAL;
        if (prev == MODE_VOICE) voiceExit();      // マイク→スピーカーへ戻す
        if (mode == MODE_TILT) tiltEnter();
        if (mode == MODE_VOICE) voiceEnter();     // スピーカー→マイクへ (切替音なし)
        else soundModeSwitch();
    }

    switch (mode) {
    case MODE_CLOCK: {
        // --- LEDの明るさ調整画面 ---
        if (clockSub == CS_LEDADJ) {
            ledAdjustLoop();
            break;
        }
        // --- タイムラプス (右長押しで開始、ボタンで中断) ---
        if (clockSub == CS_NORMAL && btnR().wasHold()) {
            clockSub = CS_LAPSE;
            lapseStartMs = millis();
            struct tm t;
            getNow(t);
            lapseBaseSec = t.tm_hour * 3600u + t.tm_min * 60u + t.tm_sec;
            prevHourNote = -1;
            lastPrevBg = {1, 1, 1};
        }
        if (clockSub == CS_LAPSE) {
            const uint32_t TOTAL = 48000;  // 24時間を48秒で
            uint32_t el = millis() - lapseStartMs;
            if (el >= TOTAL || btnR().wasClicked() || btnL().wasClicked()) {
                clockSub = CS_NORMAL;
                clockResetDraw();
            } else {
                uint32_t sec = (lapseBaseSec + uint32_t(uint64_t(el) * 86400 / TOTAL)) % 86400;
                drawColorPreview(sec, "1日を再生中");
            }
            break;
        }

        // --- タイムトラベル (タッチで文字盤をなぞる) ---
        if (touchOk) {
            auto t = M5.Touch.getDetail();
            if (t.isPressed()) {
                float dx = t.x - CX, dy = t.y - CY;
                if (clockSub == CS_NORMAL && dx * dx + dy * dy > 60 * 60) {
                    clockSub = CS_SCRUB;   // 中心から離れた場所に触れたら開始
                    prevHourNote = -1;
                    lastPrevBg = {1, 1, 1};
                }
                if (clockSub == CS_SCRUB) {
                    float ang = atan2f(dy, dx) * 57.29578f + 90.f;  // 真上=0:00
                    if (ang < 0) ang += 360.f;
                    drawColorPreview(uint32_t(ang / 360.f * 86400.f) % 86400, "タイムトラベル");
                }
            } else if (clockSub == CS_SCRUB) {
                clockSub = CS_NORMAL;      // 指を離したら現在へ戻る
                clockResetDraw();
            }
        }
        if (clockSub == CS_SCRUB) break;

        if (btnR().wasClicked()) {                 // 右: 音のオン/オフ
            if (soundOn) { soundToggleOff(); soundOn = false; }
            else         { soundOn = true; soundToggleOn(); }
            prefs.putBool("sound", soundOn);
            lastClockSec = -1;  // 表示をすぐ更新
        }
        if (btnL().wasSingleClicked()) {           // 左: LEDのオン/オフ
            ledOn = !ledOn;
            prefs.putBool("ledon", ledOn);
            lastClockSec = -1;
        }
        if (btnL().wasDoubleClicked()) {           // 左ダブル: LEDの明るさ調整画面
            clockSub = CS_LEDADJ;
            ledAdjLastInput = millis();
            ledAdjDirty = true;
            fullRedraw = true;
        }
        drawClock();
        break;
    }

    case MODE_MIXER:
        if (btnL().wasClicked()) {                 // 左: 色を選ぶ
            selIdx = (selIdx + 1) % NUM_PALETTE;
            mixerDirty = true;
            soundSelect();
        }
        if (btnR().wasClicked()) {                 // 右: 混ぜる
            mixerAddColor();
            mixerDirty = true;
            // できあがった色そのものを音にして鳴らす
            RGB8 m{uint8_t(mixR + 0.5f), uint8_t(mixG + 0.5f), uint8_t(mixB + 0.5f)};
            soundOfColor(m);
        }
        if (btnR().wasHold()) {                    // 右長押し: リセット
            mixR = mixG = mixB = 0;
            mixCount = 0;
            mixerDirty = true;
        }
        // 本体を振る: 1,677万色からランダムな色
        if (imuOk) {
            static uint32_t lastShakeMs = 0;
            if (millis() - lastShakeMs > 900) {
                M5.Imu.update();
                float ax, ay, az;
                M5.Imu.getAccel(&ax, &ay, &az);
                if (ax * ax + ay * ay + az * az > 3.2f) {   // 約1.8G以上の衝撃
                    lastShakeMs = millis();
                    mixR = random(0, 256);
                    mixG = random(0, 256);
                    mixB = random(0, 256);
                    mixCount = 1;
                    mixerDirty = true;
                    RGB8 m{uint8_t(mixR), uint8_t(mixG), uint8_t(mixB)};
                    soundOfColor(m);
                }
            }
        }
        drawMixer();
        break;

    case MODE_TILT:
        if (btnL().wasClicked()) {                 // 左: 左つぼの色
            tiltSelA = (tiltSelA + 1) % NUM_PALETTE;
            soundSelect();
        }
        if (btnR().wasClicked()) {                 // 右: 右つぼの色
            tiltSelB = (tiltSelB + 1) % NUM_PALETTE;
            soundSelect();
        }
        if (btnR().wasHold()) {                    // 右長押し: 紙をリセット
            tiltReset();
        }
        tiltStep();
        break;

    case MODE_VOICE:
        if (btnR().wasClicked() && voiceState == VS_LISTEN) {
            voicePlayback();                       // 右: もう一度「声の色」を聴く
        }
        voiceLoop();
        break;

    case MODE_STOPWATCH:
        if (btnL().wasClicked()) {                 // 左: スタート/ストップ
            if (swRunning) {
                swAccumMs += millis() - swStartMs;
                swRunning = false;
            } else {
                swStartMs = millis();
                swRunning = true;
            }
            fullRedraw = true;
        }
        if (btnR().wasClicked()) {                 // 右: ラップ / リセット
            if (swRunning) {
                swLapMs = swElapsed();
                ++swLapNo;
            } else {
                swAccumMs = 0;
                swLapMs = 0;
                swLapNo = 0;
            }
            lastSwDrawn = UINT32_MAX;
        }
        drawStopwatch();
        break;

    default:
        break;
    }

    delay((mode == MODE_STOPWATCH || mode == MODE_TILT || mode == MODE_VOICE ||
           (mode == MODE_CLOCK && clockSub != CS_NORMAL)) ? 10 : 50);
}
