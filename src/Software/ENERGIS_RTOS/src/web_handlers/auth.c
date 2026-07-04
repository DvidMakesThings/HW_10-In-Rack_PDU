/**
 * @file src/web_handlers/auth.c
 * @brief HTTP session authentication for ENERGIS PDU web interface.
 *
 * Cookie-based session with 10-minute sliding expiry.
 * Auth config (enabled flag + password) persisted in EEPROM via StorageTask.
 */

#include "../CONFIG.h"

#define AUTH_TAG "[AUTH]"
#define AUTH_PASSWORD_MAX 32

/* ---- Session state (RAM only, single active session) ---- */
static auth_config_t s_auth_cfg;
static char s_session_token[AUTH_TOKEN_LEN + 1];
static bool s_session_valid;
static TickType_t s_session_expiry;

/* ---- RNG using RP2040 ROSC random bit ---- */
#include "hardware/structs/rosc.h"

static uint8_t rosc_random_byte(void) {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte = (byte << 1) | (rosc_hw->randombit & 1);
    }
    return byte;
}

static void generate_token(char *buf, size_t len) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        buf[i] = hex[rosc_random_byte() & 0x0F];
    }
    buf[len] = '\0';
}

/* ---- Init ---- */
void auth_init(void) {
    s_session_valid = false;
    memset(s_session_token, 0, sizeof(s_session_token));

    if (!storage_get_auth(&s_auth_cfg)) {
        /* Defaults: enabled, password "admin" */
        s_auth_cfg.enabled = 1;
        strncpy(s_auth_cfg.password, "admin", AUTH_PASSWORD_MAX - 1);
        s_auth_cfg.password[AUTH_PASSWORD_MAX - 1] = '\0';
    }
    INFO_PRINT("%s Auth %s\r\n", AUTH_TAG, s_auth_cfg.enabled ? "enabled" : "disabled");
}

/* ---- Public queries ---- */
bool auth_is_enabled(void) { return s_auth_cfg.enabled != 0; }

/* ---- Cookie parsing ---- */
static const char *find_cookie_value(const char *request, const char *name) {
    const char *cookie_hdr = strstr(request, "Cookie:");
    if (!cookie_hdr)
        cookie_hdr = strstr(request, "cookie:");
    if (!cookie_hdr)
        return NULL;

    /* Skip past "Cookie:" to the value portion */
    const char *search_start = cookie_hdr + 7; /* strlen("Cookie:") */
    const char *end_of_line = strstr(search_start, "\r\n");
    if (!end_of_line)
        end_of_line = search_start + strlen(search_start);

    size_t name_len = strlen(name);
    const char *p = search_start;
    while (p < end_of_line) {
        /* Skip whitespace and semicolons */
        while (p < end_of_line && (*p == ' ' || *p == ';' || *p == '\t'))
            p++;
        if (p >= end_of_line)
            break;

        if (strncmp(p, name, name_len) == 0 && p[name_len] == '=') {
            return p + name_len + 1;
        }
        /* Skip to next cookie */
        while (p < end_of_line && *p != ';')
            p++;
    }
    return NULL;
}

bool auth_check_request(const char *http_request) {
    if (!s_auth_cfg.enabled)
        return true;

    if (!s_session_valid)
        return false;

    /* Check expiry */
    if ((xTaskGetTickCount() - s_session_expiry) > pdMS_TO_TICKS(AUTH_SESSION_EXPIRY_MS)) {
        s_session_valid = false;
        return false;
    }

    const char *token = find_cookie_value(http_request, "session");
    if (!token)
        return false;

    /* Compare token (fixed length) */
    if (strncmp(token, s_session_token, AUTH_TOKEN_LEN) != 0)
        return false;

    /* Refresh sliding expiry */
    s_session_expiry = xTaskGetTickCount();
    return true;
}

/* ---- Login / Logout ---- */
const char *auth_login(const char *password) {
    if (!password)
        return NULL;

    /* Constant-time-ish compare to prevent timing attacks */
    size_t pw_len = strlen(s_auth_cfg.password);
    size_t in_len = strlen(password);
    volatile uint8_t diff = (pw_len != in_len) ? 1 : 0;
    size_t cmp_len = pw_len < in_len ? pw_len : in_len;
    for (size_t i = 0; i < cmp_len; i++) {
        diff |= (uint8_t)(s_auth_cfg.password[i] ^ password[i]);
    }
    if (diff)
        return NULL;

    generate_token(s_session_token, AUTH_TOKEN_LEN);
    s_session_valid = true;
    s_session_expiry = xTaskGetTickCount();
    INFO_PRINT("%s Login successful\r\n", AUTH_TAG);
    return s_session_token;
}

void auth_logout(void) {
    s_session_valid = false;
    memset(s_session_token, 0, sizeof(s_session_token));
    INFO_PRINT("%s Logout\r\n", AUTH_TAG);
}

/* ---- Config get/set ---- */
void auth_get_config(auth_config_t *out) {
    if (out)
        *out = s_auth_cfg;
}

bool auth_set_config(uint8_t enabled, const char *new_password) {
    s_auth_cfg.enabled = enabled;
    if (new_password && new_password[0] != '\0') {
        strncpy(s_auth_cfg.password, new_password, AUTH_PASSWORD_MAX - 1);
        s_auth_cfg.password[AUTH_PASSWORD_MAX - 1] = '\0';
    }

    /* If auth was just disabled, clear any active session requirement */
    if (!enabled) {
        s_session_valid = false;
    }

    return storage_set_auth(&s_auth_cfg);
}

/* ---- HTTP endpoint handlers ---- */

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

void handle_auth_login(uint8_t sock, char *body) {
    char *pw = get_form_value(body, "password");
    if (pw)
        urldecode(pw);

    const char *token = NULL;
    if (pw)
        token = auth_login(pw);

    if (token) {
        char resp[256];
        int len = snprintf(resp, sizeof(resp),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Set-Cookie: session=%s; Path=/; Max-Age=600; SameSite=Strict\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Cache-Control: no-cache\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "{\"ok\":true}",
                           token);
        send(sock, (uint8_t *)resp, (uint16_t)len);
    } else {
        static const char fail[] = "HTTP/1.1 401 Unauthorized\r\n"
                                   "Content-Type: application/json\r\n"
                                   "Access-Control-Allow-Origin: *\r\n"
                                   "Cache-Control: no-cache\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "{\"ok\":false,\"error\":\"Invalid password\"}";
        send(sock, (uint8_t *)fail, sizeof(fail) - 1);
    }
    net_beat();
}

void handle_auth_logout(uint8_t sock) {
    auth_logout();
    static const char resp[] = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Set-Cookie: session=; Path=/; Max-Age=0; SameSite=Strict\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Cache-Control: no-cache\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "{\"ok\":true}";
    send(sock, (uint8_t *)resp, sizeof(resp) - 1);
    net_beat();
}

void handle_auth_status(uint8_t sock, const char *http_request) {
    bool authed = auth_check_request(http_request);
    bool enabled = auth_is_enabled();

    char body[96];
    int blen = snprintf(body, sizeof(body), "{\"enabled\":%s,\"authenticated\":%s}",
                        enabled ? "true" : "false", authed ? "true" : "false");

    char hdr[192];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %d\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        blen);
    send(sock, (uint8_t *)hdr, (uint16_t)hlen);
    send(sock, (uint8_t *)body, (uint16_t)blen);
    net_beat();
}

void handle_auth_config(uint8_t sock, char *body, const char *http_request) {
    /* Must be authenticated to change auth config */
    if (auth_is_enabled() && !auth_check_request(http_request)) {
        static const char denied[] = "HTTP/1.1 401 Unauthorized\r\n"
                                     "Content-Type: application/json\r\n"
                                     "Access-Control-Allow-Origin: *\r\n"
                                     "Connection: close\r\n"
                                     "\r\n"
                                     "{\"ok\":false,\"error\":\"Not authenticated\"}";
        send(sock, (uint8_t *)denied, sizeof(denied) - 1);
        net_beat();
        return;
    }

    /* Parse fields */
    char new_pw[AUTH_PASSWORD_MAX] = {0};
    uint8_t new_enabled = s_auth_cfg.enabled;

    char *tmp = get_form_value(body, "auth_enabled");
    if (tmp) {
        urldecode(tmp);
        new_enabled = (strcmp(tmp, "1") == 0 || strcmp(tmp, "on") == 0) ? 1 : 0;
    }

    tmp = get_form_value(body, "new_password");
    if (tmp) {
        urldecode(tmp);
        strncpy(new_pw, tmp, AUTH_PASSWORD_MAX - 1);
        new_pw[AUTH_PASSWORD_MAX - 1] = '\0';
    }

    auth_set_config(new_enabled, new_pw[0] ? new_pw : NULL);

    /* Commit to EEPROM */
    vTaskDelay(pdMS_TO_TICKS(10));
    net_beat();

    static const char ok[] = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "\r\n"
                             "{\"ok\":true}";
    send(sock, (uint8_t *)ok, sizeof(ok) - 1);
    net_beat();

    /* Persist to EEPROM immediately */
    (void)storage_commit_now(5000);
}

/* ---- Login page HTML ---- */
void auth_send_login_page(uint8_t sock) {
    static const char login_html[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html>"
        "<html lang='en'><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ENERGIS - Login</title>"
        "<style>"
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{min-height:100vh;display:flex;align-items:center;justify-content:center;"
        "background:#0a0a0a;font-family:-apple-system,BlinkMacSystemFont,'Segoe "
        "UI',Roboto,sans-serif;"
        "color:#e0e0e0}"
        ".login-card{background:rgba(30,30,30,0.95);border:1px solid rgba(245,158,11,0.3);"
        "border-radius:16px;padding:40px;width:340px;text-align:center;"
        "box-shadow:0 8px 32px rgba(0,0,0,0.5)}"
        ".login-card h1{font-size:1.5rem;color:#f59e0b;margin-bottom:8px}"
        ".login-card p{font-size:0.85rem;color:#888;margin-bottom:24px}"
        "input[type=password]{width:100%;padding:12px 16px;border-radius:8px;"
        "border:1px solid rgba(245,158,11,0.3);background:rgba(0,0,0,0.4);"
        "color:#e0e0e0;font-size:1rem;outline:none;margin-bottom:16px}"
        "input[type=password]:focus{border-color:#f59e0b}"
        "button{width:100%;padding:12px;border-radius:8px;border:none;"
        "background:linear-gradient(135deg,#f59e0b,#d97706);color:#000;"
        "font-weight:600;font-size:1rem;cursor:pointer;transition:opacity 0.2s}"
        "button:hover{opacity:0.9}"
        ".error{color:#ef4444;font-size:0.85rem;margin-top:12px;display:none}"
        "</style></head><body>"
        "<div class='login-card'>"
        "<h1>ENERGIS PDU</h1>"
        "<p>Enter password to continue</p>"
        "<form id='f' onsubmit='return doLogin(event)'>"
        "<input type='password' id='pw' placeholder='Password' autofocus>"
        "<button type='submit'>Unlock</button>"
        "</form>"
        "<div class='error' id='err'>Invalid password</div>"
        "</div>"
        "<script>"
        "function doLogin(e){"
        "e.preventDefault();"
        "var pw=document.getElementById('pw').value;"
        "var x=new XMLHttpRequest();"
        "x.open('POST','/api/auth');"
        "x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"
        "x.onload=function(){"
        "if(x.status===200){location.reload()}"
        "else{document.getElementById('err').style.display='block';"
        "document.getElementById('pw').value=''}};"
        "x.send('password='+encodeURIComponent(pw))}"
        "</script>"
        "</body></html>";

    send(sock, (uint8_t *)login_html, sizeof(login_html) - 1);
    net_beat();
}
