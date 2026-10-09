module.exports = [
  {
    "type": "heading",
    "defaultValue": "Roon Remote Settings"
  },
{
  "type": "section",
  "items": [
    {
      "type": "input",
      "messageKey": "bridge_ip",
      "defaultValue": "192.168.1.50",
      "label": "Bridge IP Address",
      "attributes": {
        "placeholder": "e.g. 192.168.1.50"
      }
    },
    {
      "type": "input",
      "messageKey": "bridge_port",
      "defaultValue": "3000",
      "label": "Bridge Port",
      "attributes": {
        "placeholder": "e.g. 3000",
        "type": "number"
      }
    }
  ]
},
{
  "type": "section",
  "items": [
    {
      "type": "toggle",
      "messageKey": "enable_watchface",
      "defaultValue": false,
      "label": "Watchface Mode",
      "description": "Displays clock time and current playback in a watchface layout."
    },
    {
      "type": "toggle",
      "messageKey": "timeout_to_app_wf",
      "defaultValue": true,
      "label": "Timeout to App Watchface",
      "description": "Inactivity timeouts drop to the idle Watchface instead of closing the app entirely."
    },
    {
      "type": "toggle",
      "messageKey": "respect_quiet_time",
      "defaultValue": true,
      "label": "Hide Music on Quiet Time",
      "description": "In watchface mode, hides music info when Quiet Time is active."
    },
    {
      "type": "toggle",
      "messageKey": "suppress_gesture_quiet",
      "defaultValue": false,
      "label": "Suppress Wake on Quiet Time",
      "description": "Disables wrist flicks from waking the watchface to Gesture Mode when Quiet Time is active."
    },
    {
      "type": "select",
      "messageKey": "timeout_app",
      "defaultValue": "0",
      "label": "App Inactivity Timeout",
      "description": "Switches to Watchface or closes the app if no buttons/gestures are used.",
      "options": [
        { "label": "Infinite (Never close)", "value": "0" },
        { "label": "15 Seconds", "value": "15" },
        { "label": "30 Seconds", "value": "30" },
        { "label": "1 Minute", "value": "60" },
        { "label": "2 Minutes", "value": "120" },
        { "label": "5 Minutes", "value": "300" }
      ]
    },
    {
      "type": "select",
      "messageKey": "timeout_disc",
      "defaultValue": "0",
      "label": "Disconnect Timeout",
      "description": "Closes the app if the Bridge connection or Bluetooth drops.",
      "options": [
        { "label": "Infinite (Never close)", "value": "0" },
        { "label": "15 Seconds", "value": "15" },
        { "label": "30 Seconds", "value": "30" },
        { "label": "1 Minute", "value": "60" },
        { "label": "2 Minutes", "value": "120" },
        { "label": "5 Minutes", "value": "300" }
      ]
    }
  ]
},
{
  "type": "section",
  "items": [
    {
      "type": "select",
      "messageKey": "theme",
      "defaultValue": "0",
      "label": "App Theme",
      "options": [
        { "label": "Dark", "value": "0" },
        { "label": "Light", "value": "1" }
      ]
    },
    {
      "type": "select",
      "messageKey": "font_size",
      "defaultValue": "1",
      "label": "Font Size",
      "options": [
        { "label": "Small", "value": "0" },
        { "label": "Normal", "value": "1" },
        { "label": "Large", "value": "2" }
      ]
    },
    {
      "type": "select",
      "messageKey": "scroll_text",
      "defaultValue": "0",
      "label": "Long Text Handling",
      "options": [
        { "label": "Truncate (Ellipsis)", "value": "0" },
        { "label": "Marquee (Scroll)", "value": "1" },
        { "label": "Word Wrap (Multi-line)", "value": "2" }
      ]
    },
    {
      "type": "toggle",
      "messageKey": "enable_accel_playpause",
      "defaultValue": true,
      "label": "Wrist Flick Play/Pause",
      "description": "Flick your wrist to toggle playback."
    },
    {
      "type": "toggle",
      "messageKey": "enable_flick_vibes",
      "defaultValue": true,
      "label": "Vibrate on Wrist Flick",
      "description": "Provide a tactile vibration when waking to Gesture Mode."
    },
    {
      "type": "select",
      "messageKey": "enable_touch",
      "defaultValue": "1",
      "label": "Screen Touch/Tap Actions",
      "options": [
        { "label": "Enabled (Gestures)", "value": "1" },
        { "label": "Disabled (Ignore Touches)", "value": "0" }
      ]
    }
  ]
},
{
  "type": "submit",
  "defaultValue": "Save to Watch"
}
];
