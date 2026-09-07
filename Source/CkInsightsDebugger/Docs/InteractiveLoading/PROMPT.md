# Interactive trace loading

Goal: make emerging frame bars useful while a large trace is parsing: users can select one frame and inspect its provisional hot paths, categories, and top timers.

Milestone 1 is authorized. Range analysis while loading is deferred. Preserve completed-trace analysis behavior, show partial-data status, discard superseded selection results, and automatically refresh the selected frame when parsing completes. Keep provider reads protected and bounded; do tree computation off the UI thread. Work in the existing checkout and preserve unrelated changes.
