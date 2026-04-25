# Chartchotic Changelog Tone Guide

Voice and style reference for changelogs and release notes.

## Voice

Indie dev, not corporate. You built this thing and you're telling people what's new. Direct, technically confident, no hedging. Short sentences over compound ones.

Not marketing copy. No "we're excited to announce" or "enhanced user experience". Just say what changed and why it matters.

## Punctuation

**No em dashes.** Use periods to break sentences, colons to introduce lists, or parentheticals for asides. Em dashes look tryhard in changelogs.

## What to include

- **User-facing changes**: what they see, what they feel. "No more UI freeze during resize" not "async background thread rebake".
- **Specific values**: note speed range, shortcut keys, fps caps. Concrete details build trust.
- **Game references**: CH/YARG/IIDX/SDVX/DDR. The audience knows these games. Use the real names.
- **Contributors**: credit by name when applicable (e.g. "textures courtesy of kanaizo").

## What to skip

- **Implementation details**: cache strategies, data structures, threading models. If a user wouldn't notice it, it doesn't get a bullet.
- **Minor polish**: graceful handling of edge cases, helpful error messages. These can merge into a parent bullet or get cut entirely. They're good engineering but boring changelog entries.
- **Source/build changes**: "source tree reorganization" is invisible to users. Skip it.

## Detail calibration

Big features get multiple bullets. Small fixes get one line. Polish items merge or get cut.

A release intro paragraph is warranted for major releases. One or two sentences framing what's new. Not every release needs one.

## Section naming

Name sections after features, not categories. "Multi-Highway" not "New Features". "Bemani Mode" not "Visual Modes".

- **"Bug Fixes"** is fine. Everyone knows what it means.
- **"Quality of Life"** for the miscellaneous improvements that don't fit elsewhere.
- **"Other"** is banned. It says nothing. Find a better name or fold the items into existing sections.
- **"Notes"** for caveats, compatibility info, known limitations.

## Personality

A little energy for genuinely cool stuff. Not every bullet, just the headline features. The Japanese text for Bemani mode (ビーマニ)? Keep it.

Don't force it. The default tone is matter-of-fact. Excitement is the exception, not the rule.

## Examples

### Bad > Good

Implementation detail that means nothing to users:
> Batched note data extraction. Resolve once per instrument instead of per-highway

Cut it. Users can't feel this.

Dev-speak in bug fixes:
> Fix drum highway misalignment in perspective mode (track cache overflow mismatch)

Drop the parenthetical:
> Fix drum highway misalignment in perspective mode

Feature bullet that's really just polish:
> Duplicate track names are handled gracefully (first match wins per instrument type)

Too minor for its own line. Fold into the parent feature description or cut.

Jargon reframed as user experience:
> Async background thread rebake. No UI freeze during resize or perspective changes

Flip the framing:
> Resize and perspective changes no longer freeze the UI

Weak section name:
> ### Other

Say what it actually is:
> ### Quality of Life
