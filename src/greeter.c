/*
 * lightdm-tty-greeter — standalone LightDM greeter for the lightdm-tty theme
 *
 * Renders the HTML/JS theme via WebKitGTK and bridges the `window.lightdm`
 * JavaScript API to liblightdm-gobject for authentication and session management.
 *
 * Build:
 *   gcc -o lightdm-tty-greeter greeter.c $(pkg-config --cflags --libs \
 *       liblightdm-gobject-1 webkit2gtk-4.1 gtk+-3.0 json-glib-1.0)
 *
 * Install:
 *   sudo cp lightdm-tty-greeter /usr/lib/lightdm/
 *   sudo mkdir -p /usr/share/lightdm-tty-theme
 *   sudo cp -r ~/Projects/lightdm-tty/{index.html,js,css,img} /usr/share/lightdm-tty-theme/
 *   sudo sh -c 'echo "greeter-session=lightdm-tty-greeter" >> /etc/lightdm/lightdm.conf'
 */

#include <lightdm.h>
#include <webkit2/webkit2.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * Data
 * ------------------------------------------------------------------------- */

typedef struct {
    GtkWidget      *window;
    WebKitWebView  *web_view;
    LightDMGreeter *greeter;
    gchar          *theme_dir;
} AppData;

static AppData app = {0};

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/** Escape a string so it can be placed inside a JS single-quoted literal. */
static gchar *
escape_js_string(const gchar *str)
{
    if (!str) return g_strdup("");
    GString *out = g_string_sized_new(strlen(str) + 16);
    for (const gchar *p = str; *p; p++) {
        switch (*p) {
            case '\\': g_string_append(out, "\\\\"); break;
            case '\'': g_string_append(out, "\\'");  break;
            case '\n': g_string_append(out, "\\n");  break;
            case '\r': g_string_append(out, "\\r");  break;
            case '\t': g_string_append(out, "\\t");  break;
            default:   g_string_append_c(out, *p);   break;
        }
    }
    return g_string_free(out, FALSE);
}

/** Convenience: evaluate a small JS snippet in the web view. */
static void
eval_js(const gchar *js)
{
    if (app.web_view)
        webkit_web_view_evaluate_javascript(app.web_view, js, -1, NULL, NULL, NULL, NULL, NULL);
}

/* ---------------------------------------------------------------------------
 * Build JSON with LightDM initial state, then inject into JS
 * ------------------------------------------------------------------------- */

static void
append_users(JsonBuilder *b)
{
    LightDMUserList *ul = lightdm_user_list_get_instance();
    GList *users = lightdm_user_list_get_users(ul);

    json_builder_set_member_name(b, "users");
    json_builder_begin_array(b);
    for (GList *l = users; l; l = l->next) {
        LightDMUser *u = LIGHTDM_USER(l->data);
        json_builder_begin_object(b);
        json_builder_set_member_name(b, "name");         json_builder_add_string_value(b, lightdm_user_get_name(u) ?: "");
        json_builder_set_member_name(b, "real_name");    json_builder_add_string_value(b, lightdm_user_get_real_name(u) ?: "");
        json_builder_set_member_name(b, "display_name"); json_builder_add_string_value(b, lightdm_user_get_display_name(u) ?: "");
        json_builder_set_member_name(b, "image");        json_builder_add_string_value(b, lightdm_user_get_image(u) ?: "");
        json_builder_set_member_name(b, "logged_in");    json_builder_add_boolean_value(b, lightdm_user_get_logged_in(u));
        json_builder_end_object(b);
    }
    json_builder_end_array(b);

    json_builder_set_member_name(b, "num_users");
    json_builder_add_int_value(b, lightdm_user_list_get_length(ul));
}

static void
append_sessions(JsonBuilder *b)
{
    GList *sessions = lightdm_get_sessions();
    LightDMSession *def = NULL;

    json_builder_set_member_name(b, "sessions");
    json_builder_begin_array(b);
    for (GList *l = sessions; l; l = l->next) {
        LightDMSession *s = LIGHTDM_SESSION(l->data);
        if (!def) def = s;
        json_builder_begin_object(b);
        json_builder_set_member_name(b, "key");     json_builder_add_string_value(b, lightdm_session_get_key(s));
        json_builder_set_member_name(b, "name");    json_builder_add_string_value(b, lightdm_session_get_name(s));
        json_builder_set_member_name(b, "comment"); json_builder_add_string_value(b, lightdm_session_get_comment(s) ?: "");
        json_builder_end_object(b);
    }
    json_builder_end_array(b);

    if (def) {
        json_builder_set_member_name(b, "default_session");
        json_builder_begin_object(b);
        json_builder_set_member_name(b, "key");  json_builder_add_string_value(b, lightdm_session_get_key(def));
        json_builder_set_member_name(b, "name"); json_builder_add_string_value(b, lightdm_session_get_name(def));
        json_builder_end_object(b);
    }
}

static gchar *
build_lightdm_json(void)
{
    JsonBuilder *b = json_builder_new();
    json_builder_begin_object(b);

    json_builder_set_member_name(b, "hostname");
    json_builder_add_string_value(b, lightdm_get_hostname());

    gchar *motd = lightdm_get_motd();
    if (motd) {
        json_builder_set_member_name(b, "motd");
        json_builder_add_string_value(b, motd);
        g_free(motd);
    }

    json_builder_set_member_name(b, "can_suspend");  json_builder_add_boolean_value(b, lightdm_get_can_suspend());
    json_builder_set_member_name(b, "can_hibernate"); json_builder_add_boolean_value(b, lightdm_get_can_hibernate());
    json_builder_set_member_name(b, "can_restart");   json_builder_add_boolean_value(b, lightdm_get_can_restart());
    json_builder_set_member_name(b, "can_shutdown");  json_builder_add_boolean_value(b, lightdm_get_can_shutdown());
    json_builder_set_member_name(b, "lock_hint");     json_builder_add_boolean_value(b, lightdm_greeter_get_lock_hint(app.greeter));

    LightDMLanguage *lang = lightdm_get_language();
    if (lang) {
        json_builder_set_member_name(b, "language");
        json_builder_add_string_value(b, lightdm_language_get_code(lang));
    }

    append_users(b);
    append_sessions(b);

    json_builder_end_object(b);
    JsonNode *root = json_builder_get_root(b);
    gchar *json = json_to_string(root, FALSE);
    json_node_free(root);
    g_object_unref(b);
    return json;
}

static void
inject_initial_data(void)
{
    g_autofree gchar *json = build_lightdm_json();
    g_autofree gchar *escaped = escape_js_string(json);
    g_autofree gchar *js = g_strdup_printf("window._initLightdm('%s');", escaped);
    eval_js(js);
}

/* ---------------------------------------------------------------------------
 * JS message handler — JS → C bridge
 * ------------------------------------------------------------------------- */

static void
on_js_message(WebKitUserContentManager *mgr,
              WebKitJavascriptResult   *result,
              gpointer                  user_data)
{
    (void)mgr; (void)user_data;
    JSCValue *val = webkit_javascript_result_get_js_value(result);
    g_autofree gchar *str = jsc_value_to_string(val);
    if (!str) return;

    g_autoptr(JsonParser) parser = json_parser_new();
    if (!json_parser_load_from_data(parser, str, -1, NULL))
        return;

    JsonObject *obj = json_node_get_object(json_parser_get_root(parser));
    const gchar *method = json_object_get_string_member(obj, "method");
    JsonArray  *args   = json_object_get_array_member(obj, "args");
    GError *error = NULL;

    if (g_strcmp0(method, "start_authentication") == 0) {
        const gchar *username = json_array_get_string_element(args, 0);
        if (!lightdm_greeter_authenticate(app.greeter, username, &error)) {
            g_warning("auth: %s", error->message);
            g_clear_error(&error);
        }
    }
    else if (g_strcmp0(method, "cancel_authentication") == 0) {
        lightdm_greeter_cancel_authentication(app.greeter, &error);
        g_clear_error(&error);
    }
    else if (g_strcmp0(method, "respond") == 0) {
        const gchar *secret = json_array_get_string_element(args, 0);
        if (!lightdm_greeter_respond(app.greeter, secret, &error)) {
            g_warning("respond: %s", error->message);
            g_clear_error(&error);
        }
    }
    else if (g_strcmp0(method, "start_session") == 0) {
        const gchar *session_key = json_array_get_string_element(args, 0);
        if (session_key) {
            lightdm_greeter_start_session_sync(app.greeter, session_key, &error);
            if (error) {
                g_warning("session: %s", error->message);
                g_clear_error(&error);
            }
        }
    }
    else if (g_strcmp0(method, "suspend") == 0) {
        lightdm_suspend(&error); g_clear_error(&error);
    }
    else if (g_strcmp0(method, "hibernate") == 0) {
        lightdm_hibernate(&error); g_clear_error(&error);
    }
    else if (g_strcmp0(method, "restart") == 0) {
        lightdm_restart(&error); g_clear_error(&error);
    }
    else if (g_strcmp0(method, "shutdown") == 0) {
        lightdm_shutdown(&error); g_clear_error(&error);
    }
}

/* ---------------------------------------------------------------------------
 * LightDM signal callbacks
 * ------------------------------------------------------------------------- */

static void
on_show_prompt(LightDMGreeter   *greeter,
               const gchar      *text,
               LightDMPromptType type,
               gpointer          user_data)
{
    (void)text; (void)type; (void)user_data;
    const gchar *user = lightdm_greeter_get_authentication_user(greeter) ?: "";
    g_autofree gchar *euser = escape_js_string(user);
    g_autofree gchar *js = g_strdup_printf(
        "if(window.lightdm){"
        "window.lightdm.in_authentication=true;"
        "window.lightdm.authentication_user="
        "{name:'%s',real_name:'',display_name:''};"
        "}", euser);
    eval_js(js);
}

static void
on_auth_complete(LightDMGreeter *greeter, gpointer user_data)
{
    (void)greeter; (void)user_data;
    gboolean ok = lightdm_greeter_get_is_authenticated(app.greeter);
    const gchar *user = lightdm_greeter_get_authentication_user(greeter) ?: "";
    g_autofree gchar *euser = escape_js_string(user);

    if (ok) {
        g_autofree gchar *js = g_strdup_printf(
            "if(window.lightdm){"
            "window.lightdm.in_authentication=false;"
            "window.lightdm.is_authenticated=true;"
            "window.lightdm.authentication_user="
            "{name:'%s',real_name:'',display_name:''};"
            "if(window.authentication_complete)"
            "window.authentication_complete();"
            "}", euser);
        eval_js(js);
    } else {
        eval_js(
            "if(window.lightdm){"
            "window.lightdm.in_authentication=false;"
            "window.lightdm.is_authenticated=false;"
            "window.lightdm.authentication_user=null;"
            "if(window.authentication_complete)"
            "window.authentication_complete();"
            "}");
    }
}

static void
on_show_message(LightDMGreeter   *greeter,
                const gchar      *text,
                LightDMMessageType type,
                gpointer          user_data)
{
    (void)greeter; (void)type; (void)user_data;
    if (!text || !*text) return;
    g_autofree gchar *escaped = g_markup_escape_text(text, -1);
    g_autofree gchar *js = g_strdup_printf(
        "var o=document.getElementById('stdout');"
        "if(o) o.innerHTML+='<br><span class=\"stdout-red\">%s</span><br>';",
        escaped);
    eval_js(js);
}

/* ---------------------------------------------------------------------------
 * Page load complete — connect lightdm, inject data
 * ------------------------------------------------------------------------- */

static void
on_page_loaded(WebKitWebView *view, WebKitLoadEvent event, gpointer data)
{
    (void)view; (void)data;
    if (event != WEBKIT_LOAD_FINISHED) return;

    /* Connect to LightDM daemon */
    GError *error = NULL;
    if (!lightdm_greeter_connect_to_daemon_sync(app.greeter, &error)) {
        g_warning("lightdm connect failed: %s", error->message);
        g_error_free(error);
        /* continue anyway — greeter shows empty terminal on no-daemon */
    }

    /* Connect LightDM signals */
    g_signal_connect(app.greeter, LIGHTDM_GREETER_SIGNAL_SHOW_PROMPT,
                     G_CALLBACK(on_show_prompt), NULL);
    g_signal_connect(app.greeter, LIGHTDM_GREETER_SIGNAL_AUTHENTICATION_COMPLETE,
                     G_CALLBACK(on_auth_complete), NULL);
    g_signal_connect(app.greeter, LIGHTDM_GREETER_SIGNAL_SHOW_MESSAGE,
                     G_CALLBACK(on_show_message), NULL);

    /* Inject real user/session/etc data into JS */
    inject_initial_data();
}

/* ---------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    /* Theme directory */
    const gchar *dir = g_getenv("LIGHTDM_TTY_DIR");
    if (dir) {
        app.theme_dir = g_strdup(dir);
    } else if (g_file_test("/usr/share/lightdm-tty-theme/index.html",
                           G_FILE_TEST_EXISTS)) {
        app.theme_dir = g_strdup("/usr/share/lightdm-tty-theme");
    } else {
        /* dev path */
        app.theme_dir = g_build_filename(g_get_home_dir(),
                                         "Projects", "lightdm-tty", NULL);
    }

    app.greeter = lightdm_greeter_new();
    lightdm_greeter_set_resettable(app.greeter, TRUE);

    /* --- Window --- */
    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "Login");
    gtk_window_set_decorated(GTK_WINDOW(app.window), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(app.window));
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* --- WebView with JS bridge --- */
    WebKitUserContentManager *ucm = webkit_user_content_manager_new();
    g_signal_connect(ucm, "script-message-received::lightdm",
                     G_CALLBACK(on_js_message), NULL);
    webkit_user_content_manager_register_script_message_handler(ucm, "lightdm");

    app.web_view = WEBKIT_WEB_VIEW(
        webkit_web_view_new_with_user_content_manager(ucm));
    g_object_unref(ucm);

    WebKitSettings *settings = webkit_web_view_get_settings(app.web_view);
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(settings, FALSE);

    gtk_container_add(GTK_CONTAINER(app.window), GTK_WIDGET(app.web_view));

    /* --- Load theme --- */
    g_signal_connect(app.web_view, "load-changed",
                     G_CALLBACK(on_page_loaded), NULL);
    g_autofree gchar *uri = g_strdup_printf("file://%s/index.html",
                                            app.theme_dir);
    webkit_web_view_load_uri(app.web_view, uri);

    gtk_widget_show_all(app.window);
    gtk_main();

    g_free(app.theme_dir);
    g_object_unref(app.greeter);
    return 0;
}
