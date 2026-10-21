#pragma once
#include <vector>
#include "Particle.h"

class ParticleSystem {
public:
    int   particleCount = 10000;
    bool  gravityOn     = true;
    float gravity       = 1500.0f;
    float airDrag       = 0.05f;
    float restitution   = 0.75f;
    float worldW, worldH;

    std::vector<Particle> particles;

    explicit ParticleSystem(float w, float h);

    void spawnRandom(int count);

    void update(float dt, const Vec2& attractorPos, bool attractorActive, bool repel);

    const std::vector<Particle>& getParticles() const { return particles; }

private:
    void collideWithWalls(Particle& p);
    static float randF(float a, float b);
};