# Domain recovery rollout

The new host serves `https://laptops.pcpapc172.ir`, with `https://laptops-cf.pcpapc172.ir` as the Cloudflare fallback. DNS A must point to the new host; only publish AAAA if IPv6 also reaches that host. Allow inbound TCP 80 for ACME HTTP-01 and TCP 443 for the API. Keep uvicorn on loopback 18080.

Keep the original laptop/phone recovery artifacts private. Import only after reconciliation. `server.manage import-snapshot --input PATH` accepts a validated canonical client snapshot only into an empty server; it preserves UUIDs, revisions and tombstones, assigns a fresh epoch, and invalidates sessions. It does not recover passwords, account lists, old server audit history or complete transaction receipts. Recover these from an original server backup when available; otherwise bootstrap accounts explicitly. Never copy client password verifiers into server user hashes.

On the new Ubuntu host install Python venv, nginx and certbot. Deploy the existing systemd API and backup timer with the service account and private data directory. Restore the prepared recovery DB with root-only transport and ownership `laptop-manager:laptop-manager`, mode 0600. Bootstrap users before enabling access.

For certificate issuance first install `server/deploy/nginx-domain-http.conf`, create `/var/www/laptops-acme/.well-known/acme-challenge`, and check `nginx -t`. After DNS reaches this host, run:

```sh
certbot certonly --webroot -w /var/www/laptops-acme -d laptops.pcpapc172.ir -d laptops-cf.pcpapc172.ir
```

Then install `server/deploy/nginx-domain.conf`, verify nginx configuration and reload. Add a certbot deploy hook that runs `nginx -t && systemctl reload nginx`; verify `certbot renew --dry-run`. Port 80 continues serving the ACME webroot, redirecting other requests to HTTPS.

Do not redirect old authenticated POST requests across hosts. Publish client updates that migrate the old IP endpoint to the new HTTPS domain only once health, certificate validation, login and restored counts are verified. A new recovery epoch prevents old cursors from hiding restored records. Keep any pending client work intact for review across the epoch change.
