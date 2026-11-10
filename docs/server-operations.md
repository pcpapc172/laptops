# Laptop Manager server operations

The deployed endpoint is `https://13.42.221.242`. The database is `/var/lib/laptop-manager/server.db`; application code and its virtual environment are `/opt/laptop-manager`. The service account is `laptop-manager`, without a login shell. The API listens only on loopback port 18080, behind nginx on 443. Port 80 stays free for the existing ACME standalone certificate renewal.

## Accounts and clients

The first administrator's credentials are in a private file on the owner's workstation: `~/.laptops/server-admin.json`. Do not add this file to source control. Sign in at the startup password gate, then use Users in Settings to create separate staff/viewer/admin accounts, disable accounts, or change passwords. Each account update invalidates that user's sessions. A device with unresolved pending/conflicting operations cannot switch to another account until its original operations are reviewed.

The main window stays hidden until the startup password gate accepts an online login or a valid offline password. Logout hides the records and reopens that gate. The former `--login-file` automatic login is removed. Session tokens and the password used for automatic session renewal/reconnect stay in memory only; restarting always asks for a password. A successful online login saves a random-salted PBKDF2-HMAC-SHA256 verifier (600,000 iterations). If the server is unavailable, the same account/password may unlock the cache for seven days from that online validation. Five failed offline attempts impose a persistent one-minute lockout. An online authentication rejection or TLS failure does not offer an offline bypass. Confirmed account revocation locks the app and removes the verifier. The cache SQLite file itself is not encrypted; OS file access remains a separate security boundary. Use separate OS accounts for separate private caches.

The client automatically schedules uploads shortly after each committed local mutation and fetches other users’ changes every ten seconds. It uses bounded exponential backoff after connectivity errors; an offline password unlock also retries online authentication automatically when connectivity returns. TLS certificate errors are fatal and never ignored. A queued operation is saved in the same SQLite transaction as the local change. Replayed uploads return the saved server receipt, including after a server restart. The change feed also identifies committed batches so a lost acknowledgement is resolved before replaying local stock counters. Laptop edit errors now distinguish login/permission failures from database failures.

All laptop and inventory records synchronize. Themes and other local preferences stay on the device. Server audit timestamps are generated on the server; loan/return/event times retain the device's reported occurrence time for offline history.

## Legacy data

Legacy databases are not uploaded automatically. When a first server snapshot differs from existing local loan/inventory data, the complete local record set is kept as a durable `legacy_review` proposal in Settings. Review it before discarding. The owner's original workstation database was dummy data and was cleared from the server. A separate explicitly supplied database contained important preserved data: eight laptop loans (seven active, one returned), eighteen teacher presets, eight grades, and seven stock types. These 41 records were validated, backed up, imported, and verified against the server without changing the source file.

For an explicitly approved import into an empty server, close the old desktop app, back up its database, and run the 3.0 desktop once with `--prepare-cache` so its persistent global identity mapping is present. Transfer the prepared database privately and run:

```sh
cd /opt/laptop-manager
sudo -u laptop-manager venv/bin/python -m server.manage --db /var/lib/laptop-manager/server.db import --legacy-db /private/path/laptops.db
```

If the live domain tables were already projected away but a `legacy_review` proposal exists, pass its ID to recover the preserved records instead:

```sh
sudo -u laptop-manager venv/bin/python -m server.manage --db /var/lib/laptop-manager/server.db import --legacy-db /private/path/laptops.db --review-id PRESERVED_REVIEW_ID
```

The updated client automatically resolves that review once all corresponding global identities and initial contents are confirmed on the server. The proposal payload is retained for recovery.

The importer validates all records and counts before one atomic commit; it refuses a nonempty server. Independent legacy databases require a reviewed merge rather than an overwrite.

## Backup and restore

`laptop-manager-backup.timer` creates daily consistent SQLite backups under `/var/lib/laptop-manager/backups`, retaining 30 days. These include account password hashes, receipts, tombstones, and the audit log; keep them private. Copy backups to a separate machine for protection from loss of the server itself. The timer alone does not provide offsite disaster recovery.

Restore while the service is stopped:

```sh
sudo systemctl stop laptop-manager
cd /opt/laptop-manager
sudo -u laptop-manager venv/bin/python -m server.manage --db /var/lib/laptop-manager/server.db restore --input /private/path/backup.db
sudo systemctl start laptop-manager
```

Restore preserves a copy of the previous database, changes the instance epoch, and revokes sessions. Clients must log in again; previously queued changes are preserved for review against the restored server. Stop all API writers before restoring.

## Service and certificate checks

```sh
systemctl status laptop-manager nginx laptop-manager-backup.timer
curl --fail https://13.42.221.242/health
```

The existing ACME renewal hook retains the x-ui restart action and additionally checks/reloads nginx. Certificate and key paths remain `/root/cert/ip/fullchain.pem` and `/root/cert/ip/privkey.pem`; do not copy private keys into the app. Logs do not contain bearer tokens or passwords.

## Verification

Python tests cover duplicate/lost acknowledgements across restart, concurrent stock consumption, counter bounds and atomic rollback, two users lending the same laptop, stale notes, staff/viewer restrictions, revocation, tombstones, invalid epochs, and backup/restore.

Native Qt tests cover durable SQL-trigger queues, restart, dependency chains, rejected-parent recovery, explicit retry, role restrictions, and the real HTTP login/upload/offline/reconnect flow for both laptop and inventory operations. CI runs these with a loopback fixture and isolated databases, never the deployed school data.

Password verifier implementation uses [Qt QPasswordDigestor](https://doc.qt.io/qt-6/qpassworddigestor.html) and the PBKDF2 work factor documented by [OWASP Password Storage](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).

## Deleted identity collision recovery (3.0.2)

Client integer allocation stays above all mapped identities, including remote tombstones. This prevents new records from inheriting deleted global IDs after a snapshot. For a failed new laptop loan with `ALREADY_EXISTS` and a deleted server identity, the existing retry action preserves the rejected payload and creates a fresh local/global identity. The server still checks active laptop uniqueness and permissions; it does not resurrect or overwrite deleted records. New-loan messages distinguish local queuing from server acceptance.

## Login and sync interface (3.1.0)

The startup password dialog shows a loading bar during online authentication. Escape, repeated Enter, and the window close action do not dismiss the gate; the explicit exit button closes the application and is disabled while authentication is in progress. Failed authentication leaves the prompt open. Settings shows sync progress, status and conflict controls without a second login form. Logout reopens the startup password gate.

## Seamless edits (4.0.0)

Normal descriptions, notes and metadata apply as changed-field patches to the current server record. Same-field edits use server arrival order, including uploads queued offline. Open forms preserve untouched fields updated while the dialog was open. After login with a server advertising the new policy, old ordinary `STALE_REVISION` edits are automatically requeued, retaining their original proposals and receipts. Existing quantity adjustments, deletes and import reviews still require review. Conflicts quarantined before first upload now show their durable entries in Settings.

## 4.1.0 domain recovery (2026-11-09)

The service runs on Debian 13 at `51.20.165.160` (SSH account `admin`). Primary endpoint: `https://laptops.pcpapc172.ir`; Cloudflare fallback: `https://laptops-cf.pcpapc172.ir`. Both hostnames proxy to the same loopback API and share one database. Certbot webroot HTTP-01 issued a certificate covering both names; the timer renews automatically and a deploy hook checks/reloads nginx. HTTP port 80 remains reachable for renewal. Authenticated responses include `Cache-Control: no-store`.

Recovery used consistent private backups of both device caches. The phone at cursor 300 contained every laptop row at cursor 298: 95 loans (93 returned, 2 active), 18 teachers, 8 grades, 7 stock types, and 30 tombstones. A fresh epoch invalidates old cursors. Original caches/journals were retained privately. Server-only password hashes, account lists, receipts and audit history were unavailable; a random-password admin was bootstrapped, and the owner will recreate other accounts. The generated credential is stored in a mode-0600 file; never add it or any recovery data to Git.

Clients migrate the old IP default to the primary domain, retry replay-safe requests on connection/5xx failures through the Cloudflare domain, and retain exact mutation IDs/payloads. Authentication failures/rate limits do not trigger fallback. Both certificates are validated; user creation is never blindly replayed after an uncertain response. A working fallback is retained for 60 seconds before checking the direct path again. Daily consistent backups are enabled on the new host.

## Daily off-site Cloudflare backups

`laptop-manager-cloud-backup.timer` runs at **15:00 Asia/Tehran** (11:30 UTC). Each run uses SQLite's online backup API, checks integrity, compresses a private local copy, uploads a uniquely dated object to private R2 bucket `laptops-private-backups`, downloads it again, checks its SHA-256 and opens the decompressed SQLite backup for a restore integrity check. A matching manifest is uploaded and verified; only then is `last-cloud-success.json` updated. Existing objects are not overwritten or automatically deleted. Source data, accounts, password hashes, receipts and audit history are included in these complete server backups.

The token is stored in `/etc/laptop-manager/r2-backup.json` with root-only permissions, passed to the unprivileged service using systemd `LoadCredential`. It is never in source, command-line arguments or logged API responses. Failed runs preserve the local backup, fail visibly in systemd and retry every 15 minutes (four attempts per six hours). `Persistent=true` catches up after server downtime. Daily local backups remain enabled separately.

```sh
systemctl list-timers laptop-manager-cloud-backup.timer
systemctl status laptop-manager-cloud-backup.service
journalctl -u laptop-manager-cloud-backup.service
cat /var/lib/laptop-manager/cloud-backups/last-cloud-success.json
```

For recovery, retrieve the dated `.db.gz` from the private bucket, check the SHA-256 against its manifest, decompress it, verify `PRAGMA integrity_check`, then use the server's restore command with the API stopped. Restore invalidates sessions and assigns a new server epoch. Keep credentials and recovery files out of Git.

## Direct application downloads

Release binaries are mirrored under `/var/www/laptops-downloads/4.1.0/`, served by nginx at `/downloads/` on both `laptops.pcpapc172.ir` and `laptops-cf.pcpapc172.ir`. This is a separate public directory; the server database, backups and credentials remain outside all web roots. Directory listing is disabled. The landing page links Windows ZIP, Linux tar.gz, the signed minified Android release APK, and SHA256SUMS. The files are copies of the published release artifacts, so users do not need GitHub to download them.

For future releases, stage files in a new version directory, verify hashes, then copy them into the public download root and update its index. Do not overwrite a versioned artifact with a different build. Verify the complete downloaded bytes on both routes after publishing. Keep API `no-store` response policy separate from public release download caching.

## 2026-11-10 replacement host

The current host is Ubuntu 24.04 at `77.37.65.197`. DNS names and client version remain unchanged. Restored the latest verified Cloudflare backup, `daily/server-20261109T113000Z-4ee00526.db.gz`, with all four users/password hashes, receipts, audit history and tombstones. It has 85 returned loans; ten older cached loans were subsequently deleted and were not resurrected. Both phone/laptop caches were copied first and compared: no unsynced changes, missing entities or newer revisions than the cloud backup. A new server epoch invalidates pre-failure cursors/sessions without modifying the source backup.

This host already ran Xray on 443. With explicit owner authorization, nginx stream SNI routing now sends the two laptop names to nginx TLS on `127.0.0.1:18443`, and all other names to the original Xray Trojan inbound on `127.0.0.1:19443`. The x-ui database inbound 15 was updated from public port 443 to loopback 19443, retaining its tag and TLS settings. Original configuration/database snapshots are private under `/root/laptops-migration/routing-cutover/`. Xray was restarted and the default-route TLS certificate was checked unchanged. Existing proxy clients still use public port 443; newly generated subscription links must likewise use the public route rather than the internal 19443 listener.

Use `server/deploy/shared-https/nginx-origin.conf` for this host's HTTP configuration and include `shared-https/stream.conf` at nginx main scope with the stream module loaded. Do not install the standalone nginx-domain.conf unchanged on this shared host. The API stays loopback-only on 18080. Both names have a Let’s Encrypt HTTP-01 certificate, renewal dry-run passed, and the nginx reload hook/timer are enabled. Local and 15:00 Asia/Tehran Cloudflare backup timers are enabled; a fresh backup passed download/hash/SQLite restore verification after migration. The public download files were restored and archive-verified.

The Cloudflare public route passed HTTPS/login/full-snapshot/download-index checks externally. The direct hostname's TLS was verified through the public IP from the server, but an external route used by this workstation timed out; keep the Cloudflare fallback available and do not describe that path as universally reachable.
