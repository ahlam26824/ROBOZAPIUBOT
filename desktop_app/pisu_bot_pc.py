import asyncio
import threading
import time
import datetime
import urllib.request
import json
import os
import sys

import customtkinter as ctk
from PIL import Image

# Bleak for Windows BLE
try:
    import bleak
    from bleak import BleakScanner, BleakClient
    HAS_BLEAK = True
except ImportError:
    HAS_BLEAK = False

# PySerial for Serial/COM
try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

# UUIDs matching Pisu Bot Firmware
SERVICE_UUID      = "a1b2c3d4-0001-4000-8000-00805f9b0001"
TIME_CHAR_UUID    = "a1b2c3d4-0001-4000-8000-00805f9b0002"
TEMP_CHAR_UUID    = "a1b2c3d4-0001-4000-8000-00805f9b0003"
NOTIFY_CHAR_UUID  = "a1b2c3d4-0001-4000-8000-00805f9b0004"
MODE_CHAR_UUID    = "a1b2c3d4-0001-4000-8000-00805f9b0005"
RESULT_CHAR_UUID  = "a1b2c3d4-0001-4000-8000-00805f9b0006"

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class PisuBotPCApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Pisu Bot PC Controller - by ROBOZA")
        self.geometry("920x680")
        self.minsize(850, 600)

        # Connection State
        self.connection_mode = "Disconnected" # "BLE", "SERIAL", "SIMULATOR", "Disconnected"
        self.ble_client = None
        self.serial_port = None
        self.is_connected = False

        self.watch_mode = False
        self.auto_sync_clock = True

        # Async Loop thread for BLE
        self.loop = asyncio.new_event_loop()
        self.ble_thread = threading.Thread(target=self._run_async_loop, daemon=True)
        self.ble_thread.start()

        # Build UI
        self._create_header()
        self._create_main_layout()

        # Start periodic sync timer
        self.after(2000, self._periodic_sync)

    def _run_async_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def _create_header(self):
        header_frame = ctk.CTkFrame(self, corner_radius=0, fg_color="#1E1E2E")
        header_frame.pack(side="top", fill="x", padx=0, pady=0)

        title_label = ctk.CTkLabel(
            header_frame,
            text="🤖 Pisu Bot Controller",
            font=ctk.CTkFont(family="Segoe UI", size=22, weight="bold"),
            text_color="#89B4FA"
        )
        title_label.pack(side="left", padx=20, pady=12)

        subtitle_label = ctk.CTkLabel(
            header_frame,
            text="by ROBOZA",
            font=ctk.CTkFont(family="Segoe UI", size=13, weight="bold"),
            text_color="#FAB387"
        )
        subtitle_label.pack(side="left", padx=0, pady=12)

        self.status_badge = ctk.CTkLabel(
            header_frame,
            text="🔴 Disconnected",
            font=ctk.CTkFont(family="Segoe UI", size=13, weight="bold"),
            fg_color="#313244",
            text_color="#F38BA8",
            corner_radius=8,
            padx=12,
            pady=4
        )
        self.status_badge.pack(side="right", padx=20, pady=12)

    def _create_main_layout(self):
        self.tabview = ctk.CTkTabview(self, corner_radius=12)
        self.tabview.pack(fill="both", expand=True, padx=15, pady=15)

        self.tab_conn = self.tabview.add("📡 Connection")
        self.tab_clock = self.tabview.add("⏱️ Watch & Clock")
        self.tab_notif = self.tabview.add("🔔 Notifications")
        self.tab_game = self.tabview.add("🎮 Tic-Tac-Toe")
        self.tab_express = self.tabview.add("🎭 Reactions")

        self._build_connection_tab()
        self._build_clock_tab()
        self._build_notif_tab()
        self._build_game_tab()
        self._build_express_tab()

    def _update_status(self, text, state_type="disconnected"):
        colors = {
            "connected": ("#A6E3A1", "#181825"), # Green text
            "connecting": ("#F9E2AF", "#181825"), # Yellow text
            "disconnected": ("#F38BA8", "#313244") # Red text
        }
        text_color, bg_color = colors.get(state_type, colors["disconnected"])
        self.status_badge.configure(text=text, text_color=text_color, fg_color=bg_color)

    # ------------------ 1. CONNECTION TAB ------------------
    def _build_connection_tab(self):
        frame = self.tab_conn

        lbl = ctk.CTkLabel(frame, text="Select Connection Method", font=ctk.CTkFont(size=16, weight="bold"))
        lbl.pack(anchor="w", padx=15, pady=(15, 10))

        # Method 1: Bluetooth LE
        ble_card = ctk.CTkFrame(frame, corner_radius=10)
        ble_card.pack(fill="x", padx=15, pady=8)

        ctk.CTkLabel(ble_card, text="Bluetooth LE (Wireless)", font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=15, pady=(10, 2))
        ctk.CTkLabel(ble_card, text="Scans for 'Pisu Bot' over Bluetooth Low Energy", text_color="#A6ADC8").pack(anchor="w", padx=15, pady=(0, 10))

        ble_btn_frame = ctk.CTkFrame(ble_card, fg_color="transparent")
        ble_btn_frame.pack(fill="x", padx=15, pady=(0, 10))

        self.ble_scan_btn = ctk.CTkButton(ble_btn_frame, text="Scan & Connect BLE", command=self.connect_ble, fg_color="#89B4FA", text_color="#11111B")
        self.ble_scan_btn.pack(side="left", padx=5)

        # Method 2: Serial COM Port
        serial_card = ctk.CTkFrame(frame, corner_radius=10)
        serial_card.pack(fill="x", padx=15, pady=8)

        ctk.CTkLabel(serial_card, text="USB Serial COM Port (Wired)", font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=15, pady=(10, 2))
        ctk.CTkLabel(serial_card, text="Connect directly via USB Cable (CH340/CP2102)", text_color="#A6ADC8").pack(anchor="w", padx=15, pady=(0, 10))

        serial_btn_frame = ctk.CTkFrame(serial_card, fg_color="transparent")
        serial_btn_frame.pack(fill="x", padx=15, pady=(0, 10))

        self.port_combo = ctk.CTkComboBox(serial_btn_frame, values=["COM1", "COM2", "COM3", "COM4", "COM5"])
        self.port_combo.pack(side="left", padx=5)
        self.refresh_ports()

        ctk.CTkButton(serial_btn_frame, text="Refresh Ports", command=self.refresh_ports, width=100, fg_color="#45475A").pack(side="left", padx=5)
        self.serial_conn_btn = ctk.CTkButton(serial_btn_frame, text="Connect Serial", command=self.connect_serial, fg_color="#A6E3A1", text_color="#11111B")
        self.serial_conn_btn.pack(side="left", padx=5)

        # Method 3: Simulator / Demo
        sim_card = ctk.CTkFrame(frame, corner_radius=10)
        sim_card.pack(fill="x", padx=15, pady=8)

        ctk.CTkLabel(sim_card, text="Simulator Mode (Test UI without Hardware)", font=ctk.CTkFont(size=14, weight="bold")).pack(anchor="w", padx=15, pady=(10, 2))
        ctk.CTkButton(sim_card, text="Enable Simulator Mode", command=self.enable_simulator, fg_color="#FAB387", text_color="#11111B").pack(anchor="w", padx=15, pady=(0, 12))

        # Disconnect Button
        self.disconn_btn = ctk.CTkButton(frame, text="Disconnect", command=self.disconnect_all, fg_color="#F38BA8", text_color="#11111B")
        self.disconn_btn.pack(anchor="e", padx=15, pady=15)

    def refresh_ports(self):
        if HAS_SERIAL:
            ports = [p.device for p in serial.tools.list_ports.comports()]
            if not ports:
                ports = ["No COM Ports"]
            self.port_combo.configure(values=ports)
            self.port_combo.set(ports[0])

    def connect_ble(self):
        if not HAS_BLEAK:
            self._update_status("Bleak library missing", "disconnected")
            return
        self._update_status("Scanning BLE...", "connecting")
        asyncio.run_coroutine_threadsafe(self._async_connect_ble(), self.loop)

    async def _async_connect_ble(self):
        try:
            device = await BleakScanner.find_device_by_filter(
                lambda d, ad: d.name and "Pisu Bot" in d.name,
                timeout=8.0
            )
            if not device:
                self.after(0, lambda: self._update_status("Pisu Bot Not Found", "disconnected"))
                return
            client = BleakClient(device)
            await client.connect()
            self.ble_client = client
            self.is_connected = True
            self.connection_mode = "BLE"
            self.after(0, lambda: self._update_status("🟢 Connected via BLE", "connected"))
        except Exception as e:
            print("BLE error:", e)
            self.after(0, lambda: self._update_status("BLE Connection Failed", "disconnected"))

    def connect_serial(self):
        if not HAS_SERIAL:
            return
        port = self.port_combo.get()
        if port == "No COM Ports":
            return
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.is_connected = True
            self.connection_mode = "SERIAL"
            self._update_status(f"🟢 Connected via {port}", "connected")
        except Exception as e:
            self._update_status(f"Serial Error: {e}", "disconnected")

    def enable_simulator(self):
        self.connection_mode = "SIMULATOR"
        self.is_connected = True
        self._update_status("🟡 Simulator Mode Active", "connecting")

    def disconnect_all(self):
        if self.ble_client:
            asyncio.run_coroutine_threadsafe(self.ble_client.disconnect(), self.loop)
            self.ble_client = None
        if self.serial_port:
            try:
                self.serial_port.close()
            except:
                pass
            self.serial_port = None
        self.is_connected = False
        self.connection_mode = "Disconnected"
        self._update_status("🔴 Disconnected", "disconnected")

    def send_payload(self, char_uuid, payload_str):
        if not self.is_connected:
            return
        print(f"[{self.connection_mode}] Sending to {char_uuid}: {payload_str}")

        if self.connection_mode == "BLE" and self.ble_client:
            asyncio.run_coroutine_threadsafe(
                self.ble_client.write_gatt_char(char_uuid, payload_str.encode('utf-8')),
                self.loop
            )
        elif self.connection_mode == "SERIAL" and self.serial_port:
            try:
                self.serial_port.write((payload_str + "\n").encode('utf-8'))
            except Exception as e:
                print("Serial write error:", e)

    # ------------------ 2. WATCH & CLOCK TAB ------------------
    def _build_clock_tab(self):
        frame = self.tab_clock

        card = ctk.CTkFrame(frame, corner_radius=12)
        card.pack(fill="both", expand=True, padx=15, pady=15)

        ctk.CTkLabel(card, text="Clock & Weather Sync", font=ctk.CTkFont(size=18, weight="bold")).pack(anchor="w", padx=20, pady=(20, 5))
        ctk.CTkLabel(card, text="Syncs local PC time, date, and temperature with Pisu Bot", text_color="#A6ADC8").pack(anchor="w", padx=20, pady=(0, 15))

        self.clock_preview = ctk.CTkLabel(card, text="--:-- | ---, YYYY-MM-DD", font=ctk.CTkFont(size=24, weight="bold"), text_color="#89B4FA")
        self.clock_preview.pack(pady=10)

        self.temp_preview = ctk.CTkLabel(card, text="Temperature: 24.5 °C", font=ctk.CTkFont(size=16), text_color="#A6E3A1")
        self.temp_preview.pack(pady=5)

        btn_frame = ctk.CTkFrame(card, fg_color="transparent")
        btn_frame.pack(pady=15)

        ctk.CTkButton(btn_frame, text="Sync Time & Temp Now", command=self.sync_time_temp, fg_color="#89B4FA", text_color="#11111B").pack(side="left", padx=10)

        # Watch Mode Toggle
        mode_card = ctk.CTkFrame(card, corner_radius=10)
        mode_card.pack(fill="x", padx=20, pady=15)

        ctk.CTkLabel(mode_card, text="Bot Display Mode", font=ctk.CTkFont(size=15, weight="bold")).pack(anchor="w", padx=15, pady=(10, 5))

        self.mode_switch = ctk.CTkSwitch(mode_card, text="Watch Mode (Show Clock/Temp instead of Animated Face)", command=self.toggle_watch_mode)
        self.mode_switch.pack(anchor="w", padx=15, pady=(0, 15))

    def sync_time_temp(self):
        now = datetime.datetime.now()
        time_str = now.strftime("%H:%M|%a, %Y-%m-%d")
        self.clock_preview.configure(text=time_str.replace("|", "  •  "))
        self.send_payload(TIME_CHAR_UUID, time_str)

        # Temp estimate
        temp_str = "24.0"
        self.temp_preview.configure(text=f"Temperature: {temp_str} °C")
        self.send_payload(TEMP_CHAR_UUID, temp_str)

    def toggle_watch_mode(self):
        val = "1" if self.mode_switch.get() else "0"
        self.send_payload(MODE_CHAR_UUID, val)

    def _periodic_sync(self):
        if self.is_connected:
            now = datetime.datetime.now()
            time_str = now.strftime("%H:%M|%a, %Y-%m-%d")
            self.clock_preview.configure(text=time_str.replace("|", "  •  "))
            self.send_payload(TIME_CHAR_UUID, time_str)
        self.after(20000, self._periodic_sync)

    # ------------------ 3. NOTIFICATIONS TAB ------------------
    def _build_notif_tab(self):
        frame = self.tab_notif

        card = ctk.CTkFrame(frame, corner_radius=12)
        card.pack(fill="both", expand=True, padx=15, pady=15)

        ctk.CTkLabel(card, text="Send Desktop Notification Alert", font=ctk.CTkFont(size=18, weight="bold")).pack(anchor="w", padx=20, pady=(20, 5))
        ctk.CTkLabel(card, text="Pisu Bot will play an alert sound and display your notification", text_color="#A6ADC8").pack(anchor="w", padx=20, pady=(0, 15))

        ctk.CTkLabel(card, text="Notification Title:").pack(anchor="w", padx=20, pady=(5, 2))
        self.notif_title_entry = ctk.CTkEntry(card, width=400, placeholder_text="e.g. Email Alert / Meeting")
        self.notif_title_entry.pack(anchor="w", padx=20, pady=(0, 10))

        ctk.CTkLabel(card, text="Message Body:").pack(anchor="w", padx=20, pady=(5, 2))
        self.notif_msg_entry = ctk.CTkEntry(card, width=400, placeholder_text="e.g. Time for your standup call!")
        self.notif_msg_entry.pack(anchor="w", padx=20, pady=(0, 15))

        ctk.CTkButton(card, text="🔔 Send Notification to Pisu Bot", command=self.send_notification, fg_color="#FAB387", text_color="#11111B").pack(anchor="w", padx=20, pady=10)

    def send_notification(self):
        title = self.notif_title_entry.get().strip() or "PC Alert"
        msg = self.notif_msg_entry.get().strip() or "Notification from PC"
        payload = f"{title}|{msg}"
        self.send_payload(NOTIFY_CHAR_UUID, payload)

    # ------------------ 4. TIC-TAC-TOE TAB ------------------
    def _build_game_tab(self):
        frame = self.tab_game

        card = ctk.CTkFrame(frame, corner_radius=12)
        card.pack(fill="both", expand=True, padx=15, pady=15)

        ctk.CTkLabel(card, text="Tic-Tac-Toe vs Pisu Bot", font=ctk.CTkFont(size=18, weight="bold")).pack(anchor="w", padx=20, pady=(15, 5))
        self.game_status = ctk.CTkLabel(card, text="You are X, Pisu Bot is O. Click a cell to move!", text_color="#89B4FA", font=ctk.CTkFont(size=14))
        self.game_status.pack(anchor="w", padx=20, pady=(0, 10))

        board_frame = ctk.CTkFrame(card, fg_color="transparent")
        board_frame.pack(pady=10)

        self.board_buttons = []
        self.board_state = [""] * 9

        for r in range(3):
            row_btns = []
            for c in range(3):
                idx = r * 3 + c
                btn = ctk.CTkButton(
                    board_frame,
                    text="",
                    width=80,
                    height=80,
                    font=ctk.CTkFont(size=24, weight="bold"),
                    fg_color="#313244",
                    hover_color="#45475A",
                    command=lambda i=idx: self.make_move(i)
                )
                btn.grid(row=r, column=c, padx=5, pady=5)
                row_btns.append(btn)
            self.board_buttons.append(row_btns)

        ctk.CTkButton(card, text="Reset Game", command=self.reset_game, fg_color="#45475A").pack(pady=10)

    def reset_game(self):
        self.board_state = [""] * 9
        for r in range(3):
            for c in range(3):
                self.board_buttons[r][c].configure(text="", fg_color="#313244")
        self.game_status.configure(text="Your turn (X)")

    def make_move(self, idx):
        if self.board_state[idx] != "" or self.check_winner():
            return
        self.board_state[idx] = "X"
        r, c = idx // 3, idx % 3
        self.board_buttons[r][c].configure(text="X", fg_color="#89B4FA", text_color="#11111B")

        winner = self.check_winner()
        if winner:
            self.finish_game(winner)
            return

        # Pisu's AI move (O)
        self.after(400, self.pisu_move)

    def pisu_move(self):
        empty = [i for i, v in enumerate(self.board_state) if v == ""]
        if not empty:
            self.finish_game("draw")
            return
        import random
        choice = random.choice(empty)
        self.board_state[choice] = "O"
        r, c = choice // 3, choice % 3
        self.board_buttons[r][c].configure(text="O", fg_color="#F38BA8", text_color="#11111B")

        winner = self.check_winner()
        if winner:
            self.finish_game(winner)

    def check_winner(self):
        wins = [
            (0,1,2), (3,4,5), (6,7,8),
            (0,3,6), (1,4,7), (2,5,8),
            (0,4,8), (2,4,6)
        ]
        for a, b, c in wins:
            if self.board_state[a] != "" and self.board_state[a] == self.board_state[b] == self.board_state[c]:
                return self.board_state[a]
        if "" not in self.board_state:
            return "draw"
        return None

    def finish_game(self, winner):
        if winner == "X":
            self.game_status.configure(text="🎉 You Won! (Pisu Bot is Sad 😢)")
            self.send_payload(RESULT_CHAR_UUID, "win")
        elif winner == "O":
            self.game_status.configure(text="🤖 Pisu Bot Won! (Pisu Bot is Happy 😄)")
            self.send_payload(RESULT_CHAR_UUID, "lose")
        else:
            self.game_status.configure(text="🤝 Draw Game!")
            self.send_payload(RESULT_CHAR_UUID, "draw")

    # ------------------ 5. REACTIONS TAB ------------------
    def _build_express_tab(self):
        frame = self.tab_express

        card = ctk.CTkFrame(frame, corner_radius=12)
        card.pack(fill="both", expand=True, padx=15, pady=15)

        ctk.CTkLabel(card, text="Mood & Expression Tester", font=ctk.CTkFont(size=18, weight="bold")).pack(anchor="w", padx=20, pady=(20, 5))
        ctk.CTkLabel(card, text="Test Pisu Bot's sound & facial reactions manually", text_color="#A6ADC8").pack(anchor="w", padx=20, pady=(0, 20))

        grid_frame = ctk.CTkFrame(card, fg_color="transparent")
        grid_frame.pack(pady=10)

        reactions = [
            ("😄 Happy (Pat)", "win", "#A6E3A1"),
            ("😵 Dizzy (Shake)", "lose", "#F9E2AF"),
            ("😨 Scared (Pickup)", "win", "#89B4FA"),
            ("😢 Sad (Idle)", "win", "#CBA6F7"),
            ("😠 Angry", "lose", "#F38BA8"),
            ("😐 Normal", "draw", "#45475A"),
        ]

        for idx, (label, payload, color) in enumerate(reactions):
            r, c = idx // 3, idx % 3
            btn = ctk.CTkButton(
                grid_frame,
                text=label,
                width=180,
                height=55,
                font=ctk.CTkFont(size=14, weight="bold"),
                fg_color=color,
                text_color="#11111B",
                command=lambda p=payload: self.send_payload(RESULT_CHAR_UUID, p)
            )
            btn.grid(row=r, column=c, padx=10, pady=10)

if __name__ == "__main__":
    app = PisuBotPCApp()
    app.mainloop()
