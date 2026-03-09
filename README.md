<div align="center">

```
██╗     ██╗   ██╗ ██████╗██╗  ██╗██╗   ██╗    ██╗    ██╗██╗  ██╗███████╗███████╗██╗
██║     ██║   ██║██╔════╝██║ ██╔╝╚██╗ ██╔╝    ██║    ██║██║  ██║██╔════╝██╔════╝██║
██║     ██║   ██║██║     █████╔╝  ╚████╔╝     ██║ █╗ ██║███████║█████╗  █████╗  ██║
██║     ██║   ██║██║     ██╔═██╗   ╚██╔╝      ██║███╗██║██╔══██║██╔══╝  ██╔══╝  ██║
███████╗╚██████╔╝╚██████╗██║  ██╗   ██║       ╚███╔███╔╝██║  ██║███████╗███████╗███████╗
╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝   ╚═╝        ╚══╝╚══╝ ╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝
```

### 🎡 A daily spin-to-win game for your Flipper Zero

[![Firmware](https://img.shields.io/badge/Firmware-Unleashed-orange?style=for-the-badge)](https://github.com/DarkFlippers/unleashed-firmware)
[![Language](https://img.shields.io/badge/Language-C-blue?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Category](https://img.shields.io/badge/Category-Games-brightgreen?style=for-the-badge)](.)
[![License](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-purple?style=for-the-badge)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Made by](https://img.shields.io/badge/Made%20by-gam3r999-red?style=for-the-badge)](https://www.youtube.com/@RAND3M-x8i)

<br>

> *Spin once a day. Win coins, XP, or level up your dolphin.*
> *Land on 💀 and wait 24 hours. No cheating. (Unless you know the codes...)*

<br>

</div>

---

## 🎰 What Is This?

**Lucky Wheel** is a daily spin game for the Flipper Zero. Every 24 hours you get one spin on an 8-segment prize wheel. Win coins, dolphin XP, and rare level-ups — or land on `LOCKED 💀` and come back tomorrow.

Built in C for **Unleashed firmware** with a coin economy, background shop, cheat codes, and hardware RTC anti-cheat so you can't cheese the timer by changing the clock.

---

## 🏆 Prize Table

| Prize | Chance | Reward |
|:------|:------:|:-------|
| 🏆 **LEVEL UP** | 10% | +1600 XP · +200 coins |
| ⚡ **XP Boost** | 20% | +400 XP · +25 coins |
| 🎁 **Cheat Code** | 7% | Random secret code |
| 💎 **150 Coins** | 8% | +100–150 coins |
| 💰 **100 Coins** | 10% | +100 coins |
| 🪙 **50 Coins** | 20% | +50 coins |
| 🔸 **10 Coins** | 18% | +10 coins |
| 💀 **Nothing** | 7% | Try again tomorrow |

---

## 🎮 Controls

```
Main Screen
───────────────────────────────
  OK      →  Spin the wheel
  UP      →  Open background shop
  RIGHT   →  Enter cheat code
  BACK    →  Exit app

Background Shop
───────────────────────────────
  LEFT / RIGHT  →  Browse backgrounds
  OK            →  Buy or equip
  BACK          →  Exit shop

Cheat Code Entry
───────────────────────────────
  UP / DOWN     →  Cycle letters (A–Z, 0–9)
  RIGHT / OK    →  Confirm letter
  BACK          →  Delete letter / exit
```

---

## 🔓 Cheat Codes

> Press **RIGHT** on the main screen to open the code entry screen.

| Code | Effect |
|:-----|:-------|
| `FLIPGOLD` | +500 coins instantly |
| `COINRUSH` | ×2 coins on your next spin |
| `NEONWAVE` | ×3 coins on your next spin |
| `XPBOOST1` | Guaranteed XP on next spin |
| `HACKTIME` | Reset cooldown — spin right now |
| `DOLPHIN8` | +1600 XP to your dolphin |
| `DARKSIDE` | Guaranteed Level Up on next spin |
| `SECRETZ9` | **JACKPOT** — +999 coins, ×2 coins, extra spin |

---

## 🎨 Background Shop

| # | Name | Cost | Pattern |
|:-:|:-----|:----:|:--------|
| 0 | **Default** | Free | Blank |
| 1 | **Dots** | 100 🪙 | Polka dot grid |
| 2 | **Checker** | 250 🪙 | Checkerboard |
| 3 | **Stripes** | 400 🪙 | Diagonal lines |
| 4 | **Grid** | 500 🪙 | Crosshatch |
| 5 | **Stars** | 750 🪙 | Star field |

---

## 📁 Installation

1. Upload `lucky_wheel.c`, `application.fam`, and `lucky_wheel_10px.png` to the **[FZ Online Compiler](https://github.com/gam3r999/Lucky-Wheel-Fz)**
2. Select **Unleashed** firmware and hit **Compile**
3. Copy the downloaded `.fap` to `/ext/apps/Games/` via qFlipper

---

## 💾 Save File

Everything — coins, skins, cooldown — is stored at:

```
/ext/apps_data/lucky_wheel/lucky_wheel.dat
```

> ⚠️ Deleting this file resets your coins, owned backgrounds, and spin cooldown.

---

## 🔧 Build Info

| Field | Value |
|:------|:------|
| Firmware | Unleashed |
| Entry point | `lucky_wheel_app` |
| Stack size | `2 * 1024` |
| Category | Games |
| Requires | `gui` `storage` `notification` `dolphin` |
| Icon | `lucky_wheel_10px.png` — 10×10px, 1-bit |

---

## 📜 License

This project is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International** license.

[![CC BY-NC-SA 4.0](https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

You are free to share and adapt this work for non-commercial purposes, as long as you give credit and distribute under the same license.

---

<div align="center">

**Made with 🐬 for the Flipper Zero community**

[![YouTube](https://img.shields.io/badge/YouTube-RAND3M-red?style=for-the-badge&logo=youtube)](https://www.youtube.com/@RAND3M-x8i)

*If this slaps, drop a ⭐*

</div>
