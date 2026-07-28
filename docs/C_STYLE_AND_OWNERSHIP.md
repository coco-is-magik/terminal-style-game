# C Style and Ownership Standard

- Compile as C11 with `-Wall -Wextra -Wpedantic -Werror`.
- Prefix public symbols with their module name; keep helpers `static`.
- Use `const` for borrowed read-only inputs. State ownership and transfer in public
  headers when a pointer can outlive a call.
- Use typed result enums when callers must distinguish rejection causes; use `bool`
  for one binary outcome and pointers only when `NULL` unambiguously means failure.
- Define null-input behavior at public boundaries and make destruction/clear APIs
  tolerant of partial initialization where practical.
- Check signed dimensions before conversion, then use checked `size_t` multiplication
  for counts and bytes. Parse numbers with `strtol`/`strtod`, checking range, full
  consumption, and finiteness; do not use permissive numeric conversion.
- Keep public headers narrow. Prefer accessors over exposing mutable storage and
  avoid circular includes.
- Comments explain contracts, ownership, failure behavior, and non-obvious math;
  they should not narrate obvious statements or retain speculative future work.
- Wrap long signatures consistently and declare pointers next to the variable name.
- Tests use deterministic setup/teardown, native temporary-file operations, focused
  module linkage, boundary/failure cases, and exact cleanup assertions.