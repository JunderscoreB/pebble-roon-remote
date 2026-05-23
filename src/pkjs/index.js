// --- COMPILE-TIME BUNDLING ---
// Webpack will automatically pull the contents of dev_config.json into 
// the watch app during `pebble build`, making it instantly available!
var devConfig = require('../../dev_config.json'); 

var DEFAULT_IP = devConfig.ip || "192.168.1.50"; 
var DEFAULT_PORT = "3000";

var CONFIG_URL = "https://junderscoreb.github.io/pebble-roon-remote/config.html";

// --- STATE TRACKING ---
var g_isPlaying = false;
var g_messageQueue = [];
var g_isSendingMessage = false;
var g_pollTimer = null;

// --- HELPERS ---
function getBridgeUrl() {
  var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
  var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;
  
  // Sanitize: Strip out "http://" if user typed it, and remove trailing slashes
  ip = ip.replace(/^https?:\/\//, '').replace(/\/+$/, '');
  
  return "http://" + ip + ":" + port + "/";
}

// --- MESSAGE QUEUE (CRITICAL FOR PEBBLEKIT JS) ---
function sendAppMessageQueue(dictionary) {
  g_messageQueue.push(dictionary);
  pumpQueue();
}

function pumpQueue() {
  if (g_isSendingMessage || g_messageQueue.length === 0) {
    return;
  }
  
  g_isSendingMessage = true;
  var dict = g_messageQueue[0];
  
  Pebble.sendAppMessage(dict, 
    function(e) {
      // Success! Remove from queue and pump next
      g_messageQueue.shift();
      g_isSendingMessage = false;
      pumpQueue();
    }, 
    function(e) {
      // Failed (Watch buffer busy/disconnected). Keep in queue, wait, and retry
      console.log("AppMessage delivery failed, retrying in 500ms...", JSON.stringify(e));
      g_isSendingMessage = false;
      setTimeout(pumpQueue, 500);
    }
  );
}

// --- ROON BRIDGE COMMS ---
function sendBridgeCommand(command) {
  var req = new XMLHttpRequest();
  req.open('GET', getBridgeUrl() + command, true);
  req.onload = function() {
    if (req.status === 200) sendToWatch(req.responseText);
  };
  req.send(null);
}

function scheduleNextFetch() { 
  // Clear any existing timer before creating a new one to prevent parallel overlapping loops!
  if (g_pollTimer) {
    clearTimeout(g_pollTimer);
  }
  g_pollTimer = setTimeout(fetchStatus, 3000); 
}

function fetchStatus() {
  var req = new XMLHttpRequest();
  req.open('GET', getBridgeUrl() + 'status', true);
  
  req.onload = function() {
    if (req.status === 200) {
      sendToWatch(req.responseText);
      scheduleNextFetch();
    } else {
      console.log("Bridge Error: " + req.status);
      sendErrorToWatch();
      scheduleNextFetch();
    }
  };

  req.onerror = function() {
    console.log("Bridge Connection Failed");
    sendErrorToWatch();
    scheduleNextFetch();
  };
  
  req.ontimeout = function() {
    console.log("Bridge Timeout");
    sendErrorToWatch();
    scheduleNextFetch();
  };

  req.timeout = 4000; 
  req.send(null);
}

function sendErrorToWatch() {
  sendAppMessageQueue({
    'error': 1 
  });
}

function sendToWatch(responseText) {
  try {
    var response = JSON.parse(responseText);

    if (response.is_playing !== undefined) {
      g_isPlaying = response.is_playing;
    }

    var safeVolume = -1;
    // Check carefully for volume (accounting for value of 0 being valid)
    if (response.volume !== undefined && response.volume !== null) {
      safeVolume = parseInt(response.volume, 10);
    } else if (response.volume_value !== undefined && response.volume_value !== null) {
      safeVolume = parseInt(response.volume_value, 10);
    } else if (response.level !== undefined && response.level !== null) {
      safeVolume = parseInt(response.level, 10);
    }

    if (isNaN(safeVolume)) safeVolume = -1;

    // Use our new resilient queue instead of direct sendAppMessage
    sendAppMessageQueue({
      'zone_name': response.zone || "Unknown",
      'track': response.track || "",
      'artist': response.artist || "",
      'is_playing': response.is_playing ? 1 : 0,
      'volume_val': safeVolume,
      'is_fixed': response.is_fixed_volume ? 1 : 0,
      'error': 0 
    });
  } catch (err) {
    console.log("JSON Parse Error: " + err);
  }
}

// --- APP INITIALIZATION ---
// Start the app when ready. Because we bundled devConfig via Webpack, 
// we don't need initApp() or any background XHR checks anymore!
Pebble.addEventListener('ready', function() { 
  fetchStatus(); 
});

// --- HANDLE BUTTON PRESSES FROM WATCH ---
Pebble.addEventListener('appmessage', function(e) {
  var command = e.payload['command'] || e.payload['0'] || e.payload[0];

  if (command === "retry_connection") {
     console.log("Retrying connection...");
     fetchStatus();
     return;
  }

  if ((command === "next" || command === "previous") && !g_isPlaying) {
    console.log("Skipping while paused - Initiating auto-pause sequence");
    sendBridgeCommand(command);
    // Auto-pause delay for Roon's behavior
    setTimeout(function() {
      console.log("Sending forced pause...");
      sendBridgeCommand("pause");
    }, 2500);
  } else {
    sendBridgeCommand(command);
  }
});

// --- SETTINGS PAGE ROUTING ---
Pebble.addEventListener('showConfiguration', function(e) {
  var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
  var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;
  Pebble.openURL(CONFIG_URL + "?ip=" + encodeURIComponent(ip) + "&port=" + encodeURIComponent(port));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e.response && e.response !== "CANCELLED") {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      if (config.ip) {
        localStorage.setItem('bridge_ip', config.ip);
        localStorage.setItem('bridge_port', config.port || "3000");
        fetchStatus();
      }
    } catch(err) {
      console.log("Settings parse error: ", err);
    }
  }
});
