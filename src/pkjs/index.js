/*
 * Pebble Roon Remote
 * Copyright (c) 2026 J_B
 *
 * Released under the MIT License.
 *
 * AI Disclosure: Portions of this file were generated and optimized with the assistance of generative AI.
 * (Google Gemini).
 */

var devConfig = require('../../dev_config.json');
var DEFAULT_IP = devConfig.ip || "192.168.1.50";
var DEFAULT_PORT = "3000";
var CONFIG_URL = "https://junderscoreb.github.io/pebble-roon-remote/config.html";

var g_isPlaying = false;
var g_messageQueue = [];
var g_isSendingMessage = false;
var g_pollTimer = null;

var g_cachedZones = [];
var g_currentZoneId = null;
var g_zoneSwitchTimer = null;
var g_isSwitchingZone = false;

function getBridgeUrl() {
  var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
  var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;
  ip = ip.replace(/^https?:\/\//, '').replace(/\/+$/, '');
  return "http://" + ip + ":" + port + "/";
}

function sendAppMessageQueue(dictionary) {
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
                          pumpQueue();
                        },
                        function(e) {
                          g_isSendingMessage = false;
                          setTimeout(pumpQueue, 100);
                        }
  );
}

function sendBridgeCommand(command) {
  var req = new XMLHttpRequest();
  req.open('GET', getBridgeUrl() + command, true);
  req.onload = function() {
    if (req.status === 200) sendToWatch(req.responseText);
  };
    req.send(null);
}

function scheduleNextFetch() {
  if (g_pollTimer) clearTimeout(g_pollTimer);
  g_pollTimer = setTimeout(fetchStatus, 3000);
}

function fetchStatus() {
  var req = new XMLHttpRequest();
  req.open('GET', getBridgeUrl() + 'status', true);
  req.onload = function() {
    if (req.status === 200) { sendToWatch(req.responseText); scheduleNextFetch(); }
    else { sendErrorToWatch(); scheduleNextFetch(); }
  };
  req.onerror = function() { sendErrorToWatch(); scheduleNextFetch(); };
  req.ontimeout = function() { sendErrorToWatch(); scheduleNextFetch(); };
  req.timeout = 4000;
  req.send(null);
}

function sendErrorToWatch() { sendAppMessageQueue({ 'error': 1 }); }

function sendToWatch(responseText) {
  try {
    var response = JSON.parse(responseText);

    if (response.zones) g_cachedZones = response.zones;
    if (response.zone_id && !g_isSwitchingZone) g_currentZoneId = response.zone_id;
    if (response.is_playing !== undefined) g_isPlaying = response.is_playing;

    var safeVolume = -1;
    var isFixed = false;
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

    if (response.is_fixed_volume === true) isFixed = true;
    if (isNaN(safeVolume)) safeVolume = -1;

    var rawFont = localStorage.getItem('font_size');
    var savedFont = (rawFont === 'large' || rawFont === '2') ? 2 : (rawFont === 'small' || rawFont === '0') ? 0 : 1;
    var isScrollEnabled = (localStorage.getItem('scroll_text') === '1') ? 1 : 0;
    var isTouchEnabled = (localStorage.getItem('enable_touch') === '0') ? 0 : 1;

    var timeApp = parseInt(localStorage.getItem('timeout_app') || '0', 10);
    var timeDisc = parseInt(localStorage.getItem('timeout_disc') || '0', 10);

    sendAppMessageQueue({
      'zone_name': (!g_isSwitchingZone) ? (response.zone || "Unknown") : undefined,
                        'track': response.track || "",
                        'artist': response.artist || "",
                        'is_playing': response.is_playing ? 1 : 0,
                        'volume_val': safeVolume,
                        'is_fixed': isFixed ? 1 : 0,
                        'error': 0,
                        'font_size': savedFont,
                        'scroll_text': isScrollEnabled,
                        'timeout_app': timeApp,
                        'timeout_disc': timeDisc,
                        'enable_touch': isTouchEnabled
    });
  } catch (err) { console.log("JSON Parse Error: " + err); }
}

Pebble.addEventListener('ready', function() { fetchStatus(); });

Pebble.addEventListener('appmessage', function(e) {
  var command = e.payload['command'] || e.payload['0'] || e.payload[0];
  if (command === "retry_connection") { fetchStatus(); return; }

  if (command === "next_zone" || command === "prev_zone") {
    if (g_cachedZones.length > 0) {
      g_isSwitchingZone = true;
      var idx = g_cachedZones.findIndex(function(z) { return z.id === g_currentZoneId; });
      if (idx === -1) idx = 0;

      if (command === "next_zone") idx = (idx + 1) % g_cachedZones.length;
      else idx = (idx - 1 + g_cachedZones.length) % g_cachedZones.length;

      g_currentZoneId = g_cachedZones[idx].id;

      sendAppMessageQueue({
        'zone_name': g_cachedZones[idx].name,
        'track': "Switching...",
        'artist': " "
      });

      if (g_zoneSwitchTimer) clearTimeout(g_zoneSwitchTimer);
      g_zoneSwitchTimer = setTimeout(function() {
        sendBridgeCommand("set_zone?id=" + encodeURIComponent(g_currentZoneId));
        setTimeout(function() { g_isSwitchingZone = false; }, 1000);
      }, 500);
      return;
    }
  }

  if (command === "playpause") {
    sendBridgeCommand(command);
  } else if ((command === "next" || command === "previous") && !g_isPlaying) {
    sendBridgeCommand(command);
    setTimeout(function() { sendBridgeCommand("pause"); }, 2500);
  } else {
    sendBridgeCommand(command);
  }
});

Pebble.addEventListener('showConfiguration', function(e) {
  var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
  var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;
  var rawFont = localStorage.getItem('font_size') || "1";
  var scrollText = localStorage.getItem('scroll_text') || '0';
  var timeApp = localStorage.getItem('timeout_app') || '0';
  var timeDisc = localStorage.getItem('timeout_disc') || '0';
  var enableTouch = localStorage.getItem('enable_touch') || '1';

  var cacheBuster = Math.round(Math.random() * 10000);

  var finalUrl = CONFIG_URL + "?v=" + cacheBuster +
  "&ip=" + encodeURIComponent(ip) +
  "&port=" + encodeURIComponent(port) +
  "&font_size=" + encodeURIComponent(rawFont) +
  "&scroll_text=" + encodeURIComponent(scrollText) +
  "&timeout_app=" + encodeURIComponent(timeApp) +
  "&timeout_disc=" + encodeURIComponent(timeDisc) +
  "&enable_touch=" + encodeURIComponent(enableTouch);

  Pebble.openURL(finalUrl);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e.response && e.response !== "CANCELLED") {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      if (config.ip) {
        localStorage.setItem('bridge_ip', config.ip);
        localStorage.setItem('bridge_port', config.port || "3000");
        localStorage.setItem('font_size', config.font_size || "1");
        localStorage.setItem('scroll_text', config.scroll_text || "0");
        localStorage.setItem('timeout_app', config.timeout_app || "0");
        localStorage.setItem('timeout_disc', config.timeout_disc || "0");
        localStorage.setItem('enable_touch', config.enable_touch || "1");
        fetchStatus();
      }
    } catch(err) {}
  }
});
