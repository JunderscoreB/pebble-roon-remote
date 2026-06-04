/*
 * Pebble Roon Remote
 * Copyright (c) 2026 J_B
 *
 * Released under the MIT License.
 *
 * AI Disclosure: Portions of this file were generated and optimized with the assistance of generative AI.
 * (Google Gemini).
 */

var RoonApi          = require("node-roon-api"),
RoonApiStatus    = require("node-roon-api-status"),
RoonApiTransport = require("node-roon-api-transport"),
express          = require("express"),
app              = express();

var core;
var zones = {};
var current_zone_id = null;

var roon = new RoonApi({
    extension_id:        'com.junderscoreb.pebble.remote',
    display_name:        "Pebble Roon Remote",
    display_version:     "1.0.0",
    publisher:           "J_B",
    email:               "dev@example.com",
    log_level:           "none",

    core_paired: function(core_) {
        core = core_;
        console.log("-> Paired with Roon Core:", core.display_name);

        var transport = core.services.RoonApiTransport;
        transport.subscribe_zones((response, msg) => {
            if (response == "Subscribed") {
                zones = msg.zones.reduce((map, z) => { map[z.zone_id] = z; return map; }, {});
                if (!current_zone_id && msg.zones.length > 0) current_zone_id = msg.zones[0].zone_id;
            } else if (response == "Changed") {
                if (msg.zones_added)   msg.zones_added.forEach(z => zones[z.zone_id] = z);
                if (msg.zones_removed) msg.zones_removed.forEach(z => delete zones[z.zone_id]);
                if (msg.zones_changed) msg.zones_changed.forEach(z => zones[z.zone_id] = z);
            }
        });
    },
    core_unpaired: function(core_) {
        console.log("-! Core Unpaired");
        core = undefined;
        zones = {};
    }
});

var svc_status = new RoonApiStatus(roon);
roon.init_services({
    required_services: [ RoonApiTransport ],
    provided_services: [ svc_status ]
});

svc_status.set_status("Extension enabled", false);
roon.start_discovery();

function getZone() {
    if (!core) return null;
    if (!current_zone_id || !zones[current_zone_id]) {
        var keys = Object.keys(zones);
        if (keys.length > 0) current_zone_id = keys[0];
    }
    return zones[current_zone_id];
}

function buildStatus() {
    var z = getZone();
    if (!z) return { zone: "Searching...", track: "No Core", artist: "", is_playing: false };

    var output = z.outputs && z.outputs.length > 0 ? z.outputs[0] : null;

    var line1 = "Unknown";
    var line2 = "";
    if (z.now_playing && z.now_playing.three_line) {
        line1 = z.now_playing.three_line.line1 || "No Track";
        line2 = z.now_playing.three_line.line2 || "";
    }

    var vol_val = 0;
    var is_fixed = false;

    if (output && output.volume) {
        vol_val = output.volume.value || 0;
        if (output.volume.type === 'fixed') is_fixed = true;
    } else {
        is_fixed = true;
    }

    return {
        zone: z.display_name || "Unknown",
        track: line1,
        artist: line2,
        is_playing: z.state === "playing",
        volume: vol_val,
        is_fixed_volume: is_fixed
    };
}

app.get('/status', (req, res) => { res.json(buildStatus()); });

app.get('/playpause', (req, res) => {
    if (core && getZone()) core.services.RoonApiTransport.control(getZone(), "playpause");
    res.json(buildStatus());
});

app.get('/next', (req, res) => {
    if (core && getZone()) core.services.RoonApiTransport.control(getZone(), "next");
    res.json(buildStatus());
});

app.get('/previous', (req, res) => {
    if (core && getZone()) core.services.RoonApiTransport.control(getZone(), "previous");
    res.json(buildStatus());
});

app.get('/vol_up', (req, res) => {
    var z = getZone();
    if (core && z && z.outputs && z.outputs.length > 0) {
        core.services.RoonApiTransport.change_volume(z.outputs[0], "relative_step", 1);
    }
    setTimeout(() => { res.json(buildStatus()); }, 250);
});

app.get('/vol_down', (req, res) => {
    var z = getZone();
    if (core && z && z.outputs && z.outputs.length > 0) {
        core.services.RoonApiTransport.change_volume(z.outputs[0], "relative_step", -1);
    }
    setTimeout(() => { res.json(buildStatus()); }, 250);
});

app.get('/next_zone', (req, res) => {
    var keys = Object.keys(zones);
    if (keys.length > 0) {
        var idx = keys.indexOf(current_zone_id);
        var nextIdx = (idx + 1) % keys.length;
        current_zone_id = keys[nextIdx];
    }
    setTimeout(() => { res.json(buildStatus()); }, 300);
});

app.get('/prev_zone', (req, res) => {
    var keys = Object.keys(zones);
    if (keys.length > 0) {
        var idx = keys.indexOf(current_zone_id);
        var prevIdx = (idx - 1 + keys.length) % keys.length;
        current_zone_id = keys[prevIdx];
    }
    setTimeout(() => { res.json(buildStatus()); }, 300);
});

app.listen(3000, () => {
    console.log('Pebble Bridge running on Port 3000');
});
