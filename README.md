# Pebble Roon Remote

![Banner](https://roon-community-uploads.s3.amazonaws.com/original/3X/1/8/18a68b9c52c9cd1d4eecbc934e78ec0501a325d6.jpeg)

Control your Roon music zones directly from your wrist. This app works with the Pebble web services and connects to your local Roon Core via a lightweight bridge extension.

### ⌚ Features
* **Customizable Track/Volume Controls:** Map your Up/Down buttons to instantly control volume (with a seamless quick-flash UI overlay) or change tracks.
* **Touch Support:** Capacitive touch support for Pebble Time 2 (Emery platform) and Pebble Round 2—simply tap the screen to Play/Pause!
* **Zone Selection:** View and cycle through active Roon zones directly from the watch with rapid, debounced updates.
* **Live Metadata:** See Artist, Track, and Zone Name in real-time.
* **UI Customization:** Choose your preferred font size and toggle marquee scrolling for long track names.
* **Smart Timeouts:** Preserve battery with customizable idle and disconnect timeouts that automatically exit the app when inactive.
* **Low Latency:** Optimized for instant feedback using the PebbleDict API and optimistic UI rendering.

### 🚀 [Download Latest Version (v1.0.0)](https://github.com/JunderscoreB/pebble-roon-remote/releases/latest)
### 💬 [Join the Roon Community Discussion](https://community.roonlabs.com/t/pebble-smartwatch-app-and-corresponding-roon-extension/313874)

---

### How to Install

#### 1. Install the Bridge
This app requires a lightweight server to talk to Roon.
1. Download the `bridge` folder from this repository to your computer or NAS (works perfectly on setups like a QNAP NAS).
2. **Using Docker (Recommended):** Open a terminal in the `bridge` folder and start the server with a single command:
   ```bash
   docker compose up -d --build
   ```
3. **Running Natively (Alternative):** If you prefer not to use Docker, ensure you have Node.js and Git installed on your machine. Open a terminal in the `bridge` folder, run `npm install`, and then start it with `npm start`.
4. Enable the extension in your Roon Settings (**Settings** -> **Extensions**).

#### 2. Install the Watch App
You can install the app directly from the Rebble App Store or manually sideload it.

**Option A: App Store (Recommended)**
* Simply click here from your smartphone to install directly to your watch: **[Download from Pebble App Store](https://apps.repebble.com/roon-remote-for-pebble_696edc515cd4fc0009ce6f5c)**

**Option B: Sideloading (Alternative)**
1. Download the `.pbw` file from the [Releases Page](https://github.com/JunderscoreB/pebble-roon-remote/releases).
2. Open the file on your phone. If you have the Pebble App installed, it will prompt you to load the app package onto your watch.

#### 3. Configure
1. Open the **Pebble App** on your smartphone and go to the **Apps** tab.
2. Find **Roon Remote** in the list and tap the **Settings** (gear) icon.
3. Enter your Bridge IP address, choose your desired UI settings (Font Size, Scroll, Button Mapping), and set your preferred Timeouts.
4. Tap **Save**.

---

### 🕹️ Controls

* **Tap Screen (Touch devices):** Play / Pause.
* **Select (Short Press):** Toggle between **Track Mode** and **Zone Mode**.
* **Select (Long Press):** Play / Pause.
* **Up / Down (Track Mode):** Adjust Volume (Default) OR Previous/Next Track (Customizable in Settings).
* **Up / Down (Zone Mode):** Cycle through available Roon Zones.
* **Up / Down (Volume Mode):** Only applicable if Up/Down is mapped to Tracks in settings. Allows dedicated volume adjustment.

> **Pro Tip for App Timeouts:** For the best experience, assign Roon Remote to a **Quick Launch** button on your watch (Watch Settings -> Quick Launch). When the inactivity timeout triggers, the app will gracefully exit directly to your watchface instead of the Pebble app menu!

---

### 🛠 For Developers

To test this watchapp locally without hardcoding your Roon Bridge IP address into the public source code:

1. Copy the `dev_config.example.json` file in the root directory and rename it to `dev_config.json`.
2. Update the `"ip"` field inside `dev_config.json` with your bridge's IP address.
3. Run `pebble build`. Webpack will automatically bundle this IP into your build!

> **Note:** `dev_config.json` is safely ignored by Git so your local network details will remain private. Legacy platforms such as the original architecture (Aplite platform) will fall back gracefully to monochromatic rendering layouts using target native definitions like `GColorWhite` and `GColorBlack`.
