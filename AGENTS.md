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
- [x] M0.2: SDL2 integrated  
- [ ] M0.3: working on OpenGL context
```

## Key Specs

- `spec/00-execution-plan.md` - milestones and order
- `spec/01-game-design.md` - game mechanics
- `spec/02-engine-foundation.md` - engine architecture
- `spec/03-gameplay-tech.md` - gameplay systems
- `spec/04-network-protocol.md` - networking
- `spec/05-tooling-and-debugging.md` - dev tools

## Visual Validation

**Always validate visual changes with screenshots.** Don't assume rendering code works - verify it.

Quick screenshot (runs 3 frames then exits):
```bash
./build/tankgame --screenshot screenshots/test.png --screenshot-frames 3
```

Live debugging (while game is running):
```bash
echo "screenshot screenshots/debug.png" > /tmp/tankgame_cmd
echo "quit" > /tmp/tankgame_cmd
```

View screenshots with the `read` tool - PNG files display as images.

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd sync
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
