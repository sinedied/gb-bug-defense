# Project Guidelines

## Shared Memory

The project maintains shared memory in `memory/`:

- `memory/decisions.md` — Architectural and design decisions
- `memory/conventions.md` — Established project conventions

### Reading
Before making architectural decisions or proposing changes, check existing decisions and conventions for prior context.

### Writing
When a new decision is made or convention established:
1. Read the current file
2. Append the new entry at the end
3. Do not modify or remove existing entries

**Decision format:**
```
### <Decision Title>
- **Date**: YYYY-MM-DD
- **Context**: What prompted this decision
- **Decision**: What was decided
- **Rationale**: Why this choice
- **Alternatives**: What else was considered
```

**Convention format:**
```
### <Convention Name>
<Clear description with example if helpful>
```
