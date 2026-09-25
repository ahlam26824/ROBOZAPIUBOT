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
FOCUS_CHAR_UUID   = "a1b2c3d4-0001-4000-8000-00805f9b0007"

# Exact App Palette matching Flutter app main.dart
COLOR_BG        = "#F5F4EE"
COLOR_SURFACE   = "#FFFFFF"
COLOR_BORDER    = "#E5E3DA"
COLOR_INK       = "#1F1E1D"
COLOR_INK_MUTED = "#6B6963"
COLOR_CLAY      = "#CC785C"
COLOR_CLAY_SOFT = "#F3E3DC"
COLOR_GOOD      = "#4F7A5C"
COLOR_BAD       = "#B3563A"

ctk.set_appearance_mode("Light")
ctk.set_default_color_theme("blue")

class PisuBotPCApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Pisu Bot")
        self.geometry("860x820")
        self.minsize(780, 680)
        self.configure(fg_color=COLOR_BG)

        # Connection State
        self.connection_mode = "Disconnected" # "BLE", "SERIAL", "SIMULATOR", "Disconnected"
        self.ble_client = None
        self.serial_port = None
        self.is_connected = False
        self.status_text = "Disconnected"

        self.watch_mode = False
        self.temperature_c = 24.5

        self.selected_focus_mins = 26
        self.focus_running = False
        self.sw_running = False

        # Async Loop thread for BLE
        self.loop = asyncio.new_event_loop()
        self.ble_thread = threading.Thread(target=self._run_async_loop, daemon=True)
        self.ble_thread.start()

        # Build UI
        self._create_header()
        self._create_scrollable_body()

        # Clock Ticker
        self.after(1000, self._update_clock_ticker)
        self.after(2000, self._periodic_sync)

    def _run_async_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def _create_header(self):
        header_frame = ctk.CTkFrame(self, corner_radius=0, fg_color=COLOR_BG, height=60)
        header_frame.pack(side="top", fill="x", padx=20, pady=(10, 0))

        title_label = ctk.CTkLabel(
            header_frame,
            text="Pisu Bot",
            font=ctk.CTkFont(family="Segoe UI", size=24, weight="bold"),
            text_color=COLOR_INK
        )
        title_label.pack(side="left", pady=10)

        subtitle_label = ctk.CTkLabel(
            header_frame,
            text=" by ROBOZA",
            font=ctk.CTkFont(family="Segoe UI", size=13, weight="bold"),
            text_color=COLOR_CLAY
        )
        subtitle_label.pack(side="left", pady=10)

    def _create_scrollable_body(self):
        self.scroll = ctk.CTkScrollableFrame(self, fg_color=COLOR_BG, corner_radius=0)
        self.scroll.pack(fill="both", expand=True, padx=15, pady=(0, 15))

        # 1. WatchFace Hero Card
        self._build_watchface_hero()

        # 2. Connection Section Card
        self._build_connection_card()

        # 3. Bot Display Mode Section Card
        self._build_mode_card()

        # 4. Focus Mode & Stopwatch Card
        self._build_focus_card()

        # 5. Games Section Card (Tic-Tac-Toe)
        self._build_games_card()

        # 6. Notifications Section Card
        self._build_notifications_card()

        # 7. Reactions Section Card
        self._build_reactions_card()

    # ------------------ 1. WATCHFACE HERO ------------------
    def _build_watchface_hero(self):
        hero = ctk.CTkFrame(self.scroll, fg_color=COLOR_INK, corner_radius=22)
        hero.pack(fill="x", padx=5, pady=(10, 14))

        self.hero_clock_lbl = ctk.CTkLabel(
            hero,
            text="14:32:05",
            font=ctk.CTkFont(family="Segoe UI", size=48, weight="bold"),
            text_color="#FFFFFF"
        )
        self.hero_clock_lbl.pack(pady=(24, 2))

        self.hero_date_lbl = ctk.CTkLabel(
            hero,
            text="Thursday, September 25",
            font=ctk.CTkFont(family="Segoe UI", size=14),
            text_color="#B3B2AD"
        )
        self.hero_date_lbl.pack(pady=(0, 14))

        # Weather Pill
        pill = ctk.CTkFrame(hero, fg_color="#31302E", corner_radius=20)
        pill.pack(pady=(0, 24))

        self.hero_weather_lbl = ctk.CTkLabel(
            pill,
            text="☀️ 24.5°C",
            font=ctk.CTkFont(family="Segoe UI", size=13, weight="bold"),
            text_color="#FFFFFF"
        )
        self.hero_weather_lbl.pack(padx=16, pady=6)

    def _update_clock_ticker(self):
        now = datetime.datetime.now()
        time_str = now.strftime("%H:%M:%S")
        date_str = now.strftime("%A, %B %d")
        self.hero_clock_lbl.configure(text=time_str)
        self.hero_date_lbl.configure(text=date_str)
        self.after(1000, self._update_clock_ticker)

    # Helper to make Flutter-style Section Cards
    def _create_section_card(self, title):
        card = ctk.CTkFrame(self.scroll, fg_color=COLOR_SURFACE, border_color=COLOR_BORDER, border_width=1, corner_radius=18)
        card.pack(fill="x", padx=5, pady=7)

        title_lbl = ctk.CTkLabel(
            card,
            text=title.upper(),
            font=ctk.CTkFont(family="Segoe UI", size=11, weight="bold"),
            text_color=COLOR_INK_MUTED
        )
        title_lbl.pack(anchor="w", padx=20, pady=(16, 8))
        return card

    # ------------------ 2. CONNECTION CARD ------------------
    def _build_connection_card(self):
        card = self._create_section_card("CONNECTION")

        inner = ctk.CTkFrame(card, fg_color="transparent")
        inner.pack(fill="x", padx=20, pady=(0, 16))

        self.dot_lbl = ctk.CTkLabel(inner, text="●", font=ctk.CTkFont(size=14), text_color=COLOR_BORDER)
        self.dot_lbl.pack(side="left", padx=(0, 8))

        self.conn_status_lbl = ctk.CTkLabel(
            inner,
            text="Disconnected",
            font=ctk.CTkFont(family="Segoe UI", size=14),
            text_color=COLOR_INK
        )
        self.conn_status_lbl.pack(side="left")

        # Connection Buttons
        btn_frame = ctk.CTkFrame(inner, fg_color="transparent")
        btn_frame.pack(side="right")

        self.port_combo = ctk.CTkComboBox(btn_frame, values=["COM1", "COM2", "COM3"], width=90, fg_color=COLOR_BG, text_color=COLOR_INK, button_color=COLOR_BORDER)
        self.port_combo.pack(side="left", padx=4)
        self.refresh_ports()

        self.conn_ble_btn = ctk.CTkButton(
            btn_frame,
            text="Connect BLE",
            command=self.connect_ble,
            fg_color=COLOR_CLAY,
            hover_color="#B3654B",
            text_color="#FFFFFF",
            corner_radius=10,
            height=34
        )
        self.conn_ble_btn.pack(side="left", padx=4)

        self.conn_serial_btn = ctk.CTkButton(
            btn_frame,
            text="USB Serial",
            command=self.connect_serial,
            fg_color=COLOR_GOOD,
            hover_color="#3F634A",
            text_color="#FFFFFF",
            corner_radius=10,
            height=34
        )
        self.conn_serial_btn.pack(side="left", padx=4)

        self.disconn_btn = ctk.CTkButton(
            btn_frame,
            text="Disconnect",
            command=self.disconnect_all,
            fg_color=COLOR_BG,
            hover_color=COLOR_BORDER,
            text_color=COLOR_INK,
            border_color=COLOR_BORDER,
            border_width=1,
            corner_radius=10,
            height=34
        )
        self.disconn_btn.pack(side="left", padx=4)

    def refresh_ports(self):
        if HAS_SERIAL:
            ports = [p.device for p in serial.tools.list_ports.comports()]
            if not ports:
                ports = ["COM1"]
            self.port_combo.configure(values=ports)
            self.port_combo.set(ports[0])

    def _update_conn_ui(self, connected, message):
        self.is_connected = connected
        self.conn_status_lbl.configure(text=message)
        if connected:
            self.dot_lbl.configure(text_color=COLOR_GOOD)
        else:
            self.dot_lbl.configure(text_color=COLOR_BORDER)

    def connect_ble(self):
        if not HAS_BLEAK:
            self._update_conn_ui(False, "Bleak library missing")
            return
        self._update_conn_ui(False, "Scanning BLE for Pisu Bot...")
        asyncio.run_coroutine_threadsafe(self._async_connect_ble(), self.loop)

    async def _async_connect_ble(self):
        try:
            device = await BleakScanner.find_device_by_filter(
                lambda d, ad: d.name and "Pisu Bot" in d.name,
                timeout=8.0
            )
            if not device:
                self.after(0, lambda: self._update_conn_ui(False, "Pisu Bot Not Found"))
                return
            client = BleakClient(device)
            await client.connect()
            self.ble_client = client
            self.connection_mode = "BLE"
            self.after(0, lambda: self._update_conn_ui(True, "Connected via BLE"))
        except Exception as e:
            self.after(0, lambda: self._update_conn_ui(False, f"BLE Failed: {e}"))

    def connect_serial(self):
        if not HAS_SERIAL:
            return
        port = self.port_combo.get()
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.connection_mode = "SERIAL"
            self._update_conn_ui(True, f"Connected via USB ({port})")
        except Exception as e:
            self._update_conn_ui(False, f"Serial Error ({port})")

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
        self.connection_mode = "Disconnected"
        self._update_conn_ui(False, "Disconnected")

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

    # ------------------ 3. BOT DISPLAY MODE CARD ------------------
    def _build_mode_card(self):
        card = self._create_section_card("BOT DISPLAY MODE")

        inner = ctk.CTkFrame(card, fg_color="transparent")
        inner.pack(fill="x", padx=20, pady=(0, 16))

        self.chip_face = ctk.CTkButton(
            inner,
            text="😊 Simple Face",
            command=lambda: self.set_mode(False),
            fg_color=COLOR_CLAY_SOFT,
            text_color=COLOR_CLAY,
            hover_color=COLOR_CLAY_SOFT,
            corner_radius=12,
            height=44,
            font=ctk.CTkFont(size=14, weight="bold")
        )
        self.chip_face.pack(side="left", expand=True, fill="x", padx=(0, 6))

        self.chip_watch = ctk.CTkButton(
            inner,
            text="⌚ Watch Mode",
            command=lambda: self.set_mode(True),
            fg_color=COLOR_BG,
            text_color=COLOR_INK_MUTED,
            hover_color=COLOR_BORDER,
            corner_radius=12,
            height=44,
            font=ctk.CTkFont(size=14)
        )
        self.chip_watch.pack(side="left", expand=True, fill="x", padx=(6, 0))

    def set_mode(self, watch_mode):
        self.watch_mode = watch_mode
        if watch_mode:
            self.chip_watch.configure(fg_color=COLOR_CLAY_SOFT, text_color=COLOR_CLAY, font=ctk.CTkFont(size=14, weight="bold"))
            self.chip_face.configure(fg_color=COLOR_BG, text_color=COLOR_INK_MUTED, font=ctk.CTkFont(size=14))
            self.send_payload(MODE_CHAR_UUID, "1")
        else:
            self.chip_face.configure(fg_color=COLOR_CLAY_SOFT, text_color=COLOR_CLAY, font=ctk.CTkFont(size=14, weight="bold"))
            self.chip_watch.configure(fg_color=COLOR_BG, text_color=COLOR_INK_MUTED, font=ctk.CTkFont(size=14))
            self.send_payload(MODE_CHAR_UUID, "0")

    # ------------------ 4. FOCUS MODE & STOPWATCH CARD ------------------
    def _build_focus_card(self):
        card = self._create_section_card("FOCUS MODE & STOPWATCH")

        inner = ctk.CTkFrame(card, fg_color="transparent")
        inner.pack(fill="x", padx=20, pady=(0, 16))

        # Focus Section Header
        ctk.CTkLabel(inner, text="⏱️ Focus Timer", font=ctk.CTkFont(size=14, weight="bold"), text_color=COLOR_INK).pack(anchor="w", pady=(0, 4))
        ctk.CTkLabel(inner, text="Touch for 7s on Pisu Bot or select timer below", font=ctk.CTkFont(size=12), text_color=COLOR_INK_MUTED).pack(anchor="w", pady=(0, 8))

        # Preset Chips
        chip_frame = ctk.CTkFrame(inner, fg_color="transparent")
        chip_frame.pack(anchor="w", pady=(0, 10))

        self.focus_chips = {}
        for m in [15, 26, 45, 60]:
            btn = ctk.CTkButton(
                chip_frame,
                text=f"{m}m{' (Def)' if m==26 else ''}",
                width=80,
                height=32,
                corner_radius=16,
                fg_color=COLOR_CLAY_SOFT if m==26 else COLOR_BG,
                text_color=COLOR_CLAY if m==26 else COLOR_INK_MUTED,
                command=lambda mins=m: self._select_focus_mins(mins)
            )
            btn.pack(side="left", padx=4)
            self.focus_chips[m] = btn

        # Start Focus Button
        self.focus_start_btn = ctk.CTkButton(
            inner,
            text="Start Focus Timer (26m)",
            command=self.toggle_focus_timer,
            fg_color=COLOR_CLAY,
            hover_color="#B3654B",
            text_color="#FFFFFF",
            corner_radius=10,
            height=38
        )
        self.focus_start_btn.pack(fill="x", pady=(0, 14))

        # Divider
        ctk.CTkFrame(inner, height=1, fg_color=COLOR_BORDER).pack(fill="x", pady=6)

        # Stopwatch Section Header
        ctk.CTkLabel(inner, text="⏱️ Stopwatch", font=ctk.CTkFont(size=14, weight="bold"), text_color=COLOR_INK).pack(anchor="w", pady=(8, 4))

        sw_frame = ctk.CTkFrame(inner, fg_color="transparent")
        sw_frame.pack(fill="x", pady=(4, 0))

        self.sw_toggle_btn = ctk.CTkButton(
            sw_frame,
            text="Start",
            command=self.toggle_stopwatch,
            fg_color=COLOR_CLAY_SOFT,
            text_color=COLOR_CLAY,
            corner_radius=10,
            height=34
        )
        self.sw_toggle_btn.pack(side="left", expand=True, fill="x", padx=3)

        ctk.CTkButton(
            sw_frame,
            text="Reset",
            command=lambda: self.send_payload(FOCUS_CHAR_UUID, "sw:reset"),
            fg_color=COLOR_BG,
            text_color=COLOR_INK,
            border_color=COLOR_BORDER,
            border_width=1,
            corner_radius=10,
            height=34
        ).pack(side="left", expand=True, fill="x", padx=3)

        ctk.CTkButton(
            sw_frame,
            text="Exit",
            command=self.exit_stopwatch,
            fg_color=COLOR_BG,
            text_color=COLOR_INK_MUTED,
            border_color=COLOR_BORDER,
            border_width=1,
            corner_radius=10,
            height=34
        ).pack(side="left", expand=True, fill="x", padx=3)

    def _select_focus_mins(self, mins):
        self.selected_focus_mins = mins
        for m, btn in self.focus_chips.items():
            if m == mins:
                btn.configure(fg_color=COLOR_CLAY_SOFT, text_color=COLOR_CLAY)
            else:
                btn.configure(fg_color=COLOR_BG, text_color=COLOR_INK_MUTED)
        self.focus_start_btn.configure(text=f"Start Focus Timer ({mins}m)")

    def toggle_focus_timer(self):
        if self.focus_running:
            self.send_payload(FOCUS_CHAR_UUID, "focus:0")
            self.focus_running = False
            self.focus_start_btn.configure(text=f"Start Focus Timer ({self.selected_focus_mins}m)", fg_color=COLOR_CLAY)
        else:
            self.send_payload(FOCUS_CHAR_UUID, f"focus:{self.selected_focus_mins}")
            self.focus_running = True
            self.focus_start_btn.configure(text="Stop Focus Timer", fg_color=COLOR_BAD)

    def toggle_stopwatch(self):
        if self.sw_running:
            self.send_payload(FOCUS_CHAR_UUID, "sw:stop")
            self.sw_running = False
            self.sw_toggle_btn.configure(text="Start")
        else:
            self.send_payload(FOCUS_CHAR_UUID, "sw:start")
            self.sw_running = True
            self.sw_toggle_btn.configure(text="Pause")

    def exit_stopwatch(self):
        self.send_payload(FOCUS_CHAR_UUID, "sw:off")
        self.sw_running = False
        self.sw_toggle_btn.configure(text="Start")

    # ------------------ 5. GAMES CARD (TIC-TAC-TOE) ------------------
    def _build_games_card(self):
        card = self._create_section_card("GAME")

        inner = ctk.CTkFrame(card, fg_color="transparent")
        inner.pack(fill="x", padx=20, pady=(0, 16))

        icon_box = ctk.CTkFrame(inner, fg_color=COLOR_CLAY_SOFT, corner_radius=10, width=42, height=42)
        icon_box.pack(side="left", padx=(0, 12))
        ctk.CTkLabel(icon_box, text="井", font=ctk.CTkFont(size=20, weight="bold"), text_color=COLOR_CLAY).pack(expand=True)

        info_frame = ctk.CTkFrame(inner, fg_color="transparent")
        info_frame.pack(side="left", expand=True, fill="x")

        ctk.CTkLabel(info_frame, text="Tic-Tac-Toe", font=ctk.CTkFont(size=15, weight="bold"), text_color=COLOR_INK).pack(anchor="w")
        self.game_lbl = ctk.CTkLabel(info_frame, text="Play against Pisu Bot with live reactions", font=ctk.CTkFont(size=12), text_color=COLOR_INK_MUTED)
        self.game_lbl.pack(anchor="w")

        # Board container
        self.board_frame = ctk.CTkFrame(card, fg_color="transparent")
        self.board_frame.pack(pady=(0, 16))

        self.board_buttons = []
        self.board_state = [""] * 9

        for r in range(3):
            for c in range(3):
                idx = r * 3 + c
                btn = ctk.CTkButton(
                    self.board_frame,
                    text="",
                    width=65,
                    height=65,
                    font=ctk.CTkFont(size=22, weight="bold"),
                    fg_color=COLOR_BG,
                    hover_color=COLOR_BORDER,
                    text_color=COLOR_INK,
                    corner_radius=10,
                    command=lambda i=idx: self.make_move(i)
                )
                btn.grid(row=r, column=c, padx=4, pady=4)
                self.board_buttons.append(btn)

        ctk.CTkButton(card, text="Reset Game", command=self.reset_game, fg_color=COLOR_BG, text_color=COLOR_INK_MUTED, hover_color=COLOR_BORDER, height=30).pack(pady=(0, 16))

    def reset_game(self):
        self.board_state = [""] * 9
        for btn in self.board_buttons:
            btn.configure(text="", fg_color=COLOR_BG)
        self.game_lbl.configure(text="Your turn (X)")

    def make_move(self, idx):
        if self.board_state[idx] != "" or self.check_winner():
            return
        self.board_state[idx] = "X"
        self.board_buttons[idx].configure(text="X", fg_color=COLOR_CLAY_SOFT, text_color=COLOR_CLAY)

        winner = self.check_winner()
        if winner:
            self.finish_game(winner)
            return

        self.after(350, self.pisu_move)

    def pisu_move(self):
        empty = [i for i, v in enumerate(self.board_state) if v == ""]
        if not empty:
            self.finish_game("draw")
            return
        import random
        choice = random.choice(empty)
        self.board_state[choice] = "O"
        self.board_buttons[choice].configure(text="O", fg_color="#FCE7E1", text_color=COLOR_BAD)

        winner = self.check_winner()
        if winner:
            self.finish_game(winner)

    def check_winner(self):
        wins = [(0,1,2),(3,4,5),(6,7,8),(0,3,6),(1,4,7),(2,5,8),(0,4,8),(2,4,6)]
        for a, b, c in wins:
            if self.board_state[a] != "" and self.board_state[a] == self.board_state[b] == self.board_state[c]:
                return self.board_state[a]
        if "" not in self.board_state:
            return "draw"
        return None

    def finish_game(self, winner):
        if winner == "X":
            self.game_lbl.configure(text="🎉 You won! Pisu is a little sad about it 😢")
            self.send_payload(RESULT_CHAR_UUID, "win")
        elif winner == "O":
            self.game_lbl.configure(text="🤖 Pisu wins! It's pretty pleased with itself 😄")
            self.send_payload(RESULT_CHAR_UUID, "lose")
        else:
            self.game_lbl.configure(text="🤝 Draw game!")
            self.send_payload(RESULT_CHAR_UUID, "draw")

    # ------------------ 6. NOTIFICATIONS CARD ------------------
    def _build_notifications_card(self):
        card = self._create_section_card("NOTIFICATIONS")

        inner = ctk.CTkFrame(card, fg_color="transparent")
        inner.pack(fill="x", padx=20, pady=(0, 16))

        ctk.CTkLabel(inner, text="Send custom desktop alert to Pisu Bot", font=ctk.CTkFont(size=13), text_color=COLOR_INK_MUTED).pack(anchor="w", pady=(0, 10))

        entry_frame = ctk.CTkFrame(inner, fg_color="transparent")
        entry_frame.pack(fill="x")

        self.notif_title_entry = ctk.CTkEntry(entry_frame, placeholder_text="Title (e.g. Email / Call)", fg_color=COLOR_BG, border_color=COLOR_BORDER, text_color=COLOR_INK)
        self.notif_title_entry.pack(side="left", expand=True, fill="x", padx=(0, 6))

        self.notif_msg_entry = ctk.CTkEntry(entry_frame, placeholder_text="Message body", fg_color=COLOR_BG, border_color=COLOR_BORDER, text_color=COLOR_INK)
        self.notif_msg_entry.pack(side="left", expand=True, fill="x", padx=(6, 6))

        send_btn = ctk.CTkButton(entry_frame, text="Send Alert", command=self.send_notification, fg_color=COLOR_CLAY, hover_color="#B3654B", text_color="#FFFFFF")
        send_btn.pack(side="left")

    def send_notification(self):
        title = self.notif_title_entry.get().strip() or "PC Alert"
        msg = self.notif_msg_entry.get().strip() or "Notification from PC"
        payload = f"{title}|{msg}"
        self.send_payload(NOTIFY_CHAR_UUID, payload)

    # ------------------ 7. REACTIONS CARD ------------------
    def _build_reactions_card(self):
        card = self._create_section_card("REACTIONS TESTER")

        inner = ctk.CTkFrame(card, fg_color="transparent")
        inner.pack(fill="x", padx=20, pady=(0, 16))

        reactions = [
            ("😄 Happy", "lose"),
            ("😵 Dizzy", "win"),
            ("😨 Scared", "win"),
            ("😢 Sad", "win"),
            ("😠 Angry", "lose"),
            ("😐 Normal", "draw"),
        ]

        for idx, (label, payload) in enumerate(reactions):
            r, c = idx // 3, idx % 3
            btn = ctk.CTkButton(
                inner,
                text=label,
                command=lambda p=payload: self.send_payload(RESULT_CHAR_UUID, p),
                fg_color=COLOR_BG,
                hover_color=COLOR_BORDER,
                text_color=COLOR_INK,
                border_color=COLOR_BORDER,
                border_width=1,
                corner_radius=10,
                height=38
            )
            btn.grid(row=r, column=c, padx=4, pady=4, sticky="ew")
            inner.grid_columnconfigure(c, weight=1)

    def _periodic_sync(self):
        if self.is_connected:
            now = datetime.datetime.now()
            time_str = now.strftime("%H:%M|%a, %Y-%m-%d")
            self.send_payload(TIME_CHAR_UUID, time_str)
            self.send_payload(TEMP_CHAR_UUID, f"{self.temperature_c:.1f}")
        self.after(20000, self._periodic_sync)

if __name__ == "__main__":
    app = PisuBotPCApp()
    app.mainloop()
