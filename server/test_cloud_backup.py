import gzip
import json
import sqlite3
from pathlib import Path
from unittest.mock import patch
import pytest
from server.app import Store
from server.cloud_backup import run


def test_backup_upload_download_restore_preserves_all_server_tables(tmp_path):
    source=tmp_path/'server.db';store=Store(source);store.create_user('admin','Admin','integration-test-password')
    with store.transaction() as db:
        db.execute("INSERT INTO entities VALUES('preset_grades','aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',1,0,?)",(json.dumps({'name':'Grade'}),))
    objects={}
    def transfer(self,method,key,data=None,content_type='application/octet-stream'):
        if method=='PUT':objects[key]=data;return b'{"success":true}'
        return objects[key]
    with patch('server.cloud_backup.R2.request',transfer):
        result=run(source,tmp_path/'backups',{'account_id':'a'*32,'bucket':'laptop-private','token':'test-only'})
    assert result['download_verified'] and result['sqlite_restore_verified']
    assert len(objects)==2
    restored=tmp_path/'restored.db';restored.write_bytes(gzip.decompress(objects[result['object']]))
    with sqlite3.connect(restored) as db:
        assert db.execute('SELECT username FROM users').fetchone()[0]=='admin'
        assert db.execute('SELECT COUNT(*) FROM entities').fetchone()[0]==1
    assert (tmp_path/'backups/last-cloud-success.json').exists()


def test_corrupt_download_never_reports_success(tmp_path):
    source=tmp_path/'server.db';Store(source)
    def transfer(self,method,key,data=None,content_type='application/octet-stream'):
        return b'{"success":true}' if method=='PUT' else b'corrupted'
    with patch('server.cloud_backup.R2.request',transfer),pytest.raises(RuntimeError,match='checksum'):
        run(source,tmp_path/'backups',{'account_id':'a'*32,'bucket':'laptop-private','token':'test-only'})
    assert not (tmp_path/'backups/last-cloud-success.json').exists()
    assert list((tmp_path/'backups').glob('server-*.db.gz'))
