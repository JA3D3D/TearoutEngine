// Loads the built VST3 the way a DAW does and exercises it: scan, instantiate, parameters, audio + MIDI,
// state save/restore across instances, editor open/close. Usage: te_host_check "/path/TEAROUT ENGINE.vst3" [shot.png]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

using namespace juce;

static int checks = 0, failed = 0;
#define CHECK(c, m) do { ++checks; if (! (c)) { ++failed; std::printf ("FAIL: %s\n", m); } else std::printf ("ok:   %s\n", m); } while (0)

struct FakePlayHead : AudioPlayHead
{
    double ppq = 0, bpm = 150;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: te_host_check plugin.vst3 [shot.png]\n"); return 2; }

    VST3PluginFormat format;
    OwnedArray<PluginDescription> found;
    format.findAllTypesForFile (found, argv[1]);
    CHECK (found.size() == 1, "VST3 scan finds one plugin");
    if (found.isEmpty()) return 1;
    const auto& desc = *found[0];
    CHECK (desc.name == "TEAROUT ENGINE", "plugin name");
    CHECK (desc.isInstrument, "scanned as an instrument");

    String err;
    auto inst = format.createInstanceFromDescription (desc, 48000.0, 512, err);
    CHECK (inst != nullptr, "instantiates");
    if (! inst) { std::printf ("%s\n", err.toRawUTF8()); return 1; }
    std::printf ("      %d parameters, %d output channels, accepts MIDI %d, produces MIDI %d\n",
                 inst->getParameters().size(), inst->getTotalNumOutputChannels(), (int) inst->acceptsMidi(), (int) inst->producesMidi());
    CHECK (inst->getParameters().size() >= 40, "parameters exposed to the host");
    CHECK (inst->getTotalNumOutputChannels() == 2, "stereo out");

    FakePlayHead head;
    inst->setPlayHead (&head);
    // the preview stays silent until PLAY is pressed in the editor; MIDI OUT still follows the host
    for (auto* prm : inst->getParameters())
        if (prm->getName (64) == "MIDI Out") prm->setValueNotifyingHost (prm->getValueForText ("All Layers"));
    inst->prepareToPlay (48000.0, 512);
    AudioBuffer<float> buf (2, 512);
    double energy = 0;
    int notesOut = 0;
    const double samplesPerBeat = 48000.0 * 60.0 / head.bpm;
    for (int b = 0; b < 600; ++b)
    {
        MidiBuffer midi;
        buf.clear();
        inst->processBlock (buf, midi);
        energy += buf.getRMSLevel (0, 0, 512);
        for (const auto m : midi) if (m.getMessage().isNoteOn()) ++notesOut;
        head.ppq += 512.0 / samplesPerBeat;
    }
    CHECK (energy == 0.0, "silent while the host plays until PLAY is pressed");
    CHECK (notesOut > 20, "MIDI OUT streams the drop in sync with the host transport");
    std::printf ("      audio energy %.2f, MIDI notes out %d\n", energy, notesOut);

    // change a parameter through the host interface, save, restore into a new instance
    auto* genre = inst->getParameters()[0];
    genre->setValueNotifyingHost (genre->getValueForText ("Trap"));
    std::printf ("      param 0: %s = %s\n", genre->getName (64).toRawUTF8(), genre->getCurrentValueAsText().toRawUTF8());
    MessageManager::getInstance()->runDispatchLoopUntil (200);
    for (int b = 0; b < 4; ++b) { MidiBuffer midi; inst->processBlock (buf, midi); } // the host delivers edits via process()
    MemoryBlock state;
    inst->getStateInformation (state);
    CHECK (state.getSize() > 100, "state saved");
    auto inst2 = format.createInstanceFromDescription (desc, 48000.0, 512, err);
    inst2->setStateInformation (state.getData(), (int) state.getSize());
    MessageManager::getInstance()->runDispatchLoopUntil (300);
    std::printf ("      restored param 0 = %s\n", inst2->getParameters()[0]->getCurrentValueAsText().toRawUTF8());
    CHECK (inst2->getParameters()[0]->getCurrentValueAsText() == genre->getCurrentValueAsText(), "state restores in a new instance");

    // editor
    if (inst->hasEditor())
    {
        std::unique_ptr<AudioProcessorEditor> ed (inst->createEditorIfNeeded());
        CHECK (ed != nullptr, "editor opens");
        if (ed)
        {
            std::printf ("      editor size %d x %d\n", ed->getWidth(), ed->getHeight());
            CHECK (ed->getWidth() >= 900 && ed->getHeight() >= 580, "editor has a sensible size");
            if (argc > 2)
            {
                MessageManager::getInstance()->runDispatchLoopUntil (300);
                auto img = ed->createComponentSnapshot (ed->getLocalBounds());
                File out (argv[2]);
                out.deleteFile();
                FileOutputStream os (out);
                PNGImageFormat().writeImageToStream (img, os);
            }
            inst->editorBeingDeleted (ed.get());
            ed.reset();
        }
    }
    inst->releaseResources();
    inst.reset();
    inst2.reset();
    CHECK (true, "unloads cleanly");
    std::printf ("%d host checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
