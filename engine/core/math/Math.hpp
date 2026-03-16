#pragma once
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

namespace mk {

/// GLM と SDL3 型の変換ヘルパー関数
namespace math {

// ========================================
// SDL3 → GLM 変換
// ========================================

/// SDL_FPoint を glm::vec2 に変換
inline glm::vec2 toVec2(const SDL_FPoint& point) {
    return glm::vec2(point.x, point.y);
}

/// SDL_FRect の位置を glm::vec2 に変換
inline glm::vec2 rectPosition(const SDL_FRect& rect) {
    return glm::vec2(rect.x, rect.y);
}

/// SDL_FRect のサイズを glm::vec2 に変換
inline glm::vec2 rectSize(const SDL_FRect& rect) {
    return glm::vec2(rect.w, rect.h);
}

/// SDL_FRect の中心座標を glm::vec2 に変換
inline glm::vec2 rectCenter(const SDL_FRect& rect) {
    return glm::vec2(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f);
}

// ========================================
// GLM → SDL3 変換
// ========================================

/// glm::vec2 を SDL_FPoint に変換
inline SDL_FPoint toSDLPoint(const glm::vec2& vec) {
    return SDL_FPoint{vec.x, vec.y};
}

/// 位置とサイズから SDL_FRect を作成
inline SDL_FRect toSDLRect(const glm::vec2& position, const glm::vec2& size) {
    return SDL_FRect{position.x, position.y, size.x, size.y};
}

/// 中心座標とサイズから SDL_FRect を作成（中心を基準にする）
inline SDL_FRect toSDLRectCentered(const glm::vec2& center, const glm::vec2& size) {
    return SDL_FRect{
        center.x - size.x * 0.5f,
        center.y - size.y * 0.5f,
        size.x,
        size.y
    };
}

// ========================================
// よく使う数学ユーティリティ
// ========================================

/// 線形補間（0.0 ≤ t ≤ 1.0）
inline glm::vec2 lerp(const glm::vec2& a, const glm::vec2& b, float t) {
    return glm::mix(a, b, t);
}

/// ベクトルの長さの二乗（平方根を計算しない高速版）
inline float lengthSquared(const glm::vec2& vec) {
    return glm::dot(vec, vec);
}

/// 2点間の距離の二乗（平方根を計算しない高速版）
inline float distanceSquared(const glm::vec2& a, const glm::vec2& b) {
    return lengthSquared(b - a);
}

/// 角度をラジアンに変換
inline float toRadians(float degrees) {
    return glm::radians(degrees);
}

/// ラジアンを角度に変換
inline float toDegrees(float radians) {
    return glm::degrees(radians);
}

/// 値を範囲内にクランプ
inline float clamp(float value, float min, float max) {
    return glm::clamp(value, min, max);
}

/// vec2 を範囲内にクランプ
inline glm::vec2 clamp(const glm::vec2& value, const glm::vec2& min, const glm::vec2& max) {
    return glm::clamp(value, min, max);
}

} // namespace math

} // namespace mk
