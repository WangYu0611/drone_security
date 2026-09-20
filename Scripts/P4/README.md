# P4 scripts

`generate_localization.py` is the repeatable localization source/PO generator. Run from the worktree root with `python -X utf8`; then run Unreal GatherText using the quoted `-config=Config/Localization/DroneOps.ini` argument. `localization.json` contains stable keys and source/translation pairs. `source_aliases.json` maps exact historical UI labels to those keys; it never parses Backend diagnostic messages.

The remaining Python scripts are retained one-time implementation migration records. They are not an installation or maintenance entry point and must not be rerun on the completed tree.
