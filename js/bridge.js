// bridge.js — replaces mock.js in production
// Communicates with native C greeter via window.webkit.messageHandlers.lightdm
(function() {
    if (typeof window.lightdm !== 'undefined') return;

    var bridge = window.webkit.messageHandlers.lightdm;

    window.lightdm = {
        hostname: '',
        in_authentication: false,
        is_authenticated: false,
        authentication_user: null,
        default_session: null,
        can_suspend: false,
        can_hibernate: false,
        can_restart: false,
        can_shutdown: false,
        selected_session: null,
        timed_login_delay: 0,
        timed_login_user: null,
        lock_hint: false,

        users: [],
        num_users: 0,
        sessions: [],
        languages: [],
        layouts: [],

        _post: function(method, args) {
            bridge.postMessage(JSON.stringify({method: method, args: args}));
        },

        start_authentication: function(username) {
            this.in_authentication = true;
            this._post('start_authentication', [username]);
        },

        cancel_authentication: function() {
            this.in_authentication = false;
            this._post('cancel_authentication', []);
        },

        respond: function(response) {
            this._post('respond', [response]);
        },

        provide_secret: function(secret) {
            this._post('respond', [secret]);
        },

        start_session: function(session) {
            this._post('start_session', [session]);
        },

        login: function(user, session) {
            this._post('start_session', [session]);
        },

        suspend: function() { this._post('suspend', []); },
        hibernate: function() { this._post('hibernate', []); },
        restart: function() { this._post('restart', []); },
        shutdown: function() { this._post('shutdown', []); },

        cancel_timed_login: function() {},
        get_string_property: function() {},
        get_integer_property: function() {},
        get_boolean_property: function() {}
    };

    window.authentication_complete = function() {
        if (window.lightdm && window.lightdm._pending_auth) {
            window.lightdm._pending_auth();
            window.lightdm._pending_auth = null;
        }
    };

    // Called by C after LightDM data is available
    window._initLightdm = function(jsonStr) {
        try {
            var data = JSON.parse(jsonStr);
            for (var key in data) {
                if (data.hasOwnProperty(key)) {
                    window.lightdm[key] = data[key];
                }
            }
            // Update prompt with real data
            var user = null;
            for (var i = 0; i < window.lightdm.users.length; i++) {
                if (window.lightdm.users[i].logged_in) {
                    user = window.lightdm.users[i];
                    break;
                }
            }
            var promptEl = document.getElementById('prompt');
            if (promptEl) {
                if (user) {
                    promptEl.innerHTML = '<span class="stdout-green">' + user.name + '</span><span class="stdout-red">@</span>' + window.lightdm.hostname + ' $ ';
                } else {
                    promptEl.innerHTML = window.lightdm.hostname + ' $ ';
                }
            }
            // Show MOTD if available
            var stdout = document.getElementById('stdout');
            if (stdout && window.lightdm.motd) {
                stdout.innerHTML = window.lightdm.motd + '<br>';
            }
        } catch(e) {
            console.error('_initLightdm error:', e);
        }
    };
})();
