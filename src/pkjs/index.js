var Clay = require('@rebble/clay');
var clayConfig = require('./config.js');
var customClayFile = require('./custom-clay.js');
var devConfig = {};

var customClay = new Clay(clayConfig, customClayFile, { autoHandleEvents: false });

var DEFAULT_IP = devConfig.ip || "192.168.1.50";
var DEFAULT_PORT = "3000";

var g_isPlaying = false;
var g_messageQueue = [];
var g_isSendingMessage = false;
var g_pollTimer = null;
var g_isConfiguring = false;
var g_lastCommandTimes = {};
var g_retryCount = 0;

var g_lastData = {};
var g_errorCount = 0;
var g_isWatchIdle = false;

function getBridgeUrl() {
  var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
  var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;
  ip = ip.trim().replace(/^https?:\/\//, '').replace(/\/+$/, '');
  return "http://" + ip + ":" + port.trim() + "/";
}

function sendAppMessageQueue(dictionary) {
  if (g_messageQueue.length > 1) {
    g_messageQueue.length = 1;
  }
  g_messageQueue.push(dictionary);
  pumpQueue();
}

function pumpQueue() {
  if (g_isSendingMessage || g_messageQueue.length === 0) return;
  g_isSendingMessage = true;
  var dict = g_messageQueue[0];

  Pebble.sendAppMessage(dict,
                        function(e) {
                          g_messageQueue.shift();
                          g_isSendingMessage = false;
                          g_retryCount = 0;
                          pumpQueue();
                        },
                        function(e) {
                          g_isSendingMessage = false;
                          g_retryCount++;
                          if (g_retryCount >= 3) {
                            g_messageQueue.shift();
                            g_retryCount = 0;
                          }
                          setTimeout(pumpQueue, 100);
                        }
  );
}

function sendBridgeCommand(command) {
  var req = new XMLHttpRequest();
  var url = getBridgeUrl() + command;

  req.open('GET', url, true);
  req.setRequestHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  req.onload = function() {
    if (req.status === 200) {
      sendToWatch(req.responseText);
      if (command !== 'status' && command !== 'launch') setTimeout(fetchStatus, 350);
    }
  };
  req.send(null);
}

function scheduleNextFetch() {
  if (g_pollTimer) clearTimeout(g_pollTimer);

  var pollInterval = 30000;

  if (g_errorCount > 0) {
    // Exponential backoff up to 60 seconds
    pollInterval = Math.min(60000, 3000 * Math.pow(2, g_errorCount));
  } else if (g_isPlaying) {
    pollInterval = g_isWatchIdle ? 10000 : 3000;
  }

  g_pollTimer = setTimeout(fetchStatus, pollInterval);
}

function fetchStatus(isLaunch) {
  var endpoint = (isLaunch === true) ? 'launch' : 'status';
  var req = new XMLHttpRequest();
  req.open('GET', getBridgeUrl() + endpoint, true);
  req.setRequestHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  req.onload = function() {
    if (req.status === 200) {
      sendToWatch(req.responseText);
    } else {
      sendErrorToWatch();
    }
    scheduleNextFetch();
  };
  req.onerror = req.ontimeout = function() {
    sendErrorToWatch();
    scheduleNextFetch();
  };
  req.timeout = 4000;
  req.send(null);
}

function getBasePayload() {
  var rawFont = localStorage.getItem('font_size');
  var savedFont = (rawFont === 'large' || rawFont === '2') ? 2 : (rawFont === 'small' || rawFont === '0') ? 0 : 1;
  var scrollMode = parseInt(localStorage.getItem('scroll_text'), 10) || 0;
  var rawTouch = localStorage.getItem('enable_touch');
  var isTouchEnabled = (rawTouch === 'false' || rawTouch === '0' || rawTouch === false || rawTouch === "") ? 0 : 1;
  var rawQuiet = localStorage.getItem('respect_quiet_time');
  var isQuietEnabled = (rawQuiet === 'false' || rawQuiet === '0' || rawQuiet === false || rawQuiet === "") ? 0 : 1;
  var rawWf = localStorage.getItem('enable_watchface');
  var isWfEnabled = (rawWf === 'true' || rawWf === '1' || rawWf === true) ? 1 : 0;
  var rawAccel = localStorage.getItem('enable_accel_playpause');
  var isAccelEnabled = (rawAccel === 'false' || rawAccel === '0' || rawAccel === false) ? 0 : 1;
  var rawTheme = localStorage.getItem('theme');
  var isLightMode = (rawTheme === 'true' || rawTheme === '1' || rawTheme === true) ? 1 : 0;
  var rawTimeoutWf = localStorage.getItem('timeout_to_app_wf');
  var isTimeoutWf = (rawTimeoutWf === 'true' || rawTimeoutWf === '1' || rawTimeoutWf === true) ? 1 : 0;
  var rawSuppressWf = localStorage.getItem('suppress_gesture_quiet');
  var isSuppressWf = (rawSuppressWf === 'true' || rawSuppressWf === '1' || rawSuppressWf === true) ? 1 : 0;
  var rawFlickVibes = localStorage.getItem('enable_flick_vibes');
  var isFlickVibes = (rawFlickVibes === 'false' || rawFlickVibes === '0' || rawFlickVibes === false) ? 0 : 1;

  var timeApp = parseInt(localStorage.getItem('timeout_app') || '0', 10);
  var timeDisc = parseInt(localStorage.getItem('timeout_disc') || '0', 10);

  return {
    'font_size': savedFont,
    'scroll_text': scrollMode,
    'timeout_app': timeApp,
    'timeout_disc': timeDisc,
    'enable_touch': isTouchEnabled,
    'respect_quiet_time': isQuietEnabled,
    'enable_watchface': isWfEnabled,
    'enable_accel_playpause': isAccelEnabled,
    'timeout_to_app_wf': isTimeoutWf,
    'suppress_gesture_quiet': isSuppressWf,
    'enable_flick_vibes': isFlickVibes,
    'theme': isLightMode,
    'is_configuring': g_isConfiguring ? 1 : 0
  };
}

function sendErrorToWatch() {
  g_errorCount++;
  sendAppMessageQueue({ 'error': 1 });
}

function sendConfigToWatch() {
  var payload = getBasePayload();
  payload['error'] = 0;
  sendAppMessageQueue(payload);
}

function sendToWatch(responseText) {
  try {
    var response = JSON.parse(responseText);
    g_errorCount = 0;

    if (response.is_playing !== undefined) g_isPlaying = response.is_playing;

    var safeVolume = -1;
    var isFixed = (response.is_fixed_volume === true);

    if (response.volume !== undefined && response.volume !== null) {
      if (typeof response.volume === 'object') {
        safeVolume = parseInt(response.volume.value, 10);
        if (response.volume.type === 'fixed') isFixed = true;
      } else { safeVolume = parseInt(response.volume, 10); }
    } else if (response.volume_value !== undefined && response.volume_value !== null) {
      safeVolume = parseInt(response.volume_value, 10);
    } else if (response.level !== undefined && response.level !== null) {
      safeVolume = parseInt(response.level, 10);
    }
    if (isNaN(safeVolume)) safeVolume = -1;

    var newData = {
      zone_name: response.zone || "Unknown",
      track: response.track || "",
      artist: response.artist || "",
      is_playing: response.is_playing ? 1 : 0,
      volume_val: safeVolume,
      is_fixed: isFixed ? 1 : 0
    };

    var changed = false;
    var payload = {};

    for (var key in newData) {
      if (newData[key] !== g_lastData[key]) {
        changed = true;
        payload[key] = newData[key];
        g_lastData[key] = newData[key];
      }
    }

    if (changed) {
      payload['error'] = 0;
      sendAppMessageQueue(payload);
    }
  } catch (err) { console.log("[Roon Remote] JSON Parse Error"); }
}

Pebble.addEventListener('ready', function() {
  sendConfigToWatch();
  fetchStatus(true);
});

Pebble.addEventListener('appmessage', function(e) {
  var command = e.payload['command'] || e.payload['KEY_COMMAND'] || e.payload['0'] || e.payload[0];

  if (command === "idle_true") { g_isWatchIdle = true; scheduleNextFetch(); return; }
  if (command === "idle_false") { g_isWatchIdle = false; scheduleNextFetch(); return; }

  if (command === "retry_connection") {
    g_errorCount = 0;
    return fetchStatus(false);
  }
  if (command === "status") return fetchStatus(false);

  var now = Date.now();
  var lastTime = g_lastCommandTimes[command] || 0;

  if ((command === "next" || command === "previous" || command === "playpause" || command === "next_zone" || command === "prev_zone") && (now - lastTime < 500)) {
    return;
  }
  g_lastCommandTimes[command] = now;

  if (command === "playpause") {
    sendBridgeCommand(command);
  } else if ((command === "next" || command === "previous") && !g_isPlaying) {
    sendBridgeCommand(command);
    setTimeout(function() { sendBridgeCommand("pause"); }, 2500);
  } else if (command) {
    sendBridgeCommand(command);
  }
});

Pebble.addEventListener('showConfiguration', function(e) {
  g_isConfiguring = true;
  sendAppMessageQueue({ 'is_configuring': 1 });
  Pebble.openURL(customClay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  g_isConfiguring = false;
  if (!e || !e.response || e.response === "CANCELLED") {
    sendAppMessageQueue({ 'is_configuring': 0 });
    return;
  }

  try {
    customClay.getSettings(e.response, false);
    var responseDict = JSON.parse(decodeURIComponent(e.response));

    function extractVal(key) {
      if (responseDict[key] !== undefined) {
        if (typeof responseDict[key] === 'object' && responseDict[key] !== null && 'value' in responseDict[key]) {
          return responseDict[key].value.toString();
        }
        return responseDict[key].toString();
      }
      return null;
    }

    var keys = ['bridge_ip', 'bridge_port', 'font_size', 'scroll_text', 'timeout_app', 'timeout_disc', 'enable_touch', 'respect_quiet_time', 'enable_watchface', 'enable_accel_playpause', 'theme', 'timeout_to_app_wf', 'suppress_gesture_quiet', 'enable_flick_vibes'];
    keys.forEach(function(k) {
      var val = extractVal(k);
      if (val !== null) localStorage.setItem(k, val);
    });

      sendConfigToWatch();
      fetchStatus(true);
  } catch(err) { console.log("[Roon Remote] Error parsing settings: " + err); }
});
