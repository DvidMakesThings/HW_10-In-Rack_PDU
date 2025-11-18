# Shielding Design – ENERGIS

This folder contains the **mechanical shielding parts** for the ENERGIS system.  
The shields are designed in Fusion 360 (sheet-metal workflow) and exported as **DXF flat patterns** for cutting from 0.5 mm brass sheet.  

Brass was chosen over aluminum because it can be **soldered directly to PCB GND pads**, ensuring reliable low-impedance bonding.

---

## Purpose

During early testing, two main issues appeared:

1. **EMI coupling** from the 230 V relay/AC section into the digital side.  
   - Without shielding, the FPC cable and control logic were acting as antennas.  
   - Noise could be observed on I2C and button reads.

2. **ESD vulnerability** on the display board.  
   - Button presses and connector touches occasionally caused resets.  

To mitigate these issues, I've implemented **mechanical brass shields** tied to PCB GND at multiple points.
Brass is easy to solder and bending a 0.5mm thick sheet doesn't require that much force

---

## Main Board Shield

- Covers the entire digital interface, leaving only the connectors exposed
- L-shaped fence along the isolation slot between HV and digital domains.  
- Tabs solder into test pads + screw hole for both mechanical strength and ground bonding.  
- The FPC cable runs **under the shield**; only a few cm exposed.  

![Main Board Shield](../../../images/ENERGIS_1.0.0/MainBoard_Shielding.png)

DXF: [MainBoard_Shielding.dxf](MainBoard_Shielding.dxf)  
PDF: [MainBoard_Shielding.pdf](MainBoard_Shielding.pdf)

---

## Display Board Shield

- Full brass cap covering the back side of the display board.  
- Exposed only where connectors and buttons are located.  

Front view:  
![Display Board Shield – Front](../../../images/ENERGIS_1.0.0/DisplayBoard_Shielding-Front.png)

Back view:  
![Display Board Shield](../../../images/ENERGIS_1.0.0/DisplayBoard_Shielding.png)

DXF: [DisplayBoard_Shielding.dxf](DisplayBoard_Shielding.dxf)  
PDF: [DisplayBoard_Shielding.pdf](DisplayBoard_Shielding.pdf)

---

## Notes

- Shields are designed for **0.5 mm brass**.  
- After cutting, edges must be deburred and tabs cleaned before soldering.  
- Painting is optional for corrosion resistance — but **mask the solder tabs** to keep them conductive.  

---
