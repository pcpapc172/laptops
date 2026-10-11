"""Loopback-only fixture for the real Qt network integration test."""
import argparse
import uvicorn
import json
from fastapi.responses import JSONResponse
from .app import create_app

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--db',required=True);p.add_argument('--port',type=int,required=True);a=p.parse_args()
    app=create_app(a.db)
    with app.state.store.connection() as db:exists=db.execute('SELECT COUNT(*) FROM users').fetchone()[0]
    if not exists:
        app.state.store.create_user('admin','Test admin','integration-test-password')
        app.state.store.create_user('peer','Test peer','integration-test-password','staff')
    dropped=False
    @app.middleware('http')
    async def lose_first_return_reply(request,call_next):
        global dropped
        operation=None
        if request.method=='POST' and request.url.path=='/v1/batches':
            operation=json.loads(await request.body()).get('operation')
        response=await call_next(request)
        if operation=='returnInventory' and not dropped and response.status_code==200:
            dropped=True
            return JSONResponse(status_code=503,content={'detail':'Test: committed return acknowledgement lost'})
        return response
    uvicorn.run(app,host='127.0.0.1',port=a.port,log_level='warning')
