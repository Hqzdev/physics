#pragma once   // защита от двойного включения файла
#include <cmath>

// ВЕКТОР 2D 
// Вся физика живёт в 2D. Вместо пары чисел (x, y), разбросанных
// по коду, заворачиваем их в класс — код читается как математика:
// force = a - b;  pos = pos + vel * dt;

class Vec2 {
public:
    float x, y;

    Vec2() : x(0.0f), y(0.0f) {}              // конструктор по умолчанию
    Vec2(float x, float y) : x(x), y(y) {}    // конструктор с координатами

    // operator+ позволяет писать "a + b" вместо Vec2(a.x + b.x, ...)
    // const после скобок обещает: "метод не меняет объект" — важная
    // привычка в ООП, компилятор следит за этим обещанием.
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s)       const { return Vec2(x * s, y * s); }

    // Длина вектора (теорема Пифагора)
    float length() const { return std::sqrt(x * x + y * y); }

    // Вектор того же направления, но длиной 1 (задаёт направление силы)
    Vec2 normalized() const {
        float len = length();
        if (len < 1e-6f) return Vec2(0, 0);   // защита от деления на ноль
        return Vec2(x / len, y / len);
    }
};