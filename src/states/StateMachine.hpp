#pragma once
#include <memory>
#include <SDL3/SDL.h>

namespace makai {

// ステートの基底クラス
class State {
public:
    virtual ~State() = default;

    virtual void onEnter()                    {}
    virtual void onExit()                     {}
    virtual void handleInput(SDL_Scancode key) {}
    virtual void update(float deltaTime)       {}
};

// 汎用ステートマシン
class StateMachine {
public:
    void changeState(std::unique_ptr<State> newState);

    void handleInput(SDL_Scancode key);
    void update(float deltaTime);

    State* currentState() const { return m_current.get(); }

private:
    std::unique_ptr<State> m_current;
};

} // namespace makai
