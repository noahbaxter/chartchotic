#pragma once

#include <JuceHeader.h>
#include "../UI/ToolbarComponent.h"
#include "../Visual/HighwayComponent.h"

class AssetController
{
public:
    AssetController() = default;

    // Call once after toolbar and highways are ready
    void init(ToolbarComponent& toolbar,
              std::function<void(std::function<void(HighwayComponent&)>)> forAllHighways);

    // Background images
    void scanBackgrounds();
    void loadBackground(const juce::String& filename);
    const juce::Image& getCurrentBackground() const { return backgroundImageCurrent; }
    bool isBackgroundVisible() const { return showBackground; }
    void setBackgroundVisible(bool show) { showBackground = show; }
    const juce::StringArray& getBackgroundNames() const { return backgroundNames; }
    const juce::File& getBackgroundDirectory() const { return backgroundDirectory; }

    // REAPER logo
    juce::Drawable* getReaperLogo() const { return reaperLogo.get(); }

    // Highway textures
    void scanHighwayTextures();
    void loadHighwayTexture(const juce::String& filename);
    const juce::StringArray& getHighwayTextureNames() const { return highwayTextureNames; }
    const juce::File& getHighwayTextureDirectory() const { return highwayTextureDirectory; }

private:
    ToolbarComponent* toolbar = nullptr;
    std::function<void(std::function<void(HighwayComponent&)>)> forAllHighways;

    // Backgrounds
    bool showBackground = false;
    juce::Image backgroundImageDefault;
    juce::Image backgroundImageCurrent;
    std::unique_ptr<juce::Drawable> reaperLogo;
    juce::StringArray backgroundNames;
    juce::File backgroundDirectory;

    // Highway textures
    juce::StringArray highwayTextureNames;
    juce::File highwayTextureDirectory;
};
