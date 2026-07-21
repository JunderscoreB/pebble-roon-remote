/*
 * Pebble Roon Remote Bridge
 * Copyright (c) 2026 J_B
 *
 * Released under the MIT License.
 *
 * AI Disclosure: Portions of this file were generated and optimized with the assistance of generative AI.
 * Co-Authored-By: Google Gemini <noreply@google.com>
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
    display_version:     "1.1.1",
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

// ---------------------------------------------------------
// DISCOVERY LOGIC (UDP vs Direct WebSocket)
// ---------------------------------------------------------
if (process.env.ROON_CORE_IP) {
    console.log("Connecting directly to Roon Core at " + process.env.ROON_CORE_IP);
    roon.ws_connect({ host: process.env.ROON_CORE_IP, port: 9330 });
} else {
    console.log("Starting UDP Discovery...");
    roon.start_discovery();
}

// ---------------------------------------------------------
// HELPER FUNCTIONS
// ---------------------------------------------------------
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

    var line1 = "Unknown";
    var line2 = "";
    if (z.now_playing && z.now_playing.three_line) {
        line1 = z.now_playing.three_line.line1 || "No Track";
        line2 = z.now_playing.three_line.line2 || "";
    }

    var vol_val = 0;
    var is_fixed = true;

    // Calculate group average for all non-fixed endpoints
    if (z.outputs && z.outputs.length > 0) {
        var valid_outputs = z.outputs.filter(o => o.volume && o.volume.type !== 'fixed');

        if (valid_outputs.length > 0) {
            is_fixed = false;
            // Sum all the volumes together and divide by the number of valid outputs
            var total = valid_outputs.reduce((sum, o) => sum + (o.volume.value || 0), 0);
            vol_val = Math.round(total / valid_outputs.length);
        }
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

// ---------------------------------------------------------
// EXPRESS API ROUTES
// ---------------------------------------------------------
app.get('/status', (req, res) => { res.json(buildStatus()); });

app.get('/playpause', (req, res) => {
    if (core && getZone()) core.services.RoonApiTransport.control(getZone(), "playpause");
    res.json(buildStatus());
});

app.get('/pause_all', (req, res) => {
    console.log("\n[Command] Executing Pause All Zones");

    if (core) {
        var transport = core.services.RoonApiTransport;
        var staggerDelay = 0;

        Object.values(zones).forEach(zone => {
            setTimeout(() => {
                console.log(" -> Pausing: " + zone.display_name);
                transport.control(zone, "pause");
            }, staggerDelay);
            staggerDelay += 50;
        });

        // Wait for stagger to finish + 250ms buffer to allow Roon
        // to update its internal state before sending the status back to Pebble
        setTimeout(() => {
            res.json(buildStatus());
        }, staggerDelay + 250);

    } else {
        console.log("[Error] No Core connected during pause_all attempt.");
        res.json(buildStatus());
    }
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
        // Filter out endpoints with fixed volume
        var valid_outputs = z.outputs.filter(o => o.volume && o.volume.type !== 'fixed');

        if (valid_outputs.length === 0) {
            return res.json(buildStatus());
        }

        var completed = 0;
        // Iterate through all valid outputs in the group
        valid_outputs.forEach(output => {
            core.services.RoonApiTransport.change_volume(output, "relative_step", 1, function(error) {
                completed++;
                // Wait until all outputs have acknowledged the change
                if (completed === valid_outputs.length) {
                    res.json(buildStatus());
                }
            });
        });
    } else {
        res.json(buildStatus());
    }
});

app.get('/vol_down', (req, res) => {
    var z = getZone();
    if (core && z && z.outputs && z.outputs.length > 0) {
        // Filter out endpoints with fixed volume
        var valid_outputs = z.outputs.filter(o => o.volume && o.volume.type !== 'fixed');

        if (valid_outputs.length === 0) {
            return res.json(buildStatus());
        }

        var completed = 0;
        // Iterate through all valid outputs in the group
        valid_outputs.forEach(output => {
            core.services.RoonApiTransport.change_volume(output, "relative_step", -1, function(error) {
                completed++;
                // Wait until all outputs have acknowledged the change
                if (completed === valid_outputs.length) {
                    res.json(buildStatus());
                }
            });
        });
    } else {
        res.json(buildStatus());
    }
});

app.get('/next_zone', (req, res) => {
    var keys = Object.keys(zones);
    if (keys.length > 0) {
        var idx = keys.indexOf(current_zone_id);
        var nextIdx = (idx + 1) % keys.length;
        current_zone_id = keys[nextIdx];
    }
    res.json(buildStatus());
});

app.get('/prev_zone', (req, res) => {
    var keys = Object.keys(zones);
    if (keys.length > 0) {
        var idx = keys.indexOf(current_zone_id);
        var prevIdx = (idx - 1 + keys.length) % keys.length;
        current_zone_id = keys[prevIdx];
    }
    res.json(buildStatus());
});

app.listen(3000, () => {
    console.log('Pebble Bridge running on Port 3000');
});
