#include "StateMachine.hpp"

namespace mk {

void StateMachine::changeState(std::unique_ptr<State> newState) {
    if (m_current) m_current->onExit();
    m_current = std::move(newState);
    if (m_current) m_current->onEnter();
}

void StateMachine::handleInput(SDL_Scancode key) {
    if (m_current) m_current->handleInput(key);
}

void StateMachine::update(float deltaTime) {
    if (m_current) m_current->update(deltaTime);
}

} // namespace mk
