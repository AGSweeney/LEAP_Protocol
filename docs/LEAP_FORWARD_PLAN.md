# LEAP Reference Stack — Forward Plan

Prioritized work aligned with porting readiness. Status as of May 2026.

Legend: **done** · **partial** · **open**

---

## Your 7–10 day goals vs current state

| Goal | Status | Notes |
| --- | --- | --- |
| Polish LEAP-PD in `src/services/pd/`; extract from examples | **done** | Core PD in `src/services/pd/`; Linux examples use stacks + transport adapters only. |
| Controller stack symmetry with device stack | **done** | `leap_controller_stack` mirrors bootstrap + tick-adjacent PD; `leap_device_stack` dispatches all five services. |
| Stabilize multi-peer (sequence + ownership) | **partial** | Per-peer sequence, session bind, PD validation, foreign-owner skip, security log hook. Rolling bitmap + MGMT frame-age still open. |
| PD statistics (jitter, latency, lost frames) | **done** | `LeapPdControllerStats`: latency, cycle jitter vs target, `lost_frames`, reply rejects; `leap_pd_controller_log_stats()`. |

**Reprioritized next 7–10 days (core lock-down):**

1. **PD extraction finish** — **done** — audit documented in [examples/README.md](../examples/README.md); porting path is `linux_loopback/*`; `device_minimal` stays learning-only.
2. **Controller DIAG symmetry** — **done** — `leap_controller_stack_read_diag()` + `leap_controller_stack_log_diag()`; `--diag` in Linux example (bootstrap-only or after PD).
3. **Multi-peer test gap** — **done** — `test_session_hub_bootstrap_table_skips_foreign_owner`, `test_session_hub_run_round_robin_two_peers`.
4. **Porting checklist** — **done** — “Ready for hardware” table in [docs/README.md](README.md).
5. **Manual wire smoke** — run `tools/ci/wire_smoke_*.sh` on native Linux before any platform fork.

---

## Your 3–4 week goals vs current state

| Goal | Status | Notes |
| --- | --- | --- |
| Implement LEAP-DIAG | **partial** | Device handler + stack dispatch + controller builders/parsers + unit tests + stack `read_diag` + Linux `--diag`. Hub variant and FSM poll **open**. |
| Transport (link monitoring, reconnect) | **partial** | `query_link` / `poll_link` + `link_transitions` stat; reconnect policy doc; example poll in device recv loop. Auto-reconnect FSM **open**. |
| Multi-device test coverage | **partial** | Peer table, session hub, discover mock, foreign-owner skip, round-robin PD, end-to-end `table_bootstrap_round_robin` test. |
| Spec + golden vectors | **partial** | DIAG counters + timing golden frames (§10); Wireshark DIAG + PD exchange decode. |

**Reprioritized next 3–4 weeks:**

### Week 2 — Integration & observability

- ~~Controller-side DIAG read path (post-OP, no new FSM phases).~~ **done**
- ~~Linux example: `--diag` reads counters / timing after bootstrap or PD run.~~ **done**
- ~~Session hub example or documented pattern: discover → hub → round-robin (optional binary).~~ **done** — `leap_linux_hub` + [examples/linux_loopback/README.md](../examples/linux_loopback/README.md).

### Week 3 — Transport & resilience

- ~~`LeapRawLinux` link state hook (carrier up/down) surfaced to examples.~~ **done**
- ~~Document reconnect policy: rediscover vs `bootstrap_peer` vs full stack reset.~~ **done** — [LEAP_TRANSPORT_RECONNECT.md](LEAP_TRANSPORT_RECONNECT.md)
- Expand transport stats in periodic log (drops, retries, parse errors).

### Week 4 — Conformance & release prep

- ~~Golden vector(s) for DIAG `READ_COUNTERS` / `COUNTERS_REPLY` round-trip.~~ **done** (§10)
- ~~Golden vector(s) for DIAG `READ_TIMING` / `TIMING_REPLY`.~~ **done** (§10.7)
- ~~Wireshark dissector: DIAG message types + PD exchange layout.~~ **done**
- Multi-device integration test in `ctest` (hub + 2 mock peers, no sockets).
- Tag readiness review: spec freeze candidate, manifest schema, independent implementer checklist.

---

## Explicitly defer (avoid before porting gate)

- Rolling 64-bit sequence bitmap (unless hardware shows reordering).
- Multi-controller election / priority MGMT extension.
- IP/routed transport, fragmentation for large blobs.
- Platform-specific PD timing workarounds in examples (belongs in core stats/config).

---

## Success metrics

| Milestone | Criteria |
| --- | --- |
| **Core locked** | Examples use stacks only; `ctest` ≥105 tests green; porting gate signed off |
| **DIAG complete** | Device + controller read path + example/test round-trip + golden vectors |
| **Multi-peer confident** | Hub round-robin + foreign-owner tests; multi-peer notes match behavior |
| **v1.0 candidate** | Golden vectors updated; dissector covers v1 services; manual wire smoke passed on Linux |

---

## See also

- [README.md](../README.md) — repository overview
- [LEAP_CONTROLLER_STACK_PLAN.md](LEAP_CONTROLLER_STACK_PLAN.md) — controller implementation status
- [LEAP_MULTI_PEER_NOTES.md](LEAP_MULTI_PEER_NOTES.md) — multi-peer config and failure modes
