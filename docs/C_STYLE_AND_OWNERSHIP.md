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

## Diagnostics and abnormal outcomes

These rules apply immediately to all new or behavior-changing code. Existing code
that does not yet comply must be migrated when its owning module is next changed,
or by a separately scoped remediation; do not conceal the legacy gap.

- Never silently ignore an abnormal outcome. Continuing with a fallback, entering
  repair mode, retrying, skipping malformed data, preserving previous state, or
  terminating an operation must remain observable through a structured diagnostic.
- Log a diagnostic exactly once, at the highest boundary that understands the
  consequence and owns the response. Lower layers return structured diagnostics;
  they do not also print them. A process-fatal boundary may log immediately when no
  caller can act.
- Every diagnostic uses one permanent identifier of the form
  `TSG-<DOMAIN>-<CATEGORY>-<NNNN>`. One identifier names one exact detection case
  and one canonical source site. Context fields never change its meaning.
- Every emitted identifier must have exactly one canonical entry in
  `ERROR_CATALOG.md`. Retired identifiers remain reserved and documented.
- Classify outcomes explicitly:
  - `INPUT`: malformed, unsupported, or inconsistent external/user-authored data;
  - `ENV`: uncontrollable allocation, filesystem, device, backend, or capacity
    failure despite valid program behavior;
  - `BUG`: violated internal invariant or impossible state caused by the program;
  - `STATUS`: expected cancellation, no-change, optional absence, end-of-list, or
    other ordinary control flow. A status is not an error and is not error-logged.
- A `BUG` diagnostic is never acceptable normal behavior or a recoverable user
  mistake. Preserve state where possible, fail loudly at the owning boundary, and
  fix the defect. Do not relabel a bug as input or environment failure.
- Domain APIs keep narrow typed result enums. An abnormal result also returns or
  fills a bounded diagnostic record. Do not use global mutable last-error state,
  heap-allocated formatted error strings, or an untyped context dictionary.
- Diagnostics include bounded typed context needed to act: for example path, line,
  field, object ID, supplied value, and required range. Escape or bound untrusted
  text and do not expose secrets or raw uncontrolled buffers.
- Recovery behavior and state guarantees are part of the error contract. State
  whether the live document, destination, ownership, dirty state, and unresolved
  authored references remain unchanged.
- Tests assert the exact diagnostic identifier, result category, state guarantee,
  and log cardinality. Expected statuses must be tested to produce no error log.

Source-location metadata may be captured for debugging, but it does not replace the
stable identifier or catalog entry. Moving a detection site does not change the ID
when its exact semantics remain unchanged.