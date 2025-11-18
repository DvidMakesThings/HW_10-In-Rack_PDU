### W5500 Network Parameter Importance

When configuring the **W5500 Ethernet controller**, several network parameters must be set: **IP address, subnet mask, gateway, and DNS server**.  
However, their impact on functionality depends on the type of network communication.

---

#### 1. IP Address (Critical)
- The **only mandatory parameter** for local communication.  
- Used in ARP replies and IP packet headers.  
- If incorrect, other devices cannot address the W5500 properly, and communication will fail.

---

#### 2. Subnet Mask (Context-Dependent)
- Defines which IP addresses are considered "local" vs. "remote."  
- If set incorrectly, the W5500 may assume a peer is outside its subnet and attempt to send traffic via the gateway.  
- **On a LAN:** Often not critical, because ARP broadcasts will still resolve peers regardless of the mask.  
- **For routed communication:** Must be correct to avoid misrouting packets.

---

#### 3. Gateway (Context-Dependent)
- Used only when the W5500 communicates with IP addresses *outside* its local subnet.  
- If set incorrectly:
  - **LAN communication:** Works normally (gateway not involved).  
  - **External communication (e.g., internet):** Fails, because packets are sent to the wrong router or not routed at all.

---

#### 4. DNS Server (Optional)
- The W5500 does **not** resolve hostnames internally; applications typically use raw IP addresses.  
- DNS settings only matter if a DNS client is implemented in software. (Which is not)
- Incorrect DNS configuration prevents hostname lookups but does not affect raw IP communication.

---

### Summary
- **IP address must always be correct.**  
- **Subnet mask and gateway are only critical for routed (non-LAN) communication.**  
- **DNS is only needed if using hostnames.**

For LAN-only operation, the **IP address is the only parameter that must be correct**.  
Gateway, subnet mask, and DNS can be misconfigured without affecting direct communication between devices on the same network segment.
