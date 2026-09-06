# Start with Flight-Intent Hardware Revisions

The first hardware revision will be flight-intent rather than a separate bring-up architecture: it must contain the selected Main/Backup board separation, separate power boards, recovery safety boundaries and final-configuration observability. This trades faster isolated prototyping for earlier integrated evidence and fewer late architecture changes.

## Consequences

Board revisions remain permitted after evidence identifies a defect, but every replacement remains flight-intent and must re-establish the relevant qualification evidence in the final shared enclosure.
