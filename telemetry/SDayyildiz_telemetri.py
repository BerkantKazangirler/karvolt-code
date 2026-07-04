import tkinter as tk
from tkinter import messagebox
import serial
import serial.tools.list_ports
import threading
import csv
import os
import math
import time
import string
from datetime import datetime

# ─────────────────────────────────────────
#  AYARLAR
# ─────────────────────────────────────────
BAUD_RATE     = 9600
GUNCELLEME    = 100
DOSYA_ADI     = f"telemetri_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

HIZ_ESIK      = 70
SICAKLIK_ESIK = 40
GERILIM_ESIK  = 450
BATARYA_ESIK  = 50

# ─────────────────────────────────────────
#  RENKLER
# ─────────────────────────────────────────
BG        = "#0a0e14"
CYAN      = "#00e5ff"
CYAN_DIM  = "#007a8a"
RED       = "#ff2244"
ORANGE    = "#ff8800"
GREEN     = "#00ff88"
WHITE     = "#e8f4f8"
GRAY      = "#1e2830"
DARK_GRAY = "#141a20"

# ─────────────────────────────────────────
#  VERİ
# ─────────────────────────────────────────
veri = {"hiz": 0, "sicaklik": 0, "gerilim": 0, "batarya": 100, "zaman_ms": 0}
son_veri_ms   = 0
ser           = None
csv_dosya_obj = None
csv_writer    = None
csv_yolu      = ""
blink_durum   = [True]

# ─────────────────────────────────────────
#  Windows sürücülerini listele
# ─────────────────────────────────────────
def suruculeri_listele():
    surucler = ["Masaustu"]
    for harf in string.ascii_uppercase:
        yol = harf + ":\\"
        if os.path.exists(yol):
            surucler.append(harf + ":\\")
    return surucler

# ─────────────────────────────────────────
#  CSV BAŞLAT
# ─────────────────────────────────────────
def csv_baslat(klasor):
    global csv_dosya_obj, csv_writer, csv_yolu
    if klasor == "Masaustu":
        klasor = os.path.join(os.path.expanduser("~"), "Desktop")
    csv_yolu = os.path.join(klasor, DOSYA_ADI)
    try:
        csv_dosya_obj = open(csv_yolu, "w", newline="", encoding="utf-8")
        csv_writer = csv.writer(csv_dosya_obj, delimiter=";")
        csv_writer.writerow(["zaman_ms", "hiz_kmh", "T_bat_C", "V_bat_C", "kalan_enerji_Wh"])
        csv_dosya_obj.flush()
        return True
    except Exception as e:
        messagebox.showerror("Hata", f"CSV olusturulamadi:\n{e}")
        return False

# ─────────────────────────────────────────
#  SERİAL OKUMA (ayrı thread)
# ─────────────────────────────────────────
def serial_oku():
    global son_veri_ms
    while True:
        if ser and ser.is_open:
            try:
                satir = ser.readline().decode("utf-8", errors="ignore").strip()
                if satir and ";" in satir:
                    parcalar = satir.split(";")
                    if len(parcalar) >= 5:
                        veri["zaman_ms"]  = int(parcalar[0])
                        veri["hiz"]       = int(parcalar[1])
                        veri["sicaklik"]  = int(parcalar[2])
                        veri["gerilim"]   = int(parcalar[3])
                        veri["batarya"]   = int(parcalar[4]) // 100
                        son_veri_ms = time.time() * 1000
                        if csv_writer:
                            csv_writer.writerow([
                                parcalar[0], parcalar[1],
                                parcalar[2], parcalar[3], parcalar[4]
                            ])
                            csv_dosya_obj.flush()
            except Exception:
                pass
        time.sleep(0.05)

# ─────────────────────────────────────────
#  ALARM YANIP SÖNME
# ─────────────────────────────────────────
def alarm_blink(durum_lbl):
    def _blink():
        while True:
            h = veri["hiz"]      > HIZ_ESIK
            s = veri["sicaklik"] > SICAKLIK_ESIK
            g = veri["gerilim"]  > GERILIM_ESIK
            b = veri["batarya"]  < BATARYA_ESIK
            if h or s or g or b:
                uyarilar = []
                if h: uyarilar.append("HIZ")
                if s: uyarilar.append("ISI")
                if g: uyarilar.append("VOLT")
                if b: uyarilar.append("BATARYA")
                mesaj = "KRITIK: " + " | ".join(uyarilar)
                renk = RED if blink_durum[0] else ORANGE
                blink_durum[0] = not blink_durum[0]
                durum_lbl.config(text=mesaj, fg=renk)
            else:
                durum_lbl.config(text="SISTEM NORMAL", fg=GREEN)
            time.sleep(0.5)
    threading.Thread(target=_blink, daemon=True).start()

# ─────────────────────────────────────────
#  SPEEDOMETER
# ─────────────────────────────────────────
def speedometer_ciz(canvas, hiz, max_hiz=220):
    canvas.delete("all")
    cx, cy, r = 160, 155, 130
    canvas.create_arc(cx-r, cy-r, cx+r, cy+r,
                      start=225, extent=-270,
                      outline=GRAY, width=18, style="arc")
    if hiz > 0:
        extent = -(hiz / max_hiz) * 270
        renk = GREEN if hiz < HIZ_ESIK else RED
        canvas.create_arc(cx-r, cy-r, cx+r, cy+r,
                          start=225, extent=extent,
                          outline=renk, width=6, style="arc")
    for i in range(0, max_hiz+1, 20):
        aci = math.radians(225 - (i / max_hiz) * 270)
        x1 = cx + (r-15) * math.cos(aci)
        y1 = cy - (r-15) * math.sin(aci)
        x2 = cx + r * math.cos(aci)
        y2 = cy - r * math.sin(aci)
        canvas.create_line(x1, y1, x2, y2, fill=CYAN_DIM, width=1)
        if i % 40 == 0:
            xt = cx + (r-30) * math.cos(aci)
            yt = cy - (r-30) * math.sin(aci)
            canvas.create_text(xt, yt, text=str(i), fill=WHITE, font=("Courier", 8))
    aci = math.radians(225 - (hiz / max_hiz) * 270)
    ix = cx + (r-20) * math.cos(aci)
    iy = cy - (r-20) * math.sin(aci)
    canvas.create_line(cx, cy, ix, iy, fill=RED, width=3, capstyle="round")
    canvas.create_oval(cx-6, cy-6, cx+6, cy+6, fill=RED, outline="")
    renk_hiz = RED if hiz > HIZ_ESIK else CYAN
    canvas.create_text(cx, cy+45, text=str(hiz), fill=renk_hiz, font=("Courier", 28, "bold"))
    canvas.create_text(cx, cy+72, text="km/h", fill=WHITE, font=("Courier", 10))
    canvas.create_text(cx, cy+88, text="Arac Hizi", fill=CYAN_DIM, font=("Courier", 9))

# ─────────────────────────────────────────
#  ANA UYGULAMA
# ─────────────────────────────────────────
class AyyildizApp:
    def __init__(self, root):
        self.root = root
        self.root.title("AYYILDIZ TEAM - Telemetri Izleme Merkezi")
        self.root.configure(bg=BG)
        self.root.geometry("900x600")
        self.root.resizable(False, False)
        self._ui_olustur()

    def _ui_olustur(self):
        # ── HEADER ──────────────────────────
        header = tk.Frame(self.root, bg=BG)
        header.pack(fill="x", padx=10, pady=(8, 0))

        sol = tk.Frame(header, bg=BG)
        sol.pack(side="left")
        self.tarih_lbl = tk.Label(sol, text="--/--/----", fg=CYAN, bg=BG, font=("Courier", 13, "bold"))
        self.tarih_lbl.pack(anchor="w")
        self.saat_lbl = tk.Label(sol, text="--:--:--", fg=CYAN, bg=BG, font=("Courier", 13, "bold"))
        self.saat_lbl.pack(anchor="w")

        tk.Label(header, text="AYYILDIZ TEAM", fg=WHITE, bg=BG,
                 font=("Courier", 22, "bold")).pack(side="left", expand=True)

        sag = tk.Frame(header, bg=BG)
        sag.pack(side="right")
        self.baglanti_lbl = tk.Label(sag, text="BAGLI DEGIL", fg=RED, bg=BG, font=("Courier", 10))
        self.baglanti_lbl.pack(anchor="e")
        self.sinyal_lbl = tk.Label(sag, text="Son veri: --", fg=CYAN_DIM, bg=BG, font=("Courier", 9))
        self.sinyal_lbl.pack(anchor="e")

        # ── AYAR ÇUBUĞU ─────────────────────
        ayar = tk.Frame(self.root, bg=DARK_GRAY)
        ayar.pack(fill="x", padx=10, pady=(6, 0))

        # COM Port
        tk.Label(ayar, text="PORT:", fg=CYAN_DIM, bg=DARK_GRAY,
                 font=("Courier", 9)).pack(side="left", padx=(8, 2))
        portlar = [p.device for p in serial.tools.list_ports.comports()]
        self.port_var = tk.StringVar(value=portlar[0] if portlar else "COM3")
        tk.OptionMenu(ayar, self.port_var, *(portlar if portlar else ["COM3"])).pack(side="left")
        tk.Button(ayar, text="BAGLAN", bg=CYAN, fg=BG, font=("Courier", 9, "bold"),
                  command=self._baglan).pack(side="left", padx=6)

        # SD Kart seçici
        tk.Label(ayar, text="KAYIT YERI:", fg=CYAN_DIM, bg=DARK_GRAY,
                 font=("Courier", 9)).pack(side="left", padx=(16, 2))
        surucler = suruculeri_listele()
        self.surucu_var = tk.StringVar(value=surucler[0])
        tk.OptionMenu(ayar, self.surucu_var, *surucler).pack(side="left")
        tk.Button(ayar, text="KAYDI BASLAT", bg=GREEN, fg=BG, font=("Courier", 9, "bold"),
                  command=self._kayit_baslat).pack(side="left", padx=6)

        self.kayit_lbl = tk.Label(ayar, text="Kayit yok", fg=CYAN_DIM, bg=DARK_GRAY, font=("Courier", 8))
        self.kayit_lbl.pack(side="left", padx=8)

        # ── DURUM ───────────────────────────
        self.durum_lbl = tk.Label(self.root, text="SISTEM NORMAL",
                                  fg=GREEN, bg=BG, font=("Courier", 14, "bold"))
        self.durum_lbl.pack(pady=(6, 0))

        # ── ANA PANEL ───────────────────────
        panel = tk.Frame(self.root, bg=BG)
        panel.pack(fill="both", expand=True, padx=10, pady=5)

        self._sicaklik_panel(panel)
        self._hiz_panel(panel)
        self._gerilim_panel(panel)

        alarm_blink(self.durum_lbl)
        self._guncelle()

    def _baglan(self):
        global ser
        try:
            ser = serial.Serial(self.port_var.get(), BAUD_RATE, timeout=1)
            self.baglanti_lbl.config(text=f"BAGLI  {self.port_var.get()}", fg=GREEN)
            threading.Thread(target=serial_oku, daemon=True).start()
        except Exception:
            self.baglanti_lbl.config(text="BAGLANTI HATASI", fg=RED)

    def _kayit_baslat(self):
        if csv_baslat(self.surucu_var.get()):
            # Kısa göster
            goster = csv_yolu if len(csv_yolu) < 50 else "..." + csv_yolu[-47:]
            self.kayit_lbl.config(text=f"Kaydediliyor: {goster}", fg=GREEN)
        else:
            self.kayit_lbl.config(text="Kayit baslatılamadı", fg=RED)

    def _sicaklik_panel(self, parent):
        f = tk.Frame(parent, bg=BG)
        f.pack(side="left", fill="both", expand=True, padx=5)
        tk.Label(f, text="SICAKLIK", fg=CYAN_DIM, bg=BG, font=("Courier", 9)).pack(pady=(10, 0))
        self.sic_bar = tk.Canvas(f, width=30, height=180, bg=DARK_GRAY,
                                 highlightthickness=1, highlightbackground=CYAN_DIM)
        self.sic_bar.pack(pady=4)
        self.sic_val = tk.Label(f, text="0 C", fg=CYAN, bg=BG, font=("Courier", 16, "bold"))
        self.sic_val.pack()
        tk.Label(f, text="Batarya Paketi\nSicakligi", fg=CYAN_DIM, bg=BG,
                 font=("Courier", 9), justify="center").pack()

    def _hiz_panel(self, parent):
        f = tk.Frame(parent, bg=BG)
        f.pack(side="left", fill="both", expand=True)
        self.speedo = tk.Canvas(f, width=320, height=290, bg=BG, highlightthickness=0)
        self.speedo.pack()
        tk.Label(f, text="%", fg=CYAN_DIM, bg=BG, font=("Courier", 8)).pack()
        self.bat_bar = tk.Canvas(f, width=280, height=22, bg=DARK_GRAY,
                                 highlightthickness=1, highlightbackground=CYAN_DIM)
        self.bat_bar.pack()
        self.bat_val = tk.Label(f, text="%100", fg=CYAN, bg=BG, font=("Courier", 10))
        self.bat_val.pack()
        tk.Label(f, text="Kalan Enerji Miktari (Wh)", fg=CYAN_DIM, bg=BG, font=("Courier", 9)).pack()

    def _gerilim_panel(self, parent):
        f = tk.Frame(parent, bg=BG)
        f.pack(side="left", fill="both", expand=True, padx=5)
        tk.Label(f, text="GERILIM", fg=CYAN_DIM, bg=BG, font=("Courier", 9)).pack(pady=(10, 0))
        self.ger_bar = tk.Canvas(f, width=30, height=180, bg=DARK_GRAY,
                                 highlightthickness=1, highlightbackground=CYAN_DIM)
        self.ger_bar.pack(pady=4)
        self.ger_val = tk.Label(f, text="0 V", fg=CYAN, bg=BG, font=("Courier", 16, "bold"))
        self.ger_val.pack()
        tk.Label(f, text="Toplam Batarya\nGerilimi (V)", fg=CYAN_DIM, bg=BG,
                 font=("Courier", 9), justify="center").pack()

    def _dikey_bar(self, canvas, deger, maks, esik, ters=False):
        canvas.delete("all")
        w, h = int(canvas["width"]), int(canvas["height"])
        oran = max(0, min(deger / maks, 1.0))
        kotu = (deger < esik) if ters else (deger > esik)
        renk = RED if kotu else GREEN
        canvas.create_rectangle(0, 0, w, h, fill=DARK_GRAY, outline="")
        dolu_h = int(h * oran)
        canvas.create_rectangle(0, h-dolu_h, w, h, fill=renk, outline="")
        esik_y = h - int(h * (esik / maks))
        canvas.create_line(0, esik_y, w, esik_y, fill=ORANGE, width=1, dash=(3, 2))

    def _progress_bar(self, canvas, deger, maks, esik, ters=False):
        canvas.delete("all")
        w, h = int(canvas["width"]), int(canvas["height"])
        oran = max(0, min(deger / maks, 1.0))
        kotu = (deger < esik) if ters else (deger > esik)
        renk = RED if kotu else GREEN
        canvas.create_rectangle(0, 0, w, h, fill=DARK_GRAY, outline=CYAN_DIM, width=1)
        canvas.create_rectangle(2, 2, int(2 + (w-4)*oran), h-2, fill=renk, outline="")

    def _guncelle(self):
        h = veri["hiz"]
        s = veri["sicaklik"]
        g = veri["gerilim"]
        b = veri["batarya"]

        speedometer_ciz(self.speedo, h)

        self._dikey_bar(self.sic_bar, s, 80, SICAKLIK_ESIK)
        self.sic_val.config(text=f"{s} C", fg=RED if s > SICAKLIK_ESIK else CYAN)

        self._dikey_bar(self.ger_bar, g, 500, GERILIM_ESIK)
        self.ger_val.config(text=f"{g} V", fg=RED if g > GERILIM_ESIK else CYAN)

        self._progress_bar(self.bat_bar, b, 100, BATARYA_ESIK, ters=True)
        self.bat_val.config(text=f"%{b}", fg=RED if b < BATARYA_ESIK else CYAN)

        now = datetime.now()
        self.tarih_lbl.config(text=now.strftime("%d/%m/%Y"))
        self.saat_lbl.config(text=now.strftime("%H:%M:%S"))

        if son_veri_ms > 0:
            gecen = (time.time() * 1000 - son_veri_ms) / 1000
            if gecen > 60:
                self.sinyal_lbl.config(text=f"Baglanti kopuk {int(gecen)}s", fg=RED)
            else:
                self.sinyal_lbl.config(text=f"Son veri: {gecen:.1f}s once", fg=GREEN)

        self.root.after(GUNCELLEME, self._guncelle)

# ─────────────────────────────────────────
#  BAŞLAT
# ─────────────────────────────────────────
if __name__ == "__main__":
    root = tk.Tk()
    app = AyyildizApp(root)
    root.mainloop()
    if csv_dosya_obj:
        csv_dosya_obj.close()
