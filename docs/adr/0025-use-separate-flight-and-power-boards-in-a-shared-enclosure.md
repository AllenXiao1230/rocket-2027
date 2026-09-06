# Use separate Flight Computer and Power Boards in a shared enclosure

The flight build will package Main and Backup as separate Flight Computer PCBs, each paired with its own power board, within one sealed avionics enclosure. This preserves the chosen electrical and recovery-path independence while accepting the enclosure, mechanical, thermal, ingress and environmental-load common risks as explicit qualification targets rather than claiming complete physical independence.

## Consequences

The 2027 flight evidence must exercise both controller paths in the final shared enclosure under representative environmental loads. A common power board, common recovery rail or single Flight Computer PCB is excluded.
