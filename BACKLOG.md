# Backlog

Work from the top.

---

## Up Next

- **Text events on a track are invisible.** Disco flip and `[STRICT_HAT_PEDAL_STATE]` are parsed into `DiscoFlipState` and `StrictHatPedalState` and change how a chart reads, but nothing shows where they start and stop, so a wrong one is only findable by its effect. `TextEventRenderer` already has `populate` for flip regions and `populateEventMarkers`, and the utility strip already draws EVENTS sections, so this is joining up two things that exist rather than new machinery. Worth doing before any VENUE-shaped idea: the general feature is "read the text events on the track I am looking at", and VENUE would fall out of it for free. VENUE itself is not worth targeting directly (it is mostly notes 96-110, not text, and the note set differs between RB1/RB2/RBN1 and RB3/RBN2), and BEAT is not worth reading at all since notes 12/13/14 restate the tempo map the gridlines already come from. Pairs with the undo-hash item below.
- **`PartMapping` unpin does not hand the part back to the guess.** `test_part_mapping.cpp:88` fails: after unpinning, `map.resolve(tracks)[0].sourceTrackIndex` is 0 where the test requires 5, so the part stays on the pinned track instead of falling back to the name guess. Pre-existing and unrelated to the EVENTS discovery fix (the suite is 212 passed / 1 failed both with and without it), but from the same commit, `beb406a`. The suite has been red since then.
- **Utility strip styling is a first pass.** Font sizes, footer height and the section label crowding rule were all eyeballed against one project. The pinned-label clearance in `sectionHeaderBounds` and the claim order in `labelPlates` both want a second look on charts with dense sections.
- **Strip edit mode persists, highway write mode does not.** `stripEditMode` is saved and restored in `loadState()`, so a project can reopen already armed for section editing, where clicking the strip no longer transports and only the green mode button says why. Write mode has no state property at all and always starts off. Check whether the saved mode actually feels right in use; if not, drop the restore in `loadState()` and let it start at None like write mode does.
- **Strip chrome does not match the plugin's visual language.** The mode row and layer toggles are small square outlined buttons with their own green and blue palette, while every other control in the app is a rounded segmented button filled with `Theme::purple` when active and dim-outlined when not (see `UI/Controls/SegmentedButtons.h` and `PillToggle.h`). The section rows also paint a full-width translucent band over exactly the area the note map needs. One attempt at restyling this was rejected as worse than the original, so treat it as a design problem to settle visually before writing more paint code, not as a palette swap.
- **Undo of local text events on instrument tracks goes unnoticed.** `ReaperTrackNoteProvider::getTrackHash` passes `notesonly = true` for every track except EVENTS. Disco flip and `[STRICT_HAT_PEDAL_STATE]` are text events parsed into `DiscoFlipState` and `StrictHatPedalState`, so undoing one does not change the hash and `pollForChanges()` never re-reads the track, same bug sections had. Fix is to pass `false` for tracks whose text events are parsed, but that hashes full state at 20Hz on every instrument track, so measure the cost first.
- **Test coverage expansion** — See details below in Test Coverage section.
- **UI scale setting** — User-adjustable scale factor for toolbar, footer, and panel elements (not highway rendering). Lets users with high-DPI or small screens resize the chrome independently.
- **All-difficulty overlay mode** — Show all 4 difficulties squeezed into the same highway. Each lane gets up to 4 notes stacked when all difficulties hit them. Color-code by difficulty instead of track color. All note data except specific pitches is shared across difficulties in the MIDI spec, so the data model supports this naturally.
- **Elite drums — remaining features.** Audited against the full spec (`_refs/midi/elite-drums.md`) on 2026-09-09. Gems, colours, width, SP, hi-hat Open/Closed/Indifferent, roll lanes and 9-lane authoring are done. What is left, roughly in order of how wrong it is today:
  - ~~**Kick dynamics**~~ DONE. `TrackResolver.cpp:348` reads `cfg.dynamics && (isElite || !isKick)`, so elite kicks parse ghosts and accents. Art landed too: ghost and accent are the same bar at the same size as a normal kick, told apart by rim and shading the way an open note is told from an open HOPO (`AssetManager.cpp`, `GemArt::bakeBar`'s outline pass). Sizing them apart was tried and reverted, it read as three different notes rather than one played three ways.
  - **Stomp/Splash gems** — Pedal Down (72) is dual-purpose and only its modifier half is wired. Velocity 2-126 = Stomp, 127 = Splash, 1 = no gem but still terminates hi-hat sustains. Cols 10/11 have no MIDI source, so the existing `bar_stomp` art never renders outside the harness. Appendix B is a 20-row interaction table (Yellow x Pedal x Indifferent x `[STRICT_HAT_PEDAL_STATE]`) and none of it exists. Also needs the 108 Stomp/Splash roll lane, which `isEliteRollLane` (110-118) excludes.
  - **Hi-hat sustains** — do NOT exist in any form. `resolveSustains` is guitar-only (calls `getGuitarColumn`/`resolveGuitarGem`), so drums never emit `SustainType::SUSTAIN`, only `LANE`. Generated by Open Hi-Hat and Splash gems; default (notated < 1/8) looks ahead a dotted 1/4 for a Pedal Down or Closed/Open hat to terminate on, else 1/4 sustain + 1/8 fadeout; manual (notated >= 1/8) matches note length with the last 1/8 as fadeout. Runs of Open/Splash terminate-and-renew as one sustain. Anything <= 1/16 total is omitted. Spec's visual is "a faint yellow highlight on the highway matching the Stomp/Splash span and width, fadeout drawn as a literal fade to transparency". Not the white pedal bar, which is a different feature.
  - **Flam** (87/63/39/15) — parsing, authoring and visuals are all in. Parse: per-skill `shared.modifiers.flam`, `GemWrapper.flam`, leftmost hand gem on a chord, suppressed inside a roll lane, never on kicks, unit coverage in `test_track_resolver.cpp`. Authoring: `ModifierGroup::Flam` bool toggle on F, elite only, hand lanes write the marker and kicks place a stacked 1x + 2x. Visual: hand flams are two gems squished into one lane (temp, real art is a custom flam glyph eventually), kick flams reuse KICK mode's half-bar split. Remaining: no round-trip test that places a flam and re-parses it, and the optimistic patch buffer carries only a `Gem`, so a just-placed flam won't show until the track re-parses.
  - **`[STRICT_HAT_PEDAL_STATE]`** — feature-flag text event that stops a non-Indifferent Yellow from suppressing a coincident Stomp/Splash, enabling Closed+Stomp and Open+Splash. Affects only notes after it and cannot be turned off once set.
  - **`[snare_stem <lane> <diff?>]`** — moves the snare stem for Disco No-Flip downcharts. Only matters if we ever downchart.
  - **Roll/tremolo lanes** — DONE, including the four pitch collisions with guitar/drums modifier pitches (110/111/112 tom markers, 116 SP), fixed by testing elite branches first.
  - **New open-hat gem art** — the current `cym_hihat_open.png` (cone lifted over a separated disc) fits a single ghost/accent overlay poorly. Rendering works: open hats now route to their own `OVERLAY_DRUM_HIHAT_OPEN_GHOST/ACCENT` offsets (`-0.24`/`-0.28`, vs closed `-0.10`/`-0.14`) so the ring/chevron ride up to the lifted cone, but it's a compromise. Want purpose-drawn open-hat art (or a 2-piece ghost) so the overlay sits cleanly. Closed hat is fine.
  - **Elite authoring** — the 9 lanes are DONE (placement, erase, marquee, kick split, previews), routed through `AuthoringConfig`. Still missing: authoring hi-hat Open/Closed/Indifferent state (a click has to write the coincident 72/88 modifier, not just the gem), and authoring the Stomp/Splash pedal lanes once they parse.
  - **Solo (103) and Activation/BRE (120)** — both commented out in `MidiTypes.h` for every part, so this is not elite-specific. Belongs to the star-power authoring pass, since it affects all instruments at once.
  - **Elite bake-cache variant**: elite currently bypasses `TrackImageCache` (renders live). Add a third baked variant for perf parity.
  - ~~**Purple L-Crash gem art**~~ — DONE. `cymPurpleImage` is rebuilt at init from the blue cymbal's luminance through a purple lane ramp (`AssetManager.cpp`), so it matches its neighbours with no new art.
- **Manual track → instrument override menu** — click the track-name/number label (bottom-right) to open a view that explicitly maps a REAPER track to an instrument when auto-detect is wrong or ambiguous. Discovery already re-scans live (50ms poll); this is the manual escape hatch.
- **Greyscale gem masters + tint layer (custom color profiles)** — The real win is a *coloring* architecture, not a procedural-geometry one. Ship one greyscale master per gem family (the metallic luminance) and derive every lane colour by tinting it (grey/luminance → lane ramp), instead of shipping N pre-coloured PNGs. Then blue/red/yellow/green/purple are just tint profiles and user-editable **custom color profiles** fall out for free (ties into "Note color customization (CH color profile templates)" below). Proven seed already shipped: the purple cymbal is a luminance→ramp recolour of the blue cymbal (`AssetManager.cpp`). Next step: generalize that ramp into a per-lane/per-profile table, start with cymbals (one grey cymbal master → all lanes), then backport notes/hopos/bars. NOTE: this is NOT the PDF-interpreter / procedural-shape rabbit hole (dropped 2026-07-02 as scope creep) — geometry stays as-is, only coloring changes. The dormant `Gems/` bakers + `gem_compare` on `feature/elite-drums-render-spike` hold reusable tint primitives (`GemArtCommon`).
- **Authoring gaps for guitar and 4-lane drums.** Audited 2026-09-18. The tool layer is complete: nothing on the write bar is inert, and notes, sustains, dynamics, cymbal/tom, forces, lanes, all four phrase kinds, draw/edit submodes, marquee and REAPER-backed undo all work and write correct MIDI. Five correctness bugs found in the same pass are fixed (markers following a moved or erased note, 2x kick outside Expert, lane velocity by difficulty, bar-mode kick velocity). What is left is missing operations, in the order a charter would hit them:
  - **Disco flip cannot be authored at all.** `[mix n drums0d]` parses, renders and has force-field markers, but there is no write path: the only text events ever written are `ENABLE_CHART_DYNAMICS` and `ENHANCED_OPENS` (`ensureTrackFlag`). Any Pro Drums song with a disco section has to be hand-edited. Hard stop, so it goes first.
  - **No single-note copy and no Cmd+C/V.** The stamp is the only clipboard, and both capture paths silently require two or more notes (`WriteController.cpp:1292`, `InteractionController.cpp:230`), so copying one note does nothing with no feedback. `C` is a bare key; Cmd is deliberately unbound.
  - **Phrases cannot be moved.** Only the end drags: `handlePhraseDown` pins the start to the existing one, so shifting an SP phrase left means deleting and redrawing. Edit mode is no help, since phrase views route every click to the write controller and now force draw mode.
  - **Lanes can only grow.** `commitLane` always merges with what it runs into, with no shrink path like `PhraseWriter::resize`'s `replacing` argument. Shortening a roll means erasing and redrawing.
  - **Chord sustains drag one note at a time.** `RangeTarget::pitches` is a vector so a fill can carry all five markers, but `handleBeginSustain` only ever puts one pitch in it, so extending a sustained chord takes a drag per note.
  - Cosmetic: a forced strum previews as a plain gem (`resolveGhostGem` handles Hopo and Tap only), and `SegmentedButtons laneTypeButtons` is declared but never added or laid out, superseded by the LANE/TRILL pills.
- **MIDI spec coverage gaps.** Audited the `_refs/midi/` specs against what the renderer actually draws on 2026-09-18, elite excluded (tracked above). In order of how wrong each one is today:
  - **4-lane flam (109) is not parsed.** It is in the shared Drums marker table (`drums.md:87`, mechanics at :152), so it applies to 4-lane and Pro, not just elite. The whole flam render path already exists and is fed only by elite: `GemWrapper.flam`, the two-half draw, unit coverage. Roughly ten lines: uncomment `FLAM = 109` (`MidiTypes.h:53`), add it to `getDrumModifierPitches`, drop the `isElite &&` at `TrackResolver.cpp:391`. Cheapest real win here.
  - **`[ENHANCED_OPENS]` is not honoured.** `InstrumentMapper.h:29-34` maps 59/71/83/95 to column 0 unconditionally, but note-based opens are off by default because 59 is a left-hand animation note in GH2/RB charts (`5-fret-guitar.md:93`). Official RB charts sprout phantom Easy opens. This one draws notes that are not there, which is worse than missing a feature.
  - **5-lane drums is dropped entirely.** See its own item below.
  - **Drum sustains never form.** `resolveSustains` early-returns for anything non-guitar (`TrackResolver.cpp:448`), which also blocks elite hi-hat sustains.
  - **Computed but never drawn:** tempo and time-sig markers are rebuilt every frame and discarded (`FrameDataBuilder.cpp:166` vs `HighwayComponent.cpp:611`), and sections only render inside the Sections view.
  - **`[ENABLE_CHART_DYNAMICS]` is classified and never read.** Dynamics come from the UI toggle instead, so velocity 1/127 reads as ghost/accent on charts that never opted in, and is ignored on charts that did when the toggle is off.
  - **Smaller:** versus phrases 105/106 unparsed; 1-lane vs 2-lane rolls and trill vs tremolo share identical art; a roll starting on a chord picks an arbitrary chord member instead of following the next non-chord note (`drums.md:383`).
  - **Explicitly not doing for now:** SysEx open/tap phrases (0x01/0x04), and hi-hat control beyond the primary Open/Closed/Indifferent path. Neither is required to chart anything: the note-based markers are the going-forward form and already work. See the SysEx item below for what it costs on old charts.
  - **Not started at all:** 6-fret/GHL, pro guitar, pro bass, pro keys are recognised as unimplemented track names and refused. Vocals and harmonies are not even in that list, so discovery never sees them.
- **5-lane drums support.** Nothing works today: the green pitches are commented out (`MidiTypes.h:56-73`), `getDrumColumn` stops at column 4, `DrumType::FIVE_LANE` is never set, `getRenderType` never returns `FIVE_LANE_DRUMS`, and the drum glyph table has no column 5, so a 5-lane chart silently drops every green note. Spec is three drums plus two cymbals (`drums.md:12-14`), and the conversion table (:41-46) says which is which. Art mapping: note gems for snare and the toms (Red, Blue, Green), cymbal gems for the two cymbals (Yellow, Orange), purple kick bar by default.
- **Instrument picker icons: badge the kit, do not split it.** Drums gets one icon with a badge in the bottom right, the way the guitar icon carries its `R` for rhythm: `4` for 4-lane, `E` for elite. Pro and non-pro drums are the same thing and get no separate entry. Keep a `Pro` badge in reserve for when the pro instruments land.
- **Guitar Hero style art library.** Every gem we ship is Rock Band style. Add a GH-style set as a second library the user picks between, not a replacement. Look references: <https://p.kindpng.com/picc/s/331-3310902_guitar-hero-button-graphic-hd-png-download.png> and <https://www.kindpng.com/picc/m/331-3310974_guitar-hero-different-notes-hd-png-download.png>. Reference only, the art itself is Activision's. For the coverage a complete set needs, render `render_harness --dump-glyphs out.png`: it lays out every legal (part, column, gem) the asset manager can return, hi-hat states included. Pairs with the greyscale-master item above, since if colouring is already a tint table then a second library is a second geometry set rather than another N pre-coloured PNGs, and with presets it becomes part of a game profile.
- **GHL gem art: three glyphs, not six.** 6-fret tells frets apart by shape instead of lane colour, so the whole set is a white pick with a black outline (white row), a black pick with a white outline (black row), and a rounded square split black over white (both rows at once). Star power is those three again with a blue tint and glow. HOPOs are the same shapes with a yellow-gold outline in place of the black or white one, so the outline carries both which row a gem is on and whether it is strummed. Reference at `_refs/art-reference/ghl-gems-guitar-hero-live.jpg`, from <https://images.macrumors.com/t/fzMYUcQMP6YRJhutGTVJGNn-2_c=/1600x/article-new/2015/10/Guitar-Hero-Live-iOS.jpg>. Reference only, the art is Activision's, which is why it sits outside the repo. Also unblocks the parts menu badge: `getPartBadge` returns `6` for GHL because the pick glyph it wants is not in a system font.
- **Save/load presets + default state** — Persist user settings to disk. "Save as default" makes all new plugin instances start with custom settings. Presets for game-specific profiles (GH, RB, etc.). Foundation for everything downstream.
- **Section borders + event markers** — EVENTS track parsing, blue measure lines, section name overlay. Backend partially wired (`TimeBasedEventMarker`, `populateEventMarkers`). Tempo/timesig marker infrastructure landed (`d23505b`), rendering disabled. Enable for tempo/timesig, then extend for sections, lyrics.
- **Multi-highway: missing track types** — 6-fret variants still missing (RHYTHM GHL, COOP GHL, KEYS GHL). 5-fret COOP/RHYTHM now have separate Part enum values.
- **Ratio-agnostic window resizing** — Viewport-based resize landed (`c030e7d`), but "too much space below strikeline" tuning and REAPER fullscreen restore white gap / ~98% size bug still open.
- **SysEx marker support.** Read-only compatibility with old charts, not something we need to write. Nothing requires SysEx: note-based taps (104) and note-based opens behind `[ENHANCED_OPENS]` are the going-forward form and both already work, and the spec calls the Phase Shift SysEx format clunky, badly supported and unlikely to be extended (`Overview.md:116`). What we lose by skipping it is a Phase Shift-era chart whose taps and opens exist ONLY as SysEx: those render as plain strums and as whatever fret the note sits on. Deferred 2026-09-18. The open note marker turns ALL notes in its phrase into opens, not just greens. See [chart format spec](https://thenathannator.github.io/GuitarGame_ChartFormats/Chart-File-Formats/mid-format/Format-Overview/#phase-shift-sysex-event-specification).
- **Video export** — Render chart playback as video for YouTube previews. Multiple community requests. Could be a standalone tool or a feature in the standalone build. Separate project scope.
- **Unglued MIDI items show overlapping notes** — Reported by Easyfan: split/unglued MIDI items in REAPER render notes from multiple sections at once. Gluing fixes it. Unclear whether this is a bug in the plugin's MIDI fetching (reading across item boundaries) or expected REAPER behavior (unglued items expose all takes). Needs investigation.
- **Live MIDI edits not reflecting** — Reported by Galexio: HOPO changes not updating without removing/re-adding plugin. Could not reproduce. Unclear if this is a REAPER MIDI cache staleness issue, a plugin polling gap, or user error. May be fixed by 1.2's live track detection. Monitor for further reports.
- **Native framerate caps at 60Hz on multi-monitor Windows** — Reported by Monty: primary 144Hz + secondary 60Hz. Native FPS setting caps at 60 instead of 144. Regression from `f176e65` (Timer → VBlankAttachment). Root cause: JUCE 8's `VBlankAttachment` binds to the peer's monitor at creation via `MonitorFromWindow`; a 1-second `monitorUpdateTimer` re-checks but only when `parentToAddTo != nullptr`. When a plugin opens on the secondary display, it can get stuck on 60Hz even after moving to 144Hz. Not a quick fix — options are (a) revert to `juce::Timer` (loses display sync), (b) dual-path Timer+VBlank, (c) document as known limitation. Needs a Windows machine to verify any fix. Single report so far.
- **Non-REAPER DAW support broken** — Standard pipeline (ChartchoticStd) doesn't work in Ableton as of 1.2. Plugin loads but no functionality. Needs investigation into what the standard MIDI pipeline is actually missing without REAPER APIs.
- **Standalone mode** — Proper standalone build that loads a chart/MIDI file, displays the highway, and supports video export. Pairs with video export and standalone chart loading items.
- **Audio playback** — Metronome click, guitar note sounds, drum hit sounds for listening back to charts. Assets mostly ready. Needs audio output from plugin + sync.
- **Standalone chart loading** — Open a chart file/folder and play it back without REAPER. Debug standalone already has playback; needs file open UI, chart format parsing, tempo map from file. Pairs with audio playback.
- **Update banner: show changelog** — `releaseNotes` already fetched from GitHub release body (release channel). Need: display in overlay card (taller/scrollable), strip markdown to plaintext, also fetch for dev channel.
- ~~**Solo sections**~~ DONE. Blue wash over the board for a solo phrase, drawn by `SustainRenderer::drawPhrase`. Brightened on 2026-09-18 (`0x2F6BC8` at 0.34) because the old dark value vanished under a double kick run.
- ~~**Drum fills / BRE**~~ DONE. Fills draw as a bordered lane with a darkened interior, BRE as the same lanes filled flat, and the activation gem is recut in a vivid version of its own art. Gutters are computed per side so two fills sit as far apart as an outer fill sits from the highway edge.
- **Consider removing non-bar-mode kick interaction** — Currently kicks can be placed/erased by clicking left/right of the fretboard in normal mode. Could simplify to KICK mode only, removing the right-of-fretboard click zone. Tradeoff: less accessible but cleaner separation (normal mode = gems only, KICK mode = kicks only).
- **Shift-lock time position (snap-off chord placement)** — When snap is off and shift is held, first click places at raw cursor position, subsequent clicks lock to that same time position in whatever lane is clicked. Enables placing chords at exact same QN in freetime mode. Shift currently does nothing in snap-off mode.
- **Better mouse scrolling** — shift=faster, ctrl=precise.
- **Info display** — BPM, time sig, measure, beat position.
- **Gridline label granularity flickers while scrolling** — The label column flips between `117.2.75` and `117.3` as a BEAT crosses `farFadeEnd` and pops in and out of the candidate pool (`GridlineRenderer.cpp:166`). With that BEAT absent, MEASURE+BEAT leave only two labels, the greedy filter still has quota against `threshold = 1/WRITE_LABEL_TARGET_COUNT` (3.0), so it descends into HALF_BEAT/STEP and pads with a lone sub-beat. It scrolls in, displaces the sub-beat, and the granularity snaps back. Attempted fix (reverted 2026-09-09): stop descending once a priority class leaves >= 2 labels. Broke it worse, some scroll positions ended up with a MEASURE plus a single HALF_BEAT and no BEATs at all, because candidates are rejected for three different reasons that the guard conflated: culled past `farFadeEnd`, dropped by the LOD budget before they ever become candidates (`shouldRender`, `:138`), or present but inside `threshold`. Also unexplained: some scroll positions render ~8 labels when a 1/3 threshold should permit ~3, so the interaction between `MAX_HIGHWAY_GRIDLINES` budget filling and the label filter is not what it looks like. Fix needs both systems worked through together with real numbers, not another targeted guard. Same family as the ".1" suffix bug below.
- **Label ".1" suffix bug** — Measure labels show phantom ".1" (e.g., "433.1") when no sub-measure labels are visible. Root cause chain: (1) density filter accepts a sub-measure label that's far enough from adjacent measures, (2) `anySubMeasureKept` flag triggers ".1" on ALL measures, (3) the sub-measure label is occluded behind notes at `DrawOrder::GRID` or is too small/far to read. Two distinct manifestations observed: invisible HALF_BEAT label "444.4.5" passes density filter but renders behind notes; barely-visible BEAT "436.4" at highway far end just above `WRITE_MEASURE_LABEL_MIN_FONT_PX`. Attempted fix (reverted): generate all labels at full `M.B.S` precision ("433.1.0"), density filter, then strip unnecessary specificity by scanning `labelToRender` for content diversity. This works architecturally but doesn't solve the core problem — occluded/edge labels still exist in `labelToRender` and still drive the precision decision, causing the same flickering. Any check on `labelToRender` alone cannot fix this because the problematic label IS there, just not visible. Replacing `anySubMeasureKept` with a content diversity check is functionally equivalent for the common case. The real fix needs to either: (a) prevent invisible/occluded labels from entering the candidate pool (requires cross-layer knowledge of note positions), (b) require minimum count of diverse labels before bumping precision (one outlier shouldn't dictate formatting), (c) size-weight the precision decision (tiny far-highway labels get less vote), or (d) remove the ".1" feature entirely. Files: `GridlineRenderer.cpp` lines 200-232 (density filter + suffix), `PluginEditor.cpp` lines 125-134 (ghost cursor label has same formatting). See also `formatBeatPart` lambda and `WRITE_LABEL_TARGET_COUNT` in `DrawingConstants.h`.
- **Disco ball animation upgrade** — Replace static disco ball JPG with rotating disco ball (quarter-turn loop), "DISCO FLIP" text underneath in disco/rainbow colors. Fun polish.
- **Write-mode UI cleanup pass (post-authoring)** — Bundled visual + UX polish after M3 / M4 land, before `1.3.0` ship. By then all authoring visual elements (hover ghost, drag preview, selection highlight, erase indicator, marquee) exist and the cohesive look can be designed in one pass. Collected items:
  - **Mode chip stability**: floating icon currently shifts when sub-toolbar appears/disappears. Should hold a fixed position regardless of sub-toolbar visibility. Also conflicts with instrument/difficulty selection — need a placement that doesn't interfere.
  - **Bring back the explicit DRAW/EDIT label** — sub-toolbar wash is good but a literal label was clearer at a glance.
  - **Mouse cursor glyph** — currently doesn't read well, looks shifted left in its frame, not centered. Needs a better arrow shape or a different "edit" metaphor.
  - **Tuplet "off" coloring** — should be `textWhite` like division (it's a valid setting, not a disabled state). Currently dim, indistinguishable from snap-disabled fade.
  - **Snap as checkbox / toggle** — pill aesthetic implies clickability we don't have yet. Consider checkbox or pure label, possibly with a transient toast ("Snap Enabled" / "Snap Disabled") when key shortcuts fire to give keyboard-only users feedback.
  - **Note-icon notation for division/tuplet** — could replace "1/8" / "Tuplet: 3" text with proper musical notation glyphs (like MuseScore). Tradeoff: more polished but more rendering work.
  - **Sub-toolbar height jump between Draw and Edit** — toolbar height changes between modes; should not. Want zero reflow when toggling between draw/edit (only visibility changes).
  - **Toolbar width** — currently full-width strip; consider centered, only-as-wide-as-needed. TBD.
  - **Default gridlines in write mode** — when write mode is on, ONLY the user-chosen grid should render (plus measure anchors). Currently the existing MEASURE/BEAT/HALF_BEAT all keep rendering, masking the user's selected step+tuplet grid (visible bug: 1/4 + tuplet=3 shows the default 8th-note grid drowning out the actual triplet lines). This may bite M3 usability; revisit if so during M3.
  - **Division/tuplet broken below 1/8** — likely same root cause as above.
- **Dedicated STEP gridline asset** — Currently aliased to `marker_half_beat_png`, differentiated only by opacity (STEP 0.25 vs HALF_BEAT 0.35). Add a real `marker_step.png` if a thinner / more subtle visual is wanted; register in BinaryData + AssetManager. Don't reinvent rendering paths.
- **Procedural gridlines (draw calls, not PNGs)** — the gridline markers are single arcs; there's no reason to ship them as PNGs. Bake/draw them procedurally from the same curvature constant the highway + gems + the new procedural Stomp bar use (`NOTE_CURVATURE_DRUMS`), so every arc matches intrinsically. Pattern proven by `BarGemArt::bakeStompBar` (2026-07-04): flat trapezoidal shapes + a parabolic arc are trivial draw calls. Keep PNGs ONLY for the genuinely complex shapes that are hard to interpret into draw calls (cymbals, notes, hi-hats). Distinct from the dropped PDF-interpreter rabbit hole — this is simple arcs, not mesh-gradient reproduction.
- **Highway time-position column (glass panes)** — Optional column on the right side of the highway showing musical position per gridline (measure number, beat number, time). Like a row of glass panes alongside the chart. Mostly useful in write mode for orienting against REAPER's timeline. Probably needs perspective-aware text rendering (text scales with depth) which the renderer doesn't currently do — non-trivial.
- **Write-mode MEASURE/BEAT under bar gems** — All gridlines render at `DrawOrder::GRID` (below `BAR`) so the track rails clip the marker's overhang (it's `GRIDLINE_WIDTH_SCALE = 1.12` of the highway, deliberately wider than the highway). Tradeoff: bar gems (kicks/opens) cover the rhythmic structure where they cross. The "emit above BAR" attempt (`e744fb8`) violated the rails-clip invariant — line edges stuck past the rails, looked broken. Real fix would render the on-highway line twice: once at GRID for rails clipping, once at a higher order clipped to the highway rect (no overhang). Side protrusions stay at GRID either way. Deferred — not blocking.

### Done (1.2)

**Bemani mode:**
- ~~Flat scrolling viewport~~ — Sidebar rails, per-instrument tuning, data-driven debug panel, BemaniConfig refactor.
- ~~Texture scroll snap~~ — Removed fmod wrapping from scroll offset (both REAPER and standalone paths).
- ~~Disco flip force field markers~~ — Flat horizontal markers in Bemani mode.
- ~~Sidebar edge lines~~ — Sidebar rails via DrawCallMap at TRACK_SIDEBARS draw order.
- ~~Unified note speed~~ — Both modes share 2-20 range, Bemani applies internal 1.7x ratio.
- ~~Pre-release fixes~~ — Per-highway padding, highway length scales with slot height, force horizontal row in multi-highway.

**Multi-highway:**
- ~~Pool HighwayComponents + shared track image cache~~ — 4 pooled slots (never destroyed), TrackImageCache bakes guitar+drums once at full resolution. Difficulty/instrument toggles swap overlay pointers (~0ms).
- ~~Deferred gem type calculation (v2)~~ — Zero-stutter: NoteData stores velocity only, TrackResolver computes gems at render time. Toggle changes instant.
- ~~Batched data build~~ — Extract+resolve once per lock group instead of per-slot.

**Performance:**
- ~~Async track image rebake~~ — Background thread bake with generation counter. Stale cache stretches during drag/resize, snaps on completion.
- ~~Profiling infrastructure~~ — Per-frame TSV logger, per-highway phase timing, pipeline benchmark target. Profiler lifecycle encapsulated in DebugEditorController.

**Other:**
- ~~Disco flip (Pro Drums)~~ — Text event infrastructure + DiscoFlipState + red↔yellow swap. Force field markers at start/end positions.
- ~~Update overlay improvements~~ — ESC dismisses, resizes with window.
- ~~Lane/sustain draw order~~ — SUSTAIN before BAR (bars render on top).
- ~~Source tree reorg~~ — Extract Editor/ module from PluginEditor.

**Multi-highway:**
- ~~Track re-discovery~~ — `pollForChanges` re-runs discovery, detects added/removed/renamed tracks live.
- ~~GUITAR_COOP + RHYTHM parts~~ — Separate Part enum values, own highway slots, centralized track name constants.
- ~~Remove Global/Local toggle~~ — Always use global discovery in REAPER; non-REAPER uses manual fallback.
- ~~No-tracks-found message~~ — Shows supported track names when REAPER has no instrument tracks.
- ~~Dedup duplicate parts~~ — First match per instrument type wins in discovery.

**Bug fixes:**
- ~~Drum track cache overflow~~ — Per-instrument overflow in TrackImageCache instead of shared max.
- ~~REAPER mode latch~~ — Toolbar stays in REAPER mode when API flickers during track moves.
- ~~Editor re-attach~~ — Rebuild session state when editor is recreated (plugin moved between tracks).

**Closed (not needed):**
- ~~Resolve-per-track caching~~ — Already batched per lock group in FrameDataBuilder; `resolveAllDifficulties` runs once, each slot extracts its difficulty.
- ~~ReaperMidiPipeline slimming~~ — Anticipated duplication never materialized; functions only exist in pipeline.

---

## Tech Debt

Do between features or when touching related code.

- **Verify `[coda]` actually lands on EVENTS** — `ReaperMidiWriter::setCoda` reports success without confirming the event survived. `takeCovering` falls back to `takes.front()` when no EVENTS item spans the position (`ReaperMidiWriter.cpp:458`), and REAPER discards text events written past an item's end, so a coda at the end of a song can vanish silently. `extendEventsItemTo` was added to stretch the item first, but was never confirmed working against a real project. A BRE without a coda exports as a drum fill.
- **Three frame-build paths diverge** — `FrameDataBuilder` has `buildReaper`, `buildReaperBatched` and `buildStandard`. Only the first two go through `MidiInterpreter::resolveAllDifficulties`; `buildReaperBatched` calls `TrackResolver::extract`/`resolve` directly, so anything added to the interpreter silently does nothing on the multi-highway path. This has now caused two separate bugs (elite pitches, per the comment at `FrameDataBuilder.cpp:230`, and the phrase override). Collapse them or add a shared seam.
- **A new `SustainType` silently does nothing in the sites that list types by hand.** Adding one means finding every `if (t == A || t == B || ...)` that has to learn about it, and missing one is invisible: no compiler error, no failing test, the event just takes a wrong branch. `SustainType::ACTIVATION` was added to the enum, to `TrackResolver` and to `HighwayComponent::shownHere`, but missed the phrase dispatch at `SustainRenderer.cpp:53`, so it silently rendered as an ordinary sustain and three rounds of edits to `drawPhrase` were dead code. The same shape caused the `modifiers.bre` bug in `buildReaperBatched`. Fix: give `SustainType` one predicate (`isPhraseType(t)`) that every site calls, or a `switch` with no `default:` so a new value fails the build. Known hand-written lists: `SustainRenderer::drawSustain` dispatch, `HighwayComponent::shownHere`, `HighwayComponent::typeForKind`, `UtilityStripComponent::phraseColour`.
- **Gems jump a step when they cross a curve bake bucket.** Note gems visibly shift in size and height at a fixed point up the highway, around 80%, and one tick of travel is enough to trigger it. Bars are immune because they keep the full-board master and never take the lane-width bake path. Cause: `curveSizeBucket` snaps the bake to powers of two from a floor of 32 (`NoteRenderer.cpp:845-852`), but the bake pads by a constant `+ 2` pixels (`:923`), and both values the draw site reads are ratios against `srcH` — `contentFraction = srcH/destH` (`:968`) and `yOffsetFraction` (`:966`), which set `gemSprite.height` and `offsetY` (`:151-152`). `maxShift` scales with the bake so its share is scale-invariant, but the constant is 6.25% of a 32px bake and 3.1% of a 64px one, so the ratio steps at every bucket boundary and the gem jumps in both size and position. The `ceil` adds up to another pixel of the same error. **The proportional-padding fix has landed** (`extraPx = ceil(maxShift + srcH * CURVE_BAKE_PAD_FRACTION)`), and the bake now returns canvas-relative fractions, which are scale-invariant by construction, so both stepping terms should be gone. **Not confirmed.** A width sweep from 560 to 1080 across a bucket boundary could not resolve it: the gem is 29 to 53px there and measures in whole pixels, so a 3% step hides under 1px. Confirm by watching a gem travel in the plugin, or measure sub-pixel with an alpha-weighted second moment. If a residual still reads at the 32px floor, derive the fractions from exact float geometry and let the allocation carry a row of transparent slack.
- **Gridlines are emitted far past what can resolve.** A Metal spike modelling the real frame found ~4000 gridline+lane segments over a 720px-tall slot, more than five per pixel row, and on the GPU that layer was 68% of the frame. On the CPU today GRID + LANE is 6.8ms at 720p stress and 3.8ms at medium, second only to NOTE. Culling or coalescing sub-pixel-dense lines helps both backends and needs no renderer rework. Nobody has looked at where the count comes from yet.
- **`bench_pipeline` has not compiled since March.** The MIDI-pipeline benchmark is written against APIs that no longer exist: `NoteProcessor::processModifierNotes`/`processPlayableNotes`, `GemCalculator::getGuitarGemType`/`getDrumGemType`, `MidiInterpreter::generateTrackWindow`/`generateSustainWindow`, and the `MidiProcessor` constructor. The stale includes (`Utils/Utils.h` → `Utils/ChartTypes.h`, `TimeConverter.h` → `Midi/Utils/`) are fixed; the API rot is not. `test.sh bench` now builds only `bench_rendering`, the target it actually runs, so this no longer blocks the rendering benchmark. Either rewrite it against the current API or delete it, but do not leave a third uncompilable benchmark sitting in the tree.
- **Rendering has no test coverage, and "it compiles" reads as success.** Nothing in the 890-assertion suite draws anything, so a visual change can pass every check while rendering nothing at all. This actively misled a session into three rounds of blind edits. Cheapest fix with real value: extend `render_harness` to assert draw-call counts per `SustainType` for a fixture chart, so "this phrase type reached the renderer" is testable without comparing pixels.
- **Unenforced chart-format invariants** — The spec states rules the authoring path doesn't check. None block a write; a non-blocking warning list is the right shape. Highest value first: a BRE must be followed by at least one note (`_refs/midi/5-fret-guitar.md:114`); a drum fill needs an activation note at/near its end (`drums.md`); notes must still be charted under guitar/bass BREs for character animations (`5-fret-guitar.md:115`); lane markers are Expert-only unless velocity is 41-50, which extends them to Hard; versus phrases 105/106 must share start and end when paired. Full survey was done once and not kept — redo it from `_refs/midi/` if needed.

- **Per-instrument authoring config** — The write path re-asks "which instrument is this" at every site with ad-hoc predicates (`isDrums()`, `isElite()`, `isKickLane`, `isDrumKick`, hardcoded column 6 vs 9, `maxLane()`'s nested ternary). Adding elite authoring turned up seven such sites, and the one missed for an hour let a placed 2x kick swallow every click on its row. Replace with an authoring-side bundle keyed on `RenderType`, mirroring `PositionConstants::RenderTypeConfig` (`RenderTypeConfig.h`), which already did exactly this for the renderers: lane-to-pitch, `maxLane`, kick columns, the kick-conflict pair, `isCymbalLane`, modifier slots. Then a new instrument is one table entry instead of a hunt through `Editor/`. Do it after the elite drums work lands (hi-hat open/closed states, stomp/splash pedal lanes) so the branch points are all known rather than guessed.
- **Sustain lookback is a guess** — `kSustainLookbackQN` (`AuthoringTypes.h`) is 32 QN, 8 bars of 4/4, picked not derived. A sustain longer than that is invisible to `NoteEditor::resolveOverlapsAt` (predecessor never truncated, so notes overlap on one pitch) and to `classifyNotesInRect` (erase and marquee miss it). Blind to time signature too. Fixes: search back to the previous note on the pitch, or size the window from the longest sustain in the track. Both add a scan to the write-mode hot path, so measure before choosing.
- **Stretch + Bemani mode interaction** — Stretch-to-fill doesn't work when Bemani mode is active. `onBemaniModeChanged` doesn't account for stretch state, and `resized()` computes `sceneHeight` differently in Bemani mode regardless. Preexisting.
- **NoteStateStore wrapper** — `InstrumentSlot` bundles array+lock but doesn't enforce locking via API yet. Wrap into class with scoped-lock accessors. Eliminates race condition footgun.
- **Audio-thread hygiene** — Remove std::function, preallocated vectors. DrawCallMap done, remaining allocations TBD.
- **Minimize REAPER API calls** — Profiler/DrawCallMap work done, REAPER call frequency still unaudited.
- **Settings menu reorganization** — Single gear button, scrollable category list on the right, selected category expands a detail panel on the left (macOS System Settings style). Consolidate View/Chart and Settings panels into one unified menu. Centralize control labels and tooltip text into a single source of truth file.
- **Tooltips for all controls** — Extend InfoTooltip to every control that isn't immediately obvious. Centralize tooltip strings (header with static constexpr or similar) so they're easy to maintain and translate.
- **Auto-detect stale CMake build dirs** — `build.sh`, `build-reapertest.sh`, and `scripts/test.sh` should compare `CMAKE_HOME_DIRECTORY` from any existing `CMakeCache.txt` against the current script dir and auto-`rm -rf` the build tree on mismatch before configuring. Bit us twice in one session after moving the repo from `plugins/` to `charting/`.
- **`build-reapertest.sh` should force-quit REAPER** — The "Quitting REAPER..." step hangs when REAPER has unsaved-changes or end-of-session prompts open, forcing a manual Ctrl-C. Replace graceful quit with `kill -9` / `killall -9 REAPER` so the relaunch step always makes progress.
- **Move-drag HOPO cascade** — Move preview resolves auto-HOPO for the dragged note, but doesn't recalculate HOPO state of surrounding notes affected by the move. Full fix would require running TrackResolver on the preview state. Low priority — real pipeline recalculates on commit (~1 frame delay).

### Rendering — perspective limits (do not iterate further without a true-3D plan)

**True-3D pipeline rewrite (parking-lot / future major version).** The fake-3D trapezoid in `PositionMath::createPerspectiveGlyphRect` runs two independent 1/z curves (one for X/Y/width, one for sprite height). Any "above the bar plane" element therefore can't keep a constant gap-to-sprite ratio across depth without distorting aspect or breaking lane fit. Two attempted reworks (Apr 11–12 `9128b93`+`49f71a0` and the Apr 20–21 lift-mode/crosshair session) both hit the same wall and have been reverted. Full postmortem in `docs/Z_POSITIONING.md`. Experimental code on `experiment/z-position-rework`. **Do not attempt another scalar-tweak fix.** The right path is a real view-projection matrix.

The following items are subsumed by that rewrite and only worth fixing in isolation if they bite for some specific reason:
- `PositionMath.cpp:99` — `progress` goes negative past `vanishingPointDepth (1.0)` while `HIGHWAY_POS_END = 1.12` and `FAR_FADE_DEFAULT = 1.20` render past that. Slight extrapolation artifacts at the far end.
- `NoteRenderer.cpp` guitar gem branch — reads `sNear/sFar/w/h` from per-column adjusts but never adds `ca.z` to zOff (drum branch does). All guitar Z values are 0 anyway, so no visible impact.
- `SustainRenderer.cpp` — uses raw lane `centerY` with no Z nudge, so guitar notes (when `gemZ` is non-zero) are vertically disconnected from their sustain tails.
- `getFretboardEdge` / `getColumnPosition` accept `posStart`/`posEnd` but ignore them. Highway-end tuning doesn't reach perspective math.

---

## Renderer Styles (classic vs modern)

The concrete plan for the "real view-projection matrix" the perspective-limits note above keeps
pointing at. Three separable items. Do them in order, each is useful alone.

Reference implementation: [Geomitron/chart-preview](https://github.com/Geomitron/chart-preview)
(npm `chart-preview`, MIT, TypeScript), the THREE.js highway behind the enchor.us previews, by Geo
of Chorus Encore/Bridge. Confusingly it shares a name with our own pre-rename repo. Source is
~3.4k lines across 5 files, all the interesting parts in `src/ChartPreview.ts`.

### 1. True-projection sibling in `PositionMath`

Add an alternative to `createPerspectiveGlyphRect` that projects a world point through a real 4x4
instead of evaluating the bezier falloff. Same signature, so the renderers don't care which one
ran. This is the load-bearing change and the only genuinely new math.

The difference in one line: chart-preview puts note Y *linear in time*
(`interpolate(msTime, nowMs, nowMs + 1500, -1, 1)`) and lets the camera do all the foreshortening.
We bake the falloff into the rect. That's the whole disagreement.

Their entire scene, for calibration:
- Camera `PerspectiveCamera(fov 90, aspect 1, near 0.01, far 10)`, at `z = 0.8`, `y = -1.3`, `rotation.x = 60°`
- Highway `PlaneGeometry(w, 2)` at `y = -0.1`, `w` = 1 / 0.9 / 0.7 for 5-fret / drums / 6-fret, texture `repeat(1,2)` scrolled by `offset.y`
- Strikeline sprite at `y = -1`, scale `0.19` (`0.141` for 6-fret)
- Notes at `SCALE = 0.105`, kick `0.045`, 6-fret open `0.04`
- Sustains `PlaneGeometry(SCALE * 0.3, 2 * msLength / 1500)`, open notes `SCALE * 5`
- No fog, no distance fade. `depthTest = false` with hard `renderOrder` (highway 1, kick 1, sustain 2, strikeline 3, note 4), plus clipping planes at `y = -1` and `y = 0.9`

Why this is tractable in JUCE's 2D rasterizer, which cannot do perspective-correct texture mapping:
- Their notes are `THREE.Sprite`, camera-facing billboards, not flat planes. A billboard is an affine transform, so `drawImageTransformed` reproduces it exactly.
- Their sustains are flat quads but untextured solid colour (`MeshBasicMaterial({color})`). We already draw sustains and lanes as programmatic perspective-projected trapezoids (see `docs/SKINNING.md`, "Not Currently Skinnable").
- The highway plane is the only perspective-textured surface in the scene, and we already bake and scroll a track image in `TrackRenderer`/`TrackImageCache`.

### 2. Style axis, replacing the `bemaniMode` bool

`PositionMath::bemaniMode` is already an alternate style (flat, no foreshorten), so the precedent
exists. Problem: it's a global static bool re-checked in ~20 files (`NoteRenderer`,
`SustainRenderer`, `GridlineRenderer`, `TrackRenderer`, `HitTestMapper`, `FrameDataBuilder`,
`TrackResolver`, ...). Adding style #3 the same way is combinatorial.

If "pick a style" becomes a user-facing setting, that bool becomes a projection strategy resolved
once per frame and passed down, not a flag every renderer interrogates. That refactor is the real
cost of this feature, not the projection math. Classic / modern / bemani then become presets of
`PerspectiveParams` plus a choice of projector.

### 3. Asset question for Geo (blocking any pixel-exact match)

Do not scrape the textures. MIT covers the repo source, but the art isn't in the repo, it's fetched
from `https://static.enchor.us/` (`preview-highway.png`, `preview-5fret-strikeline.png`,
`preview-drums-red-tom-ghost.webp`, ...) with no base-URL config hook. That grant doesn't clearly
extend to CDN assets, some of it looks derived from Clone Hero's own non-redistributable art, and
hotlinking his CDN from a shipping plugin is his bandwidth plus magenta placeholder squares on any
outage. Just ask him.

Default position until then: implement the projection, keep our art. Pixel-exact only matters if
the goal is diffing our output against theirs, and a browser reference render is cheaper than
shipping foreign assets.

Also prices in badly: their note textures are looping animated WebP via the `ImageDecoder` API. We
ship static PNGs plus frame-sequence hit animations. Continuously animated note art is a new path
in `AssetManager` and collides with the master-extraction refactor.

### Not doing

Embedding `chart-preview` itself as a second renderer. That's a second product, not a second style:
a `juce::WebBrowserComponent` plus a JS bundle in the plugin UI, a per-frame marshal of the
note-state map from the audio thread into JS (their player consumes a `.sng` file and drives its own
clock, so that path doesn't exist), textures pinned to someone else's CDN, and every feature after
that ships twice. It also has no Elite or Real Drums note types.

### Adjacent

`Geomitron/scan-chart` is Encore's canonical chart parser/validator and the dependency
chart-preview parses through. Good cross-check for our note/modifier handling, and probably useful
sooner than any of the above.

---

## Test Coverage

Current: 58 tests covering GemCalculator (20), InstrumentMapper (15), GridlineGenerator (11), MidiInterpreter (10), smoke (2). All pure functions, no JUCE GUI or REAPER API needed.

### Critical (add before any HOPO/sustain/disco flip refactor)

**TrackResolver note resolution** (`Midi/Processing/TrackResolver.cpp:133-251`)
- Auto-HOPO threshold: test PPQ distances at exact boundary (e.g., 170 ticks = HOPO, 171 = strum). Each threshold option (1/16, Dot 1/16, 170 Tick, 1/8) needs a boundary test.
- Chord detection: 2+ notes at same PPQ forces strum regardless of distance. Test single note after chord (should allow HOPO), chord after single (should force strum).
- Disco flip cymbal swap: when `discoFlip=true` and `proDrums=true`, red cymbals become yellow and vice versa. Test all gem types through the swap (GEM::CYMBAL stays cymbal, column changes). Test that non-cymbal notes are unaffected. Test flip disabled = no swap.
- Pro drums cymbal detection: velocity 110+ on tom pitch = cymbal. Test boundary (109 = tom, 110 = cymbal). Test with proDrums disabled = all toms regardless of velocity.

**TrackResolver sustain resolution** (`TrackResolver.cpp:257-296`)
- Duration filtering: sustains shorter than `MIDI_MIN_SUSTAIN_LENGTH` should be dropped. Test at exact boundary.
- Gem type at sustain start: sustain inherits the gem type of its starting note (HOPO/TAP/STRUM). Test sustain starting on a forced HOPO note vs strum note.
- Star power state: sustain that starts outside SP but the note-on is inside SP should be marked SP. Test boundary where SP region starts/ends mid-sustain.
- Skill filtering: sustains only appear for the correct difficulty range. Test Expert sustain doesn't leak into Hard window.

**TrackResolver lane resolution** (`TrackResolver.cpp:302-357`)
- Skill filtering: Expert always sees lanes. Hard sees lanes only with velocity 41-50. Medium/Easy never see lanes. Test each.
- Lane column discovery: LANE_1 allows up to 2 columns, LANE_2 allows up to 1. Test with notes spanning multiple columns.

**DiscoFlipState text parsing** (`Midi/DiscoFlipState.h`)
- Valid events: `[mix 3 drums0d]` (flip on, Expert), `[mix 3 drums0]` (flip off). Test all 4 difficulties (0-3).
- `dnoflip` suffix: `[mix 3 drums0dnoflip]` ends a flip region. Test that it properly closes the region.
- Malformed rejection: `[mix]`, `[mix 3]`, `[mix 3 guitar0d]`, `[mix 99 drums0d]` should all be ignored.
- Unterminated flip: if flip-on never gets flip-off, region extends to PPQ 999999.0. Test this edge case.
- Multiple difficulties: flip on for Expert only should not affect Hard. Test per-difficulty isolation.
- Overlapping regions: flip-on, flip-on, flip-off. Second on should be ignored (already flipping).

### High (add before NoteProcessor or discovery changes)

**NoteProcessor boundary conditions** (`Midi/Processing/NoteProcessor.cpp`)
- Note-off placement: `max(start + PPQ(1), end - PPQ(1))`. Test zero-length note (start == end), 1-tick note, normal note. The formula prevents note-off from being before or at note-on.
- Pitch bounds: pitch >= `array.size()` should be silently skipped. Test with pitch 128 (out of range) and pitch 127 (max valid).
- Velocity passthrough: NoteData should store the original MIDI velocity. Test that velocity 0 = note-off, velocity 1-127 = note-on with correct value.
- Muted note filtering: notes with `muted=true` from MidiCache should be skipped entirely.

**matchTrackNameToPart** (`Midi/Discovery/TrackDiscovery.h`)
- Case insensitivity: "part guitar", "PART GUITAR", "Part Guitar" should all match.
- All implemented names: test every entry in `getImplementedTrackNames()` maps to the correct Part.
- All unimplemented names: test every entry in `getUnimplementedTrackNames()` maps correctly.
- Unknown names: "PART SOMETHING", "GUITAR", empty string should return false.
- Consistency: every name in getImplementedTrackNames/getUnimplementedTrackNames should successfully match via matchTrackNameToPart.

**pollForChanges structure detection** (`Midi/InstrumentSession.cpp`)
- Track added: start with 2 tracks, re-discover finds 3. Should return true, trackData resized.
- Track removed: start with 3 tracks, re-discover finds 2. Should return true.
- Track renamed: same count but different Part on one track. Should return true.
- Track reordered: same parts but different sourceTrackIndex. Should return true.
- No change: same tracks, same parts, same indices. Hash unchanged = return false.
- Note edit only: same structure, different hash on one track. Should return true, only that track refetched.
- All tracks removed: re-discover returns empty. Should return true.

### Low (add when touching related code)

**ControlConstants helpers** (`UI/ControlConstants.h`)
- getRenderType: test every Part enum value returns expected RenderType. Catches missing switch cases when new Parts are added.
- isGuitarLike/isDrumLike: test all Parts, verify consistency with getRenderType.
- isHighwayRenderable: test all Parts. VOCALS, PRO_KEYS etc should return false.
- getPartSortOrder: test that Guitar < Rhythm < Coop < Bass < Keys < Drums. No duplicates.
- getImplementedTrackNames: every entry's Part should be highway-renderable.

### Test infrastructure notes

- All candidates are pure functions: data in, data out. No JUCE GUI, no REAPER API, no audio thread.
- Test CMakeLists (`tests/unit/CMakeLists.txt`) links specific .cpp files. When adding TrackResolver tests, it's already linked. For DiscoFlipState, it's header-only so no additional linking needed.
- NoteProcessor.cpp needs to be added to the test CMakeLists link list.
- TestFixture in `test_helpers.h` provides NoteStateMapArray + ValueTree setup. Extend it for TrackResolver tests with SharedWindow/Config builders.
- For pollForChanges tests, InstrumentSession needs a mock TrackDiscovery and mock TrackNoteProvider. These are simple interfaces (1-2 methods each) so mocking is straightforward.

---

## Future

Unordered. Pull into Up Next when the time comes.

**Chart Features:**
- Instrument autodetection (by track name — depends on EVENTS parsing)
- Note color customization (CH color profile templates)
- GH style gems toggle
- Star Power rendering (partially working, architectural approach unclear — finish or remove)

**Real Drums:**
- Flam note types, HH open/closed/foot gems
- Generic gem system (color any part of note)
- Reorderable lanes + custom colors
- 5-lane drums, 8-lane drums
- Refactored MIDI parsing (supports reductions, benefits all instruments)

**Authoring & Vocals (way out):**
- Moonscraper-style note placement, grid snapping, note eraser
- INI generation/export
- **Composer layout**: alternative to the side-by-side multi-highway grid: every part lined up as one continuous highway grid, outer highways slanted so edges meet like the Rock Band band view. A layout choice, independent of editor views (overlays draw on every highway in any arrangement). Real geometry work in `PositionMath`.
- **Copy/paste between highways**: explicit replacement for the killed Linked mode. Across instruments it needs the same guitar/drums lane-mapping rules and refusals (2x kick Expert only, no open+fret chords, tom/cymbal collapse).
- 2D pitch-based karaoke display, lyrics with rhythm timing

**DAW Integration:**
- Max4Live device for Ableton — proper Ableton support via Max4Live to expose REAPER-equivalent features (text events, timeline access, track discovery) through Ableton's LOM. Would enable disco flip, sections, multi-highway, etc. The path to full Ableton support since the standard plugin pipeline can't access what it needs without DAW-specific APIs.

**Architecture (do when it hurts):**
- AudioProcessorValueTreeState migration
- Double-buffered snapshots for renderer
- **Pop-out highway windows (one plugin, many viewports)** — Today "two windows" = two plugin instances on the same track = duplicated REAPER API traffic, duplicated polling, duplicated InstrumentSession. Replace with "Pop out" action in the toolbar that spawns a `juce::DocumentWindow` hosting an additional `HighwayComponent` off the same `SessionController`/`SlotData`. One processor, one session, one REAPER poll — N viewports. Makes the "6 instances on one track" perf cliff go away by removing the reason to do that. Side-steps the shared-project-scan-cache design (not needed if there's only one consumer). Leaves the real multi-instance case (independent tracks) as the only path where cross-instance caching would matter, which is rare enough to defer indefinitely. REAPER duplicate-track hang (fixed in `fix/reaper-duplicate-hang`, commit `da1d115`) was the forcing function.

---

## Icebox

Blocked or no clear path forward.

- **#17 — Linux REAPER scan failure** — May be resolved by cross-platform build fixes. Needs verification. [GitHub](https://github.com/noahbaxter/chartchotic/issues/17)
- **#16 — FL Studio: no notes appear** — *Blocked: don't own FL Studio.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/16)
- **#3 — Logic: AU loads as Audio FX, no MIDI** — *Blocked: don't own Logic.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/3)
- Elite Drums (beyond Real Drums)
- Extended memory for standard pipeline (notes visible when transport stops)
- REAPER auto track detection
- REAPER preset integration (.ini files)
- .ini file generation (community suggests CAT instead)
- Note length validation (possible ReaScript utility)
- Motion sickness mitigation (unclear solution)

---

## Notes

- **Moonscraper overlap**: Community consensus is Chartchotic is for preview, not charting.
- **Fills are everywhere, BREs are not**: scanned 44 Rock Band 2 charts. All 44 use 120-124; 1213 of 1215 phrases are drum fills, and exactly one chart has a `[coda]` (L7 - Pretend We're Dead). That one writes the BRE on GUITAR, BASS and DRUMS at the identical tick with all five markers. So fill authoring is the common path and must stay cheap, and a BRE is one song-global moment written across every part at once, not a per-part phrase. Zero incomplete marker sets across all 1215, though the spec allows them (`_refs/midi/drums.md:150`).
- **Key dependency chain**: EVENTS parsing -> section detection -> autodetection -> Real Drums MIDI refactor -> generic gem system
- **Community feedback source**: Discord #chartchotic channel
