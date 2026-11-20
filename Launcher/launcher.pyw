from pathlib import Path
import json
import locale
import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox, filedialog, ttk
from runtime import Runtime, SingleInstance, ROLES
from environment import load_config, preflight
from launch_modes import mode_config
from map_locations import load_map_locations, map_origin, named_location, save_map_locations
from windows import rectangles, work_area, place, focus
from deployment import (DEPLOYMENT_SELECTIONS, deployment_config,
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
        self.root = tk.Tk(); self.root.title('Drone Security Launcher'); self.root.geometry('930x735')
        self.root.minsize(870,700)
        self.root.configure(bg='#0c121b')
        self.root.protocol('WM_DELETE_WINDOW', self.close)
        self.labels = {}
        self.title = self.label(0, 'Stage1.Name', 18)
        self.state = self.label(1, 'Launcher.STOPPED', 16)
        self.mode = self.label(2, 'Launcher.Layout', 11)
        self.session = self.label(3, 'Launcher.ModeRealtime', 11)
        self.buttons = []
        self.role_buttons = {}
        selection = load_deployment_selection(ROOT)
        display_roles = ('Backend',) + ROLES
        saved_role = selection['role'] if selection['role'] in DEPLOYMENT_SELECTIONS else None
        self.role_choice = tk.StringVar(value=saved_role or 'All')
        self.deployment_path = tk.StringVar(value=(selection['path'] or '') if saved_role else '')
        selector = tk.Frame(self.root, bg='#0c121b')
        selector.grid(row=4, column=0, columnspan=4, sticky='ew', padx=12, pady=6)
        tk.Label(selector, text='本机角色 / Local role', bg='#0c121b', fg='#ebf3fa').pack(side='left')
        self.role_selector = ttk.Combobox(selector, textvariable=self.role_choice,
                                         values=DEPLOYMENT_SELECTIONS, state='readonly', width=14)
        self.role_selector.pack(side='left', padx=10)
        tk.Label(selector, text='All = 单机全部端；其余 = 本机单端 / All = single workstation',
                 bg='#0c121b', fg='#aebdcd').pack(side='left')
        self.role_selector.bind('<<ComboboxSelected>>', lambda _: self.update_buttons())
        profile = tk.Frame(self.root, bg='#0c121b')
        profile.grid(row=5, column=0, columnspan=4, sticky='ew', padx=12, pady=6)
        tk.Label(profile, text='共享配置（可选） / Shared config', bg='#0c121b', fg='#ebf3fa').pack(side='left')
        self.profile_entry = tk.Entry(profile, textvariable=self.deployment_path)
        self.profile_entry.pack(side='left', fill='x', expand=True, padx=10)
        self.browse_button = tk.Button(profile, text='选择文件 / Browse', command=self.browse_deployment)
        self.browse_button.pack(side='right')
        self.build_location_panel(6)
        for row, role in enumerate(display_roles, start=7):
            text = tk.Label(self.root, bg='#131c28', fg='#ebf3fa', anchor='w', font=('Segoe UI',12))
            text.grid(row=row,column=0,padx=12,pady=4,sticky='ew');self.labels[role]=text
            self.role_buttons[self.button(row,1,'Launcher.Restart',lambda r=role:self.action(lambda:self.runtime.restart(r)))]=role
            if role != 'Backend':
                self.role_buttons[self.button(row,2,'Launcher.Focus',lambda r=role:self.focus(r))]=role
            self.role_buttons[self.button(row,3,'Launcher.Stop',lambda r=role:self.action(lambda:self.runtime.stop_role(r)))]=role
        footer = 7 + len(display_roles)
        bar = tk.Frame(self.root, bg='#0c121b')
        bar.grid(row=footer, column=0, columnspan=4, padx=7, sticky='ew')
        for column, (key, callback) in enumerate((('Launcher.Start', self.start),
                                                  ('Launcher.Arrange', self.arrange),
                                                  ('Launcher.Shutdown', self.shutdown))):
            bar.columnconfigure(column, weight=1, uniform='actions')
            button = self.button(0, column, key, callback, parent=bar)
            if key == 'Launcher.Start':
                button.config(bg='#24735b', fg='white')
        self.error = tk.Label(self.root,bg='#0c121b',fg='#e7666e',wraplength=900,font=('Segoe UI',10))
        self.error.grid(row=footer + 1,column=0,columnspan=4,padx=12,pady=8,sticky='w')
        self.root.columnconfigure(0,weight=1)
        self.root.after(300,self.tick)
        if self.config.get('auto_start',False):
            self.root.after(500, self.start)
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
    def build_location_panel(self, grid_row):
        panel = tk.Frame(self.root, bg='#131c28', padx=12, pady=8)
        panel.grid(row=grid_row, column=0, columnspan=4, sticky='ew', padx=12, pady=8)
        panel.columnconfigure(1, weight=1)
        panel.columnconfigure(3, weight=1)
        self.location_labels = {}
        for key, row, column in (('Launcher.FavoriteLocations', 0, 0),
                                 ('Launcher.Latitude', 1, 0), ('Launcher.Longitude', 1, 2),
                                 ('Launcher.LocationName', 2, 0)):
            label = tk.Label(panel, text=self.text(key), bg='#131c28', fg='#ebf3fa', anchor='w')
            label.grid(row=row, column=column, sticky='w', padx=(0, 8), pady=2)
            self.location_labels[key] = label
        self.favorite_picker = ttk.Combobox(panel, state='readonly')
        self.favorite_picker.grid(row=0, column=1, columnspan=3, sticky='ew', padx=(0, 6))
        self.favorite_picker.bind('<<ComboboxSelected>>', self.select_location)
        self.latitude, self.longitude, self.location_name = tk.StringVar(), tk.StringVar(), tk.StringVar()
        self.location_entries = []
        for row, column, span, value in ((1, 1, 1, self.latitude), (1, 3, 1, self.longitude),
                                         (2, 1, 3, self.location_name)):
            entry = ttk.Entry(panel, textvariable=value)
            entry.grid(row=row, column=column, columnspan=span, sticky='ew', padx=(0, 6), pady=2)
            self.location_entries.append(entry)
        self.button(0, 4, 'Launcher.RemoveLocation', self.remove_location, parent=panel)
        self.button(1, 4, 'Launcher.SwitchLocation', self.switch_location, parent=panel)
        self.button(2, 4, 'Launcher.AddLocation', self.add_location, parent=panel)
        self.location_feedback = 'Launcher.LocationHint'
        self.location_hint = tk.Label(panel, bg='#131c28', fg='#aebdcd', anchor='w')
        self.location_hint.grid(row=3, column=0, columnspan=5, sticky='ew')
        try:
            self.map_locations = load_map_locations(ROOT)
        except ValueError as exc:
            self.map_locations = []
            self.messages.put(str(exc))
        self.refresh_locations()

    def refresh_locations(self):
        self.favorite_picker.config(values=[location['name'] for location in self.map_locations])
        self.favorite_picker.set('')

    def select_location(self, _event=None):
        index = self.favorite_picker.current()
        if index >= 0:
            location = self.map_locations[index]
            self.latitude.set(str(location['latitude']))
            self.longitude.set(str(location['longitude']))
            self.location_name.set(location['name'])
        self.update_buttons()

    def add_location(self):
        try:
            location = named_location(self.location_name.get(), self.latitude.get(), self.longitude.get())
            updated = load_map_locations(ROOT) + [location]
            save_map_locations(ROOT, updated)
        except (OSError, ValueError) as exc:
            self.messages.put(str(exc))
            return
        self.map_locations = updated
        self.refresh_locations()
        self.favorite_picker.current(len(updated) - 1)
        self.select_location()
        self.error.config(text='')

    def remove_location(self):
        index = self.favorite_picker.current()
        if index < 0:
            return
        updated = self.map_locations.copy()
        updated.pop(index)
        try:
            current = load_map_locations(ROOT)
            if current != self.map_locations:
                self.map_locations = current
                self.refresh_locations()
                self.update_buttons()
                raise ValueError('Saved locations changed. Select the location again. / '
                                 '常用地点已更新，请重新选择要删除的地点。')
            save_map_locations(ROOT, updated)
        except (OSError, ValueError) as exc:
            self.messages.put(str(exc))
            return
        self.map_locations = updated
        self.refresh_locations()
        self.update_buttons()
        self.error.config(text='')

    def switch_location(self):
        latitude, longitude = self.latitude.get(), self.longitude.get()

        def apply():
            origin = map_origin(latitude, longitude)
            if self.runtime.state not in ('STOPPED', 'FAILED') and self.runtime.manages_role('Map'):
                self.runtime.apply_map_location(origin)
                self.location_feedback = 'Launcher.LocationHint'
            else:
                self.location_feedback = 'Launcher.LocationNextLaunch'

        self.action(apply)

    def prepare_start(self, config):
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
        self.runtime.start()
    def start(self):
        role, path = self.role_choice.get(), self.deployment_path.get().strip()
        latitude, longitude = self.latitude.get().strip(), self.longitude.get().strip()

        def launch():
            base = load_config(ROOT)
            config = (deployment_config(ROOT, base, path, role, realtime_only=True) if path or role != 'All'
                      else mode_config(ROOT, base, 'realtime', validate_isolation=False))
            if role in ('Map', 'All') and (latitude or longitude):
                config['manual_map_origin'] = map_origin(latitude, longitude)
            self.prepare_start(config)
            self.location_feedback = 'Launcher.LocationHint'

        self.action(launch)
    def update_buttons(self):
        live_processes=any(p.process.poll() is None for p in list(self.runtime.owned.values()))
        can_start=self.runtime.state in ('STOPPED','FAILED') and not live_processes and not self.closing
        for button,key in self.buttons:
            disabled=(self.busy and not self.polling) or (key == 'Launcher.Start' and not can_start)
            disabled=disabled or (self.runtime.launch_mode is None and key in ('Launcher.Restart','Launcher.Stop','Launcher.Focus'))
            disabled=disabled or (self.runtime.state in ('STOPPED','FAILED') and key in ('Launcher.Restart','Launcher.Focus'))
            disabled=disabled or (key == 'Launcher.RemoveLocation' and self.favorite_picker.current() < 0)
            if key == 'Launcher.SwitchLocation':
                local_map = (self.role_choice.get() in ('Map', 'All') if self.runtime.state in ('STOPPED', 'FAILED')
                             else self.runtime.manages_role('Map'))
                disabled = disabled or not local_map
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
        location_editable = not self.closing and (not self.busy or self.polling)
        self.favorite_picker.config(state='readonly' if location_editable else 'disabled')
        for entry in self.location_entries:
            entry.config(state='normal' if location_editable else 'disabled')
        for key, label in self.location_labels.items():
            label.config(text=self.text(key))
        self.location_hint.config(text=self.text(self.location_feedback))
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
        rects = rectangles(area)
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
        self.mode.config(text=layout_text)
        self.state.config(text=self.text('Launcher.'+self.runtime.state))
        session_text=self.text('Launcher.ModeRealtime')
        if self.runtime.deployment_role:
            session_text += ' | ' + self.runtime.deployment_role + ' | ' + self.runtime.config['http_endpoint']
        location_key={'waiting':'Launcher.LocationWaiting','computer':'Launcher.LocationComputer',
                      'estimated':'Launcher.LocationComputerEstimated'}.get(self.runtime.location_status)
        if self.runtime.location_status=='ready':
            location_key={'manual': 'Launcher.LocationManual', 'computer': 'Launcher.LocationComputerReady'}.get(
                self.runtime.location_source, 'Launcher.LocationReady')
        if location_key:session_text+='  |  '+self.text(location_key)
        self.session.config(text=session_text)
        for role,label in self.labels.items():
            value=self.runtime.components[role]
            detail=self.text('Launcher.Existing') if role=='Backend' and self.runtime.existing_backend else self.text('Launcher.'+value)
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
        if active and not messagebox.askokcancel(self.text('Launcher.Shutdown'),self.text('Launcher.RealtimeShutdownConfirm'),parent=self.root):return False
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
