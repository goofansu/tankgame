# Agent Instructions

You are building a tank game following the specification in `spec/`.

## Build System

Use the top-level Makefile:
- `make build` - build the project
- `make run` - build and run
- `make clean` - clean build
- `make debug` / `make release` - build variants
- `make test` - run all tests
- `make format` - format code with clang-format
- `make check` - check formatting and run linter

## Before Starting Work

1. **Always read `spec/progress.md` first** to understand current status
2. Review the execution plan in `spec/00-execution-plan.md`
3. Check which milestone to work on next

## Working on the Project

- Follow the spec documents in order (00 through 05)
- Complete milestones sequentially as defined in the execution plan
- Validate each milestone before moving on

## After Completing Work

Run these commands before finishing:
```bash
make build    # Ensure it compiles
make test     # Run tests
make format   # Format code
make check    # Verify formatting and linting
```

## Pacing and Natural Stop Points

**Important:** Do NOT blindly continue through the entire execution plan in one session.

- **Stop after completing a phase** (e.g., all of Phase 1) to let the user review and commit
- **Stop after large milestones** that introduce new subsystems (e.g., renderer, physics)
- **Stop when work-in-progress** - if a milestone is partially done, note what's left in progress.md
- The user needs opportunities to review code, test manually, and commit changes
- When stopping, summarize what was completed and what the next steps would be

## Progress Tracking

Update `spec/progress.md` as you work:

- Keep entries **very short** (one line per update)
- Format: `- [x] M0.1: done` or `- [ ] M0.2: in progress`
- Only add new lines when starting or completing a milestone
- Don't duplicate entries

Example progress.md:
```
# Progress

- [x] M0.1: repo structure
- [x] M0.2: Sokol app/gfx integrated  
- [ ] M0.3: working on OpenGL context
```

## Key Specs

- `spec/00-execution-plan.md` - milestones and order
- `spec/01-game-design.md` - game mechanics
- `spec/02-engine-foundation.md` - engine architecture
- `spec/03-gameplay-tech.md` - gameplay systems
- `spec/04-network-protocol.md` - networking
- `spec/05-tooling-and-debugging.md` - dev tools

## Reference Docs

- `docs/map-format.md` - map file format (v2) with height+terrain cells, tags, lighting

## Map Editing

**Use the map tool** at `tools/map_tool.py` to create and manipulate map files programmatically.

Run `./tools/map_tool.py --help` for full API documentation and examples.

**Always use map_tool.py** over manual text editing - it handles serialization correctly and avoids formatting errors.

## Visual Validation

**Always validate visual changes with screenshots.** Don't assume rendering code works - verify it.

Place temporary debug artifacts in `debug-temp/` (gitignored).

### Quick Screenshot (Inline Script)

```bash
./build/tankgame --debug-script "frames 3; screenshot debug-temp/test.png; quit"
```

### Debug Script Files

For testing movement, firing, and gameplay mechanics, use debug scripts.
See `docs/debug-script.md` for full documentation.

```bash
./build/tankgame --debug-script-file path/to/script.dbgscript
```

Example script:
```
frames 5
screenshot debug-temp/start.png

input +down; input +right
frames 60
screenshot debug-temp/moved.png

input stop
aim 10.0 5.0; fire
frames 30
screenshot debug-temp/fired.png

dump debug-temp/state.txt
quit
```

### Live Debugging

While game is running, inject commands via the command pipe:
```bash
echo "screenshot debug-temp/live.png" > /tmp/tankgame_cmd
echo "aim 5.0 3.0; fire; frames 30; screenshot debug-temp/shot.png" > /tmp/tankgame_cmd
```

Commands use the same syntax as debug scripts (semicolons or newlines as separators).

View screenshots with the `read` tool - PNG files display as images.
