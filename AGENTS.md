# Repository boundary

The sole development repository for Drone Security is https://github.com/WangYu0611/drone_security.git.
On this workstation use D:\CodexPublish\drone_security on main. Historical UE5DroneControl worktrees are reference only.

Before edits check status, branch, origin, fetch origin, and inspect HEAD versus origin/main. The accepted baseline is 0fcbb6b64e51bc850a1184e543acfe4daeb91867 or a legitimate descendant.
Do not create feature branches or worktrees for this project unless the user explicitly changes this rule. Never reset or force-push to resolve divergence. Stop Git operations and report unexplained divergence.

Keep phases in separate commits. Push only completed, validated changes with a normal push after checking for remote changes. Keep sanitized public content sanitized. Do not copy credentials or private history from reference worktrees.

Security Plan route geometry uses ADronePathActor / DronePathVisual / M_CommandPathV2. Maintain a single displayed owner per Mission (editor, saved presentation, move preview, or execution snapshot). Backend remains authoritative. Camera Lock behavior is frozen. Native UI evidence and component/automation evidence must be reported separately.
