# Laptop Manager 4.0.0: shared server and offline synchronization

The server owns the shared records. Desktop SQLite is a durable local cache and operation queue. Synchronization transfers transactions over HTTPS; database files are never shared over the network.

The first deployment is one school on the supplied Ubuntu server. It has approximately 1 GB RAM, so the API uses server-local SQLite with WAL, FULL durability, busy timeouts, and serialized write transactions. The API is the sole writer. This keeps resource use small while enforcing multi-user stock invariants. A larger deployment can replace the storage adapter with PostgreSQL without changing the desktop protocol.

## Changes and identity

Every record has a globally unique identifier, separate from its display integer ID. Every local mutation has a unique transaction ID, the authenticated user/device identity, before/after values, base revisions, and dependencies on earlier queued mutations. Local changes and queue entries commit together. Retrying a transaction returns its original durable receipt rather than applying it twice.

Server transactions serialize validation, mutations, audit events, and change-feed revisions. Feed cursors represent committed transactions, not timestamps. Deletions are tombstones. Existing databases use a persistent source namespace so independent legacy integer IDs never collide. A consistent snapshot includes an instance ID and cursor; restoring a different server database requires cache reset rather than trusting an obsolete cursor.

## Conflict rules

Metadata edits patch only changed fields onto current server data; the last server-accepted edit wins for the same field. Deletes require the expected revision, and stock total adjustments check their starting quantity. Counter updates for returns and missing/recovered quantities use bounded deltas and revalidate totals. Available stock must stay nonnegative, and an active laptop cannot be lent twice. A transaction either commits completely or is rejected completely. Dependent changes cannot bypass a rejected parent transaction.

The local cache keeps accepted server state separately from queued changes. Rejected changes remain visible in a conflict list; they never disappear silently or overwrite server state. Users can inspect a conflict, keep its details, and explicitly discard or retry against current server state. Unrelated changes continue syncing.

## Connectivity and accounts

The UI distinguishes connected, offline, pending, authentication required, and conflict states. Requests are asynchronous, bounded by timeouts, and retried with exponential backoff. Pending operations survive application and server restarts, including a lost acknowledgement after the server committed.

Admin manages users, presets, stock metadata/adjustments, and deletions. Staff handles lending, returns, notes, and missing/recovery reports. Viewer reads only. The server authorizes every command using the current account role, including changes created offline. Revoked sessions and permissions stop resync rather than being treated as a network outage.

A password gate now hides the main window at startup and after logout/revocation. Online session tokens and reconnect passwords live only in memory. A salted PBKDF2 verifier permits password-checked offline unlock for seven days after an online login; it is invalidated after confirmed revocation. The local SQLite file remains unencrypted. Offline work is provisional. Reconnect uses the same account and retries automatically; mutations trigger uploads, and periodic polling receives other users’ updates. Shared operating-system accounts must not be treated as a security boundary between people: use separate OS accounts for private caches. User/device identity is recorded independently from recipient/returner names.

## Rollout and recovery

Import real data only after verifying its source. Old local records are never uploaded automatically: a different local database is preserved as a durable review proposal before the server snapshot is projected. An administrator may explicitly back up and import a verified database into an empty server. Back up the server via SQLite's backup API, retain transaction receipts and tombstones, and test restoring into a new instance epoch. The service runs as an unprivileged systemd account behind HTTPS, using the existing renewable IP certificate. Certificate renewal must reload the HTTPS proxy. Passwords are hashed; logs exclude credentials and tokens.

## Required verification

Concurrent lending of the last unit; simultaneous metadata edits; duplicate uploads; partial returns; crash before local commit; server commit with lost reply; offline restart and reconnect; failed dependency with unrelated successful work; revoked users; invalid/stale cursors; tombstones; bootstrap migration; TLS validation; backup and restore; old clients blocked when the protocol changes.

## References

- [SQLite isolation](https://www.sqlite.org/isolation.html)
- [SQLite WAL](https://www.sqlite.org/wal.html)
- [Qt asynchronous networking](https://doc.qt.io/qt-6/qnetworkaccessmanager.html)
- [FastAPI authentication](https://fastapi.tiangolo.com/tutorial/security/oauth2-jwt/)

## Current deployment

The supplied laptop database was identified by its owner as dummy data. The temporary import and its server backups were cleared; the administrator account was retained. Subsequently, an explicitly supplied second database containing important preserved records was backed up, validated, and imported: eight laptop loans and 33 presets/types. The original file was unchanged. The original laptop database has a private local backup. No automatic legacy upload runs on launch.

## 4.0.0 edit policy

Ordinary edits apply only changed fields to the current server record, without rejecting a stale record revision. The last edit accepted by the server wins for the same field; unrelated changes remain intact. This arrival order includes queued offline edits. Password/role checks, atomic transactions, receipt replay, deletion tombstones, active laptop uniqueness and inventory invariants still apply. Deletes retain revision checks. Inventory total adjustments reject a changed starting quantity so audit adjustment deltas remain accurate; unrelated metadata revisions do not block them. Return/recovery quantities still merge as bounded positive deltas.

The client saves only intended edits from forms: it compares with the record shown when the form opened and preserves incoming changes to fields left untouched. It automatically requeues ordinary metadata edits previously rejected with `STALE_REVISION` when the server advertises `changed_fields_last_writer`; preserved receipts and original proposals remain available. It never automatically retries legacy imports, deletes or quantity adjustments. Unsent quarantined batches expose their durable entries in Settings even if no wire payload had been frozen yet.
