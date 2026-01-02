# Agent Instructions

You are building a tank game following the specification in `spec/`.

## Build System

Use the top-level Makefile:
- `make build` - build the project
- `make run` - build and run
- `make clean` - clean build
- `make debug` / `make release` - build variants

## Before Starting Work

1. **Always read `spec/progress.md` first** to understand current status
2. Review the execution plan in `spec/00-execution-plan.md`
3. Check which milestone to work on next

## Working on the Project

- Follow the spec documents in order (00 through 05)
- Complete milestones sequentially as defined in the execution plan
- Validate each milestone before moving on

## After Completing Work

Run `make format` and `make check` before finishing a step to ensure code is formatted and passes linting.

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
