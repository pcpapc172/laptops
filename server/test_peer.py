"""Second authenticated client used by the Qt automatic incoming-sync test."""
import argparse,json,uuid,urllib.request

def main():
    p=argparse.ArgumentParser();p.add_argument('--port',type=int,required=True);p.add_argument('--edit-item');args=p.parse_args()
    endpoint=f'http://127.0.0.1:{args.port}';token='';device=uuid.uuid4().hex
    def request(path,body):
        headers={'Content-Type':'application/json'}
        if token:headers['Authorization']='Bearer '+token
        with urllib.request.urlopen(urllib.request.Request(endpoint+path,json.dumps(body).encode(),headers),timeout=10) as response:return json.load(response)
    login=request('/v1/login',dict(username='admin' if args.edit_item else 'peer',password='integration-test-password',device_id=device));token=login['token']
    if args.edit_item:
        snapshot_request=urllib.request.Request(endpoint+'/v1/snapshot',headers={'Authorization':'Bearer '+token})
        with urllib.request.urlopen(snapshot_request,timeout=10) as response:snapshot=json.load(response)
        row=next(r for r in snapshot['entities'] if r['table']=='inventory_items' and r['entity_id']==args.edit_item)
        before=row['data'];after={**before,'location':'Peer shelf','description':'Peer description'}
        body=dict(id=uuid.uuid4().hex,device_id=device,user_id=login['user']['id'],entries=[dict(table='inventory_items',entity_id=args.edit_item,before=before,after=after,revision=row['revision'])])
        if request('/v1/batches',body)['status']!='accepted':raise RuntimeError('Peer metadata edit rejected')
        request('/v1/logout',{});return
    ident=uuid.uuid4().hex
    active=dict(bell_period=1,grade_level='Grade',teacher_name='Peer teacher',recipient_name='Peer recipient',laptop_number='PEER-LAPTOP',initial_condition='Healthy',return_condition=None,returner_name=None,lend_time='2026-10-11T10:00:00',return_time=None,status='ACTIVE',notes='Archive from another user')
    def submit(before,after,revision):
        body=dict(id=uuid.uuid4().hex,device_id=device,user_id=login['user']['id'],entries=[dict(table='loans',entity_id=ident,before=before,after=after,revision=revision)])
        if request('/v1/batches',body)['status']!='accepted':raise RuntimeError('Peer transaction was rejected')
    submit(None,active,0)
    returned={**active,'status':'RETURNED','return_condition':'Healthy','returner_name':'Peer returner','return_time':'2026-10-11T11:00:00'}
    submit(active,returned,1);request('/v1/logout',{})

if __name__=='__main__':main()
