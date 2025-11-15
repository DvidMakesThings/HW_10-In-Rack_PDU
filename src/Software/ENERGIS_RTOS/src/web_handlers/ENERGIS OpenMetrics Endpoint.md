# ENERGIS OpenMetrics Endpoint

## Overview

The ENERGIS PDU exposes a `/metrics` HTTP endpoint that provides runtime telemetry in OpenMetrics/Prometheus text format (version 0.0.4). This plugs directly into Prometheus, Grafana, and similar tools.

## Configuration

### Accessing the Endpoint

```bash
curl.exe http://<pdu-ip-address>/metrics
```

### Example response

```
# HELP energis_up 1 if the metrics handler is healthy.
# TYPE energis_up gauge
energis_up 1
# HELP energis_build_info Build and device identifiers.
# TYPE energis_build_info gauge
energis_build_info{version="1.0.0",serial="SN-0167663"} 1
# HELP energis_uptime_seconds_total System uptime in seconds.
# TYPE energis_uptime_seconds_total counter
energis_uptime_seconds_total 27
# HELP energis_internal_temperature_celsius Internal temperature (calibrated).
# TYPE energis_internal_temperature_celsius gauge
energis_internal_temperature_celsius 30.668
# HELP energis_temp_calibrated 1 if temperature calibration is applied.
# TYPE energis_temp_calibrated gauge
energis_temp_calibrated 1
# HELP energis_temp_calibration_mode Calibration mode: 0=none, 1=1pt, 2=2pt.
# TYPE energis_temp_calibration_mode gauge
energis_temp_calibration_mode 1
# HELP energis_vusb_volts USB rail voltage.
# TYPE energis_vusb_volts gauge
energis_vusb_volts 5.332
# HELP energis_vsupply_volts 12V supply rail voltage.
# TYPE energis_vsupply_volts gauge
energis_vsupply_volts 12.093
# HELP energis_http_requests_total Total HTTP requests served.
# TYPE energis_http_requests_total counter
energis_http_requests_total 17
# HELP energis_channel_state Relay state (1=ON, 0=OFF).
# TYPE energis_channel_state gauge
energis_channel_state{ch="1"} 1
# HELP energis_channel_telemetry_valid 1 if cached telemetry for channel is fresh.
# TYPE energis_channel_telemetry_valid gauge
energis_channel_telemetry_valid{ch="1"} 1
# HELP energis_channel_voltage_volts Channel voltage.
# TYPE energis_channel_voltage_volts gauge
energis_channel_voltage_volts{ch="1"} 0.000
# HELP energis_channel_current_amps Channel current.
# TYPE energis_channel_current_amps gauge
energis_channel_current_amps{ch="1"} 0.000
# HELP energis_channel_power_watts Active power per channel.
# TYPE energis_channel_power_watts gauge
energis_channel_power_watts{ch="1"} 0.000
# HELP energis_channel_energy_watt_hours_total Accumulated energy per channel.
# TYPE energis_channel_energy_watt_hours_total counter
energis_channel_energy_watt_hours_total{ch="1"} 0.000
...
```

## Prometheus Integration

Add to `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'energis-pdu'
    scrape_interval: 15s
    scrape_timeout: 10s
    metrics_path: /metrics
    static_configs:
      - targets: ['192.168.0.22:80']
        labels:
          location: 'rack-1'
          device: 'pdu-main'
```

## Available Metrics

### System

* `energis_up` - Health indicator (gauge; 1 if handler responds)
* `energis_build_info{version,serial}` - Build and device info (gauge; always 1)
* `energis_uptime_seconds_total` - Uptime since boot in seconds (counter)
* `energis_internal_temperature_celsius` - Internal MCU temperature, calibrated (gauge)
* `energis_temp_calibrated` - 1 if temperature calibration is applied (gauge)
* `energis_temp_calibration_mode` - Calibration mode (0=none, 1=1pt, 2=2pt) (gauge)
* `energis_vusb_volts` - USB rail voltage (gauge)
* `energis_vsupply_volts` - 12 V rail voltage (gauge)
* `energis_http_requests_total` - Total HTTP requests served (counter)

### Per-Channel (ch = "1" … "8")

* `energis_channel_state{ch}` - Relay state: 1=ON, 0=OFF (gauge)
* `energis_channel_telemetry_valid{ch}` - 1 if cached telemetry for channel is fresh (gauge)
* `energis_channel_voltage_volts{ch}` - Channel voltage [V] (gauge)
* `energis_channel_current_amps{ch}` - Channel current [A] (gauge)
* `energis_channel_power_watts{ch}` - Active power [W] (gauge)
* `energis_channel_energy_watt_hours_total{ch}` - Accumulated energy in watt-hours (counter)

## Notes and Behavior

* Values are served from cached snapshots owned by `MeterTask`. No direct sensor I/O in the handler.
* Temperature uses the active calibration. If no calibration is stored, defaults are used.

## Handy PromQL

* MCU temperature:
  `energis_internal_temperature_celsius`
* Rails:
  `energis_vusb_volts`, `energis_vsupply_volts`
* Total active power across all channels:
  `sum(energis_channel_power_watts)`
* Telemetry freshness check:
  `sum(energis_channel_telemetry_valid) == 8`

## Example Alerts

```yaml
groups:
- name: energis
  rules:
  - alert: EnergisHighTemp
    expr: energis_internal_temperature_celsius > 70
    for: 2m
    labels: {severity: warning}
    annotations:
      summary: "ENERGIS MCU hot >70C"

  - alert: EnergisVsupplyLow
    expr: energis_vsupply_volts < 10.8
    for: 1m
    labels: {severity: critical}
    annotations:
      summary: "ENERGIS 12V rail low"

  - alert: EnergisNoTelemetry
    expr: sum(energis_channel_telemetry_valid) < 8
    for: 2m
    labels: {severity: warning}
    annotations:
      summary: "ENERGIS channel telemetry not fully valid"
```

## Performance

* Static buffer ~4 KB. No heap allocations.
* Non-blocking, minimal CPU load.
* Typical response time < 50 ms.
* If the output would overflow the buffer, the handler returns **503** to encourage a retry instead of serving a partial payload.

## Implementation Notes

* Reads from `MeterTask_GetTelemetry()` and `MeterTask_GetSystemTelemetry()`.
* Uses atomic counters for request accounting.
* OpenMetrics text format compatible with Prometheus 0.0.4.
