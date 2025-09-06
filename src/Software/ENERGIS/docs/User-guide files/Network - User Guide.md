# The Network 

The ENERGIS PDU includes a built-in Ethernet interface powered by the WIZnet W5500 controller.  
This allows the device to be monitored, controlled, and configured over a standard TCP/IP network.  
This guide explains how to connect the PDU, how to interact with it, and what the network parameters mean for everyday use.

---

## Hardware Interface

- **Ethernet Standard:** IEEE 802.3 10/100Base-T  
- **Speed:** 10 Mbps or 100 Mbps, auto-negotiated 
- **Connector:** RJ45 with integrated magnetics  
- **Status LEDs:**  
  - **Ethernet Link LED** lights when a valid network connection is established  
  - **Ethernet Activity LED** blinks during data transmission  
- **MAC Address:** Unique, factory-assigned hardware address used for identification on the LAN  
- **Isolation:** Built-in magnetics provide galvanic isolation from the mains side  

This ensures compatibility with any modern Ethernet switch, router, or firewall.

---

## Connecting the Device

1. Plug a standard Cat5e or Cat6 Ethernet cable into the PDU’s RJ45 port.  
2. Connect the other end to your network switch or router.  
3. Check the **Ethernet LED** on the PDU:  
   - Solid ON = link established  
   - Blinking = traffic activity  
4. Ensure your PC or controller is on the same subnet as the PDU.  

---

## Ways to Interact

### Web Interface
- Open the PDU’s IP address in a browser.  
- Features:  
  - Switch outlets ON/OFF individually or in groups  
  - View live status (relay states, LEDs, error indicators)  
  - Monitor voltage, current, and power usage  
  - View temperature and uptime  
  - Configure network and user preferences  
- Optimized for desktop browsers; responsive interface for mobile devices.

### SNMP (Simple Network Management Protocol)
- Fully integrated SNMP v1 agent for remote monitoring.  
- Default community: `"public"`.  
- Supports GET/SET operations:  
  - Query system information (serial number, firmware version)  
  - Monitor network parameters (IP, gateway, subnet)  
  - Read relay states, power values, and die temperature  
  - Control outlets by writing OIDs  
- Integrates with NMS tools such as Zabbix, Nagios, or PRTG.

### UART (Serial Console)
- Direct wired access for setup and recovery.  
- Available commands:  
  - `SET_IP`, `SET_GW`, `SET_SN`, `SET_DNS` for network settings  
  - `DUMP_EEPROM` to inspect stored configuration  
  - `RFS` to reset to factory defaults  
- Serial interface is recommended when first installing or if the PDU becomes unreachable due to misconfiguration.

---

## Network Parameters – What They Mean

The PDU requires basic network parameters to operate correctly.  
These are stored in EEPROM and loaded automatically at startup.

- **IP Address (Critical):** Identifies the PDU on the network. Must be unique and correct.  
- **Subnet Mask (LAN vs Routed):** Required when using routers. On a flat LAN, less critical but still recommended.  
- **Gateway:** Needed only if the PDU communicates beyond the local subnet. Not required for isolated LANs.  
- **DNS Server:** Usually not used since the PDU operates with IP addresses.  
- **MAC Address:** Fixed, unique per device, may be required by IT for access.  
- **DHCP:** Supported, but static IP is strongly recommended for servers and automation.

---

## Typical Workflow

1. On first boot, the PDU writes factory defaults into EEPROM:  
   - Static IP (documented in manual)  
   - DHCP disabled  
   - Default gateway and subnet mask set  
2. Connect your PC to the same subnet.  
3. Open the Web UI in a browser using the default IP.  
4. Configure IP/subnet/gateway as needed.  
5. Save and reboot → settings persist in EEPROM.  
6. Access via Web, SNMP, or UART as required.

---

## Everyday Use

- Use **Web UI** for direct management (switch outlets, check measurements).  
- Use **SNMP** for integration with monitoring tools.  
- Use **UART** when network configuration needs recovery.  
- Configuration is non-volatile: after a power cycle, the PDU restores its last known settings.  
- If the IP is lost or invalid, reset via UART to factory defaults.

---

## Best Practices

- Always use a **static IP** in production to prevent address changes.  
- Place the PDU in the **same subnet** as your management PC or monitoring server unless routing is correctly set up.  
- Document the assigned IP and SNMP community string for IT reference.  
- In datacenters, label devices with their network identifiers for easy maintenance.  
- For reliability, connect to a **managed switch** supporting link monitoring.  

---

## Troubleshooting

- **Ethernet LED off:** No link detected. Check cable and switch port.  
- **Web UI not accessible:** Ensure IP is correct; try pinging the device.  
- **SNMP not responding:** Verify community string, port 161 must be open.  
- **Device unreachable after config change:** Connect via UART and reset settings.  
- **Multiple PDUs:** Avoid IP conflicts by assigning unique static addresses.  

---
