/**
 * @file src/web_handlers/page_content.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Stores HTML pages as const char arrays in flash memory.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/**
 * @brief HTML content for the Control page.
 *
 * This string contains the HTML markup for the Control page,
 * allowing users to toggle power channels and monitor data.
 */
const char control_html[] =
    "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\" /><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1.0\"><title>ENERGIS PDU – Control</title><style>* "
    "{ margin: 0; padding: 0; box-sizing: border-box; font-family: sans-serif } body { background: "
    "#1a1d23; color: #e4e4e4 } a { text-decoration: none; color: #aaa } a:hover { color: #fff } "
    ".topbar { height: 50px; background: #242731; display: flex; align-items: center; padding: 0 "
    "20px } .topbar h1 { font-size: 1.2rem; color: #fff } .container { display: flex; height: "
    "calc(100vh - 50px) } .sidebar { width: 220px; background: #2e323c; padding: 20px 0 } .sidebar "
    "ul { list-style: none } .sidebar li { padding: 10px 20px } .sidebar li:hover { background: "
    "#3b404d } .main-content { flex: 1; padding: 20px; overflow-y: auto } table { width: 100%; "
    "border-collapse: collapse; margin-top: 1rem } th, td { text-align: left; padding: .75rem; "
    "border-bottom: 1px solid #3f4450 } th { background: #2e323c } .switch { position: relative; "
    "display: inline-block; width: 50px; height: 24px } .switch input { opacity: 0; width: 0; "
    "height: 0 } .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: "
    "0; background: #999; transition: .4s; border-radius: 24px } .slider:before { position: "
    "absolute; content: \"\"; height: 18px; width: 18px; left: 3px; bottom: 3px; background: #fff; "
    "transition: .4s; border-radius: 50% } input:checked+.slider { background: #3fa7ff } "
    "input:checked+.slider:before { transform: translateX(26px) } .btn { background: #3fa7ff; "
    "border: none; padding: 10px 16px; color: #fff; cursor: pointer; border-radius: 4px; "
    "font-size: .9rem; margin-right: 8px } .btn:hover { background: #1f8ae3 } .btn-green { "
    "background: #28a745 } .btn-green:hover { background: #218838 } .btn-red { background: #dc3545 "
    "} .btn-red:hover { background: #c82333 } .status { margin-top: 1rem; background: #2e323c; "
    "padding: 10px; border-radius: 4px }</style><script>let pendingChanges = new Set(); function "
    "updateStatus() { fetch('/api/status') .then(r =>r.json()) .then(data =>{ "
    "data.channels.forEach((ch, i) =>{ document.getElementById(`voltage-${i + 1}`).innerText = "
    "ch.voltage.toFixed(2); document.getElementById(`current-${i + 1}`).innerText = "
    "ch.current.toFixed(2); document.getElementById(`uptime-${i + 1}`).innerText = ch.uptime; "
    "document.getElementById(`power-${i + 1}`).innerText = ch.power.toFixed(2); if "
    "(!pendingChanges.has(i + 1)) document.getElementById(`toggle-${i + 1}`).checked = ch.state; "
    "}); document.getElementById('internal-temperature').innerText = "
    "data.internalTemperature.toFixed(1); document.getElementById('temp-unit').innerText = "
    "data.temperatureUnit || '°C'; document.getElementById('system-status').innerText = "
    "data.systemStatus; }) .catch(console.error); } function toggleChannel(c) { "
    "pendingChanges.add(c); } function applyChanges(e) { e.preventDefault(); let fd = new "
    "FormData(e.target), body = new URLSearchParams(); for (let [k] of fd) if "
    "(k.startsWith('channel')) { let i = +k.replace('channel', ''); if "
    "(document.getElementById(`toggle-${i}`).checked) body.append(k, 'on'); } "
    "fetch('/api/control', { "
    "method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: "
    "body.toString() }) .then(r =>{ if (!r.ok) throw ''; pendingChanges.clear(); updateStatus(); "
    "}) .catch(_ =>updateStatus()); } function setAll(state) { for (let i = 1; i<= 8; i++) { "
    "document.getElementById(`toggle-${i}`).checked = state; toggleChannel(i); } } "
    "window.addEventListener('load', () =>{ updateStatus(); setInterval(updateStatus, 3000); "
    "});</script></head><body><div class=\"topbar\"><h1>ENERGIS PDU</h1></div><div "
    "class=\"container\"><nav class=\"sidebar\"><ul><li><a "
    "href=\"control.html\">Control</a></li><li><a href=\"settings.html\">Settings</a></li><li><a "
    "href=\"help.html\">Help</a></li><li><a href=\"user_manual.html\">User Manual</a></li><li><a "
    "href=\"programming_manual.html\">Automation Manual</a></li></ul></nav><main "
    "class=\"main-content\"><h2>Control</h2><p>Manage power channels and monitor "
    "data.</p><form method=\"post\" action=\"/api/control\" "
    "onsubmit=\"applyChanges(event)\"><table><tr><th>Channel</th><th>Switch</th><th>Voltage "
    "(V)</th><th>Current (A)</th><th>Uptime (s)</th><th>Power "
    "(W)</th></tr><tr><td>1</td><td><label class=\"switch\"><input id=\"toggle-1\" "
    "type=\"checkbox\" name=\"channel1\" onclick=\"toggleChannel(1)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-1\">--</td><td "
    "id=\"current-1\">--</td><td id=\"uptime-1\">--</td><td "
    "id=\"power-1\">--</td></tr><tr><td>2</td><td><label class=\"switch\"><input id=\"toggle-2\" "
    "type=\"checkbox\" name=\"channel2\" onclick=\"toggleChannel(2)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-2\">--</td><td "
    "id=\"current-2\">--</td><td id=\"uptime-2\">--</td><td "
    "id=\"power-2\">--</td></tr><tr><td>3</td><td><label class=\"switch\"><input id=\"toggle-3\" "
    "type=\"checkbox\" name=\"channel3\" onclick=\"toggleChannel(3)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-3\">--</td><td "
    "id=\"current-3\">--</td><td id=\"uptime-3\">--</td><td "
    "id=\"power-3\">--</td></tr><tr><td>4</td><td><label class=\"switch\"><input id=\"toggle-4\" "
    "type=\"checkbox\" name=\"channel4\" onclick=\"toggleChannel(4)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-4\">--</td><td "
    "id=\"current-4\">--</td><td id=\"uptime-4\">--</td><td "
    "id=\"power-4\">--</td></tr><tr><td>5</td><td><label class=\"switch\"><input id=\"toggle-5\" "
    "type=\"checkbox\" name=\"channel5\" onclick=\"toggleChannel(5)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-5\">--</td><td "
    "id=\"current-5\">--</td><td id=\"uptime-5\">--</td><td "
    "id=\"power-5\">--</td></tr><tr><td>6</td><td><label class=\"switch\"><input id=\"toggle-6\" "
    "type=\"checkbox\" name=\"channel6\" onclick=\"toggleChannel(6)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-6\">--</td><td "
    "id=\"current-6\">--</td><td id=\"uptime-6\">--</td><td "
    "id=\"power-6\">--</td></tr><tr><td>7</td><td><label class=\"switch\"><input id=\"toggle-7\" "
    "type=\"checkbox\" name=\"channel7\" onclick=\"toggleChannel(7)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-7\">--</td><td "
    "id=\"current-7\">--</td><td id=\"uptime-7\">--</td><td "
    "id=\"power-7\">--</td></tr><tr><td>8</td><td><label class=\"switch\"><input id=\"toggle-8\" "
    "type=\"checkbox\" name=\"channel8\" onclick=\"toggleChannel(8)\"><span "
    "class=\"slider\"></span></label></td><td id=\"voltage-8\">--</td><td "
    "id=\"current-8\">--</td><td id=\"uptime-8\">--</td><td "
    "id=\"power-8\">--</td></tr></table><br><button type=\"submit\" class=\"btn btn-green\">Apply "
    "Changes</button><button type=\"button\" class=\"btn\" style=\"margin-left:60px\" "
    "onclick=\"setAll(true)\">All On</button><button type=\"button\" class=\"btn btn-red\" "
    "onclick=\"setAll(false)\">All Off</button></form><div class=\"status\"><p><strong>Internal "
    "Temperature:</strong><span id=\"internal-temperature\">--</span><span "
    "id=\"temp-unit\">°C</span></p><p><strong>System Status:</strong><span "
    "id=\"system-status\">--</span></p></div></main></div></body></html>\n";

/**
 * @brief HTML content for the Settings page.
 *
 * This string contains the HTML markup for the Settings page,
 * allowing users to configure network, device, and temperature unit settings.
 */
const char settings_html[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1.0\"><title>ENERGIS PDU - Settings</title>"
    "<style>*{margin:0;padding:0;box-sizing:border-box;font-family:sans-serif}"
    "body{background:#1a1d23;color:#e4e4e4}a{color:#aaa;text-decoration:none}"
    "a:hover{color:#fff}.topbar{width:100%;height:50px;background:#242731;display:flex;"
    "align-items:center;padding:0 20px}.topbar h1{font-size:1.2rem;color:#fff}"
    ".container{display:flex;width:100%;height:calc(100vh - 50px)}"
    ".sidebar{width:220px;background:#2e323c;padding:20px 0}.sidebar ul{list-style:none}"
    ".sidebar ul li{padding:10px 20px}.sidebar ul li:hover{background:#3b404d}"
    ".main-content{flex:1;padding:20px;overflow-y:auto}h2{margin-bottom:.5rem}"
    "hr{border:none;border-top:1px solid #333;margin:1rem 0}"
    ".form-group{display:flex;align-items:center;margin-bottom:.75rem}"
    ".form-group label{width:160px;margin-right:10px}"
    ".form-group input[type=text],.form-group input[type=radio]{padding:8px;"
    "border:none;border-radius:4px}.form-group input[type=text]{flex:1;"
    "max-width:200px;margin-right:10px}.btn{background:#3fa7ff;border:none;"
    "padding:10px 16px;color:#fff;cursor:pointer;border-radius:4px;font-size:.9rem}"
    ".btn:hover{background:#1f8ae3}.manuals{margin-top:2rem}</style></head>"
    "<body><div class=\"topbar\"><h1>ENERGIS PDU</h1></div><div class=\"container\">"
    "<nav class=\"sidebar\"><ul>"
    "<li><a href=\"control.html\">Control</a></li>"
    "<li><a href=\"settings.html\">Settings</a></li>"
    "<li><a href=\"help.html\">Help</a></li>"
    "<li><a href=\"user_manual.html\">User Manual</a></li>"
    "<li><a href=\"programming_manual.html\">Automation Manual</a></li>"
    "</ul></nav><main class=\"main-content\">"
    "<h2>Settings</h2><form method=\"post\" action=\"/api/settings\">"
    "<h3>Network Settings</h3><hr>"
    "<div class=\"form-group\"><label for=\"ip\">IP Address:</label>"
    "<input type=\"text\" id=\"ip\" name=\"ip\" value=\"%s\">"
    "</div>"
    "<div class=\"form-group\"><label for=\"gateway\">Default Gateway:</label>"
    "<input type=\"text\" id=\"gateway\" name=\"gateway\" value=\"%s\">"
    "</div>"
    "<div class=\"form-group\"><label for=\"subnet\">Subnet Mask:</label>"
    "<input type=\"text\" id=\"subnet\" name=\"subnet\" value=\"%s\">"
    "</div>"
    "<div class=\"form-group\"><label for=\"dns\">DNS Server:</label>"
    "<input type=\"text\" id=\"dns\" name=\"dns\" value=\"%s\">"
    "</div>"
    "<h3>Device Settings</h3><hr>"
    "<div class=\"form-group\"><label for=\"device_name\">Device Name:</label>"
    "<input type=\"text\" id=\"device_name\" name=\"device_name\" value=\"%s\">"
    "</div>"
    "<div class=\"form-group\"><label for=\"location\">Device Location:</label>"
    "<input type=\"text\" id=\"location\" name=\"location\" value=\"%s\">"
    "</div>"
    "<h3>Temperature Unit</h3><hr>"
    "<div class=\"form-group\">"
    "<input type=\"radio\" id=\"celsius\" name=\"temp_unit\" value=\"celsius\" %s>"
    "<label for=\"celsius\">Celsius</label>"
    "</div>"
    "<div class=\"form-group\">"
    "<input type=\"radio\" id=\"fahrenheit\" name=\"temp_unit\" value=\"fahrenheit\" %s>"
    "<label for=\"fahrenheit\">Fahrenheit</label>"
    "</div>"
    "<div class=\"form-group\">"
    "<input type=\"radio\" id=\"kelvin\" name=\"temp_unit\" value=\"kelvin\" %s>"
    "<label for=\"kelvin\">Kelvin</label>"
    "</div><br>"
    "<button type=\"submit\" class=\"btn\">Save Settings</button>"
    "</form>"
    "<div class=\"manuals\">"
    "<h3>Manuals</h3><hr>"
    "<p><a href=\"user_manual.html\">User Manual</a><br>"
    "<a href=\"programming_manual.html\">Programming &amp; Interfacing Manual</a></p>"
    "</div>"
    "</main></div></body></html>\n";

/**
 * @brief HTML content for the Help page.
 *
 * This string contains the HTML markup for the Help page,
 * providing guidance and links to manuals for the ENERGIS PDU.
 */
const char help_html[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\" /><meta name=\"viewport\" "
    "content=\"width=device-width, initial-scale=1.0\"><title>ENERGIS PDU - Help</title><style>* { "
    "margin: 0; padding: 0; box-sizing: border-box; font-family: sans-serif; } body { "
    "background-color: #1a1d23; color: #e4e4e4; } a { text-decoration: none; color: #aaa; } "
    "a:hover { color: #fff; } .topbar { width: 100%; height: 50px; background-color: #242731; "
    "display: flex; align-items: center; padding: 0 20px; } .topbar h1 { font-size: 1.2rem; color: "
    "#fff; } .container { display: flex; width: 100%; height: calc(100vh - 50px); } .sidebar { "
    "width: 220px; background-color: #2e323c; padding: 20px 0; } .sidebar ul { list-style: none; } "
    ".sidebar ul li { padding: 10px 20px; } .sidebar ul li:hover { background-color: #3b404d; } "
    ".main-content { flex: 1; padding: 20px; overflow-y: auto; } h2 { margin-bottom: 0.5rem; } p { "
    "margin: 0.5rem 0; } ul { margin-left: 20px; }</style></head><body><div "
    "class=\"topbar\"><h1>ENERGIS PDU</h1></div><div class=\"container\"><div "
    "class=\"sidebar\"><ul><li><a href=\"control.html\">Control</a></li><li><a "
    "href=\"settings.html\">Settings</a></li><li><a href=\"help.html\">Help</a></li><li><a "
    "href=\"user_manual.html\">User Manual</a></li><li><a "
    "href=\"programming_manual.html\">Automation Manual</a></li></ul></div><div "
    "class=\"main-content\"><h2>Help</h2><p>Welcome to the ENERGIS PDU Help page.</p><p>For "
    "detailed instructions, see:</p><ul><li><a href=\"user_manual.html\">User "
    "Manual</a></li><li><a href=\"programming_manual.html\">Programming &amp; Interfacing "
    "Manual</a></li></ul><p>Use the Control page to toggle channels and monitor power. Use the "
    "Settings page to configure network, device, and time.</p></div></div></body></html>\n";

/**
 * @brief HTML content for the User Manual page.
 *
 * This string contains the HTML markup for the User Manual page,
 * embedding the user manual PDF and providing a download link.
 */
const char user_manual_html[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\" /><meta name=\"viewport\" "
    "content=\"width=device-width, initial-scale=1.0\"><title>ENERGIS PDU – User "
    "Manual</title><style>* { margin: 0; padding: 0; box-sizing: border-box; font-family: "
    "sans-serif; } body { background: #1a1d23; color: #e4e4e4; } a { color: #aaa; text-decoration: "
    "none; } a:hover { color: #fff; } .topbar { height: 50px; background: #242731; display: flex; "
    "align-items: center; padding: 0 20px; } .topbar h1 { font-size: 1.2rem; color: #fff; } "
    ".container { display: flex; height: calc(100vh - 50px); } .sidebar { width: 220px; "
    "background: #2e323c; padding: 20px 0; } .sidebar ul { list-style: none; } .sidebar li { "
    "padding: 10px 20px; } .sidebar li:hover { background: #3b404d; } .sidebar a { color: #ccc; } "
    ".sidebar a:hover { color: #fff; } .content { flex: 1; display: flex; flex-direction: column; "
    "} .pdf-container { flex: 1; border: 1px solid #444; } .note { padding: 0.5rem; text-align: "
    "right; font-size: 0.9rem; }</style></head><body><div class=\"topbar\"><h1>ENERGIS "
    "PDU</h1></div><div class=\"container\"><div class=\"sidebar\"><ul><li><a "
    "href=\"control.html\">Control</a></li><li><a href=\"settings.html\">Settings</a></li><li><a "
    "href=\"help.html\">Help</a></li><li><a href=\"user_manual.html\">User Manual</a></li><li><a "
    "href=\"programming_manual.html\">Automation Manual</a></li></ul></div><div "
    "class=\"content\"><div class=\"pdf-container\"><iframe "
    "src=\"https://dvidmakesthings.github.io/HW_10-In-Rack_PDU/Manuals/User_Manual.pdf\" "
    "width=\"100%\" height=\"100%\" frameborder=\"0\"></iframe></div><p class=\"note\">If your "
    "browser does not display the PDF, you can download it directly<a "
    "href=\"https://dvidmakesthings.github.io/HW_10-In-Rack_PDU/Manuals/User_Manual.pdf\" "
    "target=\"_blank\">here</a>.</p></div></div></body></html>\n";

/**
 * @brief HTML content for the Automation Manual page.
 *
 * This string contains the HTML markup for the Automation Manual page,
 * embedding the programming manual PDF and providing a download link.
 */
const char programming_manual_html[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\" /><meta name=\"viewport\" "
    "content=\"width=device-width, initial-scale=1.0\"><title>ENERGIS PDU – Programming "
    "Manual</title><style>* { margin: 0; padding: 0; box-sizing: border-box; font-family: "
    "sans-serif; } body { background: #1a1d23; color: #e4e4e4; } a { color: #aaa; text-decoration: "
    "none; } a:hover { color: #fff; } .topbar { height: 50px; background: #242731; display: flex; "
    "align-items: center; padding: 0 20px; } .topbar h1 { font-size: 1.2rem; color: #fff; } "
    ".container { display: flex; height: calc(100vh - 50px); } .sidebar { width: 220px; "
    "background: #2e323c; padding: 20px 0; } .sidebar ul { list-style: none; } .sidebar li { "
    "padding: 10px 20px; } .sidebar li:hover { background: #3b404d; } .sidebar a { color: #ccc; } "
    ".sidebar a:hover { color: #fff; } /* Right-side area */ .content { flex: 1; display: flex; "
    "flex-direction: column; } .pdf-container { flex: 1; border: 1px solid #444; } .note { "
    "padding: 0.5rem; text-align: right; font-size: 0.9rem; }</style></head><body><div "
    "class=\"topbar\"><h1>ENERGIS PDU</h1></div><div class=\"container\"><div "
    "class=\"sidebar\"><ul><li><a href=\"control.html\">Control</a></li><li><a "
    "href=\"settings.html\">Settings</a></li><li><a href=\"help.html\">Help</a></li><li><a "
    "href=\"user_manual.html\">User Manual</a></li><li><a "
    "href=\"programming_manual.html\">Automation Manual</a></li></ul></div><div "
    "class=\"content\"><div class=\"pdf-container\"><iframe "
    "src=\"https://dvidmakesthings.github.io/HW_10-In-Rack_PDU/Manuals/AUTOMATION_MANUAL.pdf\" "
    "width=\"100%\" height=\"100%\" frameborder=\"0\"></iframe></div><p class=\"note\">If your "
    "browser does not display the PDF, you can download it directly <a "
    "href=\"https://dvidmakesthings.github.io/HW_10-In-Rack_PDU/Manuals/AUTOMATION_MANUAL.pdf\" "
    "target=\"_blank\">here</a>.</p></div></div></body></html>\n";

/**
 * @brief Gets the HTML content for a requested page
 * @param request The HTTP request line (e.g., "GET /api/control.html HTTP/1.1")
 * @return Pointer to the HTML content, or control.html as default
 * @note Routes HTTP requests to appropriate HTML page content
 */
const char *get_page_content(const char *request) {
    if (strstr(request, "GET /settings.html"))
        return settings_html;
    else if (strstr(request, "GET /help.html"))
        return help_html;
    else if (strstr(request, "GET /user_manual.html"))
        return user_manual_html;
    else if (strstr(request, "GET /programming_manual.html"))
        return programming_manual_html;
    else if (strstr(request, "GET /"))
        return control_html;

    /* Default to control page */
    return control_html;
}
