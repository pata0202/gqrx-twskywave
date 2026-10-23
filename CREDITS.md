致謝與第三方授權
================

本專案依 GNU General Public License 第 3 版（或更新版本）授權，全文見
[COPYING](COPYING)。部分程式碼另有第三方授權，見下方〈第三方授權〉。


作者
----

本專案分支自 [Gqrx](https://github.com/gqrx-sdr/gqrx) 2.17.7。Gqrx 由
Alexandru Csete OZ9AEC 設計與撰寫。

- 雜訊抑制器（`src/dsp/rx_noise_blanker_cc.*`）改寫自 DTTSP 的雜訊抑制程式碼。
- AGC（`src/dsp/agc_impl.*`）改寫自 Moe Wheatley 的 Cutesdr。

另感謝 Volker Schroer 與 Alexey Bazhin 為 GNU Radio 和 Gqrx 加入 Funcube Dongle
Pro+ 支援。


Gqrx 貢獻者
-----------

以下個人與組織曾為 Gqrx 貢獻程式碼（名單取自 Gqrx 原始 README）：

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
Moe Wheatley、Nadeem Hasan、Nate Temple、Nick Robinson KE5YWP、Nokia、
Oliver Grossmann DH2WQ、Pavel Milanes CO7WT、Pavel Stano、Phil Vachon、
Radoslav Gerganov、Rob Frohne、Ron Economos W6RZ、Ruslan Migirov、
Russell Dwarshuis KB8U、Ryan Volz、Shuyuan Liu、Stefano Leucci、Sultan Qasim Khan、
Sylvain Munaut、Tarmo Tanilsoo、Tomasz Lemiech、Timothy Reaves、Valentin Ochs、
Vesa Solonen、Vincent Pelletier、Vladisslav P、Will Scales、Wolfgang Fritz DK7OB、
Youssef Touil、Zero_Chaos。


第三方授權
----------

### Cutesdr AGC（`src/dsp/agc_impl.cpp`、`src/dsp/agc_impl.h`）

採 Simplified BSD License（與 GPL 相容）。依其條款，散布原始碼或執行檔時都必須
附上以下聲明：

```
Copyright 2010 Moe Wheatley. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY Moe Wheatley ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Moe Wheatley OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Moe Wheatley.
```
