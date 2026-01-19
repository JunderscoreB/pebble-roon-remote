# Pebble Roon Remote

![Banner](https://roon-community-uploads.s3.amazonaws.com/original/3X/1/8/18a68b9c52c9cd1d4eecbc934e78ec0501a325d6.jpeg)

Control your Roon music zones directly from your wrist. This app works with the "Rebble" web services and connects to your local Roon Core via a lightweight bridge extension.

### ✨ Features
* **Track Control:** Play, Pause, Next, Previous.
* **Zone Selection:** View and switch active Roon zones directly from the watch.
* **Live Metadata:** See Artist, Track, and Zone Name in real-time.
* **Smart Timeout:** The interface automatically reverts to the Track view after 4 seconds of inactivity.
* **Low Latency:** Optimized for instant feedback using the PebbleDict API.

### 📥 [Download Latest Version (v0.90 Beta)](https://github.com/JunderscoreB/pebble-roon-remote/releases/latest)

---

### How to Install

**1. Install the Bridge**
This app requires a lightweight server to talk to Roon.
1.  Download the `bridge` folder from this repository to your computer or NAS.
2.  Open a terminal in that folder and run `npm install` followed by `node .` (or use the Docker instructions in the bridge folder).
3.  Enable the extension in your Roon Settings (Settings -> Extensions).

**2. Install the Watch App**
1.  Download the `.pbw` file from the [Releases Page](https://github.com/JunderscoreB/pebble-roon-remote/releases).
2.  Open the file on your phone. If you have the Pebble App installed, it will prompt you to load the app.

**3. Configure**
1.  Open the **Pebble App** on your phone and go to the "Apps" tab.
2.  Find **Roon Remote** in the list and tap the **Settings** (gear) icon.
3.  Enter the **IP address** of the computer running the Bridge (e.g., `192.168.1.50`).
4.  Tap **Save**.

---

### 🎮 Controls

* **Select (Short Press):** Toggle between **Track Mode** and **Zone Mode**.
* **Select (Long Press):** Play / Pause.
* **Up / Down (Track Mode):** Previous / Next Track.
* **Up / Down (Zone Mode):** Cycle through available Roon Zones.

---

### 🔨 Development
Built with the Pebble C SDK and PebbleKit JS.

**Build from source:**
```bash
pebble build
pebble install --emulator emery --logs
