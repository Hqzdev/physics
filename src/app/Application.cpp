#include "Application.h"

Application::Application()
    : physics((float)WIDTH, (float)HEIGHT)   // сначала физика, потом рендерер
    , renderer(WIDTH, HEIGHT) {
    createWindow();
    renderer.attachWindow(hwnd);
}

//  ИГРОВОЙ ЦИКЛ 
int Application::run() {
    // Точный секундомер для измерения dt (QueryPerformanceCounter)
    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    // ФИКСИРОВАННЫЙ ШАГ ФИЗИКИ: независимо от того, насколько "лагает"
    // кадр, физика всегда считается шагами по 1/120 сек —
    // стабильно и предсказуемо (и в будущем легко перенести на GPU).
    const float FIXED_DT = 1.0f / 120.0f;
    float accumulator = 0.0f;

    int   frames = 0;       // для подсчёта FPS
    float fpsTimer = 0.0f;

    MSG msg{};              // структура оконного сообщения
    while (running) {
        // 1. Обработать ВСЕ накопившиеся сообщения окна (не блокируясь)
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);   // уйдёт в наш windowProc
        }

                // 2. Сколько времени прошло с прошлого кадра?
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart;
        prev = now;
        if (dt > 0.1f) dt = 0.1f;   // если окно "зависло" — не прыгать вперёд

        // 3. Накапливаем время и делаем фиксированные шаги физики
        accumulator += dt;
        while (accumulator >= FIXED_DT) {
            physics.update(FIXED_DT, mousePos, mouseL || mouseR, mouseR);
            accumulator -= FIXED_DT;
        }

        // 4. Рисуем. Берём частицы через публичный "только чтение" доступ.
        renderer.clear();
        for (const auto& p : physics.getParticles())
            renderer.drawParticle(p);
        renderer.present();

        // 5. FPS в заголовке окна
        ++frames;
        fpsTimer += dt;
        if (fpsTimer >= 0.5f) {
            updateWindowTitle(frames, fpsTimer);
            frames = 0;
            fpsTimer = 0.0f;
        }
    }
    return 0;
}

// Обновление заголовка: fps + число частиц
void Application::updateWindowTitle(int frames, float fpsTimer) {
    int fps = frames / (int)(fpsTimer * 2 + 0.5f);
    std::string title = "GPU Physics Simulator [CPU] | "
                      + std::to_string(fps) + " fps | "
                      + std::to_string(physics.getParticles().size()) + " particles";
    SetWindowTextA(hwnd, title.c_str());
}

// Обработчик событий окна. Windows вызывает его при кликах/клавишах.
LRESULT CALLBACK Application::windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Достаём указатель на Application из окна и работаем через него
    Application* app = (Application*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg) {
    case WM_MOUSEMOVE:
        if (app) { app->mousePos.x = (float)LOWORD(lParam); app->mousePos.y = (float)HIWORD(lParam); }
        return 0;
    case WM_LBUTTONDOWN: if (app) app->mouseL = true;  return 0;
    case WM_LBUTTONUP:   if (app) app->mouseL = false; return 0;
    case WM_RBUTTONDOWN: if (app) app->mouseR = true;  return 0;
    case WM_RBUTTONUP:   if (app) app->mouseR = false; return 0;
    case WM_KEYDOWN:
        if (app) {
            if (wParam == VK_ESCAPE) { app->running = false; DestroyWindow(hWnd); }
            if (wParam == 'G') app->physics.gravityOn = !app->physics.gravityOn;
            if (wParam == 'R') app->physics.spawnRandom(app->physics.particleCount);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);   // всё прочее — Windows по умолчанию
}

// Создание окна Windows (шаблонный код, пояснения в комментариях)
void Application::createWindow() {
    HINSTANCE inst = GetModuleHandle(nullptr);

    WNDCLASSA wc{};
    wc.lpfnWndProc   = windowProc;
    wc.hInstance     = inst;
    wc.lpszClassName = "GPUPhysicsSim";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);

    RECT rc{ 0, 0, WIDTH, HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);  // учесть рамку окна в размерах

    hwnd = CreateWindowA("GPUPhysicsSim", "GPU Physics Simulator [CPU]",
                         WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rc.right - rc.left, rc.bottom - rc.top,
                         nullptr, nullptr, inst, nullptr);

    // Передаём в окно указатель на себя, чтобы windowProc мог нас найти
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);

    ShowWindow(hwnd, SW_SHOW);
}