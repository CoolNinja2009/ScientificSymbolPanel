#include "UI/Animation.h"
#include "Core/Types.h"
#include "Core/Log.h"
#include <algorithm>

namespace ssp {

Animation::Animation() {
    durationMs = kAnimationDurationMs;
}

void Animation::StartFadeIn() {
    if (!enabled) {
        m_state = Idle;
        m_elapsedMs = 0.0f;
        return;
    }
    SSP_LOG_DEBUG("Animation::StartFadeIn");
    m_state = FadingIn;
    m_elapsedMs = 0.0f;
}

void Animation::StartFadeOut() {
    if (!enabled) {
        m_state = Idle;
        m_elapsedMs = 0.0f;
        return;
    }
    SSP_LOG_DEBUG("Animation::StartFadeOut");
    m_state = FadingOut;
    m_elapsedMs = 0.0f;
}

void Animation::Update(float deltaMs) {
    if (m_state == Idle) return;

    m_elapsedMs += deltaMs;

    if (m_elapsedMs >= durationMs) {
        SSP_LOG_DEBUG("Animation::Update - complete, state=%d", static_cast<int>(m_state));
        m_state = Idle;
        m_elapsedMs = 0.0f;
    }
}

void Animation::Cancel() {
    SSP_LOG_DEBUG("Animation::Cancel");
    m_state = Idle;
    m_elapsedMs = 0.0f;
}

bool Animation::IsActive() const {
    return m_state != Idle;
}

Animation::State Animation::GetState() const {
    return m_state;
}

float Animation::GetOpacity() const {
    if (m_state == Idle) return 1.0f;
    float p = Progress();

    switch (m_state) {
    case FadingIn:
        return p;                       // 0 -> 1
    case FadingOut:
        return 1.0f - p;                // 1 -> 0
    default:
        return 1.0f;
    }
}

float Animation::GetScale() const {
    if (m_state == Idle) return 1.0f;
    float p = Progress();

    switch (m_state) {
    case FadingIn:
        return 0.95f + 0.05f * p;       // 0.95 -> 1.0
    case FadingOut:
        return 1.0f - 0.05f * p;        // 1.0 -> 0.95
    default:
        return 1.0f;
    }
}

float Animation::Progress() const {
    if (durationMs <= 0.0f) return 1.0f;
    return std::clamp(m_elapsedMs / durationMs, 0.0f, 1.0f);
}

} // namespace ssp
