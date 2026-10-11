import copy
import uuid
from concurrent.futures import ThreadPoolExecutor
from fastapi.testclient import TestClient
from server.app import create_app


def uid(): return uuid.uuid4().hex


def setup(tmp_path):
    app = create_app(tmp_path / 'db.sqlite')
    app.state.store.create_user('admin', 'Admin', 'correct horse battery staple')
    c = TestClient(app)
    device = uid()
    auth = c.post('/v1/login', json={'username':'admin','password':'correct horse battery staple','device_id':device}).json()
    headers = {'Authorization':'Bearer '+auth['token']}
    return app,c,auth,device,headers


def submit(c,auth,device,headers,entries,ident=None):
    body={'id':ident or uid(),'device_id':device,'user_id':auth['user']['id'],'entries':entries}
    return c.post('/v1/batches',json=body,headers=headers),body


def entry(table,ident,before,after,revision=0):
    return dict(table=table,entity_id=ident,before=before,after=after,revision=revision)


def stock(c,auth,device,h,total=1):
    tid,iid=uid(),uid()
    t=dict(name='Cable',brand='',model='',description='')
    item=dict(type_id=tid,name='USB',brand='',model='',code='C1',description='',location='',total=total,low_stock_threshold=0)
    r,_=submit(c,auth,device,h,[entry('stock_types',tid,None,t),entry('inventory_items',iid,None,item)])
    assert r.json()['status']=='accepted'
    return tid,iid,item


def lending(iid,qty=1):
    return dict(item_id=iid,borrower='Person',quantity=qty,returned=0,missing=0,lend_time='2026-10-11T12:00:00',due_time=None,last_return_time=None,notes='')


def test_idempotency_conflict_and_restart(tmp_path):
    app,c,a,d,h=setup(tmp_path);tid,iid,item=stock(c,a,d,h)
    updated={**item,'name':'New name'}
    r,body=submit(c,a,d,h,[entry('inventory_items',iid,item,updated,1)])
    result=r.json(); assert result['status']=='accepted'
    # A lost acknowledgement is replayed after server restart without a second update.
    c2=TestClient(create_app(tmp_path/'db.sqlite'))
    assert c2.post('/v1/batches',json=body,headers=h).json()==result
    body['entries'][0]['after']['name']='Other'
    assert c2.post('/v1/batches',json=body,headers=h).status_code==409
    r,_=submit(c,a,d,h,[entry('inventory_items',iid,item,{**item,'name':'Stale'},1)])
    assert r.json()['code']=='STALE_REVISION'


def test_concurrent_last_unit(tmp_path):
    app,c,a,d,h=setup(tmp_path);_,iid,_=stock(c,a,d,h)
    def attempt(_): return submit(c,a,d,h,[entry('inventory_loans',uid(),None,lending(iid))])[0].json()
    with ThreadPoolExecutor(2) as pool: results=list(pool.map(attempt,range(2)))
    assert sorted(x['status'] for x in results)==['accepted','conflict']
    assert next(x for x in results if x['status']=='conflict')['code']=='INSUFFICIENT_STOCK'


def test_counter_merge_over_return_and_atomic_batch(tmp_path):
    app,c,a,d,h=setup(tmp_path);_,iid,_=stock(c,a,d,h,3)
    lid=uid();loan=lending(iid,3)
    assert submit(c,a,d,h,[entry('inventory_loans',lid,None,loan)])[0].json()['status']=='accepted'
    returned={**loan,'returned':1,'last_return_time':'2026-10-11T13:00:00'}
    for _ in range(3):
        assert submit(c,a,d,h,[entry('inventory_loans',lid,loan,returned,1)])[0].json()['status']=='accepted'
    eid=uid()
    event=dict(item_id=iid,action='RETURN',quantity=1,person='P',notes='',time='2026-10-11T13:00:00')
    r,_=submit(c,a,d,h,[entry('inventory_loans',lid,loan,returned,1),entry('inventory_events',eid,None,event)])
    assert r.json()['code']=='OVER_RETURN'
    snap=c.get('/v1/snapshot',headers=h).json()
    assert not any(x['entity_id']==eid for x in snap['entities'])


def test_roles_revocation_and_dependencies(tmp_path):
    app,c,a,d,h=setup(tmp_path);_,iid,item=stock(c,a,d,h)
    app.state.store.create_user('viewer','Viewer','correct horse battery staple','viewer')
    vd=uid();va=c.post('/v1/login',json=dict(username='viewer',password='correct horse battery staple',device_id=vd)).json()
    vh={'Authorization':'Bearer '+va['token']}
    assert submit(c,va,vd,vh,[entry('inventory_loans',uid(),None,lending(iid))])[0].json()['code']=='FORBIDDEN'
    parent,_=submit(c,a,d,h,[entry('inventory_items',iid,item,{**item,'name':'Stale'},99)])
    child=entry('inventory_items',iid,item,{**item,'name':'Child'},1);child['after_batch']=parent.json()['id']
    assert submit(c,a,d,h,[child])[0].json()['code']=='DEPENDENCY'
    # Unrelated work continues after a conflict.
    assert submit(c,a,d,h,[entry('inventory_loans',uid(),None,lending(iid))])[0].json()['status']=='accepted'
    app.state.store.create_user('staff','Staff','correct horse battery staple','staff')
    sd=uid();sa=c.post('/v1/login',json=dict(username='staff',password='correct horse battery staple',device_id=sd)).json();sh={'Authorization':'Bearer '+sa['token']}
    body=dict(username='staff',display_name='Staff',role='staff',active=False)
    assert c.put('/v1/users/'+sa['user']['id'],json=body,headers=h).status_code==200
    assert c.get('/v1/snapshot',headers=sh).status_code==401


def test_tombstones_feed_and_invalid_requests(tmp_path):
    app,c,a,d,h=setup(tmp_path)
    ident=uid(); row={'name':'A'}
    assert submit(c,a,d,h,[entry('preset_grades',ident,None,row)])[0].json()['status']=='accepted'
    assert submit(c,a,d,h,[entry('preset_grades',ident,row,None,1)])[0].json()['status']=='accepted'
    feed=c.get('/v1/changes',params={'instance_id':a['instance_id'],'after':0},headers=h).json()
    assert len(feed['transactions'])==2
    assert feed['transactions'][-1]['updates'][0]['deleted']
    assert c.get('/v1/changes',params={'instance_id':uid(),'after':0},headers=h).status_code==409
    assert submit(c,a,d,h,[entry('preset_grades',uid(),None,None)])[0].json()['code']=='INVALID_BATCH'
    assert c.post('/v1/login',content='{}',headers={'Content-Length':str(3*1024*1024)}).status_code==413


def test_backup_restore_epoch_and_receipts(tmp_path):
    import subprocess
    import sys
    from server.manage import backup
    app,c,a,d,h=setup(tmp_path);_,iid,item=stock(c,a,d,h)
    saved=tmp_path/'saved.db';backup(str(tmp_path/'db.sqlite'),str(saved))
    result=subprocess.run([sys.executable,'-m','server.manage','--db',str(tmp_path/'db.sqlite'),'restore','--input',str(saved)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    new=TestClient(create_app(tmp_path/'db.sqlite'))
    assert new.get('/v1/snapshot',headers=h).status_code==401
    login=new.post('/v1/login',json=dict(username='admin',password='correct horse battery staple',device_id=d)).json()
    assert login['instance_id']!=a['instance_id']
    snap=new.get('/v1/snapshot',headers={'Authorization':'Bearer '+login['token']}).json()
    assert len(snap['entities'])==2


def test_two_users_laptop_lending_edits_and_returns(tmp_path):
    app,c,a,d,h=setup(tmp_path)
    app.state.store.create_user('staff','Staff','correct horse battery staple','staff')
    device=uid();other=c.post('/v1/login',json=dict(username='staff',password='correct horse battery staple',device_id=device)).json();oh={'Authorization':'Bearer '+other['token']}
    laptop=dict(bell_period=1,grade_level='Grade',teacher_name='Teacher',recipient_name='Recipient',laptop_number='۱۲',initial_condition='Healthy',return_condition=None,returner_name=None,lend_time='2026-10-11T12:00:00',return_time=None,status='ACTIVE',notes='')
    ident=uid()
    assert submit(c,a,d,h,[entry('loans',ident,None,laptop)])[0].json()['status']=='accepted'
    # A different authenticated user/device cannot lend the same physical laptop with Latin digits.
    second={**laptop,'laptop_number':'12'}
    assert submit(c,other,device,oh,[entry('loans',uid(),None,second)])[0].json()['code']=='LAPTOP_BUSY'
    edited={**laptop,'notes':'Staff note'}
    assert submit(c,other,device,oh,[entry('loans',ident,laptop,edited,1)])[0].json()['status']=='accepted'
    assert submit(c,a,d,h,[entry('loans',ident,laptop,{**laptop,'notes':'Stale admin note'},1)])[0].json()['code']=='STALE_REVISION'
    returned={**edited,'status':'RETURNED','returner_name':'Returner','return_condition':'Healthy','return_time':'2026-10-11T13:00:00'}
    assert submit(c,other,device,oh,[entry('loans',ident,edited,returned,2)])[0].json()['status']=='accepted'
    assert submit(c,a,d,h,[entry('loans',uid(),None,second)])[0].json()['status']=='accepted'
    assert submit(c,other,device,oh,[entry('loans',ident,returned,None,3)])[0].json()['code']=='FORBIDDEN'
    assert submit(c,a,d,h,[entry('loans',ident,returned,None,3)])[0].json()['status']=='accepted'
