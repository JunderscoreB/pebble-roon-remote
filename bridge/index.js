var RoonApi          = require("node-roon-api"),
    RoonApiStatus    = require("node-roon-api-status"),
    RoonApiTransport = require("node-roon-api-transport"),
    express          = require("express"),
    app              = express();

var core;
var zones = {};
var current_zone_id = null;

// --- ROON SETUP ---
var roon = new RoonApi({
    extension_id:        'com.junderscoreb.pebble.remote',
    display_name:        "Pebble Watch Remote",
    display_version:     "0.99.4",
    publisher:           "J_B",
    email:               "dev@example.com",
    log_level:           "none",

    core_paired: function(core_) {
        core = core_;
        console.log("-> Paired with Roon Core:", core.display_name);
        
        // Subscribe to state updates
        var transport = core.services.RoonApiTransport;
        transport.subscribe_zones((response, msg) => {
            if (response == "Subscribed") {
                zones = msg.zones.reduce((map, z) => { map[z.zone_id] = z; return map; }, {});
                // Auto-select first zone if none selected
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

// --- WEB SERVER (Port 3000) ---

// HELPER: Get Current Zone Object
function getZone() {
    if (!core) return null;
    if (!current_zone_id || !zones[current_zone_id]) {
        // Fallback: Grab first available zone
        var keys = Object.keys(zones);
        if (keys.length > 0) current_zone_id = keys[0];
    }
    return zones[current_zone_id];
}

// ENDPOINT: Status (What the watch constantly polls)
app.get('/status', (req, res) => {
    var z = getZone();
    if (!z) {
        return res.json({ zone: "Searching...", track: "No Core", artist: "", is_playing: false });
    }

    var output = z.outputs[0]; // Primary output
    
    // Safety check for metadata
    var line1 = "Unknown"; 
    var line2 = "";
    if (z.now_playing) {
        line1 = z.now_playing.three_line.line1 || "No Track";
        line2 = z.now_playing.three_line.line2 || "";
    }

    // --- THE FIX: ROON API VOLUME PARSING ---
    var vol_val = 0;
    var is_fixed = false;

    // First check if the volume object exists at all
    if (output && output.volume) {
        vol_val = output.volume.value || 0;
        
        // Roon API uses "type": "fixed" to flag unadjustable zones
        if (output.volume.type === 'fixed') {
            is_fixed = true;
        }
    } else {
        // If the volume object is entirely missing, the endpoint is uncontrollable/fixed
        is_fixed = true;
    }

    res.json({
        zone: z.display_name,
        track: line1,
        artist: line2,
        is_playing: z.state === "playing",
        volume: vol_val,
        is_fixed_volume: is_fixed
    });
});

// ENDPOINT: Commands
app.get('/playpause', (req, res) => {
    if (core && getZone()) core.services.RoonApiTransport.control(getZone(), "playpause");
    res.send("OK");
});

app.get('/next', (req, res) => {
    if (core && getZone()) core.services.RoonApiTransport.control(getZone(), "next");
    res.send("OK");
});

app.get('/previous', (req, res) => {
    if (core && getZone()) core.services.RoonApiTransport.control(getZone(), "previous");
    res.send("OK");
});

app.get('/vol_up', (req, res) => {
    var z = getZone();
    if (core && z && z.outputs[0]) {
        core.services.RoonApiTransport.change_volume(z.outputs[0], "relative", 2);
    }
    res.send("OK");
});

app.get('/vol_down', (req, res) => {
    var z = getZone();
    if (core && z && z.outputs[0]) {
        core.services.RoonApiTransport.change_volume(z.outputs[0], "relative", -2);
    }
    res.send("OK");
});

// ENDPOINT: Cycle Zones
app.get('/next_zone', (req, res) => {
    var keys = Object.keys(zones);
    if (keys.length === 0) return res.send("No Zones");
    
    var idx = keys.indexOf(current_zone_id);
    var nextIdx = (idx + 1) % keys.length;
    current_zone_id = keys[nextIdx];
    
    res.send("Switched to " + zones[current_zone_id].display_name);
});

app.get('/prev_zone', (req, res) => {
    var keys = Object.keys(zones);
    if (keys.length === 0) return res.send("No Zones");
    
    var idx = keys.indexOf(current_zone_id);
    var prevIdx = (idx - 1 + keys.length) % keys.length;
    current_zone_id = keys[prevIdx];
    
    res.send("Switched to " + zones[current_zone_id].display_name);
});

// Start Server
app.listen(3000, () => {
    console.log('Pebble Bridge running on Port 3000');
});