#include "SceneManager.h"
#include "ParameterState.h"

namespace sculpt
{

void SceneManager::reset()
{
    for (auto& scene : scenes_)
        scene = Scene {};
    currentScene_ = 0;
}

void SceneManager::saveScene (int index, const ParameterState& state, int currentTrack, int currentPage)
{
    if (! validIndex (index))
        return;
    scenes_[static_cast<size_t> (index)].captureFrom (state, currentTrack, currentPage);
}

bool SceneManager::recallScene (int index, ParameterState& state)
{
    if (! validIndex (index) || ! scenes_[static_cast<size_t> (index)].used)
        return false;

    scenes_[static_cast<size_t> (index)].applyTo (state);
    currentScene_ = index;
    return true;
}

bool SceneManager::morphScenes (int indexA, int indexB, float amount, ParameterState& state)
{
    if (! validIndex (indexA) || ! validIndex (indexB))
        return false;

    const auto& a = scenes_[static_cast<size_t> (indexA)];
    const auto& b = scenes_[static_cast<size_t> (indexB)];
    if (! a.used || ! b.used)
        return false;

    Scene::morphInto (a, b, amount, state);
    return true;
}

bool SceneManager::isSceneUsed (int index) const
{
    return validIndex (index) && scenes_[static_cast<size_t> (index)].used;
}

const Scene* SceneManager::getScene (int index) const
{
    return validIndex (index) ? &scenes_[static_cast<size_t> (index)] : nullptr;
}

} // namespace sculpt
