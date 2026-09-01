# 🕒 UltraClock - The Absurdly Over-Engineered 100,000-Digit Atomic Clock

> ⚠️ **BIG DISCLAIMER / ВАЖНОЕ ПРЕДУПРЕЖДЕНИЕ**:
> **This is a FUNNY MEME PROJECT created strictly FOR FUN and comedic over-engineering.**
> **DO NOT SERIOUSLY RELY ON THIS SOFTWARE FOR PRODUCTION SYSTEMS, AEROSPACE NAVIGATION, MEDICAL EQUIPMENT, OR ATOMIC METROLOGY.**
>
> *Этот проект сделан исключительно ради шутки и овер-инжиниринга. Не используйте его для серьезных задач или в продакшене!*

---

## 🌟 What is UltraClock?

**UltraClock** is a high-resolution, ultra-minimalist C++ / SDL2 clock application that pushes timekeeping precision to the theoretical limit of software absurdity.

It queries global atomic time servers, applies space-grade Kalman filtering, reads hardware CPU instruction cycle counters, and can render up to **10,000 digits on screen** or log **100,000 sub-second decimal digits to a file**!

---

## ✨ Features

- 🛰️ **Global Atomic Ensemble Synchronization**: Queries NIST (US), PTB (Germany), VNIIFTRI (Russia), Google TrueTime, and Cloudflare NTS/NTP servers.
- 📐 **Marzullo's Intersection Algorithm**: Filters network latency noise and calculates strict error confidence bounds.
- 🧠 **Aerospace 2-State Discrete Kalman Filter**: State estimation tracking clock offset ($\theta$) and frequency drift rate ($\dot{\theta}$).
- ⚡ **Serialized CPU Timestamping (`__rdtscp`)**: Prevents CPU instruction out-of-order execution jitter.
- 🔬 **Picosecond ($10^{-12}\text{s}$) & Planck Time ($t_P$) Calculation**: Calculates physical time down to Planck units ($5.39 \times 10^{-44}\text{s}$).
- 💥 **10,000-Digit On-Screen Matrix & 100,000-Digit File Logger**: Generates and writes up to 100,000 sub-second decimal places to `timestamp_100k_log.txt`.
- 🎨 **Pure Minimalist UI**: High-contrast black & white SDL2 renderer with custom `stb_truetype` font baking.

---

## 🛠️ Build & Run

### Prerequisites
- C++17 compiler (`g++` or `clang++`)
- `SDL2` development libraries (`libsdl2-dev` or `sdl2`)
- `pthread`

### Building
```bash
git clone https://github.com/dotdok132/ultra-clock.git
cd ultra-clock
make
```

### Running

```bash
# Launch GUI mode
./ultra_clock

# Take a 100,000-digit CLI snapshot to timestamp_100k_log.txt without opening GUI
./ultra_clock --snapshot
```

---

## ⌨️ Controls

| Key | Action |
|---|---|
| `P` | Toggle **10,000-Digit On-Screen Matrix View** |
| `SPACE` / `L` / `1` | Save **100,000-Digit Timestamp Snapshot** to `timestamp_100k_log.txt` |
| `S` | Force Re-sync Atomic Ensemble |
| `T` | Toggle Local Time vs UTC |
| `ESC` / `Q` | Exit Application |

---

## 📜 License

MIT License - Made for fun.
