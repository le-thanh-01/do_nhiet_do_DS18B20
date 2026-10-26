#ifndef DOC_NHIET_DO_DS18B20_H
#define DOC_NHIET_DO_DS18B20_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------------- Cấu hình mặc định ----------------
#define DS18B20_CHAN_MAC_DINH   4                 // GPIO nối chân DATA của DS18B20
#define AP_SSID_MAC_DINH        "ESP32_NhietDo"   // Tên WiFi phát ra
#define AP_MATKHAU_MAC_DINH     "12345678"        // Mật khẩu (>= 8 ký tự, để "" nếu muốn WiFi mở)
#define CHU_KY_DO_MS            1000              // Chu kỳ đo hiển thị trực tiếp (ms)
#define SO_MAU_LICH_SU          300               // Số mẫu "trực tiếp" giữ trong RAM (300 x 1s = 5 phút)

// ---------------- Lưu lịch sử vào flash (LittleFS) ----------------
#define LOG_FILE                "/lichsu.csv"     // File CSV: epoch,nhietdo
#define LOG_CHU_KY_S            30                // Ghi 1 mẫu vào lịch sử mỗi 30 giây
#define LOG_BO_DEM              4                 // Gom 4 mẫu rồi mới ghi flash (giảm mòn flash)
#define LOG_TOI_DA_BYTE         400000            // Quá dung lượng này thì xoá bớt nửa cũ nhất

class DocNhietDoDS18B20 {
public:
  DocNhietDoDS18B20();

  // Khởi tạo cảm biến, AP WiFi (IP tĩnh 192.168.4.1), LittleFS và web server
  void init(uint8_t chanDuLieu = DS18B20_CHAN_MAC_DINH,
            const char* ssid = AP_SSID_MAC_DINH,
            const char* matKhau = AP_MATKHAU_MAC_DINH);

  // Gọi liên tục trong loop(): xử lý web + đọc cảm biến + ghi lịch sử (không chặn)
  void plot();

  // Nhiệt độ gần nhất (NAN nếu chưa có / lỗi cảm biến)
  float layNhietDo() const { return _nhietDo; }

private:
  OneWire*           _oneWire;
  DallasTemperature* _sensors;
  WebServer          _server;

  float    _nhietDo;
  bool     _dangChuyenDoi;
  uint32_t _tBatDau;
  uint32_t _lanDoCuoi;

  // Bộ đệm vòng cho đồ thị trực tiếp
  float    _lichSuNhiet[SO_MAU_LICH_SU];
  uint32_t _lichSuGiay[SO_MAU_LICH_SU];   // giây kể từ khi bật máy
  uint16_t _soMau;
  uint16_t _viTriGhi;

  // Lịch sử lưu flash
  bool     _fsOk;
  bool     _daDongBo;                     // đã đồng bộ giờ từ trình duyệt chưa
  uint32_t _lanLogCuoi;
  size_t   _gioiHan;
  uint32_t _bufT[LOG_BO_DEM];
  float    _bufV[LOG_BO_DEM];
  uint8_t  _bufN;

  void docCamBien();
  void luuMau(float nhiet);
  void ghiLog(float nhiet);
  void xaLog();
  void xoayVongFile();

  void xuLyTrangChu();
  void xuLyDuLieu();
  void xuLyLichSu();
  void xuLyDongBo();
  void xuLyXoa();
};

extern DocNhietDoDS18B20 doc_nhiet_do;

#endif
