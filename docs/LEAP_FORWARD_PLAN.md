# LEAP Reference Stack — Forward Plan

Prioritized open work for porting readiness. Status as of May 2026.

Completed capabilities are documented below under **Implemented capabilities**.
This file lists only remaining and deferred work.

---

## Implemented capabilities

### Core stack and LEAP-PD

- LEAP-PD lives in `src/services/pd/` (`leap_pd_common`, `leap_pd_device`, `leap_pd_controller`). Linux and Windows examples use stacks plus transport adapters only; see the porting path in [examples/README.md](../examples/README.md).
- **`leap_controller_stack`** mirrors the device-side model: bootstrap FSM, `on_frame`, release, cyclic PD, and DIAG read helpers alongside **`leap_device_stack`** dispatch for all five v1 services.
- **`LeapPdControllerStats`** tracks cycle latency, jitter vs target period, lost frames (exchange timeouts), reply rejects, overruns, and optional hub RTT/queue split. Logged via `leap_pd_controller_log_stats()`.
- Example audit complete: porting templates are `examples/linux_loopback/*` and `examples/win_l2/*`; `examples/device_minimal` remains learning-only.

### LEAP-DIAG and observability

- Device handler, stack dispatch, controller builders/parsers, and unit tests.
- Post-OP **`leap_controller_stack_read_diag()`** / **`log_diag()`** without new bootstrap FSM phases.
- **`--diag`** on Linux and Windows controllers reads counters and timing after bootstrap or a PD run.
- Golden frames for DIAG `READ_COUNTERS` / `COUNTERS_REPLY` (§10) and `READ_TIMING` / `TIMING_REPLY` (§10.7).
- Wireshark dissector covers v1 services, PD exchange layout, and DIAG message types.

### Multi-peer and session hub

- Discovery peer table with **`discover_ex`** early exit at `min_peers`, **`probe_peer`**, and MAC parse helpers.
- **`leap_controller_session_hub`**: independent MGMT/PD/session state per peer slot.
- Hub examples: **`leap_linux_hub`**, **`leap_win_hub`**, **`leap_win_discover`** (Windows parity); documented in example READMEs.
- Hub PD modes: round-robin, parallel lap, and **`--random-peer`** (one random device + one random output bit per paced cycle on Windows hub).
- Hardening: per-peer Ethernet sequence, session binding after OP, PD exchange validation, §13.4 frame age on replies, foreign-owner skip on bootstrap, optional `LEAP_LOG_SECURITY` logging.
- Integration tests (no sockets): hub bootstrap isolation, round-robin, parallel send-before-recv, foreign-owner skip, random-peer slot indexing — see `test_controller_session_hub.c`.
- Porting checklist: “Ready for hardware” table in [docs/README.md](README.md).

### Transport and resilience

- **`leap_raw_linux`**: `query_link` / `poll_link` with carrier up/down surfaced to examples; link transition stats.
- Reconnect policy documented: rediscover vs `bootstrap_peer` vs full stack reset — [LEAP_TRANSPORT_RECONNECT.md](LEAP_TRANSPORT_RECONNECT.md).
- Windows Npcap transport (`leap_raw_winpcap`) with stats; timestamped logging via **`leap_log`** across examples.

---

## Open work

### Near term — core lock-down

| Item | Notes |
| --- | --- |
| Manual wire smoke | Run `tools/ci/wire_smoke_*.sh` (Linux) and `wire_smoke_win.ps1` (Windows) on native hosts before platform forks |

### Medium term — integration and release prep

| Item | Status | Notes |
| --- | --- | --- |
| Periodic transport stats in cyclic PD logs | **partial** | Exit-time `--stats` exists; drops/retries/parse errors not yet logged during cyclic runs |
| Hub `--diag` across all peers | **open** | Session hub has no multi-peer DIAG helper; use per-slot `read_diag()` or controller `--diag` |
| DIAG auto-poll in controller FSM | **open** | Use explicit `read_diag()` or `--diag` today |
| Auto-reconnect FSM | **open** | Policy is documented; examples poll link but do not auto-rebootstrap |
| Rolling 64-bit sequence bitmap | **open** | Optional unless hardware shows reordering |
| MGMT frame-age on generic inbound path | **open** | §13.4 age check today on PD exchange reply path |
| v1.0 conformance / release readiness review | **open** | Spec freeze candidate, manifest schema, independent implementer checklist |

### Deferred (avoid before porting gate)

- Multi-controller election / priority MGMT extension.
- IP/routed transport, fragmentation for large blobs.
- Platform-specific PD timing workarounds in examples (belongs in core stats/config).

---

## Success metrics (remaining)

| Milestone | Criteria |
| --- | --- |
| **Core locked** | Manual wire smoke passed on Linux and Windows; porting gate signed off |
| **DIAG complete** | Hub DIAG variant or documented per-peer pattern; optional FSM auto-poll |
| **Multi-peer confident** | Soak validation on hardware (hub modes + foreign-owner behavior) |
| **v1.0 candidate** | Release readiness review complete; manual wire smoke recorded |

---

## See also

- [README.md](../README.md) — repository overview and feature summary
- [LEAP_CONTROLLER_STACK_PLAN.md](LEAP_CONTROLLER_STACK_PLAN.md) — controller design and API
- [LEAP_MULTI_PEER_NOTES.md](LEAP_MULTI_PEER_NOTES.md) — multi-peer config and failure modes
