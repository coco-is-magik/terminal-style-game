# Error and Diagnostic Catalog

## Authority and scope

This file is the canonical searchable registry for project diagnostic identifiers.
Normative handling rules are in `C_STYLE_AND_OWNERSHIP.md`.

The standard was adopted on 2026-07-31. It applies immediately to new and
behavior-changing code. Existing uncataloged diagnostics are a known legacy gap and
must be migrated incrementally when their modules are changed or under a bounded
remediation plan. An entry marked **Planned** defines a required future diagnostic;
it does not claim that current source emits it.

## Identifier and lifecycle

```text
TSG-<DOMAIN>-<CATEGORY>-<NNNN>
```

- Domains name the owning subsystem, such as `SCENE`, `ASSET`, `UI`, or `RENDER`.
- Categories are `INPUT`, `ENV`, or `BUG`. Expected `STATUS` results are documented
  in their domain API and do not receive error IDs unless a separate non-error
  telemetry convention is approved later.
- Numbers are four digits, unique within domain and category, and never reused.
- One ID has one exact detection condition and one canonical detection site.
- Context changes details of one occurrence, not the ID's meaning.
- Retired entries remain present with their replacement or reason.

## Required entry fields

Each implemented entry must record:

| Field | Requirement |
|---|---|
| ID and status | Stable ID; Planned, Active, or Retired |
| Category/severity | `INPUT`, `ENV`, or `BUG`; user-visible severity |
| Owner/site | Owning module and canonical source symbol |
| Detection | One exact condition |
| Context | Bounded fields attached to an occurrence |
| Meaning/action | User/operator meaning and repair or retry action |
| Recovery | Fallback, termination, or repair behavior |
| Preserved state | Exact transaction and ownership guarantees |
| Verification | Focused test proving ID and guarantees |

## R1/R2 scene diagnostics reserved by the accepted plan

All entries below are **Planned**. Canonical source symbols and exact tests must be
filled in when R2 implements the owning stage. R2 may split an entry if one proposed
case would otherwise have multiple detection meanings; it may not reuse an ID for a
different meaning.

### Scene input

| ID | Exact planned case | Recovery and preserved state |
|---|---|---|
| `TSG-SCENE-INPUT-0001` | Native scene syntax cannot be parsed at a specific line/field | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0002` | Required native scene property is absent | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0003` | A single-valued property is duplicated | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0004` | Current-version scene contains an unknown property or section | Reject rather than discard; preserve live document |
| `TSG-SCENE-INPUT-0005` | Scene type discriminator or version is unsupported/newer | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0006` | Dimensions/origin violate bounds or checked-size rules | Reject candidate before allocation; preserve live document |
| `TSG-SCENE-INPUT-0007` | Numeric value is non-finite, out of range, or not fully consumed | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0008` | Instance ID is zero or duplicated | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0009` | `next_instance_id` is zero, exhausted, or not above all persisted IDs | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0010` | Reference to another scene instance is dangling or wrong-kind | Reject candidate; preserve live document |
| `TSG-SCENE-INPUT-0011` | Reusable asset reference cannot be resolved | Commit repairable candidate with visible fallback; preserve authored reference; block normal Save |
| `TSG-SCENE-INPUT-0012` | Normal Save requested while unresolved asset diagnostics remain | Refuse Save; preserve destination and dirty/repair state |
| `TSG-SCENE-INPUT-0013` | Legacy map import is malformed or exceeds accepted legacy limits | Reject imported candidate; preserve legacy source and live document |
| `TSG-SCENE-INPUT-0014` | Height-aware schema has invalid clearance or floor/ceiling relation | Reject candidate; preserve live document; reserved for the schema that introduces heights |

### Scene environment/resource failures

| ID | Exact planned case | Recovery and preserved state |
|---|---|---|
| `TSG-SCENE-ENV-0001` | Allocation for parse, migration, validation, or candidate construction fails | Abort transaction; release candidate; preserve live document |
| `TSG-SCENE-ENV-0002` | Scene or legacy source cannot be opened or read completely | Abort load/import; preserve live document |
| `TSG-SCENE-ENV-0003` | Same-directory temporary save file cannot be created | Abort Save; preserve destination and dirty state |
| `TSG-SCENE-ENV-0004` | Serialization write, flush, sync if promised, or close fails | Remove/retain temp per documented recovery; preserve destination and dirty state |
| `TSG-SCENE-ENV-0005` | Atomic destination replacement fails | Preserve old destination and dirty state; report temp-file recovery action |
| `TSG-SCENE-ENV-0006` | Instance ID namespace is exhausted during creation | Reject command without mutation; preserve `next_instance_id` and history |

### Scene internal invariant failures

| ID | Exact planned case | Recovery and preserved state |
|---|---|---|
| `TSG-SCENE-BUG-0001` | Load commits a candidate before every required validation stage succeeds | Loud failure; never classify as malformed input |
| `TSG-SCENE-BUG-0002` | Failed load changes the live document or document-dependent state | Loud transaction-invariant failure |
| `TSG-SCENE-BUG-0003` | Failed Save changes destination identity, clean marker, or prior destination | Loud transaction-invariant failure |
| `TSG-SCENE-BUG-0004` | Runtime adapter mutates authoritative authored scene state | Loud ownership-invariant failure |
| `TSG-SCENE-BUG-0005` | A retired ID is reassigned to a different logical instance | Loud identity-invariant failure |

## Entry template

```markdown
### TSG-DOMAIN-CATEGORY-NNNN — Short exact case

- **Status:** Active | Planned | Retired
- **Category/severity:** INPUT | ENV | BUG / severity
- **Owner/site:** `module` / `symbol`
- **Detection:** exact condition
- **Context:** bounded emitted fields
- **Meaning:** what occurred
- **Recovery:** reject, retry, fallback, repair, or terminate
- **Preserved state:** exact guarantees
- **Action:** user/operator/engineer response
- **Verification:** focused test name and asserted behavior
- **Introduced:** YYYY-MM-DD
- **Retired/replaced by:** if applicable
```