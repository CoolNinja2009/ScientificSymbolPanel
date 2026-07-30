#pragma once

namespace ssp {

class Animation {
public:
    enum State { Idle, FadingIn, FadingOut };

    Animation();

    void StartFadeIn();
    void StartFadeOut();
    void Update(float deltaMs);
    void Cancel();

    bool IsActive() const;
    State GetState() const;
    float GetOpacity() const;   // 0.0 - 1.0
    float GetScale() const;     // 0.95 - 1.0

    // Config
    float durationMs = 120.0f;  // kAnimationDurationMs default
    bool enabled = true;

private:
    State m_state = Idle;
    float m_elapsedMs = 0.0f;

    float Progress() const;     // 0.0 - 1.0 clamped
};

} // namespace ssp
