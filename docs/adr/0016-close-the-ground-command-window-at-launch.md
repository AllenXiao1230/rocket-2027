# Close the Ground command window at launch

ARM and Ground Test commands are accepted only in `SAFE`, `READY` or a valid Ground Test Session. Once `LAUNCH_CONFIRMED` is latched, all ARM, Test and disarm commands are rejected so in-flight recovery is governed solely by local flight/recovery logic, Recovery Authority and hardware interlocks.
