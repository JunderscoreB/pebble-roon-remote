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
      "type": "select",
      "messageKey": "timeout_app",
      "defaultValue": "0",
      "label": "App Inactivity Timeout",
      "description": "Closes the app automatically if no buttons are pressed.",
      "options": [
        { "label": "Infinite (Never close)", "value": "0" },
        { "label": "1 Minute", "value": "1" },
        { "label": "5 Minutes", "value": "5" },
        { "label": "15 Minutes", "value": "15" },
        { "label": "30 Minutes", "value": "30" },
        { "label": "1 Hour", "value": "60" }
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
        { "label": "1 Minute", "value": "1" },
        { "label": "5 Minutes", "value": "5" },
        { "label": "15 Minutes", "value": "15" },
        { "label": "30 Minutes", "value": "30" }
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
