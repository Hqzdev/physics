#pragma once
#include <windows.h>
#include <string>
#include "../physics/ParticleSystem.h"
#include "../render/Renderer.h"

//  ПРИЛОЖЕНИЕ 
// Собирает всё вместе: создаёт окно, владеет физикой и рендерером,
// гоняет игровой цикл и обрабатывает ввод. "Дирижёр оркестра".

class Application {
public:
    static constexpr int WIDTH  = 1280;
    static constexpr int HEIGHT = 720;

    Application();          // создаёт окно и связывает рендерер с ним
    int run();              // игровой цикл; вернёт код выхода

    // Обработчик событий окна (клики, клавиши).
    // static — потому что Windows требует обычную функцию, а не метод.
    static LRESULT CALLBACK windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND hwnd = nullptr;
    ParticleSystem physics;   // физический мир
    Renderer       renderer;  // рисовалка

    // Состояние ввода (заполняется из оконных сообщений)
    Vec2  mousePos{ 0, 0 };
    bool  mouseL  = false;
    bool  mouseR  = false;
    bool  running = true;

    void createWindow();
    void updateWindowTitle(int frames, float fpsTimer);
};