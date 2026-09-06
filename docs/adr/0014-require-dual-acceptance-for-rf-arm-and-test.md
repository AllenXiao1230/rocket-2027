# Require dual acceptance for RF ARM and Test

An RF ARM or Ground Test command changes state only when Main and Backup independently validate the same session, nonce and command within a bounded window, with their own preflight and interlock conditions passing. Any rejection or timeout leaves the system inhibited.
