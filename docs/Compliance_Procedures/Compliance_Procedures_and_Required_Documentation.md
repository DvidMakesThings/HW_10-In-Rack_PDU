# ENERGIS Compliance Procedures and Required Documentation
Status: Internal instruction
Scope: Placement on the market in the European Union (EU) and the United States/Canada (US/CA)

## 1. Regional variants (controlled configuration)
Two regional variants are defined. The variant on the rating label is authoritative.

### 1.1 EU variant (European Union)
- Rated input current: 10 A
- Rated total load (all outlets combined): 10 A
- Internal overcurrent protection: T10A, 250 VAC, time-delay fuse

### 1.2 US/CA variant (United States and Canada)
- Rated input current: 15 A
- Rated total load (all outlets combined): 15 A
- Internal overcurrent protection: T15A, 250 VAC, time-delay fuse (high interrupt rating capability)

Variant control records
- Bill of materials (BOM) variant identifier
- Rating label artwork per variant
- Packaging and sales listing per variant
- Production test record includes variant identifier
- Firmware configuration flag (if used) matches physical fuse and rating label

---

## 2. European Union (EU) requirements and procedure (CE marking)

### 2.1 Applicable EU legislation (minimum set)
- Low Voltage Directive (Directive 2014/35/EU) 
- Electromagnetic Compatibility Directive (Directive 2014/30/EU) 
- RoHS Directive (Directive 2011/65/EU)
- WEEE Directive (Directive 2012/19/EU) obligations apply where ENERGIS is placed on the market as electrical and electronic equipment 

### 2.2 Conformity assessment and evidence
Evidence package includes:
- Safety evaluation evidence supporting conformity with LVD safety objectives (design review and test results)
- Electromagnetic compatibility test evidence (emissions and immunity) supporting EMC Directive requirements
- RoHS technical documentation and declarations supporting substance restrictions 
- Risk assessment and misuse analysis (electric shock, protective earth continuity, overheating, fire hazards)

Use of harmonised standards
- Harmonised standards provide presumption of conformity when applied appropriately; the Commission maintains summary lists.

### 2.3 Required EU documents (deliverables)
- EU Declaration of Conformity (EU DoC) covering applicable directives
- Technical documentation (technical file), including at minimum:
  - Product description, variants, ratings, intended use
  - Schematics, bill of materials, drawings, critical safety parts list
  - Test reports (safety and EMC)
  - Risk assessment
  - Manufacturing controls and change control rules
  - Labels and user instructions

Retention
- Technical documentation and EU DoC retained for 10 years after placement on the market (directive practice; explicitly stated in LVD guidance sources).

### 2.4 EU marking and information
- CE mark applied after conformity assessment completion under the applicable directives
- WEEE marking and producer obligations handled where applicable
- Rating label matches EU variant (10 A) and the installed fuse rating

---

## 3. United States and Canada (US/CA) requirements and procedure

### 3.1 Product safety certification (workplace market expectation)
Safety certification via a Nationally Recognized Testing Laboratory (NRTL) is commonly required for products used in US workplaces and for mainstream distribution.
- NRTL program definition and scope are maintained by the Occupational Safety and Health Administration (OSHA).

Procedure
- Select an NRTL (example routes: UL Listed, ETL Listed, CSA).
- Determine the applicable product safety standard(s) for the product category and intended installation.
- Submit representative samples of the US/CA variant for evaluation.
- Implement ongoing production controls required by the certification program (follow-up inspections are part of the NRTL concept). 

Required US/CA documents (deliverables)
- NRTL certification documentation set (certification report / construction file as issued by the certifier)
- Controlled bill of materials for the certified construction
- Rating label artwork matching US/CA variant (15 A) and the installed fuse rating
- Production test procedure and records matching the certified construction

### 3.2 Electromagnetic compatibility (FCC)
Digital electronics typically fall under Federal Communications Commission (FCC) rules for radio frequency devices.
- FCC equipment authorization framework applies to intentional and unintentional radiators.
- FCC Part 15 is the core rule set for radio frequency devices, including unintentional radiators. 

Procedure
- Determine classification (for example, digital device, Class A or Class B depending on marketed environment).
- Perform emissions testing to the applicable FCC limits.
- Maintain required compliance records and statements for the applicable authorization route. 

---

## 4. Common manufacturing controls (EU and US/CA)
Mandatory controls to maintain equivalence between tested and shipped product:
- Critical parts list and approved manufacturer list (inlet, outlets, fuse and holder, relay, insulation system, protective earth hardware, wiring, strain relief)
- Incoming inspection rules for critical parts (ratings and approvals)
- Traceability: serial number plus variant identifier (EU or US/CA)
- Engineering change control: safety-relevant changes trigger reassessment and re-test as required

---

## 5. Output package checklist (release gate)
EU release package
- EU DoC completed and signed 
- Technical file complete (safety, EMC, RoHS evidence)
- CE marking and label verified for EU variant
- WEEE handling confirmed where applicable

US/CA release package
- NRTL certification completed for US/CA variant
- FCC compliance evidence and required statements completed
- Label verified for US/CA variant
- Production control plan aligned with certification construction file