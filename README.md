# ENERGIS - the managed PDU Project for 10-Inch Rack

## Overview
The **10-Inch Rack PDU (Power Distribution Unit)** is a modular power management system designed for efficient control and monitoring of power in a rack-mounted environment. The project includes a controller board, a display board, and a relay board to handle switching and power management.

![PDU 3D View](images/Assembly/Assembled-in-case_3D_1.png)

![PDU 3D View](images/Assembly/Assembled-in-case_3D_2.png)

## Features
- **Controller Board:** Handles Ethernet communication, power conversion, and system logic.
- **Display Board:** Provides user interaction with OLED display and status LEDs.
- **Relay Board:** Manages AC switching with 8x 230V relays, fuses, and safety isolation.
- **Ethernet Connectivity:** Uses the W5500 SPI-based Ethernet chip for remote control and monitoring.
- **Power Measurement:** AC voltage and current sensing for real-time monitoring.

## Hardware Stackup
### **PCB Layer Configuration:**
- **Relay Board:** 2-layer PCB.
- **Display Board:** 2-layer PCB.
- **Controller Board:** 4-layer PCB (JLC04161H-3313, 1.56mm ±10% thickness)

![Controller Board 3D View](images/Controller%20Board/ControllerBoard_3D-1.png)

![Display Board 3D View](images/Display%20Board/DisplayBoard_3D-2.png)

![Relay Board 3D View](images/Relay%20Board/RelayBoard_3D-1.png)

### **Impedance Control:**
| Impedance (Ω)  | Type                             | Signal  | Bottom  | Trace Width (mm) | Trace Spacing (mm) |
|-------------- | -------------------------------- | ------- | ------- | ---------------- | ------------------ |
| 90             | Differential Pair (Non coplanar) | L1      | L2      | 0.1549           | 0.1905             |
| 100            | Differential Pair (Non coplanar) | L1      | L2      | 0.1209           | 0.1905             |

### **Layer Stackup:**
| Layer    | Material                   | Thickness (mm) |
|-------- | -------------------------- | -------------- |
| L1 (SIG) | Outer Copper Weight 1oz    | 0.0350 |
| Prepreg  | 3313 RC57% 4.2mil          | 0.0994 |
| L2 (GND) | Inner Copper Weight        | 0.0152 |
| Core     | 1.3mm H/HOZ with copper    | 1.2650 |
| L3 (GND) | Inner Copper Weight        | 0.0152 |
| Prepreg  | 3313 RC57% 4.2mil          | 0.0994 |
| L4 (SIG) | Outer Copper Weight 1oz    | 0.0350 |

## Usage
1. **Setup the hardware** by assembling the three boards.
2. **Power on the system** and configure settings via the display or Ethernet interface.
3. **Monitor and control** relay switching and power parameters.

## License
This project is licensed under the **GPL-3.0 License**. See the [LICENSE](LICENSE) file for details.

## Contact
For questions or feedback:
- **Email:** [s.dvid@hotmail.com](mailto:s.dvid@hotmail.com)
- **GitHub:** [DvidMakesThings](https://github.com/DvidMakesThings)

