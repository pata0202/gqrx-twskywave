gqrx-twskywave
==============

沒有圖形介面、透過網路指令控制的 SDR（軟體定義無線電）接收機，主要用來收聽
118–135 MHz 的 ATC 航空無線電。

本專案從 [Gqrx](https://github.com/gqrx-sdr/gqrx) 2.17.7 分支而來，保留了 Gqrx
以 GNU Radio 打造的接收核心，拿掉整個 Qt 圖形介面和視覺化功能，改成在背景執行，
用 TCP 文字指令（相容 hamlib rigctld 協定）控制。聲音直接從執行它的那台電腦播放，
沒有網路串流造成的延遲。


功能
----

- **硬體**：支援 gr-osmosdr 能驅動的 SDR，例如 RTL-SDR、Airspy、HackRF、
  BladeRF、SoapySDR、USRP。
- **解調模式**：AM、AM-Sync、LSB/USB、CW、窄頻 FM、原始 I/Q。
- **降噪**：溫和的頻譜降噪，壓低講話底下的背景沙沙聲，以保持人聲自然為優先，
  延遲約 5 毫秒。詳見下方〈降噪〉。
- **雜訊抑制器**：去除短暫的脈衝雜訊（喀喀聲、啪啪聲），分 NB1、NB2 兩種。
- **靜音門檻、AGC、濾波頻寬**：可用設定檔設定，靜音門檻和頻寬也能在執行中調整。
- **錄音**：音訊存成 WAV，原始 I/Q 存成 Raw 或 SigMF 格式。
- **遠端控制**：TCP port 7356，任何程式語言幾行就能接上；gpredict 等支援 hamlib
  的軟體也能直接使用。

### 與原版 Gqrx 的差異

| 拿掉的 | 原因 |
|---|---|
| Qt 圖形介面、頻譜和瀑布圖 | 改成無介面、遠端控制 |
| UDP 音訊串流、WAV 回放、AFSK1200 解碼 | 只在圖形介面裡用得到 |
| 廣播 FM（WFM、立體聲）與 RDS | 航空頻段用不到 |
| 圖示、桌面捷徑、AppImage 和 macOS 打包腳本 | 同上 |

| 新增的 | |
|---|---|
| 無介面控制器（`headless_controller`） | 直接讀設定檔、把遠端指令接到接收機 |
| 頻譜降噪（`rx_nr_ff`） | 原版沒有 |
| 雜訊抑制器的遠端與設定檔控制 | 原版只能在圖形介面開關 |


編譯
----

支援 Linux 和 macOS（需要 GNU Radio，Windows 請用 WSL）。已在 Ubuntu 26.04
（WSL）、GNU Radio 3.10.12、Qt 5.15 上測試。

需要的套件：

- GNU Radio 3.8、3.9 或 3.10（runtime、analog、audio、blocks、digital、fft、filter、pmt）
- gr-osmosdr，以及你的 SDR 的驅動程式
- Qt 5 或 Qt 6（只需要 Core 和 Network）
- PulseAudio（Linux 預設）或 PortAudio
- CMake 3.5 以上

Ubuntu / Debian 一行裝好：

```bash
sudo apt install gnuradio-dev gr-osmosdr libosmosdr-dev qtbase5-dev libpulse-dev pkg-config build-essential cmake
```

編譯：

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

執行檔在 `build/src/gqrx`。`ctest` 會跑降噪區塊的自我檢查。

建議先執行一次 `volk_profile`，它會針對你的 CPU 啟用最佳化，效能通常會明顯提升。

Linux 的音訊後端預設是 PulseAudio，可以用 `-DLINUX_AUDIO_BACKEND=Portaudio` 或
`Gr-audio` 切換。


設定檔
------

預設讀取 `~/.config/gqrx/default.conf`（有設 `XDG_CONFIG_HOME` 時改讀
`$XDG_CONFIG_HOME/gqrx/`）。格式是 ini。下面是收聽 ATC 的範例：

```ini
[General]
configversion=4

[input]
device="rtl=0"
sample_rate=2400000
frequency=118100000
hwagc=false

[receiver]
demod=AM
filter_low_cut=-4000
filter_high_cut=4000
sql_level=-60
nr_on=true
nr_level=10
nb1_on=false
nb1_threshold=3.3

[audio]
gain=-60
rec_dir=/home/me/recordings

[remote_control]
port=7356
allowed_hosts=127.0.0.1
```

常用的鍵：

| 鍵 | 說明 |
|---|---|
| `input/device` | gr-osmosdr 裝置字串，例如 `rtl=0`、`airspy=0`、`hackrf=0`。含逗號時要加引號 |
| `input/sample_rate` | 取樣率（Hz） |
| `input/frequency` | 啟動時的頻率（Hz） |
| `input/hwagc` | 硬體自動增益 |
| `receiver/demod` | `AM`、`AM-Sync`、`LSB`、`USB`、`CW-L`、`CW-U`、`Narrow FM`、`Raw I/Q`、`Demod Off` |
| `receiver/filter_low_cut`、`filter_high_cut` | 濾波範圍（Hz，相對中心頻率）。ATC 用 ±4000 左右 |
| `receiver/sql_level` | 靜音門檻（dBFS），訊號低於此值時靜音 |
| `receiver/nr_on`、`nr_level` | 降噪開關、最大壓低量（0–20 dB，預設 10） |
| `receiver/nb1_on`、`nb1_threshold` | 雜訊抑制器 1（門檻 1–20，預設 3.3，越低越積極） |
| `receiver/nb2_on`、`nb2_threshold` | 雜訊抑制器 2（門檻 0–15，預設 2.5） |
| `receiver/agc_off`、`agc_threshold`、`agc_decay` | AGC 設定 |
| `audio/gain` | 音量，單位是 0.1 dB（`-60` = -6 dB） |
| `audio/rec_dir` | 錄音存放資料夾 |
| `baseband/rec_dir`、`rec_format` | I/Q 錄製資料夾、格式（`Raw` 或 `SigMF`） |
| `remote_control/enabled`、`port`、`allowed_hosts` | 遠端控制（預設開啟、7356、只允許本機） |

射頻增益建議啟動後用遠端指令設定，例如 `L LNA_GAIN 30`，或用 `l ?` 查詢裝置有哪些增益可調。


執行
----

```bash
./src/gqrx                 # 使用 default.conf
./src/gqrx -c atc.conf     # 指定設定檔（相對路徑會到設定資料夾找）
./src/gqrx -l              # 列出現有的設定檔
./src/gqrx -r              # 刪除設定檔
```

按 Ctrl+C 結束。


遠端控制
--------

連到 TCP port 7356，每行送一個指令，接收機回一行結果（`RPRT 0` 代表成功）。可以用
`telnet 127.0.0.1 7356` 手動測試，或從程式送出：

```python
import socket

s = socket.create_connection(("127.0.0.1", 7356))
def rc(cmd):
    s.sendall((cmd + "\n").encode())
    return s.recv(256).decode().strip()

rc("F 118100000")   # 調到 118.100 MHz
rc("M AM 8000")     # AM，頻寬 8 kHz
rc("U NR 1")        # 開啟降噪
rc("L SQL -60")     # 靜音門檻
print(rc("l STRENGTH"))   # 讀訊號強度（dBFS）
```

常用指令：

| 指令 | 說明 |
|---|---|
| `F <Hz>` / `f` | 設定 / 讀取頻率 |
| `M <模式> [頻寬]` / `m` | 設定 / 讀取模式，例如 `M AM 8000` |
| `L SQL <dBFS>` / `l SQL` | 靜音門檻 |
| `L AF <dB>` / `l AF` | 音量 |
| `l STRENGTH` | 訊號強度 |
| `U NR 0/1`、`L NR <dB>` | 降噪開關、強度 |
| `U NB1 0/1`、`L NB1 <門檻>` | 雜訊抑制器（NB2 同理） |
| `U RECORD 0/1` | 錄音 |
| `U IQRECORD 0/1` | I/Q 錄製 |
| `U MUTE 0/1` | 靜音 |
| `U DSP 0/1` | 停止 / 啟動接收 |

完整指令列表見 [resources/remote-control.txt](resources/remote-control.txt)。


降噪
----

降噪處理的是**講話底下持續的背景沙沙聲**，例如飛機距離遠、訊號弱的時候。

- 把聲音切成約 5 毫秒的小段，逐頻率判斷是人聲還是噪音，只壓低噪音占主導的頻率。
- 持續追蹤背景噪音（取每個頻率約 1.5 秒內的最低點），不需要等沒人講話的空檔。
- 壓低幅度隨時間平滑變化，而且有下限（`nr_level`，預設最多壓 10 dB），避免人聲
  出現悶悶的、像在水裡講話或機械聲的感覺。
- 在 AM、AM-Sync、SSB、CW、窄頻 FM 都有作用（原始 I/Q 不經過降噪）。

實測（模擬的 AM 訊號 + 白噪音）：level 10 時噪音降低約 10 dB，level 20 時約
20 dB；人聲頻率的音量在單元測試中幾乎不變（-0.04 dB），整台接收機實測約 0–1.3 dB
（含量測誤差）。實際效果請用耳朵判斷：覺得人聲變怪就調低
`L NR`，覺得沙沙聲還太多就調高。

**不擅長的情況**：打雷前天線電暈放電造成的密集噪音，降噪和雜訊抑制器都只能減輕。
持續超過 1.5 秒完全不停頓的穩定音（例如測試音）會被當成噪音壓低，一般對話不會有
這個問題。


除錯
----

用 `QT_LOGGING_RULES` 開啟除錯訊息：

```bash
QT_LOGGING_RULES="*.debug=true;qt.*.debug=false" ./src/gqrx
```


授權與致謝
----------

依 GNU General Public License 第 3 版（或更新版本）授權，見 [COPYING](COPYING)。
部分原始碼改寫自 Moe Weatley 的 Cutesdr，採 Simplified BSD 授權，見
[LICENSE-CTK](LICENSE-CTK)。

Gqrx 由 Alexandru Csete OZ9AEC 設計與撰寫。雜訊抑制器改寫自 DTTSP。

<details>
<summary>Gqrx 原始貢獻者</summary>

Alex Grinkov、Alexander Fasching、Andrea Merello、Andrea Montefusco IW0HDV、
Andy Sloane、Anthony Willard、Anton Blanchard、AsciiWolf、Bastian Bloessl、
Ben Reese、Bob McGwier N4HY、Brandonn Etheve、charlylima、Chris Kuethe、
Christian Lindner DL2VCL、Clayton Smith VE3IRR、Dallas Epperson、Daniil Cherednik、
Darin Franklin、Davide Gerhard、Dominic Chen、Doron Behar、Doug Hammond、
Edouard Lafargue、Elias Önal、Federico Fuga、Frank Brickle AB2KT、
Frank Werner-Krippendorf HB9FXQ、Ganael Laplanche、Gisle Vanem、
Göran Weinholt SA6CJK、Grigory Shipunov、Gwenhael Goavec-Merou、Herman Semenov、
James Yuzawa、Jaroslav Škarvada、Jeff Long、Jiawei Chen、Jiří Pinkava、
Joachim Schueth DL2KCD、Jon Bergli Heier、Josh Blum、Kate Adams、
Kenji Rikitake JJ1BDX、Kitware Inc.、Konrad Beckmann、Luna Gräfje、luzpaz、
Marco Savelli、Markus Kolb、Michael Dickens、Michael Lass、Michael Tatarinov、
Moe Weatley、Nadeem Hasan、Nate Temple、Nick Robinson KE5YWP、Nokia、
Oliver Grossmann DH2WQ、Pavel Milanes CO7WT、Pavel Stano、Phil Vachon、
Radoslav Gerganov、Rob Frohne、Ron Economos W6RZ、Ruslan Migirov、
Russell Dwarshuis KB8U、Ryan Volz、Shuyuan Liu、Stefano Leucci、Sultan Qasim Khan、
Sylvain Munaut、Tarmo Tanilsoo、Tomasz Lemiech、Timothy Reaves、Valentin Ochs、
Vesa Solonen、Vincent Pelletier、Vladisslav P、Will Scales、Wolfgang Fritz DK7OB、
Youssef Touil、Zero_Chaos。

另感謝 Volker Schroer 與 Alexey Bazhin 為 GNU Radio 和 Gqrx 加入 Funcube Dongle
Pro+ 支援。

</details>
