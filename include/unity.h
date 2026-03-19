#pragma once
#include <cstdint>

// ============================================================
// Unity 2018.4.11f1 — armeabi-v7a
// Native engine types (libunity.so)
// ============================================================

// ------------------------------------------------------------
// Math types
// ------------------------------------------------------------

struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;
};

struct Vector4 {
    float x, y, z, w;
};

struct Quaternion {
    float x, y, z, w;
};

struct Matrix4x4 {
    float m[4][4];
};

struct Color {
    float r, g, b, a;
};

struct Bounds {
    Vector3 center;
    Vector3 extents;
};

struct Ray {
    Vector3 origin;
    Vector3 direction;
};

struct Rect {
    float x, y, width, height;
};
