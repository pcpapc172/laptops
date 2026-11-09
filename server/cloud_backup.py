"""Private SQLite backups to Cloudflare R2; credentials are read from a file only."""
import argparse
import datetime as dt
import gzip
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import uuid


class R2:
    def __init__(self, config):
        account=config['account_id'];bucket=config['bucket']
        if len(account)!=32 or any(c not in '0123456789abcdef' for c in account):raise ValueError('Invalid Cloudflare account')
        if not bucket or '/' in bucket:raise ValueError('Invalid bucket')
        self.base=f'https://api.cloudflare.com/client/v4/accounts/{account}/r2/buckets/{bucket}'
        self.token=config['token']

    def request(self, method, key, data=None, content_type='application/octet-stream'):
        # The standard opener refuses redirecting credentials to another host.
        class NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self, req, fp, code, msg, headers, newurl):return None
        opener=urllib.request.build_opener(NoRedirect())
        url=self.base+'/objects/'+urllib.parse.quote(key,safe='/')
        headers={'Authorization':'Bearer '+self.token,'Content-Type':content_type,'User-Agent':'LaptopManagerBackup/4.1.0'}
        req=urllib.request.Request(url,data=data,headers=headers,method=method)
        try:
            with opener.open(req,timeout=90) as response:return response.read()
        except urllib.error.HTTPError as error:
            # Do not put tokens, full API responses, or database contents in logs.
            raise RuntimeError(f'Cloudflare R2 {method} failed with HTTP {error.code}') from None

    def upload_verified(self, key, data):
        response=json.loads(self.request('PUT',key,data))
        if not response.get('success'):raise RuntimeError('Cloudflare did not confirm backup upload')
        recovered=self.request('GET',key)
        if hashlib.sha256(recovered).digest()!=hashlib.sha256(data).digest():raise RuntimeError('Downloaded backup checksum differs')
        return recovered


def consistent_snapshot(source,destination):
    uri='file:'+urllib.parse.quote(str(Path(source).resolve()),safe='/')+'?mode=ro'
    with sqlite3.connect(uri,uri=True) as src,sqlite3.connect(destination) as dst:
        src.backup(dst)
        if dst.execute('PRAGMA integrity_check').fetchone()[0]!='ok':raise RuntimeError('Source backup integrity check failed')
    os.chmod(destination,0o600)


def verify_restore(compressed,directory):
    path=Path(directory)/'restore-check.db';path.write_bytes(gzip.decompress(compressed));os.chmod(path,0o600)
    with sqlite3.connect('file:'+str(path)+'?mode=ro',uri=True) as db:
        if db.execute('PRAGMA integrity_check').fetchone()[0]!='ok':raise RuntimeError('Cloud backup restore integrity check failed')
        # Require actual server data, not just a valid empty SQLite file.
        for table in ('entities','users','receipts','audit'):
            db.execute('SELECT COUNT(*) FROM '+table).fetchone()


def run(source,directory,config):
    directory=Path(directory);directory.mkdir(mode=0o700,parents=True,exist_ok=True)
    stamp=dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    name=f'server-{stamp}-{uuid.uuid4().hex[:8]}.db.gz'
    with tempfile.TemporaryDirectory(prefix='cloud-backup-',dir=directory) as work:
        dbfile=Path(work)/'snapshot.db';consistent_snapshot(source,dbfile)
        compressed=gzip.compress(dbfile.read_bytes(),compresslevel=9,mtime=0)
        local=directory/name;local.write_bytes(compressed);os.chmod(local,0o600)
        client=R2(config);key='daily/'+name
        restored=client.upload_verified(key,compressed)
        verify_restore(restored,work)
        status={'completed_at':dt.datetime.now(dt.timezone.utc).isoformat(),'bucket':config['bucket'],'object':key,'bytes':len(compressed),'sha256':hashlib.sha256(compressed).hexdigest(),'download_verified':True,'sqlite_restore_verified':True}
        manifest=json.dumps(status,indent=2).encode()
        client.upload_verified(key+'.json',manifest)
        # Update success status only after both the DB and its manifest are verified.
        temp=directory/'last-cloud-success.tmp';temp.write_bytes(manifest);os.chmod(temp,0o600);temp.replace(directory/'last-cloud-success.json')
        return status


def main():
    os.umask(0o077)
    parser=argparse.ArgumentParser();parser.add_argument('--db',required=True);parser.add_argument('--directory',required=True);parser.add_argument('--config',required=True);args=parser.parse_args()
    config=json.loads(Path(args.config).read_text())
    result=run(args.db,args.directory,config)
    print(json.dumps(result))


if __name__=='__main__':main()
