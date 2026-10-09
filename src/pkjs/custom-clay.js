module.exports = function(minified) {
    var clayConfig = this;

    function toggleHardwareSettings() {
        var platform = 'aplite';
        if (clayConfig.meta && clayConfig.meta.activeWatchInfo && clayConfig.meta.activeWatchInfo.platform) {
            platform = clayConfig.meta.activeWatchInfo.platform;
        }

        var isTouchCapable = (platform === 'emery' || platform === 'gabbro');

        var touchItem = clayConfig.getItemByMessageKey('enable_touch');
        var accelPlaypauseItem = clayConfig.getItemByMessageKey('enable_accel_playpause');

        if (touchItem) {
            if (isTouchCapable) touchItem.show();
            else touchItem.hide();
        }

        if (accelPlaypauseItem) {
            if (!isTouchCapable) accelPlaypauseItem.show();
            else accelPlaypauseItem.hide();
        }
    }

    function toggleWatchfaceSettings() {
        var wfToggle = clayConfig.getItemByMessageKey('enable_watchface');
        var suppressQuiet = clayConfig.getItemByMessageKey('suppress_gesture_quiet');
        var timeoutWf = clayConfig.getItemByMessageKey('timeout_to_app_wf');
        var respectQuiet = clayConfig.getItemByMessageKey('respect_quiet_time');

        if (wfToggle && suppressQuiet && timeoutWf && respectQuiet) {
            if (wfToggle.get()) {
                suppressQuiet.show();
                timeoutWf.show();
                respectQuiet.show();
            } else {
                suppressQuiet.hide();
                timeoutWf.hide();
                respectQuiet.hide();
            }
        }
    }

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        toggleHardwareSettings();
        toggleWatchfaceSettings();

        var wfToggle = clayConfig.getItemByMessageKey('enable_watchface');
        if (wfToggle) {
            wfToggle.on('change', function() {
                toggleWatchfaceSettings();
            });
        }
    });
};
