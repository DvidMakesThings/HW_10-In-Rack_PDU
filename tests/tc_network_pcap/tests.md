1. **Web-UI control path sanity (live capture → analyze)**
   Capture during a few on/off cycles from `/control` and assert packet counts & timing: filter `http.request.method == "POST" && tcp.port == 80`, expect at least N POSTs, and bound inter-packet gaps to catch stalls. Use `CapturePcap(..., interface="ethX", bpf="tcp port 80", duration_s=10)` then `analyze_PCAP(..., display_filter='http.request.method == "POST"', expect_count_min=3, time_delta_ns={"max": 200_000_000})`. This leans on your capture wrapper (dumpcap/tcpdump) and analyzer’s `expect_count/time_delta` checks.

2. **Status polling cadence**
   While the browser polls `/api/status`, verify the GET rate/jitter with `display_filter='http.request.method == "GET" && tcp.port == 80'` and a `time_delta_ns` window to ensure your HTTP server loop isn’t stuttering under load.&#x20;

3. **HTTP POST body presence guard**
   Hit the device with empty-body POSTs; confirm server replies but no crash/regress (you recently hardened writable body handling). Capture and assert at least one 204 response (`display_filter='http.response.code == 204'`) during the run.&#x20;

4. **ARP/DHCP baseline**
   Cold-boot the PDU, capture for 30 s on the subnet, and assert: ≥1 ARP probe/announce, expected DHCP handshake (if DHCP mode), and no excessive retries. Filters: `arp || (bootp)` with `expect_count_min` per phase.&#x20;

5. **VLAN correctness (generated PCAP → analyze)**
   Generate frames with single and stacked VLAN tags (e.g., 802.1Q and Q-in-Q) via `pcap_create(..., payload_len=46)` and validate `vlan_expect={"stack":[(100,None)]}` and then `[(100,None),(200,None)]`. Confirms analyzer’s VLAN stack parsing.

6. **IPv4 fragmentation handling (generator self-check)**
   Create a PCAP of auto-fragmented IPv4 packets (`ip_auto_fragment_payload_size=400`) and verify exact fragment count, MF flags via display filter (`ip.flags.mf == 1` for all but last), and ordered timestamps (strictly increasing deltas).&#x20;

7. **IFG/serialization timing math**
   Use `pcap_create` with `ifg_bytes` at a defined `link_speed_bps` (“1G”) to emit 100 frames; then assert `time_delta_ns` matches your expected IFG ± tolerance. Good to catch regressions in the link-speed parser.&#x20;

8. **FCS corruption demo (offline)**
   Generate two otherwise-identical frames where one has `fcs_xormask=0xFFFFFFFF`. Analyze both to confirm lengths equal and payloads match (via `contains_hex`), demonstrating how corrupted FCS looks in tools even if NIC would drop it live.&#x20;

9. **MAC & broadcast discipline**
   During “ALL ON/OFF” in the UI, capture and assert that PDU only talks unicast to the browser (no noisy broadcasts except ARP). Filters like `eth.dst == ff:ff:ff:ff:ff:ff` with `expect_count==0` over the control window.&#x20;

10. **Throughput under UI load**
    Start a background fetch loop on `/api/status` while toggling relays; capture and ensure TCP retransmits stay below a threshold: `display_filter='tcp.analysis.retransmission'`, `expect_count==0` (or small). Exercises your HTTP server’s robustness.

11. **Payload pattern checks for correctness**
    For `/settings` posts, validate ASCII form content landed on wire (e.g., `contains_ascii="ip=192.168.0.50"`) to ensure your form helpers/URL decode paths are behaving.

12. **Round-trip latency smoke**
    Trigger a single GET `/api/status`, capture SYN→FIN, and compute min/avg `time_delta_ns` between request and first response packet by filtering to that 5-tuple; use this as a quick firmware latency KPI over builds.&#x20;
