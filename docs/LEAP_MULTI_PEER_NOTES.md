# LEAP Multi-Peer Notes

Reference-stack guidance for controlling multiple devices on one Ethernet
segment, and known gaps for true multi-controller coexistence.

## What the reference stack provides today

| Mechanism | Location | Purpose |
| --- | --- | --- |
| Per-peer `LeapControllerStack` slots | `leap_controller_session_hub` | Independent `session_id`, MGMT sequence, PD state, lease |
| Frame `sequence` tracking | `leap_controller_sequence` | Duplicate replay rejection, gap counting, optional window |
| Session binding | `leap_controller_stack` on OP entry | Drop inbound frames whose `session_id` ≠ owner session |
| PD endpoint validation | `leap_pd_profile_validate_*` | Profile ID, endpoint IDs, payload lengths from DIR map |
| Exchange reply validation | `leap_pd_validate_exchange_reply` | Endpoints + `latest_process_sequence_consumed` match |
| Linux recv demux | `leap_linux_pd.c` | `wait_exchange_reply` ignores frames from other peer MACs |
| I/O dirty flag | `LeapPdDeviceIoBinding.outputs_dirty` | Skip redundant output shadow writes |

## Configuration knobs

```c
LeapControllerStackConfig cfg;
cfg.frame_sequence.enforce_session_match = 1;  /* default when config NULL */
cfg.frame_sequence.window_size           = 64;
cfg.frame_sequence.reject_out_of_window  = 0;  /* set 1 for strict wire replay guard */

cfg.pd.validate_exchange_reply = 1;  /* default when PD config NULL */
```

## Remaining risks (not implemented in v1 reference)

### Multi-controller arbitration

LEAP v1 assumes **one active owner** per device (MGMT owner lease). Two controllers
on the same wire without coordination will race for `OPEN_SESSION`; the device
enforces single ownership — this is not a symmetric multi-master fieldbus.

Future work if needed:

- Controller priority / token in MGMT extension
- Explicit owner election or redundant-controller failover profile
- `active_owner_mac` inspection during discovery before bootstrap

### Stronger sequence + time validation

Implemented today:

- Ethernet `sequence` duplicate detection per peer (controller inbound)
- Device `process_sequence` enforcement (§13.6)
- Controller exchange-reply `process_sequence` check

Still weak / optional:

- Strict rejection of forward **frame** sequence gaps (gaps are counted, not
  rejected by default — cyclic integrity relies on `process_sequence`)
- Category A/B `max_frame_age_us` / timestamp checks on controller receive path
- Bitmap replay window across reconnect (sequence baseline reset rules)

### Performance at scale

The Linux example uses a simulated I/O shadow with an `outputs_dirty` flag.
Embedded ports should:

- Mark endpoint groups dirty only when payload bytes change
- Avoid scanning full endpoint tables when a profile has many endpoints but
  only one group updates per cycle

## Recommended bring-up flow (multi-peer)

1. `leap_controller_peer_table_discover()` — collect MACs
2. `leap_controller_session_hub_bootstrap_peer()` per target (or `_bootstrap_table()`)
3. `leap_controller_session_hub_run_round_robin()` for cyclic PD
4. `leap_controller_session_hub_on_frame()` from a recv thread for async MGMT
5. `leap_controller_session_hub_release_all()` on shutdown

## See also

- Normative process sequence rules: `docs/LEAP_PROTOCOL_SPECIFICATION.md` §13.5–§13.6
- Controller stack plan: `docs/LEAP_CONTROLLER_STACK_PLAN.md`
