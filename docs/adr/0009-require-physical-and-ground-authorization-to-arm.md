# Require physical and Ground Station authorization to arm

The dual-flight-computer system will enter `ARMED` only when an independent physical Safe/Arm condition and a verified Ground Station ARM request are both present. Either condition missing keeps recovery inhibited; the authorization is an arming precondition, not a direct recovery-driver control signal.
