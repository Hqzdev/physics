#include "ParticleSystem.h"
#include <cstdlib>
#include <ctime>

ParticleSystem::ParticleSystem(float w, float h) : worldW(w), worldH(h) {
    spawnRandom(particleCount);
}

void ParticleSystem::spawnRandom(int count) {
    std::srand((unsigned)std::time(nullptr));
    particles.clear();
    particles.reserve(count);
    for (int i = 0; i < count; ++i) {
        Vec2 pos(randF(0, worldW), randF(0, worldH * 0.5f));
        Vec2 vel(randF(-100, 100), randF(-50, 50));
        particles.emplace_back(pos, vel, randF(1.5f, 3.0f), 1.0f);
    }
}

void ParticleSystem::update(float dt, const Vec2& attractorPos,
                          bool attractorActive, bool repel) {
    for (auto& p : particles) {
        if (gravityOn) {
            p.velocity.y += gravity * dt;
        }

        if (attractorActive) {
            Vec2  toCursor = attractorPos - p.position;
            float dist     = toCursor.length();
            if (dist > 20.0f) {
                float strength = repel ? -90000.0f : 90000.0f;
                Vec2 dir = toCursor.normalized();
                p.velocity = p.velocity + dir * (strength / (dist * dist) * p.mass * dt);
            }
        }

        p.velocity = p.velocity * (1.0f - airDrag * dt);
        p.position = p.position + p.velocity * dt;
        collideWithWalls(p);
    }
}

void ParticleSystem::collideWithWalls(Particle& p) {
    if (p.position.x < p.radius) {
        p.position.x = p.radius;
        p.velocity.x = -p.velocity.x * restitution;
    }
    else if (p.position.x > worldW - p.radius) {
        p.position.x = worldW - p.radius;
        p.velocity.x = -p.velocity.x * restitution;
    }

    if (p.position.y < p.radius) {
        p.position.y = p.radius;
        p.velocity.y = -p.velocity.y * restitution;
    }
    else if (p.position.y > worldH - p.radius) {
        p.position.y = worldH - p.radius;
        p.velocity.y = -p.velocity.y * restitution;
    }
}

float ParticleSystem::randF(float a, float b) {
    return a + (b - a) * (float)std::rand() / (float)RAND_MAX;
}