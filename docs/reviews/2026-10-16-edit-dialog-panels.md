# Edit-mode dialog panels — 2026-10-16

Reference: `UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md` §1.1, §4.2, §4.4,
§4.7 and its 2026-10-13 edit-mode panel direction. Owner feedback: the exit,
save and other dialogs with old `>` selectors must match the existing floating
menus. Game Flow and Authored Menu UI need a separate interaction redesign,
not a cosmetic conversion; no workspace redesign is implemented here.

The editor's exit, dirty-open, save-name, save-overwrite, material collision and
overwrite, reload, and entity-remove dialogs now draw on a content-fitted black
panel with the shared `living_shore` material. Exit, dirty-open and save-overwrite
choices use bright text and the moving focus perimeter rather than `>`. Single
action dialogs keep their original key instructions. The dialog is drawn above
its inspector, keeps the world visible, and is bounded by the reported visible
UI-scaled region. The underlying editor input, choice order, save semantics,
and persisted data are unchanged; Reduced Motion leaves the frame still.

Manual review: exit on a dirty scene; Ctrl+S name entry and existing-file
overwrite; open/new with unsaved changes; reload, material collision and
overwrite; light/decal/sprite/trigger/object removal. Check the menu over an
inspector, prompt text and keys, selected option, 150%/200% UI scale and
Reduced Motion. Automated rendering tests do not establish on-screen acceptance.