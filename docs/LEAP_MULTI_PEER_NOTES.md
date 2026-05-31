# LEAP Multi-Peer Notes

Reference-stack guidance for controlling multiple devices on one Ethernet
segment, and known gaps for true multi-controller coexistence.

## What the reference stack provides today

| Mechanism | Location | Purpose |
| --- | --- | --- |
| Per-peer `LeapControllerStack` slots | `leap_controller_session_hub` | Independent `session_id`, MGMT sequence, PD state, lease |
| Frame `sequence` tracking | `leap_controller_sequence` | Duplicate replay rejection, gap counting/rejection, optional window |
| Session binding | `leap_controller_stack` on OP entry | Drop inbound frames whose `session_id` ≠ owner session |
| PD endpoint validation | `leap_pd_profile_validate_*` | Profile ID, endpoint IDs, payload lengths from DIR map |
| Exchange reply validation | `leap_pd_validate_exchange_reply_at` | Endpoints, `process_sequence`, optional §13.4 frame age |
| Outbound PD timestamps | `leap_pd_controller` | Stamps `controller_timestamp_us` / `max_frame_age_us` on exchange |
| Foreign-owner skip | `leap_controller_session_hub` | `bootstrap_table` skips peers with another `active_owner_mac` |
| Linux recv demux | `leap_linux_pd.c` | `wait_exchange_reply` ignores frames from other peer MACs |
| I/O dirty flag | `LeapPdDeviceIoBinding.outputs_dirty` | Skip redundant output shadow writes |

## Configuration knobs

```c
LeapControllerStackConfig cfg;
cfg.frame_sequence.enforce_session_match = 1;  /* default when config NULL */
cfg.frame_sequence.window_size           = 64;
cfg.frame_sequence.reject_out_of_window  = 0;  /* set 1 for strict wire replay guard */
cfg.frame_sequence.reject_sequence_gaps  = 0;  /* set 1 to reject forward sequence gaps */

cfg.pd.validate_exchange_reply  = 1;  /* default when PD config NULL */
cfg.pd.enforce_reply_frame_age  = 1;  /* validate echoed timestamp on EXCHANGE_REPLY */
cfg.pd.max_frame_age_us         = 0;  /* 0 => 2 * cycle_period_ms */
cfg.pd.reply_jitter_margin_us   = 0;

LeapControllerSessionHubConfig hub_cfg;
hub_cfg.skip_foreign_owned_peers = 1;  /* default when hub config NULL */
memcpy(hub_cfg.default_peer.mgmt.controller_mac, local_mac, 6);
```

## Remaining risks (not implemented in v1 reference)

### Multi-controller arbitration / election

LEAP v1 assumes **one active owner** per device (MGMT owner lease). Two controllers
on the same wire without coordination will race for `OPEN_SESSION`; the device
enforces single ownership — this is not a symmetric multi-master fieldbus.

Partial mitigation today:

- Discovery stores `active_owner_mac` from HELLO_REPLY
- `leap_controller_peer_owned_by_other()` and hub `skip_foreign_owned_peers`
  avoid bootstrapping peers already owned by another MAC

Still out of scope for v1:

- Controller priority / token in MGMT extension
- Explicit owner election or redundant-controller failover profile
- Coordinated takeover when the incumbent controller fails

### Sequence + time validation

Implemented today:

- Ethernet `sequence` duplicate detection per peer (controller inbound)
- Optional strict gap rejection (`reject_sequence_gaps`)
- Device `process_sequence` enforcement (§13.6)
- Controller exchange-reply `process_sequence` check
- Category A frame age on inbound EXCHANGE_REPLY (§13.4 echoed timestamp)

Still weak / optional:

- Frame age on generic controller MGMT receive path (not only PD exchange reply)
- Bitmap replay window across reconnect (sequence baseline reset rules)

### Performance at scale

The Linux example uses a simulated I/O shadow with an `outputs_dirty` flag.
Embedded ports should:

- Mark endpoint groups dirty only when payload bytes change
- Avoid scanning full endpoint tables when a profile has many endpoints but
  only one group updates per cycle

## Recommended bring-up flow (multi-peer)

1. `leap_controller_peer_table_discover()` — collect MACs and `active_owner_mac`
2. Skip or defer peers where `leap_controller_peer_owned_by_other()` is true
3. `leap_controller_session_hub_bootstrap_peer()` per target (or `_bootstrap_table()`)
4. `leap_controller_session_hub_run_round_robin()` for cyclic PD
5. `leap_controller_session_hub_on_frame()` from a recv thread for async MGMT
6. `leap_controller_session_hub_release_all()` on shutdown

## Multi-peer impact (what breaks, what holds)

### Happy path: N devices, one controller

| Stage | Behavior |
| --- | --- |
| Discover | Broadcast HELLO; table holds one row per device MAC + profile/state/owner |
| Bootstrap | Hub allocates one `LeapControllerStack` slot per peer; independent `session_id` and MGMT sequence |
| Cyclic PD | Round-robin calls `run_one_cycle` per OP slot; Linux transport demuxes replies by peer MAC |
| Async recv | `on_frame` routes by `src_mac`; frame sequence + session binding per slot |

### Failure modes

| Scenario | Stack response | Device state |
| --- | --- | --- |
| Two controllers race `OPEN_SESSION` | Device grants one owner; loser gets MGMT error / NOT_OWNER | Single owner enforced by device |
| Foreign `active_owner_mac` at discover | Hub `skip_foreign_owned_peers` skips bootstrap (default) | Unchanged |
| Replay old Ethernet `sequence` | Controller ignores duplicate; `duplicate_frames++` | Unaffected |
| Forward sequence gap | Counted by default; optional `reject_sequence_gaps` faults slot | PD `process_sequence` still authoritative |
| PD from wrong MAC or session | Device rejects NOT_OWNER; OP → SAFE on spoof | Safe state |
| Stale EXCHANGE_REPLY | Controller drops reply; `reply_stale_rejects++` | Unaffected |
| Cross-peer frame on wrong slot | Session mismatch or ignored (no matching slot MAC) | Unaffected |

### Field diagnostics

Compile with `-DLEAP_LOG_SECURITY` to emit `LEAP-SEC[...]` lines on sequence
violations, PD ownership rejects, and stale frame age checks. Counters on
controller sequence state and PD controller stats mirror the same events for
telemetry without stderr spam in release builds.

### Wire smoke (local only)

GitHub Actions runners block veth/bridge setup (`Attribute failed policy validation`).
Unit tests cover replay, ownership, and session binding. Run wire smoke manually
on native Linux with `sudo` when validating AF_PACKET:

```bash
LEAP_BUILD_DIR=build tools/ci/wire_smoke_lo.sh
LEAP_BUILD_DIR=build tools/ci/wire_smoke_discover_lo.sh
```

## See also

- Normative process sequence rules: `docs/LEAP_PROTOCOL_SPECIFICATION.md` §13.4–§13.6
- Controller stack plan: `docs/LEAP_CONTROLLER_STACK_PLAN.md`
