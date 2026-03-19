# Function documentation conventions

Each file documents **one** Unity native function (`libunity.so`).

## File naming

`context/<Module>/<function_name>.md`

Examples:
- `context/Transform/get_position.md`
- `context/Camera/WorldToScreenPoint.md`

## Template

```markdown
# <Module>::<function>

**Signature:** `<return_type> <function>(<Module>* self [, args...])`

## Pseudocode (IDA)

\`\`\`cpp
// paste IDA Pro pseudocode here
\`\`\`

## Reconstructed signature

\`\`\`cpp
// clean C++ signature using types from include/unity.h
\`\`\`

## Notes

- Observations about registers, patterns, inlining, etc.
- If the function is inlined elsewhere, mention it.
```

## Rules

- Always use types from `include/unity.h` in reconstructed signatures
- If the function calls other internal functions, reference them as `[see Camera::get_main]`
