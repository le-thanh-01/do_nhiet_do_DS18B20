#include "doc_nhiet_do_DS18B20.h"
#include <sys/time.h>

// Đối tượng toàn cục: chương trình chính chỉ cần gọi doc_nhiet_do.init() và doc_nhiet_do.plot()
DocNhietDoDS18B20 doc_nhiet_do;

// Thời gian chờ chuyển đổi ở độ phân giải 12 bit (tối đa 750 ms)
static const uint16_t THOI_GIAN_CHUYEN_DOI_MS = 800;
static const char* FILE_TAM = "/lichsu_tmp.csv";

// ---------------------------------------------------------------
// Trang web (lưu trong flash). Không dùng CDN/thư viện ngoài vì
// chế độ AP không có Internet -> biểu đồ tự vẽ bằng <canvas>.
// ---------------------------------------------------------------
static const char TRANG_WEB[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 - Nhiệt độ DS18B20</title>
<style>
  :root { --bg:#f4f6f8; --card:#ffffff; --text:#1d2733; --muted:#6b7785; --accent:#e4572e;
          --grid:#dfe4ea; --btn:#e9edf2; --btnon:#e4572e; --btnontxt:#fff; --nguy:#c62828; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#12171d; --card:#1b232c; --text:#e8edf2; --muted:#93a0ae; --accent:#ff7a52;
            --grid:#2d3843; --btn:#2a3541; --btnon:#ff7a52; --btnontxt:#12171d; --nguy:#ef5350; }
  }
  * { box-sizing:border-box; }
  [hidden] { display:none !important; }
  body { margin:0; padding:16px; font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
         background:var(--bg); color:var(--text); }
  .wrap { max-width:900px; margin:0 auto; }
  .top { display:flex; justify-content:space-between; align-items:center; gap:8px; margin-bottom:12px; }
  h1 { font-size:1.2rem; margin:0; }
  .card { background:var(--card); border-radius:12px; padding:16px; margin-bottom:12px;
          box-shadow:0 1px 3px rgba(0,0,0,.12); }
  .big { font-size:3.2rem; font-weight:700; color:var(--accent); line-height:1; }
  .big small { font-size:1.4rem; font-weight:500; }
  .stats { display:flex; gap:24px; flex-wrap:wrap; margin-top:12px; color:var(--muted); font-size:.95rem; }
  .stats b { color:var(--text); }
  #tt, #lsInfo, #rd { margin-top:8px; font-size:.85rem; color:var(--muted); min-height:1.2em; }
  canvas { width:100%; height:320px; display:block; }
  button, a.btn { font:inherit; font-size:.9rem; padding:7px 12px; border:0; border-radius:8px;
                  background:var(--btn); color:var(--text); cursor:pointer; text-decoration:none; display:inline-block; }
  button.on { background:var(--btnon); color:var(--btnontxt); font-weight:600; }
  button.nguy { color:var(--nguy); }
  .bar { display:flex; flex-wrap:wrap; align-items:center; gap:8px; margin-bottom:6px; }
  .bar b { margin-right:auto; }
  .grp { display:flex; gap:4px; flex-wrap:wrap; }
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <h1>Nhiệt độ DS18B20 &mdash; ESP32</h1>
    <button id="bLs">Xem lịch sử</button>
  </div>

  <div class="card">
    <div class="big"><span id="nd">--.-</span><small> °C</small></div>
    <div class="stats">
      <span>Thấp nhất: <b id="mn">--</b> °C</span>
      <span>Cao nhất: <b id="mx">--</b> °C</span>
      <span>Trung bình: <b id="tb">--</b> °C</span>
    </div>
    <div id="tt">Đang kết nối...</div>
  </div>

  <div class="card">
    <canvas id="cv"></canvas>
  </div>

  <div class="card" id="ls" hidden>
    <div class="bar">
      <b>Lịch sử nhiệt độ</b>
      <span class="grp" id="rg">
        <button data-h="1">1 giờ</button>
        <button data-h="6">6 giờ</button>
        <button data-h="24" class="on">24 giờ</button>
        <button data-h="0">Tất cả</button>
      </span>
    </div>
    <div class="bar">
      <a class="btn" href="/history" download="lichsu_nhietdo.csv">Tải CSV</a>
      <button id="bXoa" class="nguy">Xoá lịch sử</button>
    </div>
    <canvas id="cv2"></canvas>
    <div id="rd">Chạm/di chuột lên biểu đồ để xem giá trị.</div>
    <div id="lsInfo"></div>
  </div>
</div>

<script>
const CHU_KY = 1000;
let chuKyLog = 30, daDongBo = false, dangDongBo = false;
let lsMo = false, lsData = [], gioXem = 24, lsDai = false, bd = null;

const p2 = n => (n < 10 ? '0' : '') + n;

function mmss(s) {
  s = Math.max(0, Math.round(s));
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), ss = s % 60;
  return (h > 0 ? h + ':' + p2(m) : m) + ':' + p2(ss);
}
function gio(ts)  { const d = new Date(ts * 1000); return p2(d.getHours()) + ':' + p2(d.getMinutes()); }
function ngay(ts) { const d = new Date(ts * 1000); return p2(d.getDate()) + '/' + p2(d.getMonth() + 1); }
function nhanLs(ts) { return lsDai ? ngay(ts) + ' ' + gio(ts) : gio(ts); }

// Vẽ biểu đồ đường lên canvas c. t: mảng thời gian, v: mảng giá trị (null = mất dữ liệu)
function ve(c, t, v, nhan) {
  const dpr = window.devicePixelRatio || 1;
  const w = c.clientWidth, h = c.clientHeight;
  c.width = w * dpr; c.height = h * dpr;
  const g = c.getContext('2d');
  g.scale(dpr, dpr);
  g.clearRect(0, 0, w, h);

  const css = getComputedStyle(document.documentElement);
  const mauLuoi = css.getPropertyValue('--grid').trim();
  const mauChu = css.getPropertyValue('--muted').trim();
  const mauDuong = css.getPropertyValue('--accent').trim();

  const L = 46, R = 14, T = 12, B = 28;
  const pts = [];
  for (let i = 0; i < v.length; i++) if (v[i] !== null) pts.push([t[i], v[i]]);

  g.font = '12px system-ui,sans-serif';
  g.fillStyle = mauChu;
  if (pts.length < 2) {
    g.textAlign = 'center'; g.textBaseline = 'middle';
    g.fillText('Chưa đủ dữ liệu để vẽ...', w / 2, h / 2);
    return null;
  }

  let mn = Infinity, mx = -Infinity;
  for (const p of pts) { if (p[1] < mn) mn = p[1]; if (p[1] > mx) mx = p[1]; }
  const pad = Math.max((mx - mn) * 0.15, 0.5);
  mn -= pad; mx += pad;
  const t0 = t[0], t1 = t[t.length - 1];
  const span = Math.max(t1 - t0, 1);
  const X = ts => L + (ts - t0) / span * (w - L - R);
  const Y = val => T + (1 - (val - mn) / (mx - mn)) * (h - T - B);

  // Lưới + nhãn trục Y
  g.strokeStyle = mauLuoi; g.lineWidth = 1;
  g.textAlign = 'right'; g.textBaseline = 'middle';
  for (let i = 0; i <= 5; i++) {
    const val = mn + (mx - mn) * i / 5, y = Y(val);
    g.beginPath(); g.moveTo(L, y); g.lineTo(w - R, y); g.stroke();
    g.fillText(val.toFixed(1), L - 6, y);
  }
  // Lưới + nhãn trục X
  g.textBaseline = 'top';
  for (let i = 0; i <= 5; i++) {
    const ts = t0 + span * i / 5, x = X(ts);
    g.beginPath(); g.moveTo(x, T); g.lineTo(x, h - B); g.stroke();
    g.textAlign = i === 0 ? 'left' : (i === 5 ? 'right' : 'center');
    g.fillText(nhan(ts), x, h - B + 6);
  }

  // Đường nhiệt độ (ngắt đoạn khi mất dữ liệu)
  g.strokeStyle = mauDuong; g.lineWidth = 2; g.lineJoin = 'round';
  g.beginPath();
  let bat = false;
  for (let i = 0; i < v.length; i++) {
    if (v[i] === null) { bat = false; continue; }
    const x = X(t[i]), y = Y(v[i]);
    if (!bat) { g.moveTo(x, y); bat = true; } else g.lineTo(x, y);
  }
  g.stroke();

  const last = pts[pts.length - 1];
  g.fillStyle = mauDuong;
  g.beginPath(); g.arc(X(last[0]), Y(last[1]), 4, 0, 6.2832); g.fill();

  return { L, R, w, t0, span, pts };
}

// ------------------- Lịch sử -------------------
async function taiLichSu() {
  const info = document.getElementById('lsInfo');
  info.textContent = 'Đang tải lịch sử...';
  try {
    const txt = await (await fetch('/history', { cache: 'no-store' })).text();
    const out = [];
    for (const ln of txt.split('\n')) {
      const k = ln.indexOf(',');
      if (k < 0) continue;
      const t = +ln.slice(0, k), v = +ln.slice(k + 1);
      if (t > 0 && isFinite(v)) out.push([t, v]);
    }
    lsData = out;
    veLichSu();
  } catch (e) {
    info.textContent = 'Không tải được lịch sử.';
  }
}

function veLichSu() {
  const info = document.getElementById('lsInfo');
  const c = document.getElementById('cv2');
  if (!lsData.length) {
    ve(c, [0], [null], nhanLs);
    info.textContent = daDongBo
      ? 'Chưa có dữ liệu lịch sử (ESP32 ghi 1 mẫu mỗi ' + chuKyLog + ' giây).'
      : 'Chưa đồng bộ giờ nên ESP32 chưa ghi lịch sử. Hãy giữ trang này mở vài giây.';
    bd = null;
    return;
  }
  const tCuoi = lsData[lsData.length - 1][0];
  const tu = gioXem > 0 ? tCuoi - gioXem * 3600 : 0;
  let d = lsData.filter(p => p[0] >= tu);

  // Giảm số điểm để vẽ nhanh (lấy trung bình theo nhóm)
  const MAX = 600;
  let k = 1;
  if (d.length > MAX) {
    k = Math.ceil(d.length / MAX);
    const r = [];
    for (let i = 0; i < d.length; i += k) {
      const s = d.slice(i, i + k);
      let st = 0, sv = 0;
      for (const p of s) { st += p[0]; sv += p[1]; }
      r.push([st / s.length, sv / s.length]);
    }
    d = r;
  }

  // Chèn điểm null khi có khoảng trống (lúc ESP32 tắt nguồn)
  const nguong = chuKyLog * 3 * k;
  const T = [], V = [];
  for (let i = 0; i < d.length; i++) {
    if (i > 0 && d[i][0] - d[i - 1][0] > nguong) {
      T.push((d[i][0] + d[i - 1][0]) / 2); V.push(null);
    }
    T.push(d[i][0]); V.push(d[i][1]);
  }

  lsDai = d.length > 0 && (d[d.length - 1][0] - d[0][0]) > 86400;
  bd = ve(c, T, V, nhanLs);
  info.textContent = d.length
    ? lsData.length + ' mẫu đã lưu • đang xem từ ' + ngay(d[0][0]) + ' ' + gio(d[0][0]) +
      ' đến ' + ngay(d[d.length - 1][0]) + ' ' + gio(d[d.length - 1][0])
    : 'Không có dữ liệu trong khoảng này.';
}

function hover(e) {
  if (!bd) return;
  const c = document.getElementById('cv2');
  const r = c.getBoundingClientRect();
  const px = (e.touches ? e.touches[0].clientX : e.clientX) - r.left;
  const ts = bd.t0 + (px - bd.L) / (bd.w - bd.L - bd.R) * bd.span;
  let best = null, bd2 = Infinity;
  for (const p of bd.pts) {
    const dd = Math.abs(p[0] - ts);
    if (dd < bd2) { bd2 = dd; best = p; }
  }
  if (best) {
    const d = new Date(best[0] * 1000);
    document.getElementById('rd').textContent =
      p2(d.getDate()) + '/' + p2(d.getMonth() + 1) + ' ' +
      p2(d.getHours()) + ':' + p2(d.getMinutes()) + ':' + p2(d.getSeconds()) +
      '  —  ' + best[1].toFixed(2) + ' °C';
  }
}

// ------------------- Dữ liệu trực tiếp -------------------
async function capNhat() {
  const tt = document.getElementById('tt');
  try {
    const r = await fetch('/data', { cache: 'no-store' });
    const d = await r.json();
    daDongBo = d.sync; chuKyLog = d.li;

    // Đồng bộ giờ cho ESP32 (không có RTC/Internet) để lịch sử có thời gian thật
    if (!d.sync && !dangDongBo) {
      dangDongBo = true;
      fetch('/time?t=' + Math.floor(Date.now() / 1000)).finally(() => { dangDongBo = false; });
    }

    const ok = d.nd !== null;
    document.getElementById('nd').textContent = ok ? d.nd.toFixed(2) : '--.-';
    const vals = d.v.filter(x => x !== null);
    if (vals.length) {
      document.getElementById('mn').textContent = Math.min(...vals).toFixed(2);
      document.getElementById('mx').textContent = Math.max(...vals).toFixed(2);
      document.getElementById('tb').textContent = (vals.reduce((a, b) => a + b, 0) / vals.length).toFixed(2);
    }
    tt.textContent = (ok ? 'Đang cập nhật • ESP32 đã chạy ' + mmss(d.up)
                         : 'Không đọc được DS18B20 - kiểm tra dây nối và điện trở kéo lên 4.7k') +
                     (d.sync ? ' • đang ghi lịch sử' : ' • đang đồng bộ giờ...');
    ve(document.getElementById('cv'), d.t, d.v, mmss);
  } catch (e) {
    tt.textContent = 'Mất kết nối với ESP32...';
  }
}

// ------------------- Sự kiện -------------------
document.getElementById('bLs').onclick = () => {
  lsMo = !lsMo;
  document.getElementById('ls').hidden = !lsMo;
  document.getElementById('bLs').textContent = lsMo ? 'Ẩn lịch sử' : 'Xem lịch sử';
  if (lsMo) taiLichSu();
};
document.getElementById('rg').onclick = e => {
  const b = e.target.closest('button');
  if (!b) return;
  gioXem = +b.dataset.h;
  document.querySelectorAll('#rg button').forEach(x => x.classList.toggle('on', x === b));
  veLichSu();
};
document.getElementById('bXoa').onclick = async () => {
  if (!confirm('Xoá toàn bộ lịch sử đã lưu trên ESP32?')) return;
  await fetch('/xoa', { method: 'POST' });
  taiLichSu();
};
const cv2 = document.getElementById('cv2');
cv2.addEventListener('mousemove', hover);
cv2.addEventListener('touchmove', hover, { passive: true });
window.addEventListener('resize', () => { capNhat(); if (lsMo) veLichSu(); });

capNhat();
setInterval(capNhat, CHU_KY);
setInterval(() => { if (lsMo) taiLichSu(); }, 60000);
</script>
</body>
</html>
)rawliteral";

// ---------------------------------------------------------------

DocNhietDoDS18B20::DocNhietDoDS18B20()
  : _oneWire(nullptr), _sensors(nullptr), _server(80),
    _nhietDo(NAN), _dangChuyenDoi(false), _tBatDau(0), _lanDoCuoi(0),
    _soMau(0), _viTriGhi(0),
    _fsOk(false), _daDongBo(false), _lanLogCuoi(0), _gioiHan(LOG_TOI_DA_BYTE), _bufN(0) {}

void DocNhietDoDS18B20::init(uint8_t chanDuLieu, const char* ssid, const char* matKhau) {
  // --- Cảm biến DS18B20 ---
  _oneWire = new OneWire(chanDuLieu);
  _sensors = new DallasTemperature(_oneWire);
  _sensors->begin();
  _sensors->setResolution(12);
  _sensors->setWaitForConversion(false);   // không chặn loop() khi chờ chuyển đổi

  Serial.printf("[DS18B20] Tim thay %d cam bien tren GPIO%d\n", _sensors->getDeviceCount(), chanDuLieu);

  // --- Bộ nhớ flash cho lịch sử ---
  _fsOk = LittleFS.begin(true);            // true: tự format nếu chưa có
  if (_fsOk) {
    size_t tong = LittleFS.totalBytes();
    size_t gh = tong * 40 / 100;           // file lịch sử tối đa 40% dung lượng phân vùng
    _gioiHan = gh < (size_t)LOG_TOI_DA_BYTE ? gh : (size_t)LOG_TOI_DA_BYTE;
    Serial.printf("[FS] LittleFS OK, tong %u byte, gioi han lich su %u byte\n",
                  (unsigned)tong, (unsigned)_gioiHan);
  } else {
    Serial.println("[FS] LOI mo LittleFS - se khong luu duoc lich su");
  }

  // --- WiFi Access Point, IP tĩnh 192.168.4.1 ---
  IPAddress ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip, gateway, subnet);
  bool ok = (matKhau && strlen(matKhau) >= 8) ? WiFi.softAP(ssid, matKhau)
                                              : WiFi.softAP(ssid);   // WiFi mở nếu không có mật khẩu
  Serial.printf("[AP] %s - SSID: %s\n", ok ? "Da phat WiFi" : "LOI phat WiFi", ssid);
  Serial.print("[AP] Truy cap: http://");
  Serial.println(WiFi.softAPIP());

  // --- Web server ---
  _server.on("/", HTTP_GET, [this]() { xuLyTrangChu(); });
  _server.on("/data", HTTP_GET, [this]() { xuLyDuLieu(); });
  _server.on("/history", HTTP_GET, [this]() { xuLyLichSu(); });
  _server.on("/time", HTTP_GET, [this]() { xuLyDongBo(); });
  _server.on("/xoa", HTTP_POST, [this]() { xuLyXoa(); });
  _server.onNotFound([this]() { _server.send(404, "text/plain", "404"); });
  _server.begin();

  _lanDoCuoi = millis() - CHU_KY_DO_MS;    // đo ngay lần đầu
}

void DocNhietDoDS18B20::plot() {
  _server.handleClient();
  docCamBien();
}

void DocNhietDoDS18B20::docCamBien() {
  if (!_sensors) return;
  uint32_t now = millis();

  if (!_dangChuyenDoi) {
    if (now - _lanDoCuoi >= CHU_KY_DO_MS) {
      _sensors->requestTemperatures();     // bắt đầu chuyển đổi (không chờ)
      _tBatDau = now;
      _lanDoCuoi = now;
      _dangChuyenDoi = true;
    }
  } else if (now - _tBatDau >= THOI_GIAN_CHUYEN_DOI_MS) {
    float t = _sensors->getTempCByIndex(0);
    _dangChuyenDoi = false;
    if (t == DEVICE_DISCONNECTED_C || t < -55.0f || t > 125.0f) {
      _nhietDo = NAN;
    } else {
      _nhietDo = t;
    }
    luuMau(_nhietDo);
    ghiLog(_nhietDo);
    Serial.printf("Nhiet do: %.2f C\n", _nhietDo);
  }
}

void DocNhietDoDS18B20::luuMau(float nhiet) {
  _lichSuNhiet[_viTriGhi] = nhiet;
  _lichSuGiay[_viTriGhi] = millis() / 1000;
  _viTriGhi = (_viTriGhi + 1) % SO_MAU_LICH_SU;
  if (_soMau < SO_MAU_LICH_SU) _soMau++;
}

// ---------------- Lịch sử trong flash ----------------

// Gom mẫu vào bộ đệm, đủ LOG_BO_DEM mẫu thì ghi 1 lần vào flash
void DocNhietDoDS18B20::ghiLog(float nhiet) {
  if (!_fsOk || !_daDongBo || isnan(nhiet)) return;
  uint32_t now = millis();
  if (now - _lanLogCuoi < (uint32_t)LOG_CHU_KY_S * 1000UL) return;
  _lanLogCuoi = now;

  _bufT[_bufN] = (uint32_t)time(nullptr);
  _bufV[_bufN] = nhiet;
  _bufN++;
  if (_bufN >= LOG_BO_DEM) xaLog();
}

// Ghi bộ đệm xuống file CSV
void DocNhietDoDS18B20::xaLog() {
  if (!_fsOk || _bufN == 0) return;
  File f = LittleFS.open(LOG_FILE, FILE_APPEND);
  if (!f) return;
  for (uint8_t i = 0; i < _bufN; i++) {
    f.printf("%lu,%.2f\n", (unsigned long)_bufT[i], _bufV[i]);
  }
  f.close();
  _bufN = 0;

  File r = LittleFS.open(LOG_FILE, "r");
  size_t kichThuoc = r ? r.size() : 0;
  if (r) r.close();
  if (kichThuoc > _gioiHan) xoayVongFile();
}

// File quá lớn: bỏ nửa cũ, giữ nửa mới
void DocNhietDoDS18B20::xoayVongFile() {
  File in = LittleFS.open(LOG_FILE, "r");
  if (!in) return;
  File out = LittleFS.open(FILE_TAM, "w");
  if (!out) { in.close(); return; }

  in.seek(in.size() / 2);
  in.readStringUntil('\n');                // bỏ dòng bị cắt dở
  uint8_t buf[256];
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0) break;
    out.write(buf, n);
  }
  in.close();
  out.close();
  LittleFS.remove(LOG_FILE);
  LittleFS.rename(FILE_TAM, LOG_FILE);
  Serial.println("[FS] File lich su day - da xoa nua cu");
}

// ---------------- Web handlers ----------------

void DocNhietDoDS18B20::xuLyTrangChu() {
  _server.sendHeader("Cache-Control", "no-store");
  _server.send_P(200, "text/html; charset=utf-8", TRANG_WEB);
}

// JSON: {"nd":25.31,"up":123,"sync":true,"li":30,"t":[...giây...],"v":[...nhiệt độ hoặc null...]}
void DocNhietDoDS18B20::xuLyDuLieu() {
  String s;
  s.reserve(_soMau * 18 + 96);

  s += "{\"nd\":";
  s += isnan(_nhietDo) ? String("null") : String(_nhietDo, 2);
  s += ",\"up\":";
  s += String(millis() / 1000);
  s += ",\"sync\":";
  s += _daDongBo ? "true" : "false";
  s += ",\"li\":";
  s += String(LOG_CHU_KY_S);

  uint16_t batDau = (_viTriGhi + SO_MAU_LICH_SU - _soMau) % SO_MAU_LICH_SU;

  s += ",\"t\":[";
  for (uint16_t i = 0; i < _soMau; i++) {
    if (i) s += ',';
    s += String(_lichSuGiay[(batDau + i) % SO_MAU_LICH_SU]);
  }
  s += "],\"v\":[";
  for (uint16_t i = 0; i < _soMau; i++) {
    if (i) s += ',';
    float v = _lichSuNhiet[(batDau + i) % SO_MAU_LICH_SU];
    s += isnan(v) ? String("null") : String(v, 2);
  }
  s += "]}";

  _server.sendHeader("Cache-Control", "no-store");
  _server.send(200, "application/json", s);
}

// Trả về toàn bộ file CSV lịch sử (epoch,nhietdo)
void DocNhietDoDS18B20::xuLyLichSu() {
  xaLog();                                 // ghi nốt các mẫu còn trong bộ đệm
  _server.sendHeader("Cache-Control", "no-store");
  if (!_fsOk || !LittleFS.exists(LOG_FILE)) {
    _server.send(200, "text/csv", "");
    return;
  }
  File f = LittleFS.open(LOG_FILE, "r");
  _server.streamFile(f, "text/csv");
  f.close();
}

// Trình duyệt gửi giờ hiện tại: /time?t=<epoch giây>. ESP32 không có RTC/Internet nên cần bước này.
void DocNhietDoDS18B20::xuLyDongBo() {
  if (_server.hasArg("t") && !_daDongBo) {
    unsigned long t = strtoul(_server.arg("t").c_str(), nullptr, 10);
    if (t > 1700000000UL) {                // hợp lệ (sau năm 2023)
      struct timeval tv;
      tv.tv_sec = (time_t)t;
      tv.tv_usec = 0;
      settimeofday(&tv, nullptr);
      _daDongBo = true;
      _lanLogCuoi = millis() - (uint32_t)LOG_CHU_KY_S * 1000UL;   // ghi mẫu đầu tiên ngay
      Serial.printf("[TIME] Da dong bo gio: %lu\n", t);
    }
  }
  _server.send(200, "text/plain", "ok");
}

void DocNhietDoDS18B20::xuLyXoa() {
  _bufN = 0;
  if (_fsOk) LittleFS.remove(LOG_FILE);
  _server.send(200, "text/plain", "ok");
}
