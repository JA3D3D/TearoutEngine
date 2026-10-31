// TEAROUT ENGINE - editor
#pragma once

#include "NeonLookAndFeel.h"
#include "PluginProcessor.h"
#include "Widgets.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace tep
{
class TearoutEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      public juce::DragAndDropContainer,
                      private juce::Timer
{
public:
    static constexpr int kW = 1200, kH = 780;

    explicit TearoutEditor (TearoutProcessor&);
    ~TearoutEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Overlay : juce::Component
    {
        explicit Overlay (TearoutEditor& e) : ed (e) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { ed.paintOverlay (g); }
        TearoutEditor& ed;
    };

    void timerCallback() override;
    void songChanged();
    void selectLayer (int l);
    void bindLayerControls();
    void refreshProgressionStars();
    void setStatus (const juce::String& s);
    void paintOverlay (juce::Graphics&);
    void layoutRoot();
    void browseForSeed();
    void browseForSound (int layer);
    int rowAt (juce::Point<int> editorPos) const;
    bool overLayerPanel (juce::Point<int> editorPos) const;
    void setDropHover (int layer);
    void refreshSoundControls();
    void saveMidiFiles();

    TearoutProcessor& proc;
    NeonLookAndFeel lnf;
    juce::Component root;
    CircuitBackground background;
    PixelLogo logo;
    GlassPanel songPanel { "", col::cyan }, layersPanel { "LAYERS", col::magenta }, rollPanel { "DROP", col::violet },
        layerPanel { "", col::cyan }, bottomPanel { "", col::violet };
    Overlay overlay { *this };

    juce::ComboBox genreBox, keyBox, progBox, chordLenBox, barsBox, midiOutBox, patternBox;
    juce::TextButton generateBtn { "GENERATE" }, variationBtn { "VARIATION" }, playBtn { "PREVIEW" },
        captureBtn { "CAPTURE" }, clearSeedBtn { "CLEAR" }, saveBtn { "SAVE .MID" }, allViewBtn { "ALL" },
        followBtn { "FOLLOW LIVE" }, loadSoundBtn { "LOAD" }, clearSoundBtn { "CLEAR" }, rootDownBtn { "-" }, rootUpBtn { "+" };
    juce::ToggleButton simplerToggle { "Simpler-ready MIDI" };
    Knob volumeKnob { "Preview", col::cyan }, octaveKnob { "Octave", col::cyan }, movementKnob { "Movement", col::cyan },
        lengthKnob { "Length", col::cyan };
    ChordStrip chordStrip;
    PianoRoll roll;
    juce::OwnedArray<LayerRow> rows;
    DragHandle layerDrag { "DRAG LAYER", col::cyan }, allDrag { "DRAG ALL LAYERS", col::magenta }, soundDrag { "SOUND", col::cyan };
    SeedZone seedZone;

    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ComboAtt> genreAtt, keyAtt, progAtt, chordLenAtt, barsAtt, midiOutAtt, patternAtt;
    std::unique_ptr<SliderAtt> volumeAtt, octaveAtt, movementAtt, lengthAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::TooltipWindow> tooltips;

    std::shared_ptr<const te::Song> song;
    bool viewAll = false;
    juce::String status;
    juce::uint32 statusTime = 0;
    float blink = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TearoutEditor)
};
} // namespace tep
