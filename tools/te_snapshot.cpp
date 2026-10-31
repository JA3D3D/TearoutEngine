// Renders the TEAROUT ENGINE editor (and the logo) to PNG without a host. Linux / CI only.
//   te_snapshot out.png [layer|-1] [genre] [logo.png] [scale]
#include "../src/plugin/PluginEditor.h"
#include "../src/plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdlib>

using namespace juce;

static void writePng (const Image& img, const File& f)
{
    f.deleteFile();
    FileOutputStream out (f);
    PNGImageFormat png;
    png.writeImageToStream (img, out);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File out = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png");
    const int layer = argc > 2 ? String (argv[2]).getIntValue() : 2;
    const int genre = argc > 3 ? String (argv[3]).getIntValue() : 1;
    const float scale = argc > 5 ? String (argv[5]).getFloatValue() : 1.0f;

    tep::TearoutProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (auto* p = proc.apvts.getParameter (tep::ids::genre))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) genre));
    for (int l = 0; l < te::kNumLayers; ++l)
        if (auto* p = proc.apvts.getParameter (tep::ids::layer (l, "movement")))
            p->setValueNotifyingHost (p->convertTo0to1 (0.45f));
    proc.regenerateNow();
    if (auto* snd = std::getenv ("TE_SNAPSHOT_SOUND"))
    {
        String msg;
        proc.loadLayerSound (layer >= 0 ? layer : 2, File (snd), msg);
        std::printf ("%s\n", msg.toRawUTF8());
    }

    // run the preview for a moment so the playhead shows
    proc.setPreviewPlaying (true);
    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    for (int i = 0; i < 260; ++i) proc.processBlock (buf, midi);

    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (tep::TearoutEditor::kW, tep::TearoutEditor::kH);
    if (layer >= 0) proc.selectedLayer.store (layer);
    // re-create so the selection is picked up by the layer panel
    ed.reset (proc.createEditor());
    ed->setSize (tep::TearoutEditor::kW, tep::TearoutEditor::kH);
    if (layer < 0)
    {
        std::function<Component* (Component*)> find = [&] (Component* c) -> Component* {
            if (c->getComponentID() == "all") return c;
            for (auto* k : c->getChildren()) if (auto* r = find (k)) return r;
            return nullptr;
        };
        if (auto* btn = dynamic_cast<TextButton*> (find (ed.get())))
        {
            btn->setToggleState (true, dontSendNotification);
            btn->onClick();
        }
    }

    writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, scale), out);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());

    if (argc > 4)
    {
        Image logo (Image::ARGB, 1600, 400, true);
        {
            Graphics g (logo);
            tep::drawPixelLogo (g, { 40, 30, 1520, 340 }, 1.2f);
        }
        writePng (logo, File::getCurrentWorkingDirectory().getChildFile (argv[4]));
    }
    ed.reset();
    return 0;
}
