#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// RAII: каждый BeginPaint получает EndPaint; GDI-ресурсы освобождаются
// и при исключении. При нехватке памяти рисуем прямо в оконный DC.
class BufferedPaint {
public:
    explicit BufferedPaint(HWND window);
    ~BufferedPaint();
    BufferedPaint(const BufferedPaint&) = delete;
    BufferedPaint& operator=(const BufferedPaint&) = delete;
    HDC dc() const { return buffer_ ? buffer_ : screen_; }
    const RECT& bounds() const { return bounds_; }
private:
    HWND window_;
    PAINTSTRUCT paint_{};
    RECT bounds_{};
    HDC screen_ = nullptr, buffer_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previousBitmap_ = nullptr;
};
