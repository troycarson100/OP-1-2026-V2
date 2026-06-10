#pragma once

#include <array>
#include "Scene.h"
#include "../util/Constants.h"

namespace sculpt
{

class ParameterState;

// Owns the fixed bank of scenes. All operations are allocation-free and may
// be called at the start of an audio block.
class SceneManager
{
public:
    void reset();

    void saveScene (int index, const ParameterState& state, int currentTrack, int currentPage);

    // Returns true if the scene existed and was applied.
    bool recallScene (int index, ParameterState& state);

    // Blend scene a -> b into the live state. Returns false if either is empty.
    bool morphScenes (int indexA, int indexB, float amount, ParameterState& state);

    bool isSceneUsed (int index) const;

    int  getCurrentSceneIndex() const   { return currentScene_; }
    void setCurrentSceneIndex (int idx) { if (idx >= 0 && idx < kNumScenes) currentScene_ = idx; }

    const Scene* getScene (int index) const;

private:
    bool validIndex (int index) const { return index >= 0 && index < kNumScenes; }

    std::array<Scene, kNumScenes> scenes_ {};
    int currentScene_ = 0;
};

} // namespace sculpt
