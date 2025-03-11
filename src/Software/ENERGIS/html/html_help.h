#ifndef HTML_HELP_H
#define HTML_HELP_H

#define help_html           "<!DOCTYPE html>\n"\
                            "<html>\n"\
                            "<head>\n"\
                            "  <meta charset=\"UTF-8\" />\n"\
                            "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"\
                            "  <title>ENERGIS PDU - Help</title>\n"\
                            "  <style>\n"\
                            "    * { margin: 0; padding: 0; box-sizing: border-box; font-family: sans-serif; }\n"\
                            "    body { background-color: #1a1d23; color: #e4e4e4; }\n"\
                            "    a { text-decoration: none; color: #aaa; } a:hover { color: #fff; }\n"\
                            "    .topbar { width: 100%; height: 50px; background-color: #242731; display: flex; align-items: center; padding: 0 20px; }\n"\
                            "    .topbar h1 { font-size: 1.2rem; color: #fff; }\n"\
                            "    .container { display: flex; width: 100%; height: calc(100vh - 50px); }\n"\
                            "    .sidebar { width: 220px; background-color: #2e323c; padding: 20px 0; }\n"\
                            "    .sidebar ul { list-style: none; }\n"\
                            "    .sidebar ul li { padding: 10px 20px; }\n"\
                            "    .sidebar ul li:hover { background-color: #3b404d; }\n"\
                            "    .main-content { flex: 1; padding: 20px; overflow-y: auto; }\n"\
                            "    h2 { margin-bottom: 0.5rem; }\n"\
                            "    p { margin: 0.5rem 0; }\n"\
                            "    ul { margin-left: 20px; }\n"\
                            "  </style>\n"\
                            "</head>\n"\
                            "<body>\n"\
                            "  <div class=\"topbar\"><h1>ENERGIS PDU</h1></div>\n"\
                            "  <div class=\"container\">\n"\
                            "    <div class=\"sidebar\">\n"\
                            "      <ul>\n"\
                            "        <li><a href=\"control.html\">Control</a></li>\n"\
                            "        <li><a href=\"settings.html\">Settings</a></li>\n"\
                            "        <li><a href=\"help.html\">Help</a></li>\n"\
                            "        <li><a href=\"user_manual.html\">User Manual</a></li>\n"\
                            "        <li><a href=\"programming_manual.html\">Programming Manual</a></li>\n"\
                            "      </ul>\n"\
                            "    </div>\n"\
                            "    <div class=\"main-content\">\n"\
                            "      <h2>Help</h2>\n"\
                            "      <p>Welcome to the ENERGIS PDU Help page.</p>\n"\
                            "      <p>For detailed instructions, see:</p>\n"\
                            "      <ul>\n"\
                            "        <li><a href=\"user_manual.html\">User Manual</a></li>\n"\
                            "        <li><a href=\"programming_manual.html\">Programming &amp; Interfacing Manual</a></li>\n"\
                            "      </ul>\n"\
                            "      <p>Use the Control page to toggle channels and monitor power. Use the Settings page to configure network, device, and time.</p>\n"\
                            "    </div>\n"\
                            "  </div>\n"\
                            "</body>\n"\
                            "</html>\n"

#endif // HTML_HELP_H
