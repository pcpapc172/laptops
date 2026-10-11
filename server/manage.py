"""Private administration commands. Passwords are read from a file, never command arguments."""
import argparse
import hashlib
import json
import os
import secrets
import sqlite3
import time
import uuid
from pathlib import Path
from .app import Store
from .domain import TABLES, REFERENCES, validate


def legacy_records(path):
    with sqlite3.connect(f'file:{Path(path).resolve()}?mode=ro', uri=True) as db:
        db.row_factory=sqlite3.Row
        state={}
        has_mapping = db.execute("SELECT COUNT(*) FROM sqlite_master WHERE name='sync_ids'").fetchone()[0]
        def mapped(table, ident):
            if has_mapping:
                row = db.execute('SELECT global_id FROM sync_ids WHERE table_name=? AND local_id=?',(table,ident)).fetchone()
                if row:return row[0]
            return legacy_id(table,ident)
        for table,fields in TABLES.items():
            for record in db.execute('SELECT * FROM '+table):
                row={field:record[field] for field in fields}
                for field,target in REFERENCES.get(table,{}).items():
                    row[field]=mapped(target,row[field]) if row[field] else None
                state[(table,mapped(table,record['id']))]=row
        validate(state)
        return state


def review_records(path, review_id):
    with sqlite3.connect(f'file:{Path(path).resolve()}?mode=ro',uri=True) as db:
        row=db.execute("SELECT payload FROM sync_batches WHERE id=? AND operation='legacy_review'",(review_id,)).fetchone()
        if not row:raise ValueError('Preserved legacy review was not found')
        records=json.loads(row[0]).get('entities',[])
        state={}
        for record in records:
            key=(record['table'],record['entity_id'])
            if key in state:raise ValueError('Duplicate record in preserved proposal')
            state[key]=record['data']
        if not state:raise ValueError('Preserved proposal is empty')
        validate(state)
        return state


def legacy_id(table,ident):
    return hashlib.sha256(f'laptops-legacy:{table}:{ident}'.encode()).hexdigest()[:32]


def backup(source,destination):
    Path(destination).parent.mkdir(parents=True,exist_ok=True)
    if Path(destination).exists(): raise ValueError('Backup destination already exists')
    with sqlite3.connect(source) as src,sqlite3.connect(destination) as dst:
        src.backup(dst)
    os.chmod(destination,0o600)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--db',required=True)
    sub=parser.add_subparsers(dest='command',required=True)
    bootstrap=sub.add_parser('bootstrap');bootstrap.add_argument('--credentials-file',required=True)
    imp=sub.add_parser('import');imp.add_argument('--legacy-db',required=True);imp.add_argument('--review-id')
    b=sub.add_parser('backup');b.add_argument('--output',required=True)
    r=sub.add_parser('restore');r.add_argument('--input',required=True)
    args=parser.parse_args()
    if args.command=='backup':
        backup(args.db,args.output);print('Backup completed');return
    if args.command=='restore':
        # Stop the service first. Retain all receipts/tombstones, but invalidate client cursors/sessions.
        backup(args.db,args.db+'.before-restore-'+str(int(time.time())))
        for suffix in ['-wal','-shm']:
            Path(args.db+suffix).unlink(missing_ok=True)
        with sqlite3.connect(args.input) as src,sqlite3.connect(args.db) as dst:src.backup(dst)
        store=Store(args.db)
        with store.transaction() as db:
            db.execute("UPDATE meta SET value=? WHERE key='instance_id'",(uuid.uuid4().hex,))
            db.execute('DELETE FROM sessions')
        print('Restore completed with new instance epoch; all clients must log in again');return
    store=Store(args.db)
    if args.command=='bootstrap':
        with store.connection() as db:
            if db.execute('SELECT COUNT(*) FROM users').fetchone()[0]:raise ValueError('Users already exist')
        password=secrets.token_urlsafe(24)
        file=Path(args.credentials_file);file.parent.mkdir(parents=True,exist_ok=True)
        fd=os.open(file,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'w') as out:out.write(json.dumps({'server':'https://13.42.221.242','username':'admin','password':password},indent=2)+'\n')
        store.create_user('admin','مدیر',password)
        print('Administrator created; credentials written to private file')
    elif args.command=='import':
        state=review_records(args.legacy_db,args.review_id) if args.review_id else legacy_records(args.legacy_db)
        with store.transaction() as db:
            if db.execute('SELECT COUNT(*) FROM entities').fetchone()[0]:raise ValueError('Server is not empty')
            cursor,_=store.commit_state(db,state,{},set(state))
            db.execute('INSERT INTO audit(user_id,device_id,batch_id,operation,result,time) VALUES(?,?,?,?,?,?)',('system','migration',uuid.uuid4().hex,'legacy_import','accepted',int(time.time())))
        print(json.dumps({'imported':len(state),'by_table':{t:sum(k[0]==t for k in state) for t in TABLES},'cursor':cursor}))

if __name__=='__main__':main()
