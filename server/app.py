"""Laptop Manager HTTPS API. The reverse proxy terminates TLS; only it reaches this process."""
import hashlib
import json
import os
import secrets
import sqlite3
import time
import uuid
import threading
from contextlib import contextmanager
from pathlib import Path

from fastapi import FastAPI, Header, HTTPException, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field, field_validator
from pwdlib import PasswordHash

from .domain import Conflict, TABLES, apply_entries, identity, validate

PROTOCOL = 1
PASSWORDS = PasswordHash.recommended()
LOGIN_SLOTS = threading.BoundedSemaphore(2)
DUMMY_HASH = PASSWORDS.hash(secrets.token_urlsafe(32))


def json_text(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':'))

class Login(BaseModel):
    username: str = Field(min_length=1, max_length=100)
    password: str = Field(min_length=1, max_length=200)
    device_id: str = Field(pattern=r'^[0-9a-f]{32}$')

class UserInput(BaseModel):
    username: str = Field(min_length=1, max_length=100)
    display_name: str = Field(min_length=1, max_length=100)
    role: str = Field(pattern='^(admin|staff|viewer)$')
    password: str | None = Field(default=None, min_length=12, max_length=200)
    active: bool = True

class Batch(BaseModel):
    id: str = Field(pattern=r'^[0-9a-f]{32}$')
    device_id: str = Field(pattern=r'^[0-9a-f]{32}$')
    user_id: str = Field(pattern=r'^[0-9a-f]{32}$')
    protocol: int = PROTOCOL
    operation: str = Field(default='change', max_length=100)
    entries: list[dict] = Field(min_length=1, max_length=100)

    @field_validator('entries')
    @classmethod
    def entry_types(cls, entries):
        for entry in entries:
            if not isinstance(entry.get('table'), str) or not isinstance(entry.get('entity_id'), str):
                raise ValueError('table and entity_id must be strings')
            for field in ['before', 'after']:
                if entry.get(field) is not None and not isinstance(entry[field], dict):
                    raise ValueError('before and after must be objects or null')
        return entries

class Store:
    def __init__(self, path):
        self.path = str(path)
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with self.connection() as db:
            db.executescript('''
                PRAGMA journal_mode=WAL;
                CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
                CREATE TABLE IF NOT EXISTS users(id TEXT PRIMARY KEY,username TEXT UNIQUE NOT NULL,display_name TEXT NOT NULL,
                    role TEXT NOT NULL,password_hash TEXT NOT NULL,active INTEGER NOT NULL DEFAULT 1);
                CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY,user_id TEXT NOT NULL REFERENCES users(id),
                    device_id TEXT NOT NULL,expires INTEGER NOT NULL);
                CREATE TABLE IF NOT EXISTS entities(table_name TEXT NOT NULL,id TEXT NOT NULL,revision INTEGER NOT NULL,
                    deleted INTEGER NOT NULL,body TEXT,PRIMARY KEY(table_name,id));
                CREATE TABLE IF NOT EXISTS receipts(id TEXT PRIMARY KEY,user_id TEXT NOT NULL,device_id TEXT NOT NULL,
                    payload_hash TEXT NOT NULL,result TEXT NOT NULL,created INTEGER NOT NULL);
                CREATE TABLE IF NOT EXISTS changes(seq INTEGER PRIMARY KEY,updates TEXT NOT NULL);
                CREATE TABLE IF NOT EXISTS audit(seq INTEGER PRIMARY KEY AUTOINCREMENT,user_id TEXT NOT NULL,device_id TEXT NOT NULL,
                    batch_id TEXT NOT NULL,operation TEXT NOT NULL,result TEXT NOT NULL,time INTEGER NOT NULL);
                CREATE TABLE IF NOT EXISTS login_failures(peer TEXT NOT NULL,username TEXT NOT NULL,time INTEGER NOT NULL);
                CREATE INDEX IF NOT EXISTS idx_login_failures ON login_failures(peer,username,time);
            ''')
            columns={row[1] for row in db.execute('PRAGMA table_info(changes)')}
            for column in ['batch_id','user_id','device_id']:
                if column not in columns:db.execute('ALTER TABLE changes ADD COLUMN '+column+' TEXT')
            db.execute("INSERT OR IGNORE INTO meta VALUES('instance_id',?)", (uuid.uuid4().hex,))
            db.execute("INSERT OR IGNORE INTO meta VALUES('cursor','0')")
            db.commit()
        os.chmod(self.path, 0o600)

    @contextmanager
    def connection(self):
        db = sqlite3.connect(self.path, timeout=10, isolation_level=None)
        db.row_factory = sqlite3.Row
        db.execute('PRAGMA foreign_keys=ON'); db.execute('PRAGMA synchronous=FULL')
        try:
            yield db
        finally:
            db.close()

    @contextmanager
    def transaction(self):
        with self.connection() as db:
            db.execute('BEGIN IMMEDIATE')
            try:
                yield db
                db.commit()
            except BaseException:
                db.rollback()
                raise

    def instance(self, db):
        return db.execute("SELECT value FROM meta WHERE key='instance_id'").fetchone()[0]

    def create_user(self, username, display_name, password, role='admin'):
        if role not in {'admin', 'staff', 'viewer'} or len(password) < 12:
            raise ValueError('A valid role and a password of at least 12 characters are required')
        hashed = PASSWORDS.hash(password)
        with self.transaction() as db:
            db.execute('INSERT INTO users VALUES(?,?,?,?,?,1)', (uuid.uuid4().hex, username.strip(), display_name, role, hashed))

    def user(self, db, token):
        if not token:
            raise HTTPException(401, 'ورود به حساب لازم است.')
        hashed = hashlib.sha256(token.encode()).hexdigest()
        row = db.execute('''SELECT u.*,s.device_id FROM sessions s JOIN users u ON u.id=s.user_id
                            WHERE s.token_hash=? AND s.expires>? AND u.active=1''', (hashed, int(time.time()))).fetchone()
        if row is None:
            raise HTTPException(401, 'نشست منقضی یا لغو شده است؛ دوباره وارد شوید.')
        return dict(row)

    def state(self, db):
        rows = db.execute('SELECT * FROM entities').fetchall()
        state = {(r['table_name'], r['id']): json.loads(r['body']) for r in rows if not r['deleted']}
        versions = {(r['table_name'], r['id']): r['revision'] for r in rows}
        return state, versions

    def commit_state(self, db, state, versions, touched, origin=None):
        updates = []
        for table, entity_id in sorted(touched):
            body = state.get((table, entity_id))
            revision = versions.get((table, entity_id), 0) + 1
            deleted = body is None
            db.execute('INSERT OR REPLACE INTO entities VALUES(?,?,?,?,?)',
                       (table, entity_id, revision, int(deleted), json_text(body) if body is not None else None))
            updates.append({'table': table, 'entity_id': entity_id, 'revision': revision, 'deleted': deleted, 'data': body})
        cursor = int(db.execute("SELECT value FROM meta WHERE key='cursor'").fetchone()[0]) + 1
        db.execute("UPDATE meta SET value=? WHERE key='cursor'", (str(cursor),))
        origin=origin or {}
        db.execute('INSERT INTO changes(seq,updates,batch_id,user_id,device_id) VALUES(?,?,?,?,?)',
                   (cursor,json_text(updates),origin.get('batch_id'),origin.get('user_id'),origin.get('device_id')))
        return cursor, updates

    def apply(self, batch, token):
        payload = batch.model_dump()
        digest = hashlib.sha256(json_text(payload).encode()).hexdigest()
        with self.transaction() as db:
            user = self.user(db, token)
            if batch.user_id != user['id'] or batch.device_id != user['device_id']:
                raise HTTPException(403, 'تغییرات متعلق به حساب یا دستگاه دیگری است.')
            if batch.protocol != PROTOCOL:
                raise HTTPException(426, 'نسخه برنامه با سرور سازگار نیست.')
            receipt = db.execute('SELECT * FROM receipts WHERE id=?', (batch.id,)).fetchone()
            if receipt:
                if receipt['user_id'] != user['id'] or receipt['device_id'] != batch.device_id or receipt['payload_hash'] != digest:
                    raise HTTPException(409, 'شناسه تراکنش با محتوای دیگری استفاده شده است.')
                return json.loads(receipt['result'])
            state, versions = self.state(db)
            def expected(entry, key):
                parent = entry.get('after_batch')
                if not parent:
                    value = entry.get('revision', 0)
                    if type(value) is not int or value < 0:
                        raise Conflict('INVALID_REVISION', 'نسخه رکورد معتبر نیست.')
                    return value
                identity(parent)
                previous = db.execute('SELECT * FROM receipts WHERE id=?', (parent,)).fetchone()
                if not previous or previous['user_id'] != user['id'] or previous['device_id'] != batch.device_id:
                    raise Conflict('DEPENDENCY', 'تغییر قبلی هنوز توسط سرور پذیرفته نشده است.')
                result = json.loads(previous['result'])
                if result['status'] != 'accepted':
                    raise Conflict('DEPENDENCY', 'تغییر قبلی با تعارض روبرو شده است؛ این تغییر هم نیاز به بررسی دارد.')
                for update in result['updates']:
                    if (update['table'], update['entity_id']) == key:
                        return update['revision']
                raise Conflict('DEPENDENCY', 'نسخه تغییر وابسته پیدا نشد.')
            try:
                # Dependencies are checked even for counter deltas and new dependent rows.
                for entry in batch.entries:
                    if entry.get('after_batch'):
                        expected(entry, (entry.get('table'), entry.get('entity_id')))
                candidate, touched = apply_entries(state, versions, batch.entries, user['role'], expected)
                cursor, updates = self.commit_state(db, candidate, versions, touched, {'batch_id':batch.id,'user_id':user['id'],'device_id':batch.device_id})
                result = {'status': 'accepted', 'id': batch.id, 'cursor': cursor, 'updates': updates}
            except Conflict as exc:
                current = []
                for entry in batch.entries:
                    key = (entry.get('table'), entry.get('entity_id'))
                    if key[0] in TABLES:
                        current.append({'table': key[0], 'entity_id': key[1], 'revision': versions.get(key, 0),
                                        'deleted': key not in state, 'data': state.get(key)})
                result = {'status': 'conflict', 'id': batch.id, 'code': exc.code, 'message': exc.message, 'updates': current}
            db.execute('INSERT INTO receipts VALUES(?,?,?,?,?,?)',
                       (batch.id, user['id'], batch.device_id, digest, json_text(result), int(time.time())))
            db.execute('INSERT INTO audit(user_id,device_id,batch_id,operation,result,time) VALUES(?,?,?,?,?,?)',
                       (user['id'], batch.device_id, batch.id, batch.operation, result['status'], int(time.time())))
            return result


def create_app(path=None):
    store = Store(path or os.environ.get('LAPTOPS_SERVER_DB', '/var/lib/laptop-manager/server.db'))
    app = FastAPI(title='Laptop Manager Server', version='4.0.0', docs_url=None, redoc_url=None, openapi_url=None)
    app.state.store = store

    @app.middleware('http')
    async def request_limits(request, call_next):
        try:
            if int(request.headers.get('content-length', '0')) > 2 * 1024 * 1024:
                return JSONResponse(status_code=413, content={'detail': 'درخواست بیش از حد بزرگ است.'})
        except ValueError:
            return JSONResponse(status_code=400, content={'detail': 'طول درخواست معتبر نیست.'})
        return await call_next(request)

    @app.exception_handler(sqlite3.OperationalError)
    async def unavailable(request, exc):
        return JSONResponse(status_code=503, content={'detail': 'پایگاه داده موقتاً در دسترس نیست؛ دوباره تلاش کنید.'})

    def bearer(authorization):
        return authorization[7:] if authorization and authorization.startswith('Bearer ') else ''

    @app.get('/health')
    def health():
        with store.connection() as db:
            db.execute('SELECT 1')
        return {'status': 'ok', 'version': '4.0.0', 'edit_policy': 'changed_fields_last_writer', 'protocol': PROTOCOL}

    @app.post('/v1/login')
    def login(body: Login, request: Request):
        body.username = body.username.strip()
        peer = request.client.host if request.client else 'unknown'
        # The loopback proxy supplies this trusted header; public clients cannot reach the API directly.
        if peer in {'127.0.0.1', '::1'}:
            peer = request.headers.get('x-real-ip', peer)
        with store.connection() as db:
            failures = db.execute('SELECT COUNT(*) FROM login_failures WHERE peer=? AND username=? AND time>?',
                                  (peer, body.username, int(time.time()) - 900)).fetchone()[0]
            user = db.execute('SELECT * FROM users WHERE username=?', (body.username.strip(),)).fetchone()
        if failures >= 5:
            raise HTTPException(429, 'تلاش‌های ورود زیاد است؛ ۱۵ دقیقه دیگر تلاش کنید.')
        if not LOGIN_SLOTS.acquire(blocking=False):
            raise HTTPException(429, 'سرور در حال بررسی ورود است؛ دوباره تلاش کنید.')
        try:
            verified_hash = user['password_hash'] if user else DUMMY_HASH
            valid = PASSWORDS.verify(body.password, verified_hash)
        finally:
            LOGIN_SLOTS.release()
        if not valid or not user or not user['active']:
            with store.transaction() as db:
                db.execute('INSERT INTO login_failures VALUES(?,?,?)', (peer, body.username, int(time.time())))
            raise HTTPException(401, 'نام کاربری یا رمز ورود صحیح نیست.')
        token = secrets.token_urlsafe(48)
        with store.transaction() as db:
            db.execute('DELETE FROM login_failures WHERE time<? OR (peer=? AND username=?)', (int(time.time()) - 86400, peer, body.username))
            # Recheck revocation between password verification and issuing the session.
            user = db.execute('SELECT * FROM users WHERE id=? AND active=1', (user['id'],)).fetchone()
            if not user or user['password_hash'] != verified_hash:
                raise HTTPException(401, 'حساب غیرفعال شده است.')
            db.execute('DELETE FROM sessions WHERE expires<?', (int(time.time()),))
            db.execute('INSERT INTO sessions VALUES(?,?,?,?)',
                       (hashlib.sha256(token.encode()).hexdigest(), user['id'], body.device_id, int(time.time()) + 43200))
            instance = store.instance(db)
        return {'token': token, 'user': {k: user[k] for k in ['id', 'username', 'display_name', 'role']},
                'instance_id': instance, 'edit_policy': 'changed_fields_last_writer', 'protocol': PROTOCOL, 'expires_in': 43200}

    @app.post('/v1/logout')
    def logout(authorization: str | None = Header(default=None)):
        with store.transaction() as db:
            db.execute('DELETE FROM sessions WHERE token_hash=?', (hashlib.sha256(bearer(authorization).encode()).hexdigest(),))
        return {'ok': True}

    @app.post('/v1/batches')
    def batches(body: Batch, authorization: str | None = Header(default=None)):
        return store.apply(body, bearer(authorization))

    @app.get('/v1/snapshot')
    def snapshot(authorization: str | None = Header(default=None)):
        with store.connection() as db:
            db.execute('BEGIN')
            user = store.user(db, bearer(authorization))
            rows = db.execute('SELECT * FROM entities').fetchall()
            return {'instance_id': store.instance(db), 'edit_policy': 'changed_fields_last_writer', 'protocol': PROTOCOL,
                    'cursor': int(db.execute("SELECT value FROM meta WHERE key='cursor'").fetchone()[0]),
                    'role': user['role'], 'entities': [{'table': r['table_name'], 'entity_id': r['id'], 'revision': r['revision'],
                        'deleted': bool(r['deleted']), 'data': json.loads(r['body']) if r['body'] else None} for r in rows]}

    @app.get('/v1/changes')
    def changes(instance_id: str, after: int = 0, authorization: str | None = Header(default=None)):
        with store.connection() as db:
            db.execute('BEGIN'); user = store.user(db, bearer(authorization))
            latest = int(db.execute("SELECT value FROM meta WHERE key='cursor'").fetchone()[0])
            if instance_id != store.instance(db) or after < 0 or after > latest:
                raise HTTPException(409, 'نسخه سرور تغییر کرده است؛ دریافت کامل اطلاعات لازم است.')
            rows = db.execute('SELECT * FROM changes WHERE seq>? ORDER BY seq LIMIT 200', (after,)).fetchall()
            return {'instance_id': instance_id, 'cursor': rows[-1]['seq'] if rows else after, 'has_more': bool(rows and rows[-1]['seq'] < latest),
                    'role': user['role'], 'transactions': [{'cursor':r['seq'],'updates':json.loads(r['updates']),'batch_id':r['batch_id'],'user_id':r['user_id'],'device_id':r['device_id']} for r in rows]}

    @app.get('/v1/users')
    def users(authorization: str | None = Header(default=None)):
        with store.connection() as db:
            user = store.user(db, bearer(authorization))
            if user['role'] != 'admin': raise HTTPException(403, 'دسترسی مدیر لازم است.')
            return [dict(r) for r in db.execute('SELECT id,username,display_name,role,active FROM users ORDER BY username')]

    @app.post('/v1/users')
    def add_user(body: UserInput, authorization: str | None = Header(default=None)):
        if not body.password: raise HTTPException(422, 'رمز ورود الزامی است.')
        if not body.username.strip() or not body.display_name.strip(): raise HTTPException(422, 'نام کاربری و نام نمایشی الزامی است.')
        password = PASSWORDS.hash(body.password)
        with store.transaction() as db:
            actor = store.user(db, bearer(authorization))
            if actor['role'] != 'admin': raise HTTPException(403, 'دسترسی مدیر لازم است.')
            try:
                user_id = uuid.uuid4().hex
                db.execute('INSERT INTO users VALUES(?,?,?,?,?,?)', (user_id, body.username.strip(), body.display_name, body.role, password, int(body.active)))
            except sqlite3.IntegrityError:
                raise HTTPException(409, 'نام کاربری قبلاً استفاده شده است.')
        return {'id': user_id}

    @app.put('/v1/users/{user_id}')
    def edit_user(user_id: str, body: UserInput, authorization: str | None = Header(default=None)):
        if not body.username.strip() or not body.display_name.strip(): raise HTTPException(422, 'نام کاربری و نام نمایشی الزامی است.')
        password = PASSWORDS.hash(body.password) if body.password else None
        with store.transaction() as db:
            actor = store.user(db, bearer(authorization))
            if actor['role'] != 'admin': raise HTTPException(403, 'دسترسی مدیر لازم است.')
            if user_id == actor['id'] and (body.role != 'admin' or not body.active):
                raise HTTPException(409, 'مدیر نمی‌تواند دسترسی حساب خودش را حذف کند.')
            row = db.execute('SELECT * FROM users WHERE id=?', (user_id,)).fetchone()
            if not row: raise HTTPException(404, 'کاربر پیدا نشد.')
            if row['role'] == 'admin' and (body.role != 'admin' or not body.active):
                if db.execute("SELECT COUNT(*) FROM users WHERE role='admin' AND active=1").fetchone()[0] <= 1:
                    raise HTTPException(409, 'حداقل یک مدیر فعال باید باقی بماند.')
            try:
                db.execute('UPDATE users SET username=?,display_name=?,role=?,password_hash=?,active=? WHERE id=?',
                           (body.username.strip(), body.display_name, body.role, password or row['password_hash'], int(body.active), user_id))
            except sqlite3.IntegrityError: raise HTTPException(409, 'نام کاربری تکراری است.')
            db.execute('DELETE FROM sessions WHERE user_id=?', (user_id,))
        return {'ok': True}

    @app.post('/v1/import')
    def import_records(body: dict, authorization: str | None = Header(default=None)):
        with store.transaction() as db:
            actor = store.user(db, bearer(authorization))
            if actor['role'] != 'admin': raise HTTPException(403, 'دسترسی مدیر لازم است.')
            if db.execute('SELECT COUNT(*) FROM entities').fetchone()[0]:
                raise HTTPException(409, 'وارد کردن دیتابیس فقط به سرور خالی مجاز است.')
            records = body.get('entities', [])
            if len(records) > 100000: raise HTTPException(413, 'تعداد رکورد بیش از حد است.')
            state = {}
            try:
                for record in records:
                    key = (record['table'], identity(record['entity_id']))
                    if key in state: raise Conflict('DUPLICATE_ID', 'شناسه تکراری است.')
                    state[key] = record['data']
                validate(state)
            except (KeyError, TypeError): raise HTTPException(422, 'فایل وارد شده معتبر نیست.')
            except Conflict as exc: raise HTTPException(409, exc.message)
            cursor, _ = store.commit_state(db, state, {}, set(state))
            db.execute('INSERT INTO audit(user_id,device_id,batch_id,operation,result,time) VALUES(?,?,?,?,?,?)',
                       (actor['id'], actor['device_id'], uuid.uuid4().hex, 'legacy_import', 'accepted', int(time.time())))
            return {'imported': len(state), 'cursor': cursor}

    return app
