#include "Widgets.h"

namespace gx::ui
{
// ---------------------------------------------------------------- Settings
Settings::Settings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Memoria";
    o.filenameSuffix = "settings";
    o.folderName = "DSK Music/Memoria";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    o.processLock = nullptr;
    props = std::make_unique<juce::PropertiesFile> (Library::defaultRoot().getChildFile ("settings.xml"), o);
}

// ---------------------------------------------------------------- UndoStack
UndoStack::UndoStack (MemoriaProcessor& p) : proc (p) { last = Preset::capture (proc); }

bool UndoStack::differs (const Preset& a) const
{
    for (int i = 0; i < P::numParams; ++i)
        if (a.params[(size_t) i] != proc.getParam (i)) return true;
    return false;
}

void UndoStack::poll (bool dragging)
{
    if (applying || dragging || ! differs (last)) return;
    undo.push_back (last);
    if (undo.size() > 100) undo.erase (undo.begin());
    redo.clear();
    last = Preset::capture (proc);
}

void UndoStack::performUndo()
{
    if (undo.empty()) return;
    redo.push_back (Preset::capture (proc));
    applying = true;
    auto meta = proc.currentMeta;
    undo.back().applyTo (proc);
    proc.currentMeta = meta;
    applying = false;
    last = undo.back();
    undo.pop_back();
}

void UndoStack::performRedo()
{
    if (redo.empty()) return;
    undo.push_back (Preset::capture (proc));
    applying = true;
    auto meta = proc.currentMeta;
    redo.back().applyTo (proc);
    proc.currentMeta = meta;
    applying = false;
    last = redo.back();
    redo.pop_back();
}

void UndoStack::reset() { undo.clear(); redo.clear(); last = Preset::capture (proc); }

// ---------------------------------------------------------------- clipboard
juce::String fieldOf (const juce::String& id) { return id.fromFirstOccurrenceOf ("_", false, false); }

static std::map<juce::String, float>& clip() { static std::map<juce::String, float> c; return c; }
static juce::String& clipKind() { static juce::String k; return k; }
static juce::String kindOf (const juce::String& group) { return group.retainCharacters ("abcdefghijklmnopqrstuvwxyz."); }

void copySection (MemoriaProcessor& p, const juce::String& group)
{
    clip().clear();
    clipKind() = kindOf (group);
    for (auto& d : paramDefs())
        if (d.group == group) clip()[fieldOf (d.id)] = p.getParam (d.index);
}

bool pasteSection (MemoriaProcessor& p, const juce::String& group)
{
    if (clip().empty() || clipKind() != kindOf (group)) return false;
    for (auto& d : paramDefs())
        if (d.group == group)
            if (auto it = clip().find (fieldOf (d.id)); it != clip().end()) p.setParamPlain (d.index, it->second);
    return true;
}

// ---------------------------------------------------------------- ParamKnob
ParamKnob::ParamKnob (Context& c, int i, const juce::String& label, const juce::String& tip) : ctx (c), index (i)
{
    const auto& d = paramDefs()[(size_t) i];
    name = label.isNotEmpty() ? label : d.name;
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr, 2000);
    slider.setDoubleClickReturnValue (true, (double) d.def);
    slider.setTooltip (tip.isNotEmpty() ? tip : d.name);
    slider.getProperties().set ("bipolar", d.range.start < 0 && d.range.end > 0);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ctx.proc.apvts, d.id, slider);
    slider.textFromValueFunction = [d] (double v)
    {
        if (d.unit == "Hz") return v >= 1000 ? juce::String (v / 1000.0, 2) + " kHz" : (v < 100 ? juce::String (v, 1) : juce::String ((int) std::lround (v))) + " Hz";
        if (d.unit == "s") return v < 1 ? juce::String ((int) std::lround (v * 1000.0)) + " ms" : juce::String (v, 1) + " s";
        if (d.unit == "ms") return juce::String ((int) std::lround (v)) + " ms";
        if (d.unit == "dB") return juce::String (v, 1) + " dB";
        if (d.kind == pkInt) return juce::String ((int) std::lround (v)) + (d.unit.isNotEmpty() ? " " + d.unit : juce::String());
        if (d.range.start == 0 && d.range.end <= 1.5f && d.unit.isEmpty()) return juce::String ((int) std::lround (v * 100.0)) + " %";
        return juce::String (v, 2) + (d.unit.isNotEmpty() ? " " + d.unit : juce::String());
    };
    slider.updateText();
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom ((int) (labelSize + 4));
    const int s = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void ParamKnob::paint (juce::Graphics& g)
{
    g.setColour (col::textDim);
    g.setFont (font (labelSize, true));
    g.drawFittedText (name.toUpperCase(), getLocalBounds().removeFromBottom ((int) (labelSize + 4)), juce::Justification::centred, 1, 0.7f);
}

void ParamKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showMenu();
}

void ParamKnob::showMenu()
{
    juce::PopupMenu m;
    const auto& d = paramDefs()[(size_t) index];
    m.addSectionHeader (d.name);
    m.addItem (1, "MIDI Learn");
    m.addItem (2, L ("Valor por defecto", "Default value"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, def = d.def] (int r)
    {
        if (r == 1 && ctx.startMidiLearn) ctx.startMidiLearn (index);
        if (r == 2) ctx.proc.setParamPlain (index, def);
    });
}

// ---------------------------------------------------------------- toggles/combos
ParamToggle::ParamToggle (Context& c, int i, const juce::String& label)
{
    const auto& d = paramDefs()[(size_t) i];
    setButtonText (label.isNotEmpty() ? label : d.name.fromLastOccurrenceOf (" ", false, false).toUpperCase());
    setTooltip (d.name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (c.proc.apvts, d.id, *this);
}

ParamCombo::ParamCombo (Context& c, int i)
{
    const auto& d = paramDefs()[(size_t) i];
    addItemList (d.choices, 1);
    setTooltip (d.name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (c.proc.apvts, d.id, *this);
}

// ---------------------------------------------------------------- Section
Section::Section (Context& c, juce::String t, juce::String g) : title (std::move (t)), group (std::move (g)), ctx (c) {}

void Section::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    auto head = getLocalBounds().reduced (8, 4).removeFromTop (16);
    g.setColour (accent);
    g.fillRoundedRectangle (head.removeFromLeft (3).toFloat(), 1.5f);
    head.removeFromLeft (6);
    g.setColour (col::text);
    g.setFont (font (12.0f, true));
    g.drawText (title.toUpperCase(), head, juce::Justification::centredLeft);
}

void Section::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || group.isEmpty() || e.y > 24) return;
    juce::PopupMenu m;
    m.addItem (1, L ("Copiar ", "Copy ") + title);
    m.addItem (2, L ("Pegar en ", "Paste into ") + title);
    m.showMenuAsync ({}, [this] (int r)
    {
        if (r == 1) { copySection (ctx.proc, group); if (ctx.toast) ctx.toast (title + L (" copiado", " copied")); }
        if (r == 2 && ! pasteSection (ctx.proc, group) && ctx.toast) ctx.toast (L ("El portapapeles no es compatible con ", "Clipboard is not compatible with ") + title);
    });
}

// ---------------------------------------------------------------- AutoPanel
AutoPanel::AutoPanel (Context& c, const juce::String& t, const juce::String& grp, int cols, std::vector<int> skip)
    : Section (c, t, grp), columns (cols)
{
    for (auto& d : paramDefs())
    {
        if (d.group != grp || std::find (skip.begin(), skip.end(), d.index) != skip.end()) continue;
        std::unique_ptr<juce::Component> comp;
        if (d.kind == pkBool) comp = std::make_unique<ParamToggle> (c, d.index);
        else if (d.kind == pkChoice) comp = std::make_unique<ParamCombo> (c, d.index);
        else comp = std::make_unique<ParamKnob> (c, d.index);
        addAndMakeVisible (*comp);
        controls.push_back (std::move (comp));
    }
}

void AutoPanel::resized()
{
    auto r = content();
    // toggles + combos in a top strip, knobs in a grid
    std::vector<juce::Component*> top, knobs;
    for (auto& c : controls) (dynamic_cast<ParamKnob*> (c.get()) ? knobs : top).push_back (c.get());
    if (! top.empty())
    {
        auto strip = r.removeFromTop (24);
        const int w = juce::jmin (130, strip.getWidth() / (int) top.size());
        for (auto* c : top) c->setBounds (strip.removeFromLeft (w).reduced (2, 1));
        r.removeFromTop (4);
    }
    if (knobs.empty()) return;
    const int cols = columns > 0 ? columns : juce::jmax (1, r.getWidth() / 62);
    const int rows = ((int) knobs.size() + cols - 1) / cols;
    const int cw = r.getWidth() / cols, ch = juce::jmin (78, r.getHeight() / juce::jmax (1, rows));
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (r.getX() + (int) (i % (size_t) cols) * cw, r.getY() + (int) (i / (size_t) cols) * ch, cw, ch);
}
}
