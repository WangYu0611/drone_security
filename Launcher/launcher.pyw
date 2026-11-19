from pathlib import Path
import json
import locale
import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, filedialog, ttk
from runtime import Runtime, SingleInstance, ROLES, PILOT_IDENTITIES
from environment import load_config, preflight
from windows import rectangles, work_area, place, focus
from deployment import (DEPLOYMENT_SELECTIONS, PILOT_ROLE, deployment_config,
                        load_deployment_selection, save_deployment_selection)

ROOT = Path(__file__).resolve().parents[1]

class Application:
    def __init__(self):
        self.config = load_config(ROOT)
        self.guard = SingleInstance('launcher:' + str(ROOT.resolve()))
        idle_config = {**self.config, 'runtime_directory': 'Saved/Launcher/Idle'}
        idle_config.pop('launch_mode', None)
        self.runtime = Runtime(ROOT, idle_config)
        self.catalog = json.loads((ROOT/'Scripts/P4/localization.json').read_text(encoding='utf-8-sig'))
        self.language = 'zh-Hans' if (locale.getlocale()[0] or '').lower().startswith(('zh','chinese')) else 'en'
        self.busy = False; self.polling = False; self.closing = False; self.placed = set(); self.messages = queue.Queue()
        self.root = tk.Tk(); self.root.title('Drone Security Launcher'); self.root.geometry('930x680')
        self.root.minsize(870,650)
        self.root.configure(bg='#0c121b')
        self.root.protocol('WM_DELETE_WINDOW', self.close)
        self.labels = {}
        self.title = self.label(0, 'Stage1.Name', 18)
        self.state = self.label(1, 'Launcher.STOPPED', 16)
        self.mode = self.label(2, 'Launcher.Layout', 11)
        self.session = self.label(3, 'Launcher.ModeUnset', 11)
        self.buttons = []
        self.role_buttons = {}
        selection = load_deployment_selection(ROOT)
        pilot_configured = bool(self.config.get(PILOT_IDENTITIES[PILOT_ROLE])) or selection['role'] == PILOT_ROLE
        display_roles = ('Backend',) + ROLES + ((PILOT_ROLE,) if pilot_configured else ())
        selection_roles = DEPLOYMENT_SELECTIONS + ((PILOT_ROLE,) if pilot_configured else ())
        self.role_choice = tk.StringVar(value=selection['role'] or 'All')
        self.deployment_path = tk.StringVar(value=selection['path'] or str(ROOT / 'Launcher/deployment.json'))
        selector = tk.Frame(self.root, bg='#0c121b')
        selector.grid(row=4, column=0, columnspan=4, sticky='ew', padx=12, pady=6)
        tk.Label(selector, text='本机角色 / Local role', bg='#0c121b', fg='#ebf3fa').pack(side='left')
        self.role_selector = ttk.Combobox(selector, textvariable=self.role_choice,
                                         values=selection_roles, state='readonly', width=14)
        self.role_selector.pack(side='left', padx=10)
        tk.Label(selector, text='All = 单机全部端；其余 = 本机单端 / All = single workstation',
                 bg='#0c121b', fg='#aebdcd').pack(side='left')
        self.role_selector.bind('<<ComboboxSelected>>', lambda _: self.update_buttons())
        profile = tk.Frame(self.root, bg='#0c121b')
        profile.grid(row=5, column=0, columnspan=4, sticky='ew', padx=12, pady=6)
        tk.Label(profile, text='共享配置 / Shared config', bg='#0c121b', fg='#ebf3fa').pack(side='left')
        self.profile_entry = tk.Entry(profile, textvariable=self.deployment_path)
        self.profile_entry.pack(side='left', fill='x', expand=True, padx=10)
        self.browse_button = tk.Button(profile, text='选择文件 / Browse', command=self.browse_deployment)
        self.browse_button.pack(side='right')
        for i, role in enumerate(display_roles):
            text = tk.Label(self.root, bg='#131c28', fg='#ebf3fa', anchor='w', font=('Segoe UI',12))
            text.grid(row=i+6,column=0,padx=12,pady=4,sticky='ew');self.labels[role]=text
            self.role_buttons[self.button(i+6,1,'Launcher.Restart',lambda r=role:self.action(lambda:self.runtime.restart(r)))]=role
            if role != 'Backend':
                self.role_buttons[self.button(i+6,2,'Launcher.Focus',lambda r=role:self.focus(r))]=role
            self.role_buttons[self.button(i+6,3,'Launcher.Stop',lambda r=role:self.action(lambda:self.runtime.stop_role(r)))]=role
        footer = 6 + len(display_roles)
        for row,items in ((footer,(('Launcher.StartDemo',lambda:self.start('demo')),
                              ('Launcher.StartRealtime',lambda:self.start('realtime')))),
                          (footer + 2,(('Launcher.Arrange',self.arrange),('Launcher.Shutdown',self.shutdown)))):
            bar=tk.Frame(self.root,bg='#0c121b')
            bar.grid(row=row,column=0,columnspan=4,sticky='ew')
            for column,(key,callback) in enumerate(items):
                bar.columnconfigure(column,weight=1,uniform='actions')
                self.button(0,column,key,callback,parent=bar)
        self.deployment_start = tk.Button(self.root, text='按配置启动 / Start from config',
                                         command=self.start_deployment, bg='#24735b', fg='white', relief='flat', pady=8)
        self.deployment_start.grid(row=footer + 1, column=0, columnspan=4, padx=12, pady=5, sticky='ew')
        self.error = tk.Label(self.root,bg='#0c121b',fg='#e7666e',wraplength=900,font=('Segoe UI',10))
        self.error.grid(row=footer + 3,column=0,columnspan=4,padx=12,pady=8,sticky='w')
        self.root.columnconfigure(0,weight=1)
        self.root.after(300,self.tick)
        if self.config.get('auto_start',False):
            self.root.after(500,lambda:self.start_deployment() if selection['role'] is not None
                            else self.start(self.config.get('startup_mode','demo')))
        self.update_buttons()
    def text(self,key):return self.catalog[key][1 if self.language=='zh-Hans' else 0]
    def label(self,row,key,size):
        w=tk.Label(self.root,text=self.text(key),bg='#0c121b',fg='#ebf3fa',anchor='w',font=('Segoe UI',size))
        w.grid(row=row,column=0,columnspan=4,padx=12,pady=5,sticky='ew');return w
    def button(self,row,column,key,action,columnspan=1,parent=None):
        w=tk.Button(parent or self.root,text=self.text(key),command=action,bg='#1b2837',fg='#ebf3fa',relief='flat',padx=10,pady=5)
        w.grid(row=row,column=column,columnspan=columnspan,padx=5,pady=4,sticky='ew');self.buttons.append((w,key))
        return w
    def browse_deployment(self):
        path = filedialog.askopenfilename(parent=self.root, title='选择共享部署配置 / Shared deployment config',
                                         filetypes=[('JSON', '*.json')])
        if path: self.deployment_path.set(path)
    def prepare_start(self, config, mode=None):
        issues = preflight(ROOT, config)
        if issues: raise RuntimeError('\n'.join(issues))
        if any(p.process.poll() is None for p in self.runtime.owned.values()):
            raise RuntimeError('请先关闭当前会话 / Shut down the current session first')
        if self.runtime.state not in ('STOPPED', 'FAILED'):
            raise RuntimeError('请先关闭当前会话 / Shut down the current session first')
        for role in tuple(self.runtime.owned):
            self.runtime.stop_role(role)
        self.runtime = Runtime(ROOT, config)
        self.placed.clear()
        save_deployment_selection(ROOT, config.get('deployment_role'), config.get('deployment_path'))
        self.runtime.start(mode)
    def start(self,mode):
        if self.role_choice.get() != 'All': return
        self.action(lambda:self.prepare_start(load_config(ROOT), mode))
    def start_deployment(self):
        role, path = self.role_choice.get(), self.deployment_path.get()
        self.action(lambda:self.prepare_start(deployment_config(ROOT, load_config(ROOT), path, role)))
    def update_buttons(self):
        live_processes=any(p.process.poll() is None for p in list(self.runtime.owned.values()))
        can_start=self.runtime.state in ('STOPPED','FAILED') and not live_processes and not self.closing
        for button,key in self.buttons:
            disabled=(self.busy and not self.polling) or (key in ('Launcher.StartDemo','Launcher.StartRealtime') and not can_start)
            disabled=disabled or (key in ('Launcher.StartDemo','Launcher.StartRealtime') and self.role_choice.get() != 'All')
            disabled=disabled or (self.runtime.launch_mode is None and key in ('Launcher.Restart','Launcher.Stop','Launcher.Focus'))
            disabled=disabled or (self.runtime.state in ('STOPPED','FAILED') and key in ('Launcher.Restart','Launcher.Focus'))
            role = self.role_buttons.get(button)
            if role:
                disabled = disabled or not self.runtime.manages_role(role)
                disabled = disabled or (role == 'Backend' and self.runtime.existing_backend)
                if key in ('Launcher.Stop', 'Launcher.Focus'):
                    disabled = disabled or role not in self.runtime.owned
            button.config(text=self.text(key),state='disabled' if disabled else 'normal')
        editable = can_start and (not self.busy or self.polling)
        self.role_selector.config(state='readonly' if editable else 'disabled')
        self.profile_entry.config(state='normal' if editable else 'disabled')
        self.browse_button.config(state='normal' if editable else 'disabled')
        all_caption = ('按配置启动全部本机端 / Start all from config' if self.config.get(PILOT_IDENTITIES[PILOT_ROLE])
                       else '按配置启动四端 / Start all from config')
        self.deployment_start.config(state='normal' if editable else 'disabled',
                                     text=all_caption if self.role_choice.get() == 'All'
                                     else '启动本机角色 / Start selected role')
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
        area = work_area()
        local_clients = self.runtime.local_client_roles
        rects = {PILOT_ROLE: area} if local_clients == (PILOT_ROLE,) else rectangles(area, pilot=PILOT_ROLE in local_clients)
        for role in local_clients:
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
        layout_text = self.text('Launcher.Layout') if self.role_choice.get() == 'All' else (
            '共享配置部署 | 本机只启动所选角色' if self.language == 'zh-Hans'
            else 'Shared deployment | Only the selected role runs locally')
        if self.role_choice.get() == PILOT_ROLE:
            layout_text = '本机只启动 Mock 驾驶员端' if self.language == 'zh-Hans' else 'Local Mock Pilot client'
        self.mode.config(text=layout_text)
        self.state.config(text=self.text('Launcher.'+self.runtime.state))
        mode_key={'demo':'Launcher.ModeDemo','realtime':'Launcher.ModeRealtime'}.get(self.runtime.launch_mode,'Launcher.ModeUnset')
        session_text=self.text(mode_key)
        if self.runtime.deployment_role:
            session_text += ' | ' + self.runtime.deployment_role + ' | ' + self.runtime.config['http_endpoint']
        location_key={'waiting':'Launcher.LocationWaiting','computer':'Launcher.LocationComputer',
                      'estimated':'Launcher.LocationComputerEstimated'}.get(self.runtime.location_status)
        if self.runtime.location_status=='ready':
            location_key='Launcher.LocationComputerReady' if self.runtime.location_source=='computer' else 'Launcher.LocationReady'
        if location_key:session_text+='  |  '+self.text(location_key)
        self.session.config(text=session_text)
        for role,label in self.labels.items():
            value=self.runtime.components[role]
            detail=self.text('Launcher.Existing') if role=='Backend' and self.runtime.existing_backend else self.text('Launcher.'+value)
            if role == PILOT_ROLE and not self.runtime.manages_role(role):
                detail = '当前会话不启动' if self.language == 'zh-Hans' else 'Outside this session'
            host = self.runtime.config.get('deployment_hosts', {}).get(role)
            if host:
                scope = ('本机' if self.runtime.manages_role(role) else '远程') if self.language == 'zh-Hans' else ('Local' if self.runtime.manages_role(role) else 'Remote')
                detail += '  | ' + scope + '  ' + host
            label.config(text=role+'    '+detail,fg='#58c397' if value=='ONLINE' else '#ebb453')
        self.update_buttons()
        if not self.busy:
            self.place_ready()
            self.action(self.runtime.poll, clear_error=False)
        self.root.after(1000,self.tick)
    def shutdown(self):
        try:active=self.runtime.has_active_execution() if self.runtime.state!='STOPPED' and 'Backend' in self.runtime.owned else False
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
