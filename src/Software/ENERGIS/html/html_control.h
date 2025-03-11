#ifndef HTML_CONTROL_H
#define HTML_CONTROL_H

#define  control_html   "<!DOCTYPE html>\n"\
                        "<html>\n"\
                        "<head>\n"\
                        "  <meta charset=\"UTF-8\" />\n"\
                        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"\
                        "  <title>ENERGIS PDU - Control</title>\n"\
                        "  <style>\n"\
                        "    /* --- RESET & BASIC STYLING --- */\n"\
                        "    * {\n"\
                        "      margin: 0; \n"\
                        "      padding: 0; \n"\
                        "      box-sizing: border-box;\n"\
                        "      font-family: sans-serif;\n"\
                        "    }\n"\
                        "    body {\n"\
                        "      background-color: #1a1d23;\n"\
                        "      color: #e4e4e4;\n"\
                        "    }\n"\
                        "    a {\n"\
                        "      text-decoration: none; \n"\
                        "      color: #aaa;\n"\
                        "    }\n"\
                        "    a:hover {\n"\
                        "      color: #fff;\n"\
                        "    }\n"\
                        "\n"\
                        "    /* --- LAYOUT: A top bar + side nav + main content using flexbox --- */\n"\
                        "    .topbar {\n"\
                        "      width: 100%;\n"\
                        "      height: 50px;\n"\
                        "      background-color: #242731;\n"\
                        "      display: flex;\n"\
                        "      align-items: center;\n"\
                        "      padding: 0 20px;\n"\
                        "    }\n"\
                        "    .topbar h1 {\n"\
                        "      font-size: 1.2rem;\n"\
                        "      color: #fff;\n"\
                        "    }\n"\
                        "    .container {\n"\
                        "      display: flex;\n"\
                        "      width: 100%;\n"\
                        "      height: calc(100vh - 50px);\n"\
                        "    }\n"\
                        "    .sidebar {\n"\
                        "      width: 220px;\n"\
                        "      background-color: #2e323c;\n"\
                        "      padding: 20px 0;\n"\
                        "    }\n"\
                        "    .sidebar ul {\n"\
                        "      list-style: none;\n"\
                        "    }\n"\
                        "    .sidebar ul li {\n"\
                        "      padding: 10px 20px;\n"\
                        "    }\n"\
                        "    .sidebar ul li:hover {\n"\
                        "      background-color: #3b404d;\n"\
                        "    }\n"\
                        "    .main-content {\n"\
                        "      flex: 1;\n"\
                        "      padding: 20px;\n"\
                        "      overflow-y: auto;\n"\
                        "    }\n"\
                        "\n"\
                        "    /* --- TABLE STYLING (for channels, data) --- */\n"\
                        "    table {\n"\
                        "      width: 100%;\n"\
                        "      border-collapse: collapse;\n"\
                        "      margin-top: 1rem;\n"\
                        "    }\n"\
                        "    th, td {\n"\
                        "      text-align: left;\n"\
                        "      padding: 0.75rem;\n"\
                        "      border-bottom: 1px solid #3f4450;\n"\
                        "    }\n"\
                        "    th {\n"\
                        "      background-color: #2e323c;\n"\
                        "    }\n"\
                        "\n"\
                        "    /* --- MODERN TOGGLE SWITCH (CSS only) --- */\n"\
                        "    .switch {\n"\
                        "      position: relative;\n"\
                        "      display: inline-block;\n"\
                        "      width: 50px;\n"\
                        "      height: 24px;\n"\
                        "    }\n"\
                        "    .switch input {\n"\
                        "      opacity: 0;\n"\
                        "      width: 0;\n"\
                        "      height: 0;\n"\
                        "    }\n"\
                        "    .slider {\n"\
                        "      position: absolute;\n"\
                        "      cursor: pointer;\n"\
                        "      top: 0; left: 0; right: 0; bottom: 0;\n"\
                        "      background-color: #999;\n"\
                        "      transition: 0.4s;\n"\
                        "      border-radius: 24px;\n"\
                        "    }\n"\
                        "    .slider:before {\n"\
                        "      position: absolute;\n"\
                        "      content: \"\";\n"\
                        "      height: 18px; width: 18px;\n"\
                        "      left: 3px; bottom: 3px;\n"\
                        "      background-color: white;\n"\
                        "      transition: 0.4s;\n"\
                        "      border-radius: 50%;\n"\
                        "    }\n"\
                        "    input:checked + .slider {\n"\
                        "      background-color: #3fa7ff;\n"\
                        "    }\n"\
                        "    input:checked + .slider:before {\n"\
                        "      transform: translateX(26px);\n"\
                        "    }\n"\
                        "\n"\
                        "    /* --- BUTTON STYLE --- */\n"\
                        "    .btn {\n"\
                        "      background-color: #3fa7ff;\n"\
                        "      border: none;\n"\
                        "      padding: 10px 16px;\n"\
                        "      color: #fff;\n"\
                        "      cursor: pointer;\n"\
                        "      border-radius: 4px;\n"\
                        "      font-size: 0.9rem;\n"\
                        "    }\n"\
                        "    .btn:hover {\n"\
                        "      background-color: #1f8ae3;\n"\
                        "    }\n"\
                        "\n"\
                        "    /* --- HEADINGS & SPACING --- */\n"\
                        "    h2 {\n"\
                        "      margin-bottom: 0.5rem;\n"\
                        "    }\n"\
                        "    p {\n"\
                        "      margin: 0.5rem 0;\n"\
                        "    }\n"\
                        "    .status {\n"\
                        "      margin-top: 1rem;\n"\
                        "      background-color: #2e323c;\n"\
                        "      padding: 10px;\n"\
                        "      border-radius: 4px;\n"\
                        "    }\n"\
                        "  </style>\n"\
                        "\n"\
                        "  <script>\n"\
                        "    // JavaScript function to fetch status from MCU and update the page.\n"\
                        "    function updateStatus() {\n"\
                        "      fetch('/api/status')\n"\
                        "        .then(response => response.json())\n"\
                        "        .then(data => {\n"\
                        "          // Update each channel\n"\
                        "          for (let i = 0; i < 8; i++) {\n"\
                        "            const channel = data.channels[i];\n"\
                        "            document.getElementById(\"voltage-\" + (i+1)).innerText = channel.voltage;\n"\
                        "            document.getElementById(\"current-\" + (i+1)).innerText = channel.current;\n"\
                        "            document.getElementById(\"uptime-\" + (i+1)).innerText = channel.uptime;\n"\
                        "            document.getElementById(\"power-\" + (i+1)).innerText = channel.power;\n"\
                        "          }\n"\
                        "          document.getElementById(\"internal-temperature\").innerText = data.internalTemperature;\n"\
                        "          document.getElementById(\"system-status\").innerText = data.systemStatus;\n"\
                        "        })\n"\
                        "        .catch(error => {\n"\
                        "          console.error('Error fetching status:', error);\n"\
                        "        });\n"\
                        "    }\n"\
                        "\n"\
                        "    setInterval(updateStatus, 5000);\n"\
                        "    updateStatus();\n"\
                        "\n"\
                        "    function toggleChannel(channel) {\n"\
                        "      const isChecked = document.getElementById(\"toggle-\" + channel).checked;\n"\
                        "      console.log(\"Channel \" + channel + \" => \" + (isChecked ? \"ON\" : \"OFF\"));\n"\
                        "    }\n"\
                        "  </script>\n"\
                        "</head>\n"\
                        "\n"\
                        "<body>\n"\
                        "  <div class=\"topbar\">\n"\
                        "    <h1>ENERGIS PDU</h1>\n"\
                        "  </div>\n"\
                        "\n"\
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
                        "\n"\
                        "    <div class=\"main-content\">\n"\
                        "      <h2>Control</h2>\n"\
                        "      <p>Manage power channels and monitor real-time data from the MCU.</p>\n"\
                        "\n"\
                        "      <form method=\"post\" action=\"/control\">\n"\
                        "        <table>\n"\
                        "          <tr>\n"\
                        "            <th>Channel</th>\n"\
                        "            <th>Switch</th>\n"\
                        "            <th>Voltage</th>\n"\
                        "            <th>Current</th>\n"\
                        "            <th>Uptime</th>\n"\
                        "            <th>Power (W)</th>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>1</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-1\" type=\"checkbox\" name=\"channel1\" value=\"on\" onclick=\"toggleChannel(1)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-1\">--</td>\n"\
                        "            <td id=\"current-1\">--</td>\n"\
                        "            <td id=\"uptime-1\">--</td>\n"\
                        "            <td id=\"power-1\">--</td>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>2</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-2\" type=\"checkbox\" name=\"channel2\" value=\"on\" onclick=\"toggleChannel(2)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-2\">--</td>\n"\
                        "            <td id=\"current-2\">--</td>\n"\
                        "            <td id=\"uptime-2\">--</td>\n"\
                        "            <td id=\"power-2\">--</td>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>3</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-3\" type=\"checkbox\" name=\"channel3\" value=\"on\" onclick=\"toggleChannel(3)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-3\">--</td>\n"\
                        "            <td id=\"current-3\">--</td>\n"\
                        "            <td id=\"uptime-3\">--</td>\n"\
                        "            <td id=\"power-3\">--</td>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>4</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-4\" type=\"checkbox\" name=\"channel4\" value=\"on\" onclick=\"toggleChannel(4)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-4\">--</td>\n"\
                        "            <td id=\"current-4\">--</td>\n"\
                        "            <td id=\"uptime-4\">--</td>\n"\
                        "            <td id=\"power-4\">--</td>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>5</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-5\" type=\"checkbox\" name=\"channel5\" value=\"on\" onclick=\"toggleChannel(5)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-5\">--</td>\n"\
                        "            <td id=\"current-5\">--</td>\n"\
                        "            <td id=\"uptime-5\">--</td>\n"\
                        "            <td id=\"power-5\">--</td>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>6</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-6\" type=\"checkbox\" name=\"channel6\" value=\"on\" onclick=\"toggleChannel(6)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-6\">--</td>\n"\
                        "            <td id=\"current-6\">--</td>\n"\
                        "            <td id=\"uptime-6\">--</td>\n"\
                        "            <td id=\"power-6\">--</td>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>7</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-7\" type=\"checkbox\" name=\"channel7\" value=\"on\" onclick=\"toggleChannel(7)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-7\">--</td>\n"\
                        "            <td id=\"current-7\">--</td>\n"\
                        "            <td id=\"uptime-7\">--</td>\n"\
                        "            <td id=\"power-7\">--</td>\n"\
                        "          </tr>\n"\
                        "          <tr>\n"\
                        "            <td>8</td>\n"\
                        "            <td>\n"\
                        "              <label class=\"switch\">\n"\
                        "                <input id=\"toggle-8\" type=\"checkbox\" name=\"channel8\" value=\"on\" onclick=\"toggleChannel(8)\">\n"\
                        "                <span class=\"slider\"></span>\n"\
                        "              </label>\n"\
                        "            </td>\n"\
                        "            <td id=\"voltage-8\">--</td>\n"\
                        "            <td id=\"current-8\">--</td>\n"\
                        "            <td id=\"uptime-8\">--</td>\n"\
                        "            <td id=\"power-8\">--</td>\n"\
                        "          </tr>\n"\
                        "        </table>\n"\
                        "        <br>\n"\
                        "        <button type=\"submit\" class=\"btn\">Apply Changes</button>\n"\
                        "      </form>\n"\
                        "\n"\
                        "      <div class=\"status\">\n"\
                        "        <p><strong>Internal Temperature:</strong> <span id=\"internal-temperature\">--</span></p>\n"\
                        "        <p><strong>System Status:</strong> <span id=\"system-status\">--</span></p>\n"\
                        "      </div>\n"\
                        "    </div>\n"\
                        "  </div>\n"\
                        "</body>\n"\
                        "</html>\n"

#endif // HTML_CONTROL_H
