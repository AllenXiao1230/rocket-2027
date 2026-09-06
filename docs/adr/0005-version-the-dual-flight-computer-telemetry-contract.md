# Version the dual-flight-computer telemetry contract as v3

The dual-flight-computer system will use a new telemetry v3 contract rather than repurposing the existing v2 64-byte frame. v3 must carry Flight Computer identity, boot-session identity, recovery authority and recovery evidence; v2 remains available only for historical or compatibility reading. This prevents a dual-source safety model from being silently interpreted by the single-source v2 parser.
