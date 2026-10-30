from pathlib import Path
import json
import locale
import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox
from runtime import Runtime, SingleInstance, ROLES
from environment import load_config, preflight
from windows import rectangles, work_area, place, focus

ROOT = Path(__file__).resolve().parents[1]

class Application:
    def __init__(self):
        self.config = load_config(ROOT)
        issues = preflight(ROOT, self.config)
        if issues:
            raise RuntimeError('\n\n'.join(issues))
        self.guard = SingleInstance(self.config['http_endpoint'])
        self.runtime = Runtime(ROOT, self.config)
        self.catalog = json.loads((ROOT/'Scripts/P4/localization.json').read_text(encoding='utf-8-sig'))
        self.language = 'zh-Hans' if (locale.getlocale()[0] or '').lower().startswith(('zh','chinese')) else 'en'
        self.busy = False; self.polling = False; self.closing = False; self.placed = set(); self.messages = queue.Queue()
        self.root = tk.Tk(); self.root.title('Drone Security Launcher'); self.root.geometry('850x530')
        self.root.minsize(780,510)
        self.root.configure(bg='#0c121b')
        self.root.protocol('WM_DELETE_WINDOW', self.close)
        self.labels = {}
        self.title = self.label(0, 'Stage1.Name', 18)
        self.state = self.label(1, 'Launcher.STOPPED', 16)
        self.mode = self.label(2, 'Launcher.Layout', 11)
        self.session = self.label(3, 'Launcher.ModeUnset', 11)
        self.buttons = []
        for i, role in enumerate(('Backend',)+ROLES):
            text = tk.Label(self.root, bg='#131c28', fg='#ebf3fa', anchor='w', font=('Segoe UI',12))
            text.grid(row=i+4,column=0,padx=12,pady=4,sticky='ew');self.labels[role]=text
            self.button(i+4,1,'Launcher.Restart',lambda r=role:self.action(lambda:self.runtime.restart(r)))
            if role in ROLES:
                self.button(i+4,2,'Launcher.Focus',lambda r=role:self.focus(r))
            self.button(i+4,3,'Launcher.Stop',lambda r=role:self.action(lambda:self.runtime.stop_role(r)))
        for row,items in ((8,(('Launcher.StartDemo',lambda:self.start('demo')),
                              ('Launcher.StartRealtime',lambda:self.start('realtime')))),
                          (9,(('Launcher.Arrange',self.arrange),('Launcher.Shutdown',self.shutdown)))):
            bar=tk.Frame(self.root,bg='#0c121b')
            bar.grid(row=row,column=0,columnspan=4,sticky='ew')
            for column,(key,callback) in enumerate(items):
                bar.columnconfigure(column,weight=1,uniform='actions')
                self.button(0,column,key,callback,parent=bar)
        self.error = tk.Label(self.root,bg='#0c121b',fg='#e7666e',wraplength=820,font=('Segoe UI',10))
        self.error.grid(row=10,column=0,columnspan=4,padx=12,pady=8,sticky='w')
        self.root.columnconfigure(0,weight=1)
        self.root.after(300,self.tick)
        if self.config.get('auto_start',False):
            self.root.after(500,lambda:self.start(self.config.get('startup_mode','demo')))
    def text(self,key):return self.catalog[key][1 if self.language=='zh-Hans' else 0]
    def label(self,row,key,size):
        w=tk.Label(self.root,text=self.text(key),bg='#0c121b',fg='#ebf3fa',anchor='w',font=('Segoe UI',size))
        w.grid(row=row,column=0,columnspan=4,padx=12,pady=5,sticky='ew');return w
    def button(self,row,column,key,action,columnspan=1,parent=None):
        w=tk.Button(parent or self.root,text=self.text(key),command=action,bg='#1b2837',fg='#ebf3fa',relief='flat',padx=10,pady=5)
        w.grid(row=row,column=column,columnspan=columnspan,padx=5,pady=4,sticky='ew');self.buttons.append((w,key))
    def start(self,mode):
        self.action(lambda:self.runtime.start(mode))
    def update_buttons(self):
        live_processes=any(p.process.poll() is None for p in list(self.runtime.owned.values()))
        can_start=self.runtime.state in ('STOPPED','FAILED') and not live_processes and not self.closing
        for button,key in self.buttons:
            disabled=(self.busy and not self.polling) or (key in ('Launcher.StartDemo','Launcher.StartRealtime') and not can_start)
            disabled=disabled or (self.runtime.launch_mode is None and key in ('Launcher.Restart','Launcher.Stop','Launcher.Focus'))
            disabled=disabled or (self.runtime.state in ('STOPPED','FAILED') and key in ('Launcher.Restart','Launcher.Focus'))
            button.config(text=self.text(key),state='disabled' if disabled else 'normal')
    def action(self, callback, clear_error=True):
        if self.busy:
            if self.polling and clear_error:self.root.after(100, lambda:self.action(callback, clear_error))
            return
        self.busy=True
        self.polling=not clear_error
        self.update_buttons()
        if clear_error:self.error.config(text='')
        def run():
            try:callback()
            except Exception as exc:self.messages.put(str(exc))
            finally:self.polling=False;self.busy=False
        threading.Thread(target=run,daemon=True).start()
    def arrange(self):
        self.placed.clear();self.place_ready()
    def place_ready(self):
        rects=rectangles(work_area())
        for role in ROLES:
            p=self.runtime.owned.get(role)
            if p and p.process.poll() is None and p.process.pid not in self.placed and self.runtime.components[role]=='ONLINE':
                if place(p.process.pid,rects[role],'Drone Security | '+role):
                    self.placed.add(p.process.pid);self.runtime.log('window_placed',role=role,rect=rects[role],pid=p.process.pid)
    def focus(self,role):
        p=self.runtime.owned.get(role)
        if p and p.process.poll() is None:focus(p.process.pid)
    def tick(self):
        if self.closing and not self.busy:
            if self.runtime.state=='STOPPED':self.guard.close();self.root.destroy();return
            self.closing=False
        while not self.messages.empty():self.error.config(text=self.text('Launcher.Error')+' '+self.messages.get())
        snap=self.runtime.snapshot
        if snap:self.language=snap['preferences']['language']
        self.title.config(text=self.text('Stage1.Name'))
        self.mode.config(text=self.text('Launcher.Layout'))
        self.state.config(text=self.text('Launcher.'+self.runtime.state))
        mode_key={'demo':'Launcher.ModeDemo','realtime':'Launcher.ModeRealtime'}.get(self.runtime.launch_mode,'Launcher.ModeUnset')
        session_text=self.text(mode_key)
        location_key={'waiting':'Launcher.LocationWaiting','computer':'Launcher.LocationComputer',
                      'estimated':'Launcher.LocationComputerEstimated'}.get(self.runtime.location_status)
        if self.runtime.location_status=='ready':
            location_key='Launcher.LocationComputerReady' if self.runtime.location_source=='computer' else 'Launcher.LocationReady'
        if location_key:session_text+='  |  '+self.text(location_key)
        self.session.config(text=session_text)
        for role,label in self.labels.items():
            value=self.runtime.components[role]
            detail=self.text('Launcher.Existing') if role=='Backend' and self.runtime.existing_backend else self.text('Launcher.'+value)
            label.config(text=role+'    '+detail,fg='#58c397' if value=='ONLINE' else '#ebb453')
        self.update_buttons()
        if not self.busy:
            self.place_ready()
            self.action(self.runtime.poll, clear_error=False)
        self.root.after(1000,self.tick)
    def shutdown(self):
        try:active=self.runtime.has_active_execution() if self.runtime.state!='STOPPED' else False
        except Exception:active=bool(self.runtime.snapshot and any(e.get('state') not in ('COMPLETED','ABORTED','FAILED') for e in self.runtime.snapshot['plans'].get('executions',{}).values()))
        confirmation='Launcher.RealtimeShutdownConfirm' if self.runtime.launch_mode=='realtime' else 'Execution.ShutdownConfirm'
        if active and not messagebox.askokcancel(self.text('Launcher.Shutdown'),self.text(confirmation),parent=self.root):return False
        self.action(self.runtime.stop);return True
    def close(self):
        if self.busy:
            self.runtime.stopping.set();self.root.after(500,self.close);return
        if self.shutdown():self.closing=True
    def run(self):self.root.mainloop()

if __name__=='__main__':
    try:Application().run()
    except Exception as exc:
        (ROOT/'Saved').mkdir(exist_ok=True)
        with (ROOT/'Saved/launcher-error.log').open('a',encoding='utf-8') as f:f.write(str(exc)+'\n')
        messagebox.showerror('Drone Security Launcher',str(exc))
