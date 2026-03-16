#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Presets.h"
#include "MidiExport.h"
#include "SettingsPage.h"
#include "Localization.h"
#include "PsyMelodyLookAndFeel.h"
#include <set>

// Forward declaration
class PianoRollView;

// Parameter lane displayed below the piano roll
class ParamLaneView : public juce::Component {
public:
    enum class LaneType { Velocity, Pan, Pitch };

    ParamLaneView();
    void setData(const std::vector<PsyMelody::NoteEvent>* events, int bars);
    void setLaneType(LaneType type) { laneType = type; repaint(); }
    LaneType getLaneType() const { return laneType; }
    void setZoomX(float z) { zoomX = z; repaint(); }
    void setScrollX(float s) { scrollX = s; repaint(); }
    void setProcessor(PsyMelodyProcessor* p) { processor = p; }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    std::function<void()> onParamChanged;

private:
    PsyMelodyProcessor* processor = nullptr;
    const std::vector<PsyMelody::NoteEvent>* noteEvents = nullptr;
    int numBars = 4;
    LaneType laneType = LaneType::Velocity;
    float zoomX = 1.0f;
    float scrollX = 0.0f;

    float totalWidth() const { return (float)getWidth() * zoomX; }
    float beatWidth() const { return totalWidth() / (numBars * 4.0f); }
    float xForBeat(double beat) const { return (float)beat * beatWidth() - scrollX; }
    int findNoteNear(float x) const;

    juce::Colour bgColour     {0xff0a0a16};
    juce::Colour barLineColour{0xff333355};
    juce::Colour velColour    {0xff00cc66};
    juce::Colour panColour    {0xff44aaff};
    juce::Colour pitchColour  {0xffff6644};
};

// Editable piano roll display with playback cursor and zoom
class PianoRollView : public juce::Component,
                      public juce::Timer {
public:
    PianoRollView();
    void setNoteEvents(const std::vector<PsyMelody::NoteEvent>& events, int bars);
    void setProcessor(PsyMelodyProcessor* p) { processor = p; }
    void clearSelection() { selectedNotes.clear(); repaint(); }
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    std::function<void()> onNotesChanged;
    std::function<void()> onZoomScrollChanged;

    float getZoomX() const { return zoomX; }
    float getZoomY() const { return zoomY; }
    void setZoomX(float z) { zoomX = std::clamp(z, 0.5f, 8.0f); notifyZoomScroll(); repaint(); }
    void setZoomY(float z) { zoomY = std::clamp(z, 0.5f, 4.0f); notifyZoomScroll(); repaint(); }
    float getScrollX() const { return scrollX; }
    float getScrollY() const { return scrollY; }
    void setScrollX(float s) { scrollX = std::max(0.0f, s); notifyZoomScroll(); repaint(); }
    void setScrollY(float s) { scrollY = std::max(0.0f, s); notifyZoomScroll(); repaint(); }
    float getMaxScrollX() const { return std::max(0.0f, totalWidth() - (float)getWidth()); }
    float getMaxScrollY() const { return std::max(0.0f, totalNoteHeight() - (float)getHeight()); }

private:
    PsyMelodyProcessor* processor = nullptr;
    std::vector<PsyMelody::NoteEvent> noteEvents;
    int numBars = 4;
    int lowestNote = 127, highestNote = 0;
    float zoomX = 1.0f, zoomY = 1.0f, scrollX = 0.0f, scrollY = 0.0f;

    // Selection state
    std::set<int> selectedNotes;
    std::vector<PsyMelody::NoteEvent> clipboard;

    // Rubber band selection
    bool isRubberBanding = false;
    juce::Rectangle<float> selectionRect;
    juce::Point<float> rubberBandStart;

    // Multi-note drag: store original positions of all selected notes
    struct DragOriginal { int noteNumber; double startBeat; };
    std::vector<DragOriginal> dragOriginals;

    enum class DragMode { None, MoveNote, ResizeNote, RubberBand };
    DragMode dragMode = DragMode::None;
    int dragNoteIndex = -1;
    int dragOrigNote = 0;
    double dragOrigBeat = 0.0, dragOrigDuration = 0.0;
    juce::Point<int> dragStartPos;

    void notifyZoomScroll() { if (onZoomScrollChanged) onZoomScrollChanged(); }
    float totalWidth() const { return (float)getWidth() * zoomX; }
    float beatWidth() const { return totalWidth() / (numBars * 4.0f); }
    float noteHeight() const;
    float totalNoteHeight() const;
    int noteAtY(float y) const;
    double beatAtX(float x) const;
    float xForBeat(double beat) const;
    float yForNote(int note) const;
    int findNoteAt(float x, float y) const;
    bool isOnNoteRightEdge(float x, float y, int noteIndex) const;
    void recalcNoteRange();
    double snapToGrid(double beat) const { return std::round(beat * 4.0) / 4.0; }

    juce::Colour bgColour{0xff0d0d1a}, gridColour{0xff222244}, barLineColour{0xff444466};
    juce::Colour noteColour{0xff00cc66}, accentColour{0xffff6644}, slideColour{0xff44aaff};
    juce::Colour graceColour{0xffaa66ff}, cursorColour{0xffff4488};
    juce::Colour selectedColour{0xffffff44}, resizeColour{0xffff8800};
};

class PsyMelodyEditor : public juce::AudioProcessorEditor,
                        public juce::ScrollBar::Listener {
public:
    explicit PsyMelodyEditor(PsyMelodyProcessor&);
    ~PsyMelodyEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void scrollBarMoved(juce::ScrollBar* bar, double newRangeStart) override;

private:
    PsyMelodyProcessor& psyProcessor;
    PsyMelodyLookAndFeel psyLnf;

    // Settings page
    PsyMelody::SettingsPage settingsPage;
    juce::TextButton settingsBtn;
    bool showSettings = false;
    PsyMelody::Lang currentLang = PsyMelody::Lang::EN;

    std::vector<PsyMelody::Preset> presets;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetBtn{"Save"};
    juce::Label presetLabel;

    PianoRollView pianoRoll;
    ParamLaneView paramLane;
    juce::ComboBox laneTypeSelector;
    juce::Label laneLabel;

    juce::TextButton zoomInXBtn{"H+"}, zoomOutXBtn{"H-"};
    juce::TextButton zoomInYBtn{"V+"}, zoomOutYBtn{"V-"};
    juce::TextButton zoomFitBtn{"Fit"};
    juce::ScrollBar hScrollBar{false};
    juce::ScrollBar vScrollBar{true};

    juce::ComboBox rootNoteSelector, scaleSelector, patternCategorySelector, progressionSelector;
    juce::ComboBox subgenreSelector, genModeSelector;
    juce::ComboBox bassStyleSelector, voicingStyleSelector;
    juce::ComboBox previewWaveSelector;
    juce::ToggleButton previewToggle{"Preview"};
    juce::Slider previewVolSlider;
    juce::Slider densitySlider, acidSlider, ornamentSlider, graceSlider;
    juce::Slider rhythmVarSlider, pitchRangeSlider, phraseLengthSlider, octaveSlider;
    juce::TextButton importMidiBtn{"Import MIDI"};
    juce::TextButton generateButton{"Generate"}, variationButton{"Variation"};
    juce::TextButton undoBtn{"Undo"}, redoBtn{"Redo"};
    juce::TextButton exportMidiBtn{"Export MIDI"}, copyMidiBtn{"Quick Save"}, quickSaveDirBtn{"..."};

    juce::Label rootLabel, scaleLabel, patternLabel, progressionLabel, densityLabel;
    juce::Label acidLabel, ornamentLabel, graceLabel, rhythmLabel, pitchLabel, phraseLabel, octaveLabel;
    juce::Label subgenreLabel, genModeLabel, bassStyleLabel, voicingStyleLabel, previewVolLabel;

    juce::File quickSaveDir;  // Quick save directory for Copy MIDI
    int sectionPresetY = 0, sectionGenY = 0, sectionExpY = 0; // section header Y positions

    void setupSlider(juce::Slider&, juce::Label&, const juce::String&, double, double, double, double step=0.01);
    void syncFromParams();
    void syncToParams();
    void updatePianoRoll();
    void loadPreset(int index);
    void saveUserPreset();
    void rebuildPresetList();
    juce::File getUserPresetDir();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsyMelodyEditor)
};
