# Sketch upload, backup, edit, and sharing — design brief for the community website

> **Audience:** the team forking the community website backend + frontend off
> [`usememos/memos`](https://github.com/usememos/memos). Spelling note: the
> correct upstream org/repo is **`usememos/memos`** (one `s`, no `r` between
> `use` and `memos`). Several internal docs refer to it as `usermemos/memos`;
> that spelling does not resolve. **Do not** create a fork named
> `usermemos-memos-fork`; use `usememos-memos-…` (or a fresh name you prefer).
>
> **Source of truth for the device side:** `docs/DESIGN.md` in the
> `ravine.1une.cc` firmware repository, specifically **§11 (multi-sketch
> storage & restore)** and **§14 (Wi-Fi, web binding, and sketch upload)**.
> Please read §14 in full before responding — it pins the on-the-wire format
> and the lifecycle expectations, and the prompts in this brief defer to it
> for every numeric limit and state-machine name.
>
> **Project context.** The **ESP32-S3 PO-33 remake** is an open-source
> MIT-licensed firmware that emulates a Teenage Engineering PO-33 K.O! on a
> $5 microcontroller board. A "sketch" on the device is **one** unit of
> creative work — 16 sample slots, 16 patterns of up to 16 steps each, and
> a chain of up to 128 patterns — auto-named in a Docker-style
> `<genre>_<musician>` convention (e.g. `bebop_coltrane`). The device can
> hold up to 16 sketches on flash. The website community is **optional**:
> the device works fine without it. The community exists so sketch owners
> can back up, browse, edit metadata, and share their work with people who
> may or may not own a device themselves.
>
> **Status of this brief.** Draft 0.1, dated 2026-10-17. The author
> (firmware team) will respond to line-level comments and PRs against this
> file in the firmware repo. When the website team replies, please respond
> in the same place so the two sides share a single audit trail.

## 0. Source documents and version pins

| Source | Pin | Date | Why this pin |
|---|---|---|---|
| `ravine.1une.cc` `docs/DESIGN.md` | commit `a475334424a55438ba153d5f36fc3b4c0cb4514b` (`a475334`) | 2026-10-17 | Pinned at the commit that introduced §14. The brief assumes §14 numbers (≤50 MB blob, draft-by-default, Ed25519 mandatory) are stable. |
| `usememos/memos` `main` | commit `1a51572406e1db3e97c24b2497013c8005e88317` (`1a51572`) | 2026-10-16 | The most recent commit on upstream `main` as of writing. |
| `usememos/memos` latest release | tag `v0.31.0` at commit `2b2192d4e153bd04f1d325b60fd880cf00d68b01` | 2026-09-19 | The most recent tagged release. |

Both SHAs are point-in-time pins. If upstream `main` moves during our
back-and-forth, **re-pin to the new SHA in your reply** so the audit trail
is honest.

### Upstream `v0.31.0` facts that affect this brief

These are derived from the `v0.31.0` release notes (2026-09-19) and
should be treated as constraints:

- **RSS feed support has been removed.** Existing feed URLs no longer
  provide feeds. Do not propose an RSS endpoint for sketches; the
  convention does not exist upstream anymore.
- **Reactions API simplification.** The `contentId` field has been
  removed; reactions remain scoped to their memo. **This is exactly
  why we recommend forking a `sketch_reaction` table** — there is no
  upstream polymorphic-reaction pattern to copy.
- **`View` concept exists.** Shortcuts were migrated to Views;
  operations live under `UserService`. Sketches fit naturally into
  this pattern (a "view" is a saved filter / curated subset of a
  user's content), but it is out of v1 scope — see §13.
- **Administrator read scope expanded.** Active instance admins can
  read and manage individual memos (including private ones)
  without being the author. The equivalent for sketches is
  flagged in §7 (unbind / revoke policy).

## 1. What we are asking for

We need a **sketch resource** on the website, parallel to a "memo" in the
upstream `usememos/memos` data model, that:

1. **Accepts uploads from a bound ESP32-S3 PO-33 remake** using a
   multipart payload whose format is fixed by the firmware (see §3).
2. **Stores the upload opaquely and durably** — object storage or
   filesystem, your choice — and surfaces a canonical URL the device
   can remember.
3. **Presents a browsable, searchable, commentable, likeable** view of
   sketches on the web, in the same general shape as upstream memos.
4. **Supports an edit-and-republish flow** that **never** modifies the
   original uploaded blob silently. Re-uploads (whether device-initiated
   or web-initiated) create a new version; the old blob is retained.
5. **Survives device unbind, rebind, revoke, and loss** with the
   attribution chain intact.

A working `usememos/memos` v0.31.x install handles most of the
plumbing (users, sessions, markdown, tags, attachments, comments,
reactions, search, visibility levels). Our job is to add the *sketch*
resource alongside `memo` and decide how it slots into the existing
data model.

## 2. What is a sketch (device-side, for context)

Per `docs/DESIGN.md` §11.2a, a sketch's on-flash layout is:

```
/sketches/<4hex-id>/
  meta.bin            24-byte NUL-padded display name (Docker-style, ≤24 chars)
  samples.bin         256-B slot table + raw int16 LE PCM (16 slots, back-to-back)
  patterns.bin        16 * sizeof(pattern_t) raw
  chain.bin           1-byte len + len bytes (entries)
```

The **semantic** content is:

| Item | Count | Notes |
|---|---|---|
| Display name | 1 | ≤24 chars, Docker-style, ASCII letters + digits + 1–2 underscores |
| Sketch ID | 1 | 4 hex chars (`0000`–`FFFF`) |
| Sample slots | 16 | each: empty / sample rate / length / trim-start / trim-end |
| Patterns | 16 | each: 16 steps; each step: sample-assignment + up-to-2 effect-locks + punch-in-effect |
| Chain | 1 | sequence of 1–128 pattern indices |
| Sketch-level settings | 1 set | BPM (preset + fine-tune), master volume (5 levels), swing, sync mode (5 modes) |
| Per-step punch-in effects | 16 fixed (F-019) | pick-one-of-16 on each step |

The **web side never parses `samples.bin`, `patterns.bin`, or
`chain.bin` directly**. It stores them opaquely. It only needs to
understand the JSON metadata the device sends in the multipart
`metadata` part (see §3). On the wire, the device ships the sketch
folder as a **tarball** of the layout above; the server stores the
tarball in object storage as `backup_file` and never unpacks it
server-side.

## 3. The upload payload (fixed by the device, do not redesign)

Per `docs/DESIGN.md` §14.8, an upload is **multipart/form-data** with
exactly these parts in this order:

| Part name | Content-Type | Size cap | Required | Signed? | Description |
|---|---|---:|:---:|:---:|---|
| `metadata` | `application/json` | 16 KB | yes | yes | Canonical-JSON sketch header (see §4) |
| `signature` | `text/plain` | 16 KB | yes | — | Base64 Ed25519 signature of the exact bytes of the `metadata` part (88 chars with `=` padding; decodes to 64 raw bytes) |
| `backup_file` | `application/octet-stream` | **50 MB** | yes | no | Opaque blob: a tarball of the sketch folder shown in §2 |
| `sample_wav` | `audio/wav` | 10 MB each | no | no | Optional per-slot WAV sidecars; one per non-empty slot, indexed by slot number |

Size caps are **hard**. The device rejects its own upload if
`backup_file` exceeds 50 MB before transmitting. The server returns
`413 Payload Too Large` for any part over its cap.

The signature uses **Ed25519** with the device's per-device keypair
generated at first boot (§14.4.1). The public key is registered at bind
time. The server verifies the signature over the exact bytes of the
`metadata` part, no prefix, no suffix, no whitespace normalization.
Canonical-JSON rules are in §14.4.2 (UTF-8 NFC, lex-sorted keys, no
insignificant whitespace, no scientific notation, no NaN/±Infinity,
minimal escaping, `schema: 1`).

> **Server-side implications.**
> - Object storage must accept up to 50 MB blobs. (Upstream memos'
>   resource size cap is well below this — verify and bump if needed.)
> - The signature verification step is **mandatory** before any
>   server-side parsing of `metadata`. Failed verification → `401`,
>   not `400`.
> - The optional `sample_wav` parts are for **server-side preview
>   playback** in the web UI; the device can include or omit them
>   per upload. Treat them as untrusted attachments (re-encode on
>   the way out).

## 4. The sketch resource — server-side data model

**[R] Recommended mapping onto the `usememos/memos` data model:** treat a
sketch as a *first-class resource* with its own table (`sketch`),
parallel to `memo`. Reuse the upstream `resource` table for the WAV
sidecars and the canonical upload blob. Reuse `user`, `tag`, and
`memo_visibility`-style enum for the visibility states. Do **not** try
to overload `memo` with a new kind — keep the two domains separate.

Fields on the `sketch` table (proposed):

| Field | Type | Notes |
|---|---|---|
| `id` | int64 (server) | Primary key. **Not** the same as the device-side 4-hex ID. |
| `device_sketch_id` | string(4) | The 4-hex ID assigned by the device. **Not unique** server-side (two devices can both have a `0000`). Used for human-readable URLs. |
| `owner_user_id` | int64 FK | The owning user. Required. |
| `source_device_id` | int64 FK, nullable | The bound device this sketch came from, if any. Null for web-only uploads. |
| `source_device_fingerprint` | string(64), nullable | The **latest** SHA-256 fingerprint from §14.6 (serial ‖ MAC ‖ pubkey_sha256 ‖ git_sha). Updated on each new version; the *original* fingerprint is preserved on `sketch_version.source_device_fingerprint` (see §6, §14.5.5). |
| `display_name` | string(24) | Docker-style sketch name from §11.6a. May be renamed on the web without affecting the device-side ID. |
| `title` | string(120) | **[?]** Optional human-readable title. Upstream memos have no title; we'd add one here because sketches are searchable, named things. |
| `description` | markdown | **[?]** Long-form description, markdown-rendered. Optional. |
| `metadata_json` | jsonb | The exact `metadata` part of the last successful upload. Server-verified canonical JSON. |
| `backup_blob_ref` | string(255) | Object-storage key for the latest `backup_file`. |
| `signature_blob_ref` | string(255) | Object-storage key for the latest `signature` (so the device's signature is durably preserved, not just verified-and-discarded). |
| `preview_wav_refs` | string(255)[] | Object-storage keys for the per-slot WAV sidecars. |
| `visibility` | enum | See §5. |
| `state` | enum | See §5 — `draft` or `published`. |
| `pinned` | bool | Mirrors the upstream `memo.pinned` convention. |
| `created_at` | timestamp | When the device (or web uploader) first created the draft. |
| `updated_at` | timestamp | When the *metadata* was last changed. **Not** the same as the device's modification time, which we also store separately. |
| `device_modified_at` | timestamp, nullable | The device-side "last modified" timestamp from `metadata.modified_at`. Useful for conflict detection. |
| `current_version_id` | int64 FK | Points at the active `sketch_version` row. See §6. |
| `tags` | string[] | Reuse the upstream `tag` table. |

We will need a `sketch_version` table too (see §6), and likely a
`sketch_comment`, `sketch_reaction`, and `sketch_visibility_acl` for
PROTECTED-mode sharing. **[?]** Whether to fully reuse upstream's
comment / reaction tables or fork them is a real choice — the upstream
comment table is memo-typed, and `v0.31.0` simplified the reactions
API to be memo-scoped. The conservative answer is **fork**; upstream
doesn't have a polymorphic-comment pattern.

## 5. Lifecycle and visibility

**[R]** Lift the device-side lifecycle from `docs/DESIGN.md` §11.4 and
§14.7 onto the server.

```
                +---------+    create     +----------+
                | (none)  | ------------> |  DRAFT   |  default state, owner-only
                +---------+               +----+-----+
                                              |
                                              | owner presses "Publish"
                                              v
                +---------+    unpublish  +----+-----+
                | ARCHIVED| <----------- |PUBLISHED |  visible per visibility setting
                +---------+              +-----+-----+
                                             |
                                             | admin action
                                             v
                                       +-----------+
                                       |  HIDDEN   |  soft-removed, kept for audit
                                       +-----------+
```

- **State is server-side**, not device-side. The device only knows
  draft vs. uploaded. (See §14.7: "publish is web-only".)
- **All uploads arrive in `DRAFT`.** No upload auto-publishes.
- **Visibility ∈ { PRIVATE, PROTECTED, PUBLIC }**, reusing upstream's
  `memo_visibility` semantics:
  - `PRIVATE` — owner only. Default.
  - `PROTECTED` — anyone with the link, OR signed-in members. **[?]** Match upstream, or restrict to signed-in?
  - `PUBLIC` — indexed, listed, searchable, surfaced on the author's profile.
- **Drafts are a *new* state, not lifted from upstream.** Upstream
  `usememos/memos` has no draft state at all; memos are saved
  directly. Drafts are introduced here to satisfy §14.7's
  "publishing is a web-only action" requirement. The implementation
  is straightforward: a `state` column on `sketch` plus a UI gate
  on the publish endpoint.
- **Drafts are always private**, even if the sketch's `visibility` is
  set to `PUBLIC`. The owner publishes explicitly. **[R]** mirror
  upstream memos' convention here: a memo can be marked PUBLIC and
  still be a "draft" if it is unpublished; the public flag is
  informational until publish.
- **Republish requires owner.** No collab editing in v1.
- **Hidden is admin-only.** A `hidden` flag, plus a `hidden_reason`
  and `hidden_by` (admin user_id), is sufficient. We do not need a
  full mod queue in v1.

## 6. Versioning, edit history, and re-import

**[R]** Every upload (device-initiated or web-initiated) creates a new
`sketch_version` row. Versions are append-only and immutable. The
`sketch.current_version_id` pointer is what the API returns by default.

`sketch_version` fields:

| Field | Type | Notes |
|---|---|---|
| `id` | int64 | Primary key. |
| `sketch_id` | int64 FK | Parent sketch. |
| `version_number` | int | Monotonically increasing per sketch. |
| `uploaded_by` | int64 FK | User who triggered the upload. |
| `uploaded_at` | timestamp | When the multipart upload completed. |
| `uploaded_from_device` | bool | True if the upload came in over the RFC 8628 device flow; false if it came in via the web UI. |
| `source_device_fingerprint` | string(64), nullable | The device fingerprint at upload time. **This is the per-version snapshot from §14.5.5** — it stays even after unbind, rebind, or admin revoke. |
| `metadata_json` | jsonb | The exact `metadata` part. |
| `backup_blob_ref` | string(255) | The `backup_file` for this version. |
| `signature_blob_ref` | string(255) | The `signature` for this version. |
| `preview_wav_refs` | string(255)[] | The WAV sidecars for this version. |
| `change_note` | string(255), nullable | **[?]** Owner-supplied changelog note. Upstream memos don't have this; sketches probably should. |

- **Re-import to a device.** The web side offers a "Download backup"
  endpoint that returns the `backup_file` blob for a chosen
  `sketch_version`, **plus** the original `signature` and a freshly
  generated access token the device can use to verify the blob. The
  device-side flow is: download blob → verify Ed25519 signature with
  the registered pubkey → write to `/sketches/<new-id>/` using the
  same atomic protocol as a normal save. **[?]** Should the device
  always re-sign the re-imported sketch with its own key (yielding a
  re-signed `metadata`), or preserve the original signature? **[R]**
  re-sign — the device is the new owner of the sketch on the local
  filesystem, and the server keeps the original signature in
  `signature_blob_ref` for provenance.
- **Conflict detection on device-side re-upload.** If the device's
  `device_modified_at` is older than `sketch.updated_at` on the
  server, the server returns `409 Conflict` with a diff link. The
  user is asked to "keep mine / keep theirs / merge on the web"
  before the new version is created. **[?]** Auto-merge is a v2
  problem; v1 requires explicit user choice.
- **History is preserved through unbind and revoke.** Per §14.5.5,
  the server keeps a sketch-history fingerprint snapshot. This is
  the `source_device_fingerprint` field on `sketch_version` — we
  already capture it. No special table needed.

## 7. Ownership, attribution, and device binding

Per `docs/DESIGN.md` §14.1, binding is **optional**. A user account
can do everything except device-initiated uploads without binding a
device. So:

- **`owner_user_id` is required. `source_device_id` is optional.**
  Web-only uploads have `source_device_id = NULL` and
  `source_device_fingerprint = NULL`.
- **A sketch has exactly one owner** (matches §14.2: 1 device ↔ 1
  user; 1 user ↔ N devices). Transfer of ownership is not in v1.
- **Unbind and revoke do not touch existing sketches.** The
  sketches stay in the owner's account. The `source_device_id`
  pointer is set to NULL on unbind (we keep the
  `source_device_fingerprint` on each version for provenance, per
  §14.5.5).
- **Orphaned device-side sketches** (device lost, owner not
  re-bound) remain on the device, never reach the server, and are
  not the server's problem.
- **One device can have many sketches on the server.** There is no
  per-user cap in v1 (matches §14.2).
- **Public-by-default sketches from a device that is later revoked:**
  they stay public. The fingerprint on each version is the audit
  trail. Admin can `HIDDEN` individual sketches independently.
  The v0.31.0 admin-scope expansion (active admins can read and
  manage individual memos) extends naturally to sketches.

## 8. Cross-cutting — your call

These are the things the upstream `usememos/memos` already does for
memos. Decide whether to share, fork, or omit per item:

| Concern | Recommendation | Open question |
|---|---|---|
| Tags | Share upstream `tag` table. | Allow `sketch_tag` to be a many-to-many join or a simple text[] column? **[?]** |
| Comments | Fork: add `sketch_comment` table. | None — upstream's comment table is memo-typed, and v0.31.0 did not make it polymorphic. |
| Reactions / likes | Fork: add `sketch_reaction` table. | None — v0.31.0 simplified reactions to be memo-scoped; a separate table is the honest path. |
| Search | Reuse upstream's full-text search index. Add a `sketch` doc type alongside `memo`. | **[?]** Index `description` and `title`? Index the metadata JSON? |
| Pinned | Share the convention (`pinned` bool on `sketch`). | None. |
| `View` (curated filters) | Out of v1 scope. Sketch content is a natural fit for a future "sketch view" feature. | **[?]** Add a `sketch_view` table now for forward-compat, or wait? **[R]** Wait. |
| RSS / Atom feed of a user's sketches | **No.** Upstream removed RSS in v0.31.0. | None. |
| Embedding (oEmbed / iframe) | Add an oEmbed endpoint for `/s/<id>`. | **[?]** Do we want external sites to embed sketches? |
| Markdown | Reuse upstream's markdown renderer. | None. |
| i18n | Reuse upstream's i18n keys; add `sketch.*` namespace. | None. |
| GDPR / data export | Extend the upstream "export my data" flow to also include `sketch` and `sketch_version` rows + the blobs from object storage. | None. |
| GDPR / right-to-be-forgotten | A `deleted` tombstone is enough — we soft-delete the `sketch`, hard-delete blobs after 30 days. | **[?]** Should we keep the metadata even after blob purge, as a tombstone, for thread integrity? **[R]** Yes — keep metadata, hard-delete blobs. |
| Mod tools | Add `HIDDEN` flag and a `hidden_reason` enum. No full mod queue in v1. | **[?]** Should mods see deleted-but-not-yet-purged sketches? **[R]** No. |
| Rate limits | Reuse upstream's per-user / per-IP limits; add a per-device limit (10 uploads / hour) keyed on `source_device_id`. | None. |
| Notifications | Reuse upstream's notification bus; add a "your sketch was liked" event. | **[?]** Do we want email digests? **[R]** No in v1. |

## 9. API and URL design

**[?]** Open. Recommended defaults:

- **Style:** keep upstream's ProtoBuf-defined services + REST gateway.
  Add `SketchService` alongside `MemoService`.
- **URLs:**
  - Web UI: `/sketch/<int64-id>` (canonical), `/sketch/<int64-id>/v/<n>` (a specific version).
  - Public-by-id shortlink: **`/s/<int64-id>`** (like upstream's `/m/<id>` for memos). **[R]**.
  - API: `/api/v1/sketches`, `/api/v1/sketches/{id}`, `/api/v1/sketches/{id}/versions`, etc. **[R]** keep the upstream API prefix convention.
- **Device upload endpoint:** `POST /api/v1/devices/{device_id}/uploads` (multipart, see §3). The device authenticates with the RFC 8628 access token (§14.3.5).
- **Pagination:** cursor-based, matching upstream's `page_token` convention.
- **Filtering:** `?visibility=public&state=published&tag=…&author=…&since=…`. Mirror upstream's parameter naming.
- **Sort:** `recent` (default), `popular` (by reactions), `trending` (by reactions per day over last 7 days), `random`. **[?]** `random` is upstream's default for the home page; we recommend `recent` for sketches.

## 10. What the device needs back from the server

The device-side flow is upload-and-store, not a continuous sync. The
device only needs:

1. **`201 Created` + `{ sketch_id, version_id, canonical_url }`**
   on successful upload.
2. **`401`/`403`** on auth / signature / size failure.
3. **`409 Conflict` + diff URL** on a stale-device-side `device_modified_at`.
4. **`413 Payload Too Large`** on size-cap violation.
5. **`429`** on rate-limit.
6. **[?]** An optional server-pushed "config blob" on successful
   upload — e.g. "you have 3 likes on this sketch, view at
   <url>". **[R]** No. The device doesn't have a UI for this; the
   owner views likes on the web. Keep the device response minimal.

No streaming, no long-poll, no websockets. The device is offline-first
and only opens the radio for the duration of an upload.

## 11. Constraints and out-of-scope

**In scope for v1:**
- Backup: device → server, one direction.
- Edit-metadata-on-web: owner can rename, retag, re-describe, change visibility.
- Share: PUBLIC, PROTECTED-link, owner-only-private.
- Re-import: server → device, via a "Download backup" endpoint.

**Out of scope for v1 (per `docs/DESIGN.md` §14.0):**
- Live audio streaming or low-latency live playback.
- Real-time collaborative editing.
- A "sync protocol" with the original Teenage Engineering PO-33. The device's §3.10 jam sync is unrelated.
- A "push" mechanism from the server to the device. The device polls on its own schedule.
- Per-step effect synthesis / DAW features on the web. The web is for metadata, samples-as-audio, comments, likes, sharing. Not editing the patterns themselves.
- A `View` / curated-filter feature (see §8).

**Hard constraints from the device side:**
- 50 MB max blob.
- Ed25519 mandatory.
- Private-by-default.
- One-owner-per-sketch.
- Draft-by-default; publish is web-only.

## 12. Acceptance criteria / deliverables we expect back

A complete response to this brief includes:

1. **An RFC PR** (`docs/rfcs/0001-sketch-resource.md` or similar) in
   the website repo, restating the data model in §4 and the lifecycle
   in §5, with the open questions from §8–§9 resolved or explicitly
   deferred.
2. **A schema PR** adding the `sketch`, `sketch_version`, and
   (optionally) `sketch_comment` / `sketch_reaction` tables, with
   migrations.
3. **A ProtoBuf PR** adding `SketchService` and the `sketch.proto`
   definitions, with the upload endpoint wired into the existing
   REST gateway.
4. **A minimal prototype** that:
   - Accepts a hand-crafted `multipart/form-data` request with the
     four parts in §3, verifies the Ed25519 signature against a
     test pubkey, stores the `backup_file` in object storage, and
     returns `201` with `{ sketch_id, version_id, canonical_url }`.
   - Serves `/s/<id>` for a `PUBLIC` sketch, rendering a play
     button for each WAV sidecar.
   - Implements the `DRAFT → PUBLISHED` transition.
5. **An ADR list** (`docs/adr/000N-…`) capturing the choices made on
   the open questions in §8–§9.
6. **A gap note** flagging any place where the proposed design
   diverges from the device-side assumptions in `docs/DESIGN.md`
   §11 or §14. We would rather find divergences now than at the
   integration test stage.

## 13. Open questions for us (the firmware team) to resolve before kickoff

These are **our** homework, not yours:

1. **Per-version WAV sidecars vs. one per upload.** Should the
   device always send all 16 slots' WAVs, or only the slots that
   changed since the last upload? Server-side it's cheaper to
   always-send (no diff logic) but it triples the upload size for
   sketches with 4 used slots.
2. **What does the device do with the server's `canonical_url`?**
   We had assumed the device just stores it for display. The web
   team may want the device to display a QR code on the TFT that
   encodes the URL. **[R]** add a "Press WRITE + 8 to show QR" UI
   to v2 of the firmware if the web team wants it.
3. **5xx retry semantics.** §14.3.4 already specifies the four
   RFC 8628 error codes the firmware must handle during the
   **bind** flow. We need an analogous table for the **upload**
   flow. The current draft: `5xx` → exponential backoff up to
   `interval * 6`, then surface "Upload failed — try again later,
   the sketch is still saved on this device" and leave the draft
   on the device. **[R]** match the §14.3.4 shape; document as
   §14.11 in a future revision.
4. **Unpublish vs. delete semantics.** When an owner "unpublishes"
   a sketch, does the server keep the `metadata` and `backup_file`
   for re-publish later (an "archive"), or hard-delete them with
   a 30-day tombstone? **[R]** archive (keep everything; flip
   `state` from `PUBLISHED` to `ARCHIVED`; metadata and blobs
   remain available). Hard delete is a separate, GDPR-driven
   flow.
5. **Search indexing of `metadata_json`.** Should the FTS index
   cover fields inside the canonical-JSON metadata (e.g. the
   `display_name`, the per-slot labels)? The metadata is signed,
   so the server can trust its contents after signature
   verification. **[?]** search and security teams should agree.
6. **Comment moderation policy.** Upstream memos allow the
   memo author to delete any comment on their own memo. Do
   we mirror that for sketches? **[R]** yes, with the same
   "soft-delete, hard-purge after 30 days" rule we already
   apply to the sketch itself.

## 14. End of brief

### Change Log

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-10-17 | Initial draft. Pinned to `docs/DESIGN.md` `a475334` and `usememos/memos` `main` `1a51572` / `v0.31.0` at `2b2192d`. |

### How to reply

The firmware team's preferred order:

1. **Open a PR** against `docs/community-website-prompt.md` in the
   `ravine.1une.cc` firmware repo. Inline comments on the specific
   sections, plus a `## Reply` section at the bottom of the file
   with the answers to every **[?]** you want to take a position on
   (or "deferred" with a reason).
2. **Cross-link** the website repo's RFC PR (per §12 deliverable 1)
   in that `## Reply` section so the two PRs can be reviewed
   together.
3. **Re-pin** to the new `usememos/memos` SHA if your fork was
   branched from a SHA different from `1a51572`.

If you want to do this asynchronously, a GitHub issue with the
`## Reply` content pasted inline is acceptable as a starting point,
but the PR is preferred for the final answer.

### Contact

The author of this brief is the `ravine.1une.cc` firmware team. The
repo README and `docs/architecture-decisions.md` have the project's
contribution conventions. Treat this brief the same way you would
treat a long-form issue: respond in line, mark open questions
explicitly, and prefer concrete proposals over open-ended
discussions.
