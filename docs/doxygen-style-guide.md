# Style Guide (Doxygen)

This file defines the concise, idiomatic in-source documentation style for CRUMBS.

## Key rules

- Public API comments (headers and exported symbols) use Doxygen-style block comments (`/** ... */`) with a short @brief and tags for @param and @return when useful.
- Keep @brief to 1 short sentence (max ~120 characters). If more explanation is required, use one short additional sentence in the block.
- Avoid repeating the function name or obvious implementation details. Focus on what the function does, its inputs/outputs, and failure conditions.
- Internal helpers (static functions) get small inline comments that explain the reasoning/algorithmic intent ("why"), not literal line-by-line descriptions of "how".

## Tagging conventions

- Use @param name description for parameters. Use the parameter name exactly as in the signature.
- Use @return description for return values. Mention success/failure codes if non-obvious.
- When a pointer may be NULL, document that explicitly (e.g., "May be NULL").

## Notes

- Keep comments short and actionable. The primary audience is library users and maintainers.
- Keep generated files untouched.
