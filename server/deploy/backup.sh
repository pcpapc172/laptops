#!/bin/sh
set -eu
umask 077
backup_path="/var/lib/laptop-manager/backups/server-$(date -u +%Y%m%dT%H%M%SZ).db"
cd /opt/laptop-manager
/opt/laptop-manager/venv/bin/python -m server.manage --db /var/lib/laptop-manager/server.db backup --output "$backup_path"
# Retain at least 30 days; never prune the source database.
find /var/lib/laptop-manager/backups -type f -name 'server-*.db' -mtime +30 -delete
