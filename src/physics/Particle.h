#pragma once
#include "../math/Vec2.h" 

// ЧАСТИЦА 
// Одна частица = положение + скорость + радиус + масса.
// Здесь только ДАННЫЕ, логика движения живёт в ParticleSystem —
// так каждый класс отвечает за одну вещь (принцип единственной
// ответственности).

class Particle {
    public:
    Vec2 position; // позиция по пикселям 
    Vec2 velocity; // куда и как быстро летит
    float radius; // размер для отрисовки и будущих столкновений
    float mass; // масса для будущих сил
    

    Particle(Vec2 pos, Vec2 vel, float r, float m)
        : position(pos), velocity(vel), radius(r), mass(m) {}

    float speed() const { return velocity.length(); }
};
