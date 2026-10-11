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
