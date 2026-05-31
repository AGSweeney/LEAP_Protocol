# LEAP Controller Stack — Design Plan

**Status:** Implemented (May 2026) — Phases 1–5 complete; see §11.  
**Goal:** Mirror `leap_device_stack` with a transport-agnostic controller-side
integration layer for discovery, directory, MGMT bootstrap, and cyclic PD.

---

## Implementation summary (current)

The Linux example (`controller_main.c`) is now **transport + stack only**:

```
leap_controller_stack_bootstrap()  →  OP
leap_controller_stack_run_cyclic_pd()  or  leap_controller_stack_pd_single_write()
leap_controller_stack_release()
```

| Component | File(s) | Status |
| --- | --- | --- |
| DISC helpers | `leap_disc_controller` | done |
| Bootstrap FSM | `leap_controller_stack` | done |
| Async inbound | `leap_controller_stack_on_frame` | done |
| Graceful shutdown | `leap_controller_stack_release` | done |
| Frame sequence | `leap_controller_sequence` | done |
| Multi-peer discover | `leap_controller_peer` | done (`discover_ex`, probe, `parse_mac`) |
| Concurrent sessions | `leap_controller_session_hub` | done |
| Cyclic PD entry | `leap_controller_stack_run_cyclic_pd` | done |
| DIAG read (post-OP) | `leap_controller_stack_read_diag`, `log_diag` | done |
| Linux IO adapter | `leap_linux_controller_io` | done |
| Windows IO adapter | `leap_win_io` + `win_l2/controller_main.c` | done |
| DIAG auto-poll in FSM | — | **open** (use `read_diag()` explicitly; Linux/Windows `--diag` flag) |

Multi-peer hardening (sequence window, session bind, exchange validation, frame
age, foreign-owner skip) is documented in `LEAP_MULTI_PEER_NOTES.md`.

---

## 1. Problem (original)

The Linux example previously owned ~300 lines of sequential bootstrap logic:

```
HELLO → HELLO_REPLY → SELECT_PROFILE → PROFILE_REPLY →
OPEN_SESSION → OPEN_SESSION_REPLY → SET_STATE → STATE_REPLY → PD
```

Core building blocks existed in isolation; the controller needed the symmetric
**active** model that `leap_device_stack` provides on the device side.

**Resolved:** bootstrap and PD are unified under `leap_controller_stack`.

---

## 2. Design Principles

1. **Transport-agnostic** — no sockets in the stack; I/O via callbacks (same
   pattern as `LeapPdControllerIo`).
2. **Reuse existing services** — wrap, do not rewrite, `mgmt` / `dir` / `pd`
   controller modules.
3. **Explicit FSM** — bootstrap phases are named states with documented
   transitions and status codes.
4. **Single-owner first** — v1 targets one active owner per device; session hub
   supports N independent device sessions on one controller.
5. **Sync + async** — blocking `bootstrap()` *and* `on_frame()` for recv threads.

---

## 3. Module layout (as built)

```
inc/leap/
  leap_disc_controller.h
  leap_controller_stack.h
  leap_controller_peer.h
  leap_controller_session_hub.h
  leap_controller_sequence.h

src/
  leap_controller_stack.c
  leap_controller_peer.c
  leap_controller_session_hub.c
  leap_controller_sequence.c
  services/disc/leap_disc_controller.c

examples/linux_loopback/
  controller_main.c           # transport + stack only
  leap_linux_controller_io.c
  discover_main.c             # multi-device HELLO scan demo
  hub_main.c                  # session hub round-robin demo

examples/win_l2/
  controller_main.c           # Windows Npcap transport + stack
  device_main.c
  hub_main.c                  # session hub round-robin
  discover_main.c             # HELLO scan utility

examples/win_smoke/
  wire_smoke_main.c           # single-process cooperative smoke
```

---

## 4. Core Types

### 4.1 Transport I/O (`LeapControllerStackIo`)

Generalizes service sends/recvs for bootstrap and MGMT:

```c
typedef struct LeapControllerStackIo
{
    void* user_ctx;
    int (*send_frame)(...);
    int (*recv_frame)(...);
    uint64_t (*monotonic_us)(void* user_ctx);
} LeapControllerStackIo;
```

Cyclic PD uses **`LeapPdControllerIo`** (shared transport pointer via
`leap_linux_pd`).

### 4.2 Stack context

```c
typedef struct LeapControllerStack
{
    LeapControllerStackConfig config;
    LeapControllerStackPhase  phase;
    LeapMgmtControllerContext mgmt;
    LeapPdControllerContext   pd;
    uint8_t                   peer_mac[6];
    LeapControllerFrameSequenceState frame_seq;
} LeapControllerStack;
```

### 4.3 Public API (high level)

| Function | Purpose |
| --- | --- |
| `leap_controller_stack_init` | Configure MGMT/PD/sequence defaults |
| `leap_controller_stack_bootstrap` | DISC → DIR → MGMT → OP (single peer) |
| `leap_controller_stack_bootstrap_peer` | Same for known MAC + cached HELLO |
| `leap_controller_stack_on_frame` | Route async inbound by peer MAC |
| `leap_controller_stack_release` | OWNER_RELEASE + reset |
| `leap_controller_stack_run_cyclic_pd` | Cyclic PD via bound peer |
| `leap_controller_stack_pd_single_write` | One-shot PD write |
| `leap_controller_stack_read_diag` | Post-OP READ_COUNTERS + READ_TIMING |
| `leap_controller_stack_log_diag` | Print DIAG counters/timing to stdout |

Session hub wraps multiple `LeapControllerStack` slots — see
`leap_controller_session_hub.h`.

---

## 11. Implementation phases

### Phase 1 — Foundation

- [x] `leap_disc_controller.h/c`
- [x] `leap_controller_stack.h/c` with `init`, `reset`, `step`, `bootstrap`
- [x] `test_disc_controller.c`, `test_controller_stack.c`

### Phase 2 — Linux example

- [x] `leap_linux_controller_io.c`
- [x] `controller_main.c` uses stack API only
- [x] Manual loopback on native Linux
- [ ] CI wire smoke on GHA (blocked — run scripts locally; see multi-peer notes)

### Phase 3 — Hardening

- [x] `leap_controller_stack_on_frame()` for async MGMT
- [x] `leap_controller_stack_release()` graceful shutdown
- [x] Frame-level sequence tracking + optional window/gap reject
- [x] MGMT ERROR → FAULT; security log hook (`LEAP_LOG_SECURITY`)

### Phase 4 — Multi-device

- [x] Peer table + `leap_controller_peer_table_discover()`
- [x] `leap_controller_stack_bootstrap_peer()`
- [x] `leap_linux_discover` example + local wire smoke script

### Phase 5 — Concurrent sessions

- [x] `leap_controller_session_hub` — per-peer stack slots
- [x] `on_frame` demux by MAC, round-robin PD
- [x] Exchange reply MAC filter; PD profile validation

---

## 12. Non-Goals (v1)

- Automatic DIAG polling inside controller bootstrap FSM (use `read_diag()` or `--diag`)
- Hub-level `--diag` across all peers (session hub has no DIAG helper yet)
- Fragment transmission or reassembly (inbound fragments rejected)
- IP/routed transport
- Controller-side DIR `read_directory` polling in FSM
- Full redundant controller failover / election
- Rolling 64-bit sequence bitmap (optional post-v1)

---

## 13. Open questions (resolved for v1)

1. **Sequence ownership** — MGMT sequence via `leap_mgmt_controller_next_sequence()`;
   per-peer frame sequence in `leap_controller_sequence` after OP.
2. **Blocking vs threaded recv** — `recv_timeout_ms` in bootstrap; dedicated recv
   thread calls `on_frame()`.
3. **Error responses** — MGMT ERROR → `LEAP_CTRL_STACK_FAULT` with `error_code` in event.

---

## 14. Success criteria

- [x] `controller_main.c` bootstrap/PD via stack calls only
- [x] ≥4 unit tests for controller stack FSM (replay, session mismatch, release, …)
- [x] No socket code inside `leap_controller_stack.c`
- [x] Public API documented in headers
- [ ] Wire smoke in CI (deferred — manual on native Linux)

---

## See also

- [LEAP_MULTI_PEER_NOTES.md](LEAP_MULTI_PEER_NOTES.md)
- [../README.md](../README.md) — repository overview and roadmap
