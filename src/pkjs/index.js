var DEFAULT_IP = "192.168.1.50"; 
var DEFAULT_PORT = "3000";

// The offline config.html encoded as a Data URI for production users
var CONFIG_URL = "data:text/html;base64,PCFET0NUWVBFIGh0bWw+CjxodG1sPgo8aGVhZD4KICA8bWV0YSBjaGFyc2V0PSJ1dGYtOCI+CiAgPG1ldGEgbmFtZT0idmlld3BvcnQiIGNvbnRlbnQ9IndpZHRoPWRldmljZS13aWR0aCwgaW5pdGlhbC1zY2FsZT0xIj4KICA8dGl0bGU+Um9vbiBCcmlkZ2UgU2V0dGluZ3M8L3RpdGxlPgogIDxzdHlsZT4KICAgIGJvZHkgeyBmb250LWZhbWlseTogc2Fucy1zZXJpZjsgcGFkZGluZzogMjBweDsgYmFja2dyb3VuZDogIzIyMjsgY29sb3I6ICNlZWU7IH0KICAgIGxhYmVsIHsgZGlzcGxheTogYmxvY2s7IG1hcmdpbi1ib3R0b206IDEwcHg7IGZvbnQtd2VpZ2h0OiBib2xkOyB9CiAgICBpbnB1dCB7IHdpZHRoOiAxMDAlOyBwYWRkaW5nOiAxMHB4OyBtYXJnaW4tYm90dG9tOiAyMHB4OyBib3JkZXItcmFkaXVzOiA1cHg7IGJvcmRlcDogbm9uZTsgYm94LXNpemluZzogYm9yZGVyLWJveDsgfQogICAgYnV0dG9uIHsgd2lkdGg6IDEwMCU7IHBhZGRpbmc6IDE1cHg7IGJvcmRlcDogbm9uZTsgYm9yZGVyLXJhZGl1czogNXB4OyBmb250LXNpemU6IDE2cHg7IGN1cnNvcjogcG9pbnRlcjsgbWFyZ2luLWJvdHRvbTogMTBweDsgZm9udC13ZWlnaHQ6IGJvbGQ7IH0KICAgIC5idG4tdGVzdCB7IGJhY2tncm91bmQ6ICM0Y2FmNTA7IGNvbG9yOiB3aGl0ZTsgfQogICAgLmJ0bi1zYXZlIHsgYmFja2dyb3VuZDogI2ZmNTcyMjsgY29sb3I6IHdoaXRlOyB9CiAgICAjc3RhdHVzLW1zZyB7IG1hcmdpbi10b3A6IDE1cHg7IGZvbnQtc2l6ZTogMThweDsgdGV4dC1hbGlnbjogY2VudGVyOyBmb250LXdlaWdodDogYm9sZDsgcGFkZGluZzogMTBweDsgYm9yZGVyLXJhZGl1czogNXB4OyB9CiAgICAuc3VjY2VzcyB7IGJhY2tncm91bmQ6IHJnYmEoNzYsIDE3NSwgODAsIDAuMik7IGNvbG9yOiAjNGNhZjUwOyBib3JkZXI6IDFweCBzb2xpZCAjNGNhZjUwOyB9CiAgICAuZXJyb3IgeyBiYWNrZ3JvdW5kOiByZ2JhKDI0NCwgNjcsIDU0LCAwLjIpOyBjb2xvcjogI2Y0NDMzNjsgYm9yZGVyOiAxcHggc29saWQgI2Y0NDMzNjsgfQogIDwvc3R5bGU+CjwvaGVhZD4KPGJvZHk+CiAgPGgyPlJvb24gQnJpZGdlIFNldHRpbmdzPC9oMj4KICAKICA8bGFiZWwgZm9yPSJpcCI+RXh0ZW5zaW9uIElQIEFkZHJlc3M8L2xhYmVsPgogIDxpbnB1dCB0eXBlPSJ0ZXh0IiBpZD0iaXAiIHBsYWNlaG9sZGVyPSIxOTIuMTY4LjEuNTAiIC8+CiAgCiAgPGxhYmVsIGZvcj0icG9ydCI+UG9ydDwvbGFiZWw+CiAgPGlucHV0IHR5cGU9InRleHQiIGlkPSJwb3J0IiBwbGFjZWhvbGRlcj0iMzAwMCIgdmFsdWU9IjMwMDAiIC8+CiAgCiAgPGJ1dHRvbiBjbGFzcz0iYnRuLXRlc3QiIG9uY2xpY2s9InRlc3RDb25uZWN0aW9uKCkiPlRlc3QgQ29ubmVjdGlvbjwvYnV0dG9uPgogIDxidXR0b24gY2xhc3M9ImJ0bi1zYXZlIiBvbmNsaWNrPSJzYXZlKCkiPlNhdmUgdG8gV2F0Y2g8L2J1dHRvbj4KCiAgPGRpdiBpZD0ic3RhdHVzLW1zZyI+PC9kaXY+CgogIDxzY3JpcHQ+CiAgICBmdW5jdGlvbiBnZXRRdWVyeVBhcmFtKGgpIHsKICAgICAgdmFyIHJlc3VsdCA9IG5ldyBSZWdFeHAoJ1s/Jl0nICsgaCArICc9KFteJiNdKiknKS5leGVjKHdpbmRvdy5sb2NhdGlvbi5ocmVmKTsKICAgICAgcmV0dXJuIHJlc3VsdCAmJiBkZWNvZGVVUklDb21wb25lbnQocmVzdWx0WzFdKSB8fCAnJzsKICAgIH0KICAgIAogICAgZG9jdW1lbnQuZ2V0RWxlbWVudEJ5SWQoJ2lwJykudmFsdWUgPSBnZXRRdWVyeVBhcmFtKCdpcCcpIHx8ICcnOwogICAgZG9jdW1lbnQuZ2V0RWxlbWVudEJ5SWQoJ3BvcnQnKS52YWx1ZSA9IGdldFF1ZXJ5UGFyYW0oJ3BvcnQnKSB8fCAnMzAwMCc7CgogICAgZnVuY3Rpb24gdGVzdENvbm5lY3Rpb24oKSB7CiAgICAgIHZhciBpcCA9IGRvY3VtZW50LmdldEVsZW1lbnRCeUlkKCdpcCcpLnZhbHVlOwogICAgICB2YXIgcG9ydCA9IGRvY3VtZW50LmdldEVsZW1lbnRCeUlkKCdwb3J0JykudmFsdWU7CiAgICAgIHZhciBtc2cgPSBkb2N1bWVudC5nZXRFbGVtZW50QnlJZCgnc3RhdHVzLW1zZycpOwogICAgICAKICAgICAgbXNnLnRleHRDb250ZW50ID0gIlBpbmdpbmcgQnJpZGdlLi4uIjsKICAgICAgbXNnLmNsYXNzTmFtZSA9ICIiOwoKICAgICAgZmV0Y2goJ2h0dHA6Ly8nICsgaXAgKyAnOicgKyBwb3J0ICsgJy9zdGF0dXMnKQogICAgICAgIC50aGVuKGZ1bmN0aW9uKHJlc3BvbnNlKSB7CiAgICAgICAgICBpZiAocmVzcG9uc2Uub2spIHsKICAgICAgICAgICAgbXNnLnRleHRDb250ZW50ID0gIvCdjIUgQnJpZGdlIEFjdGl2ZSAmIEZvdW5kISI7CiAgICAgICAgICAgIG1zZy5jbGFzc05hbWUgPSAic3VjY2VzcyI7CiAgICAgICAgICB9IGVsc2UgewogICAgICAgICAgICBtc2cudGV4dENvbnRlbnQgPSAi4pqg77iPIEZvdW5kLCBidXQgcmV0dXJuZWQgZXJyb3IgY29kZTogIiArIHJlc3BvbnNlLnN0YXR1czsKICAgICAgICAgICAgbXNnLmNsYXNzTmFtZSA9ICJlcnJvciI7CiAgICAgICAgICB9CiAgICAgICAgfSkKICAgICAgICAuY2F0Y2goZnVuY3Rpb24oZXJyKSB7CiAgICAgICAgICBtc2cudGV4dENvbnRlbnQgPSAi4p2MIEJyaWRnZSBOb3QgRm91bmQgb3IgT2ZmbGluZS4iOwogICAgICAgICAgbXNnLmNsYXNzTmFtZSA9ICJlcnJvciI7CiAgICAgICAgICBjb25zb2xlLmVycm9yKCJDb25uZWN0aW9uIGVycm9yOiIsIGVycik7CiAgICAgICAgfSk7CiAgICB9CgogICAgZnVuY3Rpb24gc2F2ZSgpIHsKICAgICAgdmFyIGNvbmZpZyA9IHsgImlwIjogZG9jdW1lbnQuZ2V0RWxlbWVudEJ5SWQoJ2lwJykudmFsdWUsICJwb3J0IjogZG9jdW1lbnQuZ2V0RWxlbWVudEJ5SWQoJ3BvcnQnKS52YWx1ZSB9OwogICAgICB2YXIgcmV0dXJuX3RvID0gZ2V0UXVlcnlQYXJhbSgncmV0dXJuX3RvJykgfHwgJ3BlYmJsZWpzOi8vYmxvc2UjJzsKICAgICAgbG9jYXRpb24uaHJlZiA9IHJldHVybl90byArIGVuY29kZVVSSUNvbXBvbmVudChKU09OLnN0cmluZ2lmeShjb25maWcpKTsKICAgIH0KICA8L3NjcmlwdD4KPC9ib2R5Pgo8L2h0bWw+";

// --- STATE TRACKING ---
var g_isPlaying = false;

function getBridgeUrl() {
  var ip = localStorage.getItem('bridge_ip') || DEFAULT_IP;
  var port = localStorage.getItem('bridge_port') || DEFAULT_PORT;
  return "http://" + ip + ":" + port + "/";
}

// --- HELPER FOR SENDING COMMANDS ---
function sendBridgeCommand(command) {
  var req = new XMLHttpRequest();
  req.open('GET', getBridgeUrl() + command, true);
  req.onload = function() {
    if (req.status === 200) sendToWatch(req.responseText);
  };
  req.send(null);
}

function scheduleNextFetch() { 
  setTimeout(fetchStatus, 3000); 
}

// --- STATUS POLLING & ERROR HANDLING ---
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
  Pebble.sendAppMessage({
    'error': 1 
  });
}

// --- PARSE AND SEND TO WATCH ---
function sendToWatch(responseText) {
  try {
    var response = JSON.parse(responseText);

    if (response.is_playing !== undefined) {
      g_isPlaying = response.is_playing;
    }

    var safeVolume = -1;
    if (response.volume !== undefined && response.volume !== null) {
      safeVolume = parseInt(response.volume);
    } else if (response.volume_value !== undefined) {
      safeVolume = parseInt(response.volume_value);
    } else if (response.level !== undefined) {
      safeVolume = parseInt(response.level);
    }

    if (isNaN(safeVolume)) safeVolume = -1;

    Pebble.sendAppMessage({
      'zone_name': response.zone || "Unknown",
      'track': response.track || "",
      'artist': response.artist || "",
      'is_playing': response.is_playing ? 1 : 0,
      'volume_val': safeVolume,
      'is_fixed': response.is_fixed_volume ? 1 : 0,
      'error': 0 
    });
  } catch (err) {
    console.log("JSON Err: " + err);
  }
}

// --- APP INITIALIZATION (DEV SIDELOAD LOGIC) ---
function initApp() {
  // Silently check if the developer has a python server running with a dev_config.json
  var req = new XMLHttpRequest();
  req.open('GET', 'http://localhost:8080/dev_config.json', true);
  req.timeout = 1000; // Fail instantly if no local server is running
  
  req.onload = function() {
    if (req.status === 200) {
      try {
        var devConfig = JSON.parse(req.responseText);
        if (devConfig.ip) {
          console.log("DEV OVERRIDE LOADED: " + devConfig.ip);
          localStorage.setItem('bridge_ip', devConfig.ip);
        }
      } catch(e) { 
        console.log("Error parsing dev_config"); 
      }
    }
    fetchStatus(); 
  };
  
  // If the user is running the app normally, this simply fails and proceeds
  req.onerror = function() { fetchStatus(); }; 
  req.ontimeout = function() { fetchStatus(); };
  req.send(null);
}

// Start the app when ready
Pebble.addEventListener('ready', function() { 
  initApp(); 
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
  if (e.response) {
    var config = JSON.parse(decodeURIComponent(e.response));
    if (config.ip) {
      localStorage.setItem('bridge_ip', config.ip);
      localStorage.setItem('bridge_port', config.port || "3000");
      fetchStatus();
    }
  }
});
