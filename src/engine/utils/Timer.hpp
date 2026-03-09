#pragma once

namespace mk {

// 経過時間カウンター（クールダウン・イベントトリガー用）
class Timer {
public:
    explicit Timer(float duration) : m_duration(duration) {}

    void update(float deltaTime) {
        if (!m_running) return;
        m_elapsed += deltaTime;
        if (m_elapsed >= m_duration) {
            m_elapsed = m_duration;
            m_running = false;
            m_finished = true;
        }
    }

    void start()  { m_elapsed = 0.0f; m_running = true; m_finished = false; }
    void stop()   { m_running = false; }
    void reset()  { m_elapsed = 0.0f; m_running = false; m_finished = false; }

    bool isRunning()  const { return m_running; }
    bool isFinished() const { return m_finished; }

    float elapsed()  const { return m_elapsed; }
    float duration() const { return m_duration; }
    float progress() const { return m_duration > 0 ? m_elapsed / m_duration : 1.0f; }

    void setDuration(float d) { m_duration = d; }

private:
    float m_duration;
    float m_elapsed  = 0.0f;
    bool  m_running  = false;
    bool  m_finished = false;
};

} // namespace mk
