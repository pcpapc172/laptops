"""Loopback-only fixture for the real Qt network integration test."""
import argparse
import uvicorn
from .app import create_app

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--db',required=True);p.add_argument('--port',type=int,required=True);a=p.parse_args()
    app=create_app(a.db)
    with app.state.store.connection() as db:exists=db.execute('SELECT COUNT(*) FROM users').fetchone()[0]
    if not exists:app.state.store.create_user('admin','Test admin','integration-test-password')
    uvicorn.run(app,host='127.0.0.1',port=a.port,log_level='warning')
