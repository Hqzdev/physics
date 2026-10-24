#include "BufferedPaint.h"

BufferedPaint::BufferedPaint(HWND window) : window_(window) {
    screen_ = BeginPaint(window_, &paint_);
    GetClientRect(window_, &bounds_);
    if (bounds_.right <= 0 || bounds_.bottom <= 0) return;
    buffer_ = CreateCompatibleDC(screen_);
    if (!buffer_) return;
    bitmap_ = CreateCompatibleBitmap(screen_, bounds_.right, bounds_.bottom);
    if (!bitmap_) { DeleteDC(buffer_); buffer_ = nullptr; return; }
    previousBitmap_ = SelectObject(buffer_, bitmap_);
}

BufferedPaint::~BufferedPaint() {
    if (buffer_) {
        BitBlt(screen_, 0, 0, bounds_.right, bounds_.bottom, buffer_, 0, 0, SRCCOPY);
        SelectObject(buffer_, previousBitmap_);
        DeleteObject(bitmap_);
        DeleteDC(buffer_);
    }
    EndPaint(window_, &paint_);
}
