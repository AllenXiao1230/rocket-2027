# Record Local Inhibit Readback in each v3 snapshot

Main and Backup will independently read their local recovery-energy inhibit state and include that evidence in their respective v3 snapshots. Neither controller may treat the peer's reported inhibit state as proof of its own safety condition.

## Consequences

Ground Station can compare the two sources without becoming an authority path. The pre-arm disposition of a missing or contradictory Local Inhibit Readback remains a separate decision.
