#include "PluginEditor.h"
#include "ScaleDefinitions.h"
#include <algorithm>
#include <map>

// ============================================================
// ParamLaneView - Velocity / Pan / Pitch editing
// ============================================================
ParamLaneView::ParamLaneView() {}

void ParamLaneView::setData(const std::vector<PsyMelody::NoteEvent>* events, int bars)
{
    noteEvents = events;
    numBars = std::max(1, bars);
    repaint();
}

int ParamLaneView::findNoteNear(float x) const
{
    if (!noteEvents) return -1;
    float bw = beatWidth();
    int best = -1;
    float bestDist = 999.0f;
    for (int i = 0; i < (int)noteEvents->size(); ++i) {
        const auto& ev = (*noteEvents)[static_cast<size_t>(i)];
        float nx = xForBeat(ev.startBeat);
        float nw = std::max(6.0f, (float)ev.duration * bw);
        float center = nx + nw * 0.5f;
        float dist = std::abs(x - center);
        if (dist < bestDist && x >= nx - 4 && x <= nx + nw + 4) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

void ParamLaneView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(bgColour);

    float bw = beatWidth();
    int tb = numBars * 4;

    // Grid lines
    for (int b = 0; b <= tb; ++b) {
        float x = xForBeat((double)b);
        if (x < -1 || x > bounds.getWidth() + 1) continue;
        g.setColour(b % 4 == 0 ? barLineColour : barLineColour.withAlpha(0.3f));
        g.drawLine(x, 0, x, bounds.getHeight(), b % 4 == 0 ? 1.0f : 0.5f);
    }

    if (!noteEvents) return;

    // Center line for Pan and Pitch
    if (laneType == LaneType::Pan || laneType == LaneType::Pitch) {
        float centerY = bounds.getHeight() * 0.5f;
        g.setColour(juce::Colour(0xff48474d).withAlpha(0.5f));
        g.drawLine(0, centerY, bounds.getWidth(), centerY, 1.0f);
    }

    // Draw bars for each note
    juce::Colour barColour;
    if (laneType == LaneType::Velocity) barColour = velColour;
    else if (laneType == LaneType::Pan) barColour = panColour;
    else barColour = pitchColour;

    for (const auto& ev : *noteEvents) {
        float x = xForBeat(ev.startBeat);
        float w = std::max(4.0f, (float)ev.duration * bw - 1.0f);
        if (x + w < 0 || x > bounds.getWidth()) continue;

        float barH, barY;

        if (laneType == LaneType::Velocity) {
            barH = ev.velocity * bounds.getHeight();
            barY = bounds.getHeight() - barH;
            g.setColour(barColour.withAlpha(0.7f));
            g.fillRect(x, barY, w, barH);
            g.setColour(barColour);
            g.fillRect(x, barY, w, 2.0f);
        }
        else if (laneType == LaneType::Pan) {
            float centerY = bounds.getHeight() * 0.5f;
            float val = ev.pan; // -1 to 1
            barH = std::abs(val) * centerY;
            barY = val < 0 ? centerY : centerY - barH;
            g.setColour(barColour.withAlpha(0.6f));
            g.fillRect(x, barY, w, barH);
            g.setColour(barColour);
            float lineY = centerY - val * centerY;
            g.fillRect(x, lineY - 1.0f, w, 2.0f);
        }
        else { // Pitch
            float centerY = bounds.getHeight() * 0.5f;
            float normalized = (float)ev.pitchBend / 8192.0f; // -1 to 1
            barH = std::abs(normalized) * centerY;
            barY = normalized < 0 ? centerY : centerY - barH;
            g.setColour(barColour.withAlpha(0.6f));
            g.fillRect(x, barY, w, barH);
            g.setColour(barColour);
            float lineY = centerY - normalized * centerY;
            g.fillRect(x, lineY - 1.0f, w, 2.0f);
        }
    }

    // Label
    g.setColour(juce::Colour(0xffacaab1));
    g.setFont(juce::Font(9.0f));
    juce::String label;
    if (laneType == LaneType::Velocity) label = "VEL";
    else if (laneType == LaneType::Pan) label = "PAN  L|R";
    else label = "PITCH";
    g.drawText(label, 4, 2, 60, 12, juce::Justification::centredLeft);
}

void ParamLaneView::mouseDown(const juce::MouseEvent& e)
{
    if (processor) processor->pushUndoState();
    mouseDrag(e);
}

void ParamLaneView::mouseDrag(const juce::MouseEvent& e)
{
    if (!processor || !noteEvents) return;
    int idx = findNoteNear((float)e.x);
    if (idx < 0) return;

    auto phrase = processor->getCurrentPhrase();
    float h = (float)getHeight();
    float y = (float)e.y;

    if (laneType == LaneType::Velocity) {
        float vel = std::clamp(1.0f - y / h, 0.05f, 1.0f);
        phrase[static_cast<size_t>(idx)].velocity = vel;
    }
    else if (laneType == LaneType::Pan) {
        float pan = std::clamp(-((y / h) * 2.0f - 1.0f), -1.0f, 1.0f);
        phrase[static_cast<size_t>(idx)].pan = pan;
    }
    else { // Pitch
        float normalized = std::clamp(-((y / h) * 2.0f - 1.0f), -1.0f, 1.0f);
        phrase[static_cast<size_t>(idx)].pitchBend = (int)(normalized * 8192.0f);
    }

    processor->updatePhrase(phrase);
    noteEvents = &processor->getCurrentPhrase();
    repaint();
    if (onParamChanged) onParamChanged();
}

// ============================================================
// PianoRollView
// ============================================================
PianoRollView::PianoRollView()
{
    setWantsKeyboardFocus(true);
    startTimerHz(30);

    // Zoom overlay buttons (children of piano roll)
    auto setupBtn = [this](juce::TextButton& b) {
        b.setButtonText("");
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xbb131319));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff81ecff));
        addAndMakeVisible(b);
    };
    setupBtn(zoomOutXBtn); setupBtn(zoomInXBtn);
    setupBtn(zoomOutYBtn); setupBtn(zoomInYBtn);
    setupBtn(zoomFitBtn);

    zoomOutXBtn.onClick = [this] { setZoomX(getZoomX() / 1.3f); };
    zoomInXBtn.onClick  = [this] { setZoomX(getZoomX() * 1.3f); };
    zoomOutYBtn.onClick = [this] { setZoomY(getZoomY() / 1.3f); };
    zoomInYBtn.onClick  = [this] { setZoomY(getZoomY() * 1.3f); };
    zoomFitBtn.onClick  = [this] { setZoomX(1); setZoomY(1); setScrollX(0); setScrollY(0); };
}

void PianoRollView::paintOverChildren(juce::Graphics& g)
{
    if (!zoomOutXBtn.isVisible()) return;

    auto drawMagnifier = [&](juce::Rectangle<int> btn, bool isPlus) {
        float cx = (float)btn.getCentreX();
        float cy = (float)btn.getCentreY() - 1.0f;
        float r = 5.5f;
        float handleLen = 4.5f;

        // Glass circle
        g.setColour(cursorColour.withAlpha(0.7f));
        g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.3f);

        // Handle
        float hx = cx + r * 0.7f;
        float hy = cy + r * 0.7f;
        g.drawLine(hx, hy, hx + handleLen, hy + handleLen, 1.6f);

        // +/- inside
        g.setColour(cursorColour);
        float s = r * 0.45f;
        g.drawLine(cx - s, cy, cx + s, cy, 1.3f);
        if (isPlus)
            g.drawLine(cx, cy - s, cx, cy + s, 1.3f);
    };

    auto drawFitIcon = [&](juce::Rectangle<int> btn) {
        float cx = (float)btn.getCentreX();
        float cy = (float)btn.getCentreY();
        float s = 4.5f;
        g.setColour(cursorColour.withAlpha(0.7f));
        // Four corner brackets
        g.drawLine(cx - s, cy - s, cx - s + 3, cy - s, 1.2f);
        g.drawLine(cx - s, cy - s, cx - s, cy - s + 3, 1.2f);
        g.drawLine(cx + s, cy - s, cx + s - 3, cy - s, 1.2f);
        g.drawLine(cx + s, cy - s, cx + s, cy - s + 3, 1.2f);
        g.drawLine(cx - s, cy + s, cx - s + 3, cy + s, 1.2f);
        g.drawLine(cx - s, cy + s, cx - s, cy + s - 3, 1.2f);
        g.drawLine(cx + s, cy + s, cx + s - 3, cy + s, 1.2f);
        g.drawLine(cx + s, cy + s, cx + s, cy + s - 3, 1.2f);
    };

    drawMagnifier(zoomOutXBtn.getBounds(), false);
    drawMagnifier(zoomInXBtn.getBounds(), true);
    drawMagnifier(zoomOutYBtn.getBounds(), false);
    drawMagnifier(zoomInYBtn.getBounds(), true);
    drawFitIcon(zoomFitBtn.getBounds());

    // H / V labels
    g.setColour(juce::Colour(0xffacaab1).withAlpha(0.6f));
    g.setFont(juce::Font(8.0f, juce::Font::bold));
    g.drawText("H", zoomOutXBtn.getX() - 11, zoomOutXBtn.getY(), 10, zoomOutXBtn.getHeight(),
               juce::Justification::centred);
    g.drawText("V", zoomOutYBtn.getX() - 11, zoomOutYBtn.getY(), 10, zoomOutYBtn.getHeight(),
               juce::Justification::centred);
}

void PianoRollView::setNoteEvents(const std::vector<PsyMelody::NoteEvent>& events, int bars)
{
    noteEvents = events;
    numBars = std::max(1, bars);
    // Only clear selection if note count changed significantly (e.g. generate/preset)
    // Keep selection for edits (paste, transpose, etc.)
    // Remove any selected indices that are out of range
    std::set<int> validSelection;
    for (int idx : selectedNotes)
        if (idx >= 0 && idx < (int)noteEvents.size())
            validSelection.insert(idx);
    selectedNotes = validSelection;
    recalcNoteRange();
    repaint();
}

void PianoRollView::recalcNoteRange()
{
    lowestNote = 127; highestNote = 0;
    for (const auto& e : noteEvents) {
        lowestNote = std::min(lowestNote, e.noteNumber);
        highestNote = std::max(highestNote, e.noteNumber);
    }
    if (!noteEvents.empty()) {
        lowestNote = std::max(0, lowestNote - 3);
        highestNote = std::min(127, highestNote + 3);
    } else { lowestNote = 48; highestNote = 72; }
    if (highestNote - lowestNote < 12) {
        int c = (highestNote + lowestNote) / 2;
        lowestNote = std::max(0, c - 6);
        highestNote = std::min(127, c + 6);
    }
}

float PianoRollView::noteHeight() const
{
    int r = highestNote - lowestNote + 1;
    return r <= 0 ? 10.0f : ((float)getHeight() * zoomY) / (float)r;
}

float PianoRollView::totalNoteHeight() const
{
    int r = highestNote - lowestNote + 1;
    return r <= 0 ? 10.0f : (float)r * noteHeight();
}

int PianoRollView::noteAtY(float y) const
{
    float nh = noteHeight();
    return std::clamp(highestNote - (int)((y + scrollY) / nh), lowestNote, highestNote);
}

static constexpr float pianoLabelMargin = 28.0f;
double PianoRollView::beatAtX(float x) const { return (double)((x - pianoLabelMargin + scrollX) / beatWidth()); }
float PianoRollView::xForBeat(double beat) const { return (float)beat * beatWidth() - scrollX + pianoLabelMargin; }

float PianoRollView::yForNote(int note) const
{
    float nh = noteHeight();
    return (float)(highestNote - note) * nh - scrollY;
}

int PianoRollView::findNoteAt(float x, float y) const
{
    for (int i = (int)noteEvents.size() - 1; i >= 0; --i) {
        const auto& ev = noteEvents[static_cast<size_t>(i)];
        float nx = xForBeat(ev.startBeat), nw = std::max(4.0f, (float)ev.duration * beatWidth());
        float ny = yForNote(ev.noteNumber), nh = noteHeight();
        if (x >= nx && x <= nx + nw && y >= ny && y <= ny + nh) return i;
    }
    return -1;
}

bool PianoRollView::isOnNoteRightEdge(float x, float, int idx) const
{
    if (idx < 0 || idx >= (int)noteEvents.size()) return false;
    const auto& ev = noteEvents[static_cast<size_t>(idx)];
    float re = xForBeat(ev.startBeat) + std::max(4.0f, (float)ev.duration * beatWidth());
    return std::abs(x - re) < 6.0f;
}

void PianoRollView::mouseDown(const juce::MouseEvent& e)
{
    if (!processor) return;
    grabKeyboardFocus();
    // Save undo state once at the start of any editing action
    processor->pushUndoState();
    dragStartPos = e.getPosition();
    int idx = findNoteAt((float)e.x, (float)e.y);

    if (e.mods.isRightButtonDown()) {
        if (idx >= 0) {
            // If right-clicking a selected note, remove all selected from highest index first
            if (selectedNotes.count(idx)) {
                std::vector<int> sorted(selectedNotes.begin(), selectedNotes.end());
                std::sort(sorted.rbegin(), sorted.rend());
                for (int si : sorted)
                    processor->removeNoteAt(si);
                selectedNotes.clear();
            } else {
                processor->removeNoteAt(idx);
            }
            noteEvents = processor->getCurrentPhrase();
            recalcNoteRange(); repaint();
            if (onNotesChanged) onNotesChanged();
        }
        return;
    }

    if (idx >= 0) {
        // Shift+click: toggle note in/out of selection
        if (e.mods.isShiftDown()) {
            if (selectedNotes.count(idx))
                selectedNotes.erase(idx);
            else
                selectedNotes.insert(idx);
            dragMode = DragMode::None;
            dragNoteIndex = -1;
            repaint();
            return;
        }

        // Clicking on a note that is part of multi-selection: drag all selected
        if (selectedNotes.count(idx) && selectedNotes.size() > 1) {
            dragNoteIndex = idx;
            dragOrigNote = noteEvents[static_cast<size_t>(idx)].noteNumber;
            dragOrigBeat = noteEvents[static_cast<size_t>(idx)].startBeat;
            dragOrigDuration = noteEvents[static_cast<size_t>(idx)].duration;
            // Store originals for all selected notes
            dragOriginals.clear();
            for (int si : selectedNotes) {
                if (si >= 0 && si < (int)noteEvents.size()) {
                    dragOriginals.push_back({noteEvents[static_cast<size_t>(si)].noteNumber,
                                             noteEvents[static_cast<size_t>(si)].startBeat});
                }
            }
            dragMode = DragMode::MoveNote;
            repaint();
            return;
        }

        // Clicking on unselected note without shift: select only this note
        selectedNotes.clear();
        selectedNotes.insert(idx);
        dragNoteIndex = idx;
        dragOrigNote = noteEvents[static_cast<size_t>(idx)].noteNumber;
        dragOrigBeat = noteEvents[static_cast<size_t>(idx)].startBeat;
        dragOrigDuration = noteEvents[static_cast<size_t>(idx)].duration;
        dragOriginals.clear();
        dragMode = isOnNoteRightEdge((float)e.x, (float)e.y, idx) ? DragMode::ResizeNote : DragMode::MoveNote;
        repaint();
    } else {
        // Clicked on empty space
        if (e.mods.isShiftDown()) {
            // Shift+click on empty: start rubber band without clearing selection
        } else {
            selectedNotes.clear();
        }
        // Start rubber band selection
        isRubberBanding = true;
        rubberBandStart = e.position;
        selectionRect = juce::Rectangle<float>(rubberBandStart.x, rubberBandStart.y, 0, 0);
        dragMode = DragMode::RubberBand;
        dragNoteIndex = -1;
        repaint();
    }
}

void PianoRollView::mouseDrag(const juce::MouseEvent& e)
{
    if (!processor) return;

    // Rubber band selection
    if (dragMode == DragMode::RubberBand) {
        selectionRect = juce::Rectangle<float>(
            std::min(rubberBandStart.x, e.position.x),
            std::min(rubberBandStart.y, e.position.y),
            std::abs(e.position.x - rubberBandStart.x),
            std::abs(e.position.y - rubberBandStart.y));
        repaint();
        return;
    }

    if (dragNoteIndex < 0) return;
    if (dragMode == DragMode::MoveNote) {
        int noteDelta = noteAtY((float)e.y) - noteAtY((float)dragStartPos.y);
        double beatDelta = beatAtX((float)e.x) - beatAtX((float)dragStartPos.x);

        // Multi-note drag: move all selected notes together
        if (selectedNotes.size() > 1 && selectedNotes.count(dragNoteIndex) && !dragOriginals.empty()) {
            auto ph = processor->getCurrentPhrase();
            int origIdx = 0;
            for (int si : selectedNotes) {
                if (si >= 0 && si < (int)ph.size() && origIdx < (int)dragOriginals.size()) {
                    int nn = std::clamp(dragOriginals[static_cast<size_t>(origIdx)].noteNumber + noteDelta, 0, 127);
                    double nb = std::clamp(snapToGrid(dragOriginals[static_cast<size_t>(origIdx)].startBeat + beatDelta),
                                           0.0, (double)numBars * 4.0 - 0.25);
                    ph[static_cast<size_t>(si)].noteNumber = nn;
                    ph[static_cast<size_t>(si)].startBeat = nb;
                    origIdx++;
                }
            }
            processor->updatePhrase(ph);
            noteEvents = processor->getCurrentPhrase();
        } else {
            // Single note drag
            int nn = noteAtY((float)e.y);
            double db = beatAtX((float)e.x) - beatAtX((float)dragStartPos.x);
            double nb = std::clamp(snapToGrid(dragOrigBeat + db), 0.0, (double)numBars * 4.0 - 0.25);
            processor->moveNote(dragNoteIndex, nn, nb);
            noteEvents = processor->getCurrentPhrase();
            for (int i = 0; i < (int)noteEvents.size(); ++i)
                if (noteEvents[static_cast<size_t>(i)].noteNumber == nn && std::abs(noteEvents[static_cast<size_t>(i)].startBeat - nb) < 0.01)
                    { dragNoteIndex = i; break; }
        }
    } else if (dragMode == DragMode::ResizeNote) {
        double nd = std::max(0.0625, snapToGrid(beatAtX((float)e.x) - dragOrigBeat));
        if (dragNoteIndex < (int)noteEvents.size()) {
            auto ph = processor->getCurrentPhrase();
            ph[static_cast<size_t>(dragNoteIndex)].duration = nd;
            processor->updatePhrase(ph);
            noteEvents = processor->getCurrentPhrase();
        }
    }
    repaint();
}

void PianoRollView::mouseUp(const juce::MouseEvent&)
{
    // Complete rubber band selection
    if (dragMode == DragMode::RubberBand && isRubberBanding) {
        for (int i = 0; i < (int)noteEvents.size(); ++i) {
            const auto& ev = noteEvents[static_cast<size_t>(i)];
            float nx = xForBeat(ev.startBeat);
            float nw = std::max(4.0f, (float)ev.duration * beatWidth());
            float ny = yForNote(ev.noteNumber);
            float nh2 = noteHeight();
            juce::Rectangle<float> noteRect(nx, ny, nw, nh2);
            if (selectionRect.intersects(noteRect))
                selectedNotes.insert(i);
        }
        isRubberBanding = false;
        selectionRect = {};
    }
    if (dragMode != DragMode::None && dragMode != DragMode::RubberBand && onNotesChanged)
        onNotesChanged();
    dragNoteIndex = -1; dragMode = DragMode::None; repaint();
}

void PianoRollView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown()) {
        zoomX = std::clamp(zoomX + w.deltaY * 0.5f, 0.5f, 8.0f);
        scrollX = std::clamp(scrollX, 0.0f, getMaxScrollX());
    } else if (e.mods.isShiftDown()) {
        zoomY = std::clamp(zoomY + w.deltaY * 0.3f, 0.5f, 4.0f);
        scrollY = std::clamp(scrollY, 0.0f, getMaxScrollY());
    } else {
        // Wheel vertical → vertical scroll, wheel horizontal → horizontal scroll
        scrollY = std::clamp(scrollY - w.deltaY * 40.0f, 0.0f, getMaxScrollY());
        if (std::abs(w.deltaX) > 0.001f)
            scrollX = std::clamp(scrollX - w.deltaX * 80.0f, 0.0f, getMaxScrollX());
    }
    notifyZoomScroll();
    repaint();
}

void PianoRollView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(bgColour);
    int noteRange = std::max(1, highestNote - lowestNote + 1);
    float bw = beatWidth(), nh = noteHeight();

    // Draw grid rows
    for (int n = lowestNote; n <= highestNote; ++n) {
        float y = yForNote(n);
        if (y > bounds.getHeight() || y + nh < 0) continue;
        int nio = n % 12;
        bool bk = (nio==1||nio==3||nio==6||nio==8||nio==10);
        // Grid area background (right of piano keys)
        g.setColour(bk ? juce::Colour(0xff131319) : bgColour);
        if (nio == 0) g.setColour(juce::Colour(0xff19191f));
        g.fillRect(pianoLabelMargin, y, bounds.getWidth() - pianoLabelMargin, nh);
        // Grid lines
        g.setColour(juce::Colour(0xff48474d).withAlpha(0.10f));
        g.drawHorizontalLine((int)y, pianoLabelMargin, bounds.getWidth());
    }

    // Draw piano keys in left margin
    // First pass: white key background for ALL rows (full width, no border lines)
    for (int n = lowestNote; n <= highestNote; ++n) {
        float y = yForNote(n);
        if (y > bounds.getHeight() || y + nh < 0) continue;
        g.setColour(juce::Colour(0xff2a2a32));
        g.fillRect(0.0f, y, pianoLabelMargin, nh + 1.0f);
    }
    // Second pass: black keys (shorter width, on top)
    float blackKeyW = pianoLabelMargin * 0.6f;
    for (int n = lowestNote; n <= highestNote; ++n) {
        float y = yForNote(n);
        if (y > bounds.getHeight() || y + nh < 0) continue;
        int nio = n % 12;
        bool bk = (nio==1||nio==3||nio==6||nio==8||nio==10);
        if (bk) {
            g.setColour(juce::Colour(0xff151518));
            g.fillRect(0.0f, y, blackKeyW, nh);
        }
    }
    // Separator line between keys and grid
    g.setColour(juce::Colour(0xff48474d).withAlpha(0.3f));
    g.drawVerticalLine((int)pianoLabelMargin, 0.0f, bounds.getHeight());

    // Note labels on keys
    g.setFont(juce::Font(9.0f));
    for (int n = lowestNote; n <= highestNote; ++n) {
        float y = yForNote(n);
        if (y > bounds.getHeight() || y + nh < 0) continue;
        int nio = n % 12;
        if (nio == 0) {
            g.setColour(juce::Colour(0xffacaab1));
            g.drawText("C" + juce::String(n/12-1), 0, (int)y, (int)pianoLabelMargin - 3, (int)nh, juce::Justification::centredRight);
        }
    }

    int tb = numBars * 4;
    for (int b = 0; b <= tb; ++b) {
        float x = xForBeat((double)b);
        if (x < -1 || x > bounds.getWidth()+1) continue;
        if (b%4==0) {
            g.setColour(barLineColour.withAlpha(0.3f)); g.drawLine(x,0,x,bounds.getHeight(),1.0f);
            g.setColour(juce::Colour(0xffacaab1)); g.setFont(juce::Font(9.0f));
            g.drawText(juce::String(b/4+1),(int)x+2,0,20,12,juce::Justification::centredLeft);
        } else { g.setColour(juce::Colour(0xff48474d).withAlpha(0.10f)); g.drawLine(x,0,x,bounds.getHeight(),0.5f); }
    }

    for (int i = 0; i < (int)noteEvents.size(); ++i) {
        const auto& ev = noteEvents[static_cast<size_t>(i)];
        float x = xForBeat(ev.startBeat), w = std::max(2.0f, (float)ev.duration*bw);
        float y = yForNote(ev.noteNumber), h = std::max(2.0f, nh-1.0f);
        if (x+w<0||x>bounds.getWidth()||y+h<0||y>bounds.getHeight()) continue;
        juce::Colour c;
        if (i==dragNoteIndex && dragMode==DragMode::ResizeNote) c = resizeColour;
        else if (selectedNotes.count(i)) c = selectedColour;
        else if (i==dragNoteIndex) c = selectedColour;
        else if (ev.isGraceNote) c = graceColour;
        else if (ev.accent) c = accentColour;   // Magenta for rhythm/trigger
        else if (ev.slide) c = slideColour;      // Cyan for frequency/time
        else c = noteColour;
        // Note body - sharp corners, no rounding
        g.setColour(c.withAlpha(0.75f)); g.fillRect(x,y,w,h);
        // Left border accent (2px) - matches note type colour
        juce::Colour borderC = c.brighter(0.3f);
        if (selectedNotes.count(i)) borderC = selectedColour;
        g.setColour(borderC); g.fillRect(x,y,2.0f,h);
        // Glow effect for velocity
        g.setColour(c.withAlpha(ev.velocity * 0.25f));
        g.fillRect(x,y,w,h);
    }

    float ly = bounds.getHeight()-14; g.setFont(juce::Font(9.0f));
    auto dl=[&](float lx,juce::Colour c,const juce::String& t){
        g.setColour(c); g.fillRect(lx,ly,8.0f,8.0f);
        g.setColour(juce::Colour(0xffacaab1)); g.drawText(t,(int)lx+10,(int)ly-1,50,12,juce::Justification::centredLeft);
    };
    float lm = pianoLabelMargin + 8.0f;
    dl(lm,noteColour,"Note"); dl(lm+50,accentColour,"Accent"); dl(lm+110,slideColour,"Slide"); dl(lm+160,graceColour,"Grace");
    g.setColour(juce::Colour(0xff76747b));
    g.drawText("Click:add | Drag:move | Edge:resize | RClick:del | Ctrl+Wh:zoom",
               (int)(lm+216),(int)ly-1,(int)(bounds.getWidth()-lm-220),12,juce::Justification::centredLeft);

    // Rubber band selection rectangle
    if (isRubberBanding && !selectionRect.isEmpty()) {
        g.setColour(juce::Colour(0xff81ecff).withAlpha(0.1f));
        g.fillRect(selectionRect);
        g.setColour(juce::Colour(0xff81ecff).withAlpha(0.5f));
        g.drawRect(selectionRect, 1.0f);
    }

    // Playhead - CRT scanline style with cyan glow
    if (processor && processor->isCurrentlyPlaying() && processor->getPhraseLengthBeats()>0) {
        float cx = xForBeat(processor->getPlaybackPositionBeats());
        if (cx>=0 && cx<=bounds.getWidth()) {
            // 1px primary line with 4px outer glow
            g.setColour(cursorColour.withAlpha(0.15f)); g.fillRect(cx-4.0f,0.0f,8.0f,bounds.getHeight());
            g.setColour(cursorColour); g.drawLine(cx,0,cx,bounds.getHeight(),1.0f);
        }
    }
}

bool PianoRollView::keyPressed(const juce::KeyPress& key)
{
    if (!processor) return false;

    // Ctrl+A: select all
    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0)) {
        selectedNotes.clear();
        for (int i = 0; i < (int)noteEvents.size(); ++i)
            selectedNotes.insert(i);
        repaint();
        return true;
    }

    // Escape: deselect all
    if (key == juce::KeyPress::escapeKey) {
        selectedNotes.clear();
        repaint();
        return true;
    }

    // Ctrl+C: copy selected notes to clipboard
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)) {
        if (!selectedNotes.empty()) {
            clipboard.clear();
            double minBeat = 1e9;
            for (int si : selectedNotes) {
                if (si >= 0 && si < (int)noteEvents.size())
                    minBeat = std::min(minBeat, noteEvents[static_cast<size_t>(si)].startBeat);
            }
            for (int si : selectedNotes) {
                if (si >= 0 && si < (int)noteEvents.size()) {
                    auto note = noteEvents[static_cast<size_t>(si)];
                    note.startBeat -= minBeat; // store relative to first note
                    clipboard.push_back(note);
                }
            }
        }
        return true;
    }

    // Ctrl+V: paste clipboard at current scroll position
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0)) {
        if (!clipboard.empty()) {
            processor->pushUndoState();
            double pasteAt = snapToGrid(beatAtX(0.0f));
            if (pasteAt < 0.0) pasteAt = 0.0;
            selectedNotes.clear();
            // Collect pasted notes for matching
            std::vector<PsyMelody::NoteEvent> pasted;
            for (const auto& note : clipboard) {
                auto n = note;
                n.startBeat += pasteAt;
                if (n.startBeat < (double)numBars * 4.0) {
                    processor->addNote(n);
                    pasted.push_back(n);
                }
            }
            noteEvents = processor->getCurrentPhrase();
            // Select pasted notes by matching beat + noteNumber
            for (int i = 0; i < (int)noteEvents.size(); ++i) {
                for (const auto& p : pasted) {
                    if (noteEvents[static_cast<size_t>(i)].noteNumber == p.noteNumber &&
                        std::abs(noteEvents[static_cast<size_t>(i)].startBeat - p.startBeat) < 0.01) {
                        selectedNotes.insert(i);
                        break;
                    }
                }
            }
            recalcNoteRange();
            repaint();
            if (onNotesChanged) onNotesChanged();
        }
        return true;
    }

    // Delete/Backspace: delete selected notes
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        if (!selectedNotes.empty()) {
            processor->pushUndoState();
            std::vector<int> sorted(selectedNotes.begin(), selectedNotes.end());
            std::sort(sorted.rbegin(), sorted.rend());
            for (int si : sorted)
                processor->removeNoteAt(si);
            selectedNotes.clear();
            noteEvents = processor->getCurrentPhrase();
            recalcNoteRange();
            repaint();
            if (onNotesChanged) onNotesChanged();
        }
        return true;
    }

    // Ctrl+Z: undo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) {
        processor->undo();
        noteEvents = processor->getCurrentPhrase();
        selectedNotes.clear();
        recalcNoteRange();
        repaint();
        if (onNotesChanged) onNotesChanged();
        return true;
    }

    // Ctrl+Shift+Z or Ctrl+Y: redo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0) ||
        key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0)) {
        processor->redo();
        noteEvents = processor->getCurrentPhrase();
        selectedNotes.clear();
        recalcNoteRange();
        repaint();
        if (onNotesChanged) onNotesChanged();
        return true;
    }

    // Up/Down arrows: transpose selected notes
    if (key.isKeyCode(juce::KeyPress::upKey) || key.isKeyCode(juce::KeyPress::downKey)) {
        if (!selectedNotes.empty()) {
            processor->pushUndoState();
            int semitones = key.isKeyCode(juce::KeyPress::upKey) ? 1 : -1;
            if (key.getModifiers().isShiftDown())
                semitones *= 12; // octave transpose

            auto ph = processor->getCurrentPhrase();
            for (int si : selectedNotes) {
                if (si >= 0 && si < (int)ph.size()) {
                    int nn = std::clamp(ph[static_cast<size_t>(si)].noteNumber + semitones, 0, 127);
                    ph[static_cast<size_t>(si)].noteNumber = nn;
                }
            }
            processor->updatePhrase(ph);
            noteEvents = processor->getCurrentPhrase();
            recalcNoteRange();
            repaint();
            if (onNotesChanged) onNotesChanged();
        }
        return true;
    }

    return false;
}

void PianoRollView::timerCallback()
{
    if (processor && processor->isCurrentlyPlaying()) repaint();
}

// ============================================================
// PsyMelodyEditor
// ============================================================
// Sidebar nav helpers
void PsyMelodyEditor::updateNavSelection()
{
    auto style = [&](juce::TextButton& b, int idx) {
        bool active = (activeNavIndex == idx);
        b.setColour(juce::TextButton::buttonColourId,
                     active ? psyLnf.surfaceContainerHigh : psyLnf.surfaceContainerLow);
        b.setColour(juce::TextButton::textColourOffId,
                     active ? psyLnf.primary : psyLnf.onSurfaceVariant.withAlpha(0.4f));
    };
    // When MANUAL is open, dim all mode buttons
    if (showSettings) {
        auto dim = [&](juce::TextButton& b) {
            b.setColour(juce::TextButton::buttonColourId, psyLnf.surfaceContainerLow);
            b.setColour(juce::TextButton::textColourOffId, psyLnf.onSurfaceVariant.withAlpha(0.25f));
        };
        dim(navMelodyBtn); dim(navBasslineBtn); dim(navChordBtn);
    } else {
        style(navMelodyBtn, 0); style(navBasslineBtn, 1); style(navChordBtn, 2);
    }
    // Settings gear button - keep transparent, icon drawn in paintOverChildren
    navManualBtn.setColour(juce::TextButton::buttonColourId,
                           showSettings ? psyLnf.surfaceContainerHigh : juce::Colours::transparentBlack);
    navManualBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    repaint();
}

void PsyMelodyEditor::updateSubgenreSelection()
{
    int sel = subgenreSelector.getSelectedId() - 1;
    auto style = [&](juce::TextButton& b, int idx) {
        bool active = (sel == idx);
        b.setColour(juce::TextButton::buttonColourId,
                     active ? psyLnf.surfaceContainerHigh : psyLnf.surfaceContainerLow);
        b.setColour(juce::TextButton::textColourOffId,
                     active ? psyLnf.primary : psyLnf.onSurfaceVariant.withAlpha(0.5f));
    };
    style(subGoaBtn, 0); style(subFullOnBtn, 1); style(subDarkBtn, 2); style(subProgBtn, 3);
}

void PsyMelodyEditor::updateLaneTabSelection()
{
    int sel = laneTypeSelector.getSelectedId();
    auto style = [&](juce::TextButton& b, int idx) {
        bool active = (sel == idx);
        b.setColour(juce::TextButton::buttonColourId, psyLnf.surfaceContainerLow);
        b.setColour(juce::TextButton::textColourOffId,
                     active ? psyLnf.primary : psyLnf.onSurfaceVariant.withAlpha(0.5f));
    };
    style(laneVelBtn, 1); style(lanePanBtn, 2); style(lanePitchBtn, 3);
}

PsyMelodyEditor::PsyMelodyEditor(PsyMelodyProcessor& p)
    : AudioProcessorEditor(&p), psyProcessor(p)
{
    setLookAndFeel(&psyLnf);
    setSize(960, 720);

    // Settings button
    settingsBtn.setButtonText("SET");
    settingsBtn.setColour(juce::TextButton::buttonColourId, psyLnf.surfaceContainerHigh);
    settingsBtn.setColour(juce::TextButton::textColourOffId, psyLnf.onSurfaceVariant);
    settingsBtn.onClick = [this] {
        showSettings = !showSettings;
        settingsPage.setVisible(showSettings);
        // Hide/show main UI components
        auto toggle = [&](juce::Component& c) { c.setVisible(!showSettings); };
        // presetSelector/savePresetBtn stay visible (in header)
        toggle(rootNoteSelector); toggle(scaleSelector); toggle(bpmSlider); toggle(bpmLabel);
        toggle(patternCategorySelector); toggle(progressionSelector);
        toggle(phraseLengthSlider); toggle(densitySlider);
        toggle(acidSlider); toggle(ornamentSlider); toggle(graceSlider);
        toggle(rhythmVarSlider); toggle(pitchRangeSlider); toggle(octaveSlider);
        toggle(subgenreSelector); toggle(genModeSelector);
        toggle(previewToggle); toggle(previewWaveSelector); toggle(previewWaveLabel); toggle(previewVolSlider); toggle(previewVolLabel);
        toggle(generateButton); toggle(variationButton);
        toggle(undoBtn); toggle(redoBtn);
        toggle(exportMidiBtn); toggle(importMidiBtn); toggle(copyMidiBtn); toggle(quickSaveDirBtn);
        toggle(pianoRoll); toggle(paramLane); toggle(laneTypeSelector);
        toggle(hScrollBar); toggle(vScrollBar);
        // Labels
        toggle(rootLabel); toggle(scaleLabel);
        toggle(patternLabel); toggle(progressionLabel); toggle(phraseLabel);
        toggle(densityLabel); toggle(acidLabel); toggle(ornamentLabel);
        toggle(graceLabel); toggle(rhythmLabel); toggle(pitchLabel);
        toggle(octaveLabel); toggle(subgenreLabel); toggle(genModeLabel);
        toggle(laneLabel);
        // Subgenre pills and lane tabs
        toggle(subGoaBtn); toggle(subFullOnBtn); toggle(subDarkBtn); toggle(subProgBtn);
        toggle(laneVelBtn); toggle(lanePanBtn); toggle(lanePitchBtn);

        // Bass Style / Voicing Style: respect current GenMode
        if (!showSettings) {
            int modeId = genModeSelector.getSelectedId();
            bassStyleSelector.setVisible(modeId == 2);
            bassStyleLabel.setVisible(modeId == 2);
            voicingStyleSelector.setVisible(modeId == 3);
            voicingStyleLabel.setVisible(modeId == 3);
        } else {
            bassStyleSelector.setVisible(false);
            bassStyleLabel.setVisible(false);
            voicingStyleSelector.setVisible(false);
            voicingStyleLabel.setVisible(false);
        }

        settingsPage.toFront(true);
        updateNavSelection();
        repaint();
        resized();
    };
    settingsBtn.setVisible(false);  // Hidden - MANUAL nav button replaces it
    addChildComponent(settingsBtn);

    // Settings page (hidden by default, added last so it draws on top)
    settingsPage.setVisible(false);
    settingsPage.onLanguageChanged = [this](PsyMelody::Lang lang) {
        currentLang = lang;
        generateButton.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::Generate).toUpperCase());
        variationButton.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::Variation).toUpperCase());
        exportMidiBtn.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::ExportMidi).toUpperCase());
        copyMidiBtn.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::QuickSave).toUpperCase());
        repaint();
    };
    addChildComponent(settingsPage);  // addChild, NOT addAndMakeVisible

    // Presets
    presets = PsyMelody::getFactoryPresets();

    // Load user presets from disk
    auto presetDir = getUserPresetDir();
    if (presetDir.isDirectory()) {
        for (auto& file : presetDir.findChildFiles(juce::File::findFiles, false, "*.xml")) {
            auto xml = juce::XmlDocument::parse(file);
            if (xml && xml->getTagName() == "PsyMelodyPreset") {
                PsyMelody::Preset userPreset;
                userPreset.name = xml->getStringAttribute("name").toStdString();
                userPreset.category = "User";
                userPreset.params.rootNote = xml->getIntAttribute("rootNote", 0);
                userPreset.params.scaleIndex = xml->getIntAttribute("scaleIndex", 0);
                userPreset.params.phraseLengthBars = xml->getIntAttribute("phraseLengthBars", 4);
                userPreset.params.baseOctave = xml->getIntAttribute("baseOctave", 4);
                userPreset.params.patternCategory = xml->getIntAttribute("patternCategory", 1);
                userPreset.params.progression = xml->getIntAttribute("progression", 0);
                userPreset.params.density = (float)xml->getDoubleAttribute("density", 0.6);
                userPreset.params.acidAmount = (float)xml->getDoubleAttribute("acidAmount", 0.5);
                userPreset.params.ornamentAmount = (float)xml->getDoubleAttribute("ornamentAmount", 0.3);
                userPreset.params.rhythmVariation = (float)xml->getDoubleAttribute("rhythmVariation", 0.4);
                userPreset.params.pitchRange = (float)xml->getDoubleAttribute("pitchRange", 0.5);

                // Load sequence
                auto* seqXml = xml->getChildByName("Sequence");
                if (seqXml) {
                    for (auto* noteXml : seqXml->getChildIterator()) {
                        if (noteXml->getTagName() == "Note") {
                            PsyMelody::NoteEvent note;
                            note.noteNumber = noteXml->getIntAttribute("nn", 60);
                            note.velocity = (float)noteXml->getDoubleAttribute("vel", 0.75);
                            note.pan = (float)noteXml->getDoubleAttribute("pan", 0.0);
                            note.startBeat = noteXml->getDoubleAttribute("start", 0.0);
                            note.duration = noteXml->getDoubleAttribute("dur", 0.25);
                            note.pitchBend = noteXml->getIntAttribute("pb", 0);
                            note.accent = noteXml->getBoolAttribute("acc", false);
                            note.slide = noteXml->getBoolAttribute("sld", false);
                            note.isGraceNote = noteXml->getBoolAttribute("grace", false);
                            userPreset.sequence.push_back(note);
                        }
                    }
                }

                presets.push_back(userPreset);
            }
        }
    }

    presetLabel.setText("Preset", juce::dontSendNotification);
    addAndMakeVisible(presetLabel);
    rebuildPresetList();
    presetSelector.onChange = [this] {
        int idx = presetSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < (int)presets.size()) loadPreset(idx);
    };
    addAndMakeVisible(presetSelector);

    savePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    savePresetBtn.onClick = [this] { saveUserPreset(); };
    addAndMakeVisible(savePresetBtn);

    // Piano roll
    pianoRoll.setProcessor(&psyProcessor);
    pianoRoll.onNotesChanged = [this] { updatePianoRoll(); };
    pianoRoll.onZoomScrollChanged = [this]() {
        paramLane.setZoomX(pianoRoll.getZoomX());
        paramLane.setScrollX(pianoRoll.getScrollX());

        // Update horizontal scrollbar
        double totalW = (double)pianoRoll.getWidth() * pianoRoll.getZoomX();
        double visibleW = (double)pianoRoll.getWidth();
        hScrollBar.setRangeLimits(0.0, totalW, juce::dontSendNotification);
        hScrollBar.setCurrentRange(pianoRoll.getScrollX(), visibleW, juce::dontSendNotification);

        // Update vertical scrollbar
        double totalH = (double)pianoRoll.getMaxScrollY() + pianoRoll.getHeight();
        double visibleH = (double)pianoRoll.getHeight();
        vScrollBar.setRangeLimits(0.0, totalH, juce::dontSendNotification);
        vScrollBar.setCurrentRange(pianoRoll.getScrollY(), visibleH, juce::dontSendNotification);
    };
    addAndMakeVisible(pianoRoll);

    // Scrollbars
    hScrollBar.setRangeLimits(0.0, 1.0);
    hScrollBar.setAutoHide(false);
    hScrollBar.addListener(this);
    addAndMakeVisible(hScrollBar);

    vScrollBar.setRangeLimits(0.0, 1.0);
    vScrollBar.setAutoHide(false);
    vScrollBar.addListener(this);
    addAndMakeVisible(vScrollBar);

    // Parameter lane
    paramLane.setProcessor(&psyProcessor);
    paramLane.onParamChanged = [this] { updatePianoRoll(); };
    addAndMakeVisible(paramLane);

    // Lane type selector
    laneLabel.setText("Lane", juce::dontSendNotification);
    addAndMakeVisible(laneLabel);
    laneTypeSelector.addItem("Velocity", 1);
    laneTypeSelector.addItem("Pan", 2);
    laneTypeSelector.addItem("Pitch", 3);
    laneTypeSelector.setSelectedId(1, juce::dontSendNotification);
    laneTypeSelector.onChange = [this] {
        int id = laneTypeSelector.getSelectedId();
        if (id == 1) paramLane.setLaneType(ParamLaneView::LaneType::Velocity);
        else if (id == 2) paramLane.setLaneType(ParamLaneView::LaneType::Pan);
        else paramLane.setLaneType(ParamLaneView::LaneType::Pitch);
        updateLaneTabSelection();
    };
    laneTypeSelector.setVisible(false); // Hidden - replaced by tab buttons
    laneLabel.setVisible(false);
    addAndMakeVisible(laneTypeSelector);

    // Lane tab buttons
    auto setupLaneTab = [this](juce::TextButton& b, int idx) {
        b.setColour(juce::TextButton::buttonColourId, psyLnf.surfaceContainerLow);
        b.onClick = [this, idx] {
            laneTypeSelector.setSelectedId(idx, juce::sendNotificationSync);
            updateLaneTabSelection();
        };
        addAndMakeVisible(b);
    };
    setupLaneTab(laneVelBtn, 1); setupLaneTab(lanePanBtn, 2); setupLaneTab(lanePitchBtn, 3);
    updateLaneTabSelection();

    // Selectors
    rootLabel.setText("Root", juce::dontSendNotification);
    addAndMakeVisible(rootLabel);
    for (int i = 0; i < 12; ++i)
        rootNoteSelector.addItem(PsyMelody::NOTE_NAMES[static_cast<size_t>(i)], i + 1);
    rootNoteSelector.onChange = [this] { syncToParams(); };
    addAndMakeVisible(rootNoteSelector);

    scaleLabel.setText("Scale", juce::dontSendNotification);
    addAndMakeVisible(scaleLabel);
    for (size_t i = 0; i < PsyMelody::GOA_SCALES.size(); ++i)
        scaleSelector.addItem(PsyMelody::GOA_SCALES[i].name, (int)i + 1);
    scaleSelector.onChange = [this] { syncToParams(); };
    addAndMakeVisible(scaleSelector);

    patternLabel.setText("Pattern", juce::dontSendNotification);
    addAndMakeVisible(patternLabel);
    for (size_t i = 0; i < PsyMelody::PATTERN_CATEGORY_NAMES.size(); ++i)
        patternCategorySelector.addItem(PsyMelody::PATTERN_CATEGORY_NAMES[i], (int)i + 1);
    patternCategorySelector.onChange = [this] { syncToParams(); };
    addAndMakeVisible(patternCategorySelector);

    progressionLabel.setText("Chords", juce::dontSendNotification);
    addAndMakeVisible(progressionLabel);
    // Display order: Drone, Chromatic Drone, then the rest
    {
        const int displayOrder[] = {0, 7, 1, 2, 3, 4, 5, 6, 8};
        for (int idx : displayOrder) {
            if (idx < (int)PsyMelody::GOA_PROGRESSION_NAMES.size())
                progressionSelector.addItem(PsyMelody::GOA_PROGRESSION_NAMES[static_cast<size_t>(idx)], idx + 1);
        }
    }
    progressionSelector.onChange = [this] { syncToParams(); };
    addAndMakeVisible(progressionSelector);

    // Subgenre selector
    subgenreLabel.setText("Subgenre", juce::dontSendNotification);
    addAndMakeVisible(subgenreLabel);
    for (size_t i = 0; i < PsyMelody::SUBGENRE_NAMES.size(); ++i)
        subgenreSelector.addItem(PsyMelody::SUBGENRE_NAMES[i], (int)i + 1);
    subgenreSelector.setSelectedId(1, juce::dontSendNotification);
    subgenreSelector.onChange = [this] { syncToParams(); updateSubgenreSelection(); };
    subgenreSelector.setVisible(false); // Hidden - replaced by pill buttons
    subgenreLabel.setVisible(false);
    addAndMakeVisible(subgenreSelector);

    // Subgenre pill buttons
    auto setupPill = [this](juce::TextButton& b, int idx) {
        b.setComponentID("subgenre_pill");
        b.setColour(juce::TextButton::buttonColourId, psyLnf.surfaceContainerLow);
        b.onClick = [this, idx] {
            subgenreSelector.setSelectedId(idx + 1, juce::sendNotificationSync);
            updateSubgenreSelection();
        };
        addAndMakeVisible(b);
    };
    setupPill(subGoaBtn, 0); setupPill(subFullOnBtn, 1);
    setupPill(subDarkBtn, 2); setupPill(subProgBtn, 3);
    updateSubgenreSelection();

    // Generation mode selector
    genModeLabel.setText("Mode", juce::dontSendNotification);
    addAndMakeVisible(genModeLabel);
    genModeSelector.addItem("Melody", 1);
    genModeSelector.addItem("Bassline", 2);
    genModeSelector.addItem("Chord Voicing", 3);
    genModeSelector.setSelectedId(1, juce::dontSendNotification);
    genModeSelector.onChange = [this] {
        int id = genModeSelector.getSelectedId();
        psyProcessor.setGenMode(static_cast<PsyMelodyProcessor::GenMode>(id - 1));
        bool isBass = (id == 2);
        bool isChord = (id == 3);
        bassStyleSelector.setVisible(isBass);
        bassStyleLabel.setVisible(isBass);
        voicingStyleSelector.setVisible(isChord);
        voicingStyleLabel.setVisible(isChord);
        activeNavIndex = id - 1;
        updateNavSelection();
        resized();
        repaint();
    };
    genModeSelector.setVisible(false); // Hidden - replaced by sidebar nav
    genModeLabel.setVisible(false);
    addAndMakeVisible(genModeSelector);

    // Sidebar navigation buttons
    auto setupNav = [this](juce::TextButton& b, int idx) {
        b.setColour(juce::TextButton::buttonColourId, psyLnf.surfaceContainerLow);
        b.onClick = [this, idx] {
            // Close settings page if open
            if (showSettings) settingsBtn.triggerClick();
            activeNavIndex = idx;
            genModeSelector.setSelectedId(std::min(idx + 1, 3), juce::sendNotificationSync);
            updateNavSelection();
        };
        addAndMakeVisible(b);
    };
    setupNav(navMelodyBtn, 0); setupNav(navBasslineBtn, 1);
    setupNav(navChordBtn, 2);
    // MANUAL button opens settings page
    navManualBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    navManualBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    navManualBtn.onClick = [this] { settingsBtn.triggerClick(); };
    addAndMakeVisible(navManualBtn);
    updateNavSelection();

    // Bass style selector
    bassStyleLabel.setText("Bass Style", juce::dontSendNotification);
    addAndMakeVisible(bassStyleLabel);
    for (size_t i = 0; i < PsyMelody::BASS_STYLE_NAMES.size(); ++i)
        bassStyleSelector.addItem(PsyMelody::BASS_STYLE_NAMES[i], (int)i + 1);
    bassStyleSelector.setSelectedId(1, juce::dontSendNotification);
    bassStyleSelector.setVisible(false);
    bassStyleLabel.setVisible(false);
    addAndMakeVisible(bassStyleSelector);

    // Voicing style selector
    voicingStyleLabel.setText("Voicing", juce::dontSendNotification);
    addAndMakeVisible(voicingStyleLabel);
    for (size_t i = 0; i < PsyMelody::VOICING_STYLE_NAMES.size(); ++i)
        voicingStyleSelector.addItem(PsyMelody::VOICING_STYLE_NAMES[i], (int)i + 1);
    voicingStyleSelector.setSelectedId(1, juce::dontSendNotification);
    voicingStyleSelector.setVisible(false);
    voicingStyleLabel.setVisible(false);
    addAndMakeVisible(voicingStyleSelector);

    // Preview synth controls
    previewToggle.setComponentID("preview_toggle");
    previewToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
    previewToggle.onClick = [this] { psyProcessor.setPreviewEnabled(previewToggle.getToggleState()); };
    addAndMakeVisible(previewToggle);

    previewWaveSelector.addItem("Saw", 1);
    previewWaveSelector.addItem("Square", 2);
    previewWaveSelector.addItem("Sine", 3);
    previewWaveSelector.addItem("Triangle", 4);
    previewWaveSelector.setSelectedId(1, juce::dontSendNotification);
    previewWaveSelector.onChange = [this] {
        psyProcessor.setPreviewWaveType(
            static_cast<PsyMelody::PreviewSynth::WaveType>(previewWaveSelector.getSelectedId() - 1));
    };
    addAndMakeVisible(previewWaveSelector);

    previewWaveLabel.setText("OSC", juce::dontSendNotification);
    previewWaveLabel.setFont(juce::Font(12.0f));
    previewWaveLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    previewWaveLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(previewWaveLabel);

    previewVolSlider.setRange(0, 1, 0.01);
    previewVolSlider.setValue(0.15);
    previewVolSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    previewVolSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    previewVolSlider.onValueChange = [this] { psyProcessor.setPreviewVolume((float)previewVolSlider.getValue()); };
    previewVolLabel.setText("", juce::dontSendNotification);
    addAndMakeVisible(previewVolSlider);
    addAndMakeVisible(previewVolLabel);

    // Sliders
    // BPM slider
    bpmSlider.setRange(60, 200, 1);
    bpmSlider.setValue(145);
    bpmSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bpmSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff000000));
    bpmSlider.textFromValueFunction = [](double v) { return juce::String((int)v) + " bpm"; };
    bpmSlider.setMouseDragSensitivity(200);
    bpmSlider.onValueChange = [this] {
        psyProcessor.getGeneratorParams().bpm = bpmSlider.getValue();
    };
    bpmLabel.setText("BPM", juce::dontSendNotification);
    addAndMakeVisible(bpmSlider);
    addAndMakeVisible(bpmLabel);

    setupSlider(phraseLengthSlider, phraseLabel, "Bars", 1, 16, 4, 1);
    phraseLengthSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    phraseLengthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    phraseLengthSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff000000));
    phraseLengthSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff81ecff));
    phraseLengthSlider.textFromValueFunction = [](double v) { return juce::String((int)v) + " steps"; };
    phraseLengthSlider.setMouseDragSensitivity(150);
    setupSlider(densitySlider, densityLabel, "Density", 0, 1, 0.6);
    setupSlider(acidSlider, acidLabel, "Acid", 0, 1, 0.5);
    setupSlider(ornamentSlider, ornamentLabel, "Ornament", 0, 1, 0.3);
    setupSlider(graceSlider, graceLabel, "Grace", 0, 1, 0.0);
    setupSlider(rhythmVarSlider, rhythmLabel, "Rhythm Var", 0, 1, 0.4);
    setupSlider(pitchRangeSlider, pitchLabel, "Pitch Range", 0, 1, 0.5);
    setupSlider(octaveSlider, octaveLabel, "Octave", 2, 6, 4, 1);
    octaveSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    octaveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    octaveSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff000000));
    octaveSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff81ecff));
    octaveSlider.textFromValueFunction = [](double v) { return juce::String((int)v) + " oct"; };
    octaveSlider.setMouseDragSensitivity(150);

    // Buttons
    generateButton.onClick = [this] {
        syncToParams();
        pianoRoll.clearSelection();
        auto mode = psyProcessor.getGenMode();
        if (mode == PsyMelodyProcessor::GenMode::Bassline) {
            PsyMelody::BassParams bp;
            const auto& gp = psyProcessor.getGeneratorParams();
            bp.rootNote = gp.rootNote;
            bp.scaleIndex = gp.scaleIndex;
            bp.bpm = gp.bpm;
            bp.phraseLengthBars = gp.phraseLengthBars;
            bp.bassStyle = bassStyleSelector.getSelectedId() - 1;
            bp.density = gp.density;
            bp.acidAmount = gp.acidAmount;
            bp.variation = gp.rhythmVariation;
            psyProcessor.generateBassline(bp);
        } else if (mode == PsyMelodyProcessor::GenMode::ChordVoicing) {
            PsyMelody::ChordVoicingParams cp;
            const auto& gp = psyProcessor.getGeneratorParams();
            cp.rootNote = gp.rootNote;
            cp.scaleIndex = gp.scaleIndex;
            cp.bpm = gp.bpm;
            cp.phraseLengthBars = gp.phraseLengthBars;
            cp.progression = gp.progression;
            cp.voicingStyle = voicingStyleSelector.getSelectedId() - 1;
            cp.baseOctave = gp.baseOctave;
            cp.density = gp.density;
            psyProcessor.generateChordVoicing(cp);
        } else {
            psyProcessor.generateNewPhrase();
        }
        updatePianoRoll();
    };
    generateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff005762));
    generateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff81ecff));
    addAndMakeVisible(generateButton);

    variationButton.onClick = [this] { syncToParams(); pianoRoll.clearSelection(); psyProcessor.generateVariation(); updatePianoRoll(); };
    variationButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f1f26));
    addAndMakeVisible(variationButton);

    // Undo/Redo buttons
    undoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f1f26));
    undoBtn.onClick = [this] {
        psyProcessor.undo();
        updatePianoRoll();
    };
    addAndMakeVisible(undoBtn);

    redoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f1f26));
    redoBtn.onClick = [this] {
        psyProcessor.redo();
        updatePianoRoll();
    };
    addAndMakeVisible(redoBtn);

    // Export MIDI button
    exportMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f1f26));
    exportMidiBtn.onClick = [this] {
        const auto& phrase = psyProcessor.getCurrentPhrase();
        if (phrase.empty()) return;

        activeFileChooser = std::make_shared<juce::FileChooser>(
            "Export MIDI", juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                              .getChildFile("PsyMelody.mid"),
            "*.mid");
        activeFileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File()) {
                auto f = file.hasFileExtension(".mid") ? file : file.withFileExtension(".mid");
                double bpm = psyProcessor.getGeneratorParams().bpm;
                // Try to get DAW BPM
                if (auto* ph = psyProcessor.getPlayHead()) {
                    if (auto pos = ph->getPosition()) {
                        if (auto b = pos->getBpm()) bpm = *b;
                    }
                }
                bool ok = PsyMelody::MidiExport::exportToFile(
                    psyProcessor.getCurrentPhrase(), f, bpm);
                if (ok) {
                    exportMidiBtn.setButtonText("EXPORTED!");
                    juce::Timer::callAfterDelay(2000, [this] { exportMidiBtn.setButtonText("EXPORT MIDI"); });
                }
            }
        });
    };
    addAndMakeVisible(exportMidiBtn);

    // Quick save MIDI
    copyMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f1f26));
    copyMidiBtn.onClick = [this] {
        const auto& phrase = psyProcessor.getCurrentPhrase();
        if (phrase.empty()) return;

        auto doSave = [this](const juce::File& dir) {
            quickSaveDir = dir;
            auto file = dir.getChildFile("PsyMelody.mid");
            if (file.existsAsFile()) {
                int n = 1;
                while (dir.getChildFile("PsyMelody_" + juce::String(n) + ".mid").existsAsFile())
                    n++;
                file = dir.getChildFile("PsyMelody_" + juce::String(n) + ".mid");
            }
            double bpm = psyProcessor.getGeneratorParams().bpm;
            if (auto* ph = psyProcessor.getPlayHead()) {
                if (auto pos = ph->getPosition()) {
                    if (auto b = pos->getBpm()) bpm = *b;
                }
            }
            bool ok = PsyMelody::MidiExport::exportToFile(
                psyProcessor.getCurrentPhrase(), file, bpm);
            if (ok) {
                copyMidiBtn.setButtonText(file.getFileName());
                juce::Timer::callAfterDelay(2000, [this] { copyMidiBtn.setButtonText("QUICK SAVE"); });
            }
        };

        if (quickSaveDir.isDirectory()) {
            doSave(quickSaveDir);
        } else {
            activeFileChooser = std::make_shared<juce::FileChooser>(
                "Select Quick Save Folder",
                juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
            activeFileChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [this, doSave](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.isDirectory())
                        doSave(result);
                });
        }
    };
    addAndMakeVisible(copyMidiBtn);

    // Change quick save folder button
    quickSaveDirBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f1f26));
    quickSaveDirBtn.onClick = [this] {
        activeFileChooser = std::make_shared<juce::FileChooser>(
            "Select Quick Save Folder",
            quickSaveDir.isDirectory() ? quickSaveDir
                : juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
        activeFileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.isDirectory()) {
                    quickSaveDir = result;
                    quickSaveDirBtn.setButtonText("...");
                }
            });
    };
    addAndMakeVisible(quickSaveDirBtn);

    // Import MIDI
    importMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f1f26));
    importMidiBtn.onClick = [this] {
        importMidiBtn.setButtonText("SELECTING...");

        activeFileChooser = std::make_shared<juce::FileChooser>(
            "Import MIDI",
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
            "*.mid;*.midi");
        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles;

        activeFileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (!file.existsAsFile()) {
                importMidiBtn.setButtonText("IMPORT MIDI");
                return;
            }

            importMidiBtn.setButtonText("LOADING...");

            // Read file via JUCE File API
            auto inputStream = file.createInputStream();
            if (inputStream == nullptr) {
                importMidiBtn.setButtonText("OPEN FAILED");
                juce::Timer::callAfterDelay(2000, [this] { importMidiBtn.setButtonText("IMPORT MIDI"); });
                return;
            }

            juce::MidiFile midiFile;
            midiFile.readFrom(*inputStream);

            if (midiFile.getNumTracks() == 0) {
                importMidiBtn.setButtonText("NO TRACKS");
                juce::Timer::callAfterDelay(2000, [this] { importMidiBtn.setButtonText("IMPORT MIDI"); });
                return;
            }

            std::vector<PsyMelody::NoteEvent> imported;
            int ticksPerBeat = midiFile.getTimeFormat();
            if (ticksPerBeat <= 0) ticksPerBeat = 480;

            for (int track = 0; track < midiFile.getNumTracks(); ++track) {
                const auto* seq = midiFile.getTrack(track);
                if (!seq) continue;

                struct NoteStart { double tick; float velocity; };
                std::map<int, NoteStart> activeNotes;
                float currentPan = 0.0f;
                int currentPitchBend = 0;

                for (int i = 0; i < seq->getNumEvents(); ++i) {
                    const auto& evt = seq->getEventPointer(i)->message;

                    if (evt.isController() && evt.getControllerNumber() == 10)
                        currentPan = (evt.getControllerValue() - 64) / 64.0f;
                    else if (evt.isPitchWheel())
                        currentPitchBend = evt.getPitchWheelValue() - 8192;
                    else if (evt.isNoteOn() && evt.getVelocity() > 0)
                        activeNotes[evt.getNoteNumber()] = {evt.getTimeStamp(), evt.getFloatVelocity()};
                    else if (evt.isNoteOff() || (evt.isNoteOn() && evt.getVelocity() == 0)) {
                        auto it = activeNotes.find(evt.getNoteNumber());
                        if (it != activeNotes.end()) {
                            PsyMelody::NoteEvent note;
                            note.noteNumber = evt.getNoteNumber();
                            note.velocity = it->second.velocity;
                            note.pan = currentPan;
                            note.startBeat = it->second.tick / ticksPerBeat;
                            note.duration = (evt.getTimeStamp() - it->second.tick) / ticksPerBeat;
                            if (note.duration < 0.01) note.duration = 0.25;
                            note.pitchBend = currentPitchBend;
                            note.accent = false;
                            note.slide = false;
                            note.isGraceNote = false;
                            imported.push_back(note);
                            activeNotes.erase(it);
                        }
                    }
                }
            }

            if (!imported.empty()) {
                psyProcessor.updatePhrase(imported);
                double maxBeat = 0;
                for (const auto& n : imported)
                    maxBeat = std::max(maxBeat, n.startBeat + n.duration);
                int bars = std::max(1, (int)std::ceil(maxBeat / 4.0));
                psyProcessor.getGeneratorParams().phraseLengthBars = std::min(bars, 16);
                syncFromParams();
                updatePianoRoll();
                importMidiBtn.setButtonText(juce::String((int)imported.size()) + " NOTES");
                juce::Timer::callAfterDelay(2000, [this] { importMidiBtn.setButtonText("IMPORT MIDI"); });
            } else {
                importMidiBtn.setButtonText("NO NOTES");
                juce::Timer::callAfterDelay(2000, [this] { importMidiBtn.setButtonText("IMPORT MIDI"); });
            }
        });
    };
    addAndMakeVisible(importMidiBtn);

    // Zoom buttons are now owned by pianoRoll (set up in PianoRollView constructor)

    syncFromParams();
    updatePianoRoll();
    resized();  // Ensure all label fonts are applied after full setup
}

PsyMelodyEditor::~PsyMelodyEditor()
{
    setLookAndFeel(nullptr);
    hScrollBar.removeListener(this);
    vScrollBar.removeListener(this);
}

void PsyMelodyEditor::rebuildPresetList()
{
    presetSelector.clear(juce::dontSendNotification);

    // Group by category with section headings
    std::string lastCat;
    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].category != lastCat) {
            lastCat = presets[i].category;
            presetSelector.addSectionHeading(lastCat);
        }
        presetSelector.addItem(presets[i].name, (int)i + 1);
    }
}

void PsyMelodyEditor::loadPreset(int index)
{
    if (index < 0 || index >= (int)presets.size()) return;

    const auto& preset = presets[static_cast<size_t>(index)];
    auto& p = psyProcessor.getGeneratorParams();
    p = preset.params;
    pianoRoll.clearSelection();

    // Reset to Melody mode for factory presets
    psyProcessor.setGenMode(PsyMelodyProcessor::GenMode::Melody);
    genModeSelector.setSelectedId(1, juce::dontSendNotification);
    bassStyleSelector.setVisible(false);
    bassStyleLabel.setVisible(false);
    voicingStyleSelector.setVisible(false);
    voicingStyleLabel.setVisible(false);

    syncFromParams();

    if (preset.hasSequence()) {
        psyProcessor.updatePhrase(preset.sequence);
    } else {
        psyProcessor.generateNewPhrase();
    }
    updatePianoRoll();
    resized();
    repaint();
}

juce::File PsyMelodyEditor::getUserPresetDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("EDEN").getChildFile("PsyMelody").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

void PsyMelodyEditor::saveUserPreset()
{
    // Simple dialog using AlertWindow
    auto* dialog = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::AlertWindow::NoIcon);
    dialog->addTextEditor("name", "", "Name:");
    dialog->addButton("Save", 1);
    dialog->addButton("Cancel", 0);

    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog](int result) {
            if (result == 1) {
                auto name = dialog->getTextEditorContents("name").toStdString();
                if (!name.empty()) {
                    syncToParams();
                    PsyMelody::Preset newPreset;
                    newPreset.name = name;
                    newPreset.category = "User";
                    newPreset.params = psyProcessor.getGeneratorParams();
                    newPreset.sequence = psyProcessor.getCurrentPhrase();
                    presets.push_back(newPreset);
                    rebuildPresetList();
                    presetSelector.setSelectedId((int)presets.size(), juce::dontSendNotification);

                    // Save to file
                    auto file = getUserPresetDir().getChildFile(
                        juce::String(name).replaceCharacters(" /\\", "___") + ".xml");
                    juce::ValueTree state("PsyMelodyPreset");
                    const auto& p = newPreset.params;
                    state.setProperty("name", juce::String(name), nullptr);
                    state.setProperty("rootNote", p.rootNote, nullptr);
                    state.setProperty("scaleIndex", p.scaleIndex, nullptr);
                    state.setProperty("phraseLengthBars", p.phraseLengthBars, nullptr);
                    state.setProperty("baseOctave", p.baseOctave, nullptr);
                    state.setProperty("patternCategory", p.patternCategory, nullptr);
                    state.setProperty("progression", p.progression, nullptr);
                    state.setProperty("density", p.density, nullptr);
                    state.setProperty("acidAmount", p.acidAmount, nullptr);
                    state.setProperty("ornamentAmount", p.ornamentAmount, nullptr);
                    state.setProperty("rhythmVariation", p.rhythmVariation, nullptr);
                    state.setProperty("pitchRange", p.pitchRange, nullptr);

                    // Save sequence
                    juce::ValueTree seqNode("Sequence");
                    for (size_t ni = 0; ni < newPreset.sequence.size(); ++ni) {
                        const auto& note = newPreset.sequence[ni];
                        juce::ValueTree noteNode("Note");
                        noteNode.setProperty("nn", note.noteNumber, nullptr);
                        noteNode.setProperty("vel", note.velocity, nullptr);
                        noteNode.setProperty("pan", note.pan, nullptr);
                        noteNode.setProperty("start", note.startBeat, nullptr);
                        noteNode.setProperty("dur", note.duration, nullptr);
                        noteNode.setProperty("pb", note.pitchBend, nullptr);
                        noteNode.setProperty("acc", note.accent, nullptr);
                        noteNode.setProperty("sld", note.slide, nullptr);
                        noteNode.setProperty("grace", note.isGraceNote, nullptr);
                        seqNode.addChild(noteNode, -1, nullptr);
                    }
                    state.addChild(seqNode, -1, nullptr);

                    auto xml = state.createXml();
                    if (xml) xml->writeTo(file);
                }
            }
            delete dialog;
        }
    ), true);
}

void PsyMelodyEditor::setupSlider(juce::Slider& slider, juce::Label& label,
                                    const juce::String& text,
                                    double min, double max, double defaultVal, double step)
{
    slider.setRange(min, max, step); slider.setValue(defaultVal);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    slider.onValueChange = [this] { syncToParams(); };
    addAndMakeVisible(slider);
    label.setText(text, juce::dontSendNotification);
    // Labels positioned manually in resized()
    addAndMakeVisible(label);
}

void PsyMelodyEditor::syncFromParams()
{
    const auto& p = psyProcessor.getGeneratorParams();
    rootNoteSelector.setSelectedId(p.rootNote + 1, juce::dontSendNotification);
    scaleSelector.setSelectedId(p.scaleIndex + 1, juce::dontSendNotification);
    patternCategorySelector.setSelectedId(p.patternCategory + 1, juce::dontSendNotification);
    progressionSelector.setSelectedId(p.progression + 1, juce::dontSendNotification);
    subgenreSelector.setSelectedId(p.subgenre + 1, juce::dontSendNotification);
    bpmSlider.setValue(p.bpm, juce::dontSendNotification);
    phraseLengthSlider.setValue(p.phraseLengthBars, juce::dontSendNotification);
    densitySlider.setValue(p.density, juce::dontSendNotification);
    acidSlider.setValue(p.acidAmount, juce::dontSendNotification);
    ornamentSlider.setValue(p.ornamentAmount, juce::dontSendNotification);
    graceSlider.setValue(p.graceAmount, juce::dontSendNotification);
    rhythmVarSlider.setValue(p.rhythmVariation, juce::dontSendNotification);
    pitchRangeSlider.setValue(p.pitchRange, juce::dontSendNotification);
    octaveSlider.setValue(p.baseOctave, juce::dontSendNotification);
}


void PsyMelodyEditor::syncToParams()
{
    auto& p = psyProcessor.getGeneratorParams();
    p.rootNote = rootNoteSelector.getSelectedId() - 1;
    p.scaleIndex = scaleSelector.getSelectedId() - 1;
    p.patternCategory = patternCategorySelector.getSelectedId() - 1;
    p.progression = progressionSelector.getSelectedId() - 1;
    p.subgenre = subgenreSelector.getSelectedId() - 1;
    p.bpm = bpmSlider.getValue();
    p.phraseLengthBars = (int)phraseLengthSlider.getValue();
    p.density = (float)densitySlider.getValue();
    p.acidAmount = (float)acidSlider.getValue();
    p.ornamentAmount = (float)ornamentSlider.getValue();
    p.graceAmount = (float)graceSlider.getValue();
    p.rhythmVariation = (float)rhythmVarSlider.getValue();
    p.pitchRange = (float)pitchRangeSlider.getValue();
    p.baseOctave = (int)octaveSlider.getValue();
}

void PsyMelodyEditor::updatePianoRoll()
{
    pianoRoll.setNoteEvents(psyProcessor.getCurrentPhrase(),
                            psyProcessor.getGeneratorParams().phraseLengthBars);
    paramLane.setData(&psyProcessor.getCurrentPhrase(),
                      psyProcessor.getGeneratorParams().phraseLengthBars);
    paramLane.setZoomX(pianoRoll.getZoomX());
    paramLane.setScrollX(pianoRoll.getScrollX());
    // Update scrollbar ranges
    double maxH = std::max(1.0, (double)pianoRoll.getMaxScrollX() + pianoRoll.getWidth());
    hScrollBar.setRangeLimits(0.0, maxH);
    hScrollBar.setCurrentRange(pianoRoll.getScrollX(), (double)pianoRoll.getWidth(), juce::dontSendNotification);
    double maxV = std::max(1.0, (double)pianoRoll.getMaxScrollY() + pianoRoll.getHeight());
    vScrollBar.setRangeLimits(0.0, maxV);
    vScrollBar.setCurrentRange(pianoRoll.getScrollY(), (double)pianoRoll.getHeight(), juce::dontSendNotification);
}

void PsyMelodyEditor::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &hScrollBar) {
        pianoRoll.setScrollX((float)newRangeStart);
        paramLane.setScrollX((float)newRangeStart);
    } else if (bar == &vScrollBar) {
        pianoRoll.setScrollY((float)newRangeStart);
    }
}

void PsyMelodyEditor::paint(juce::Graphics& g)
{
    // Flat surface background
    g.fillAll(psyLnf.surface);

    // ---- Header bar ----
    g.setColour(psyLnf.surface);
    g.fillRect(0, 0, getWidth(), headerH);
    g.setColour(psyLnf.primary.withAlpha(0.10f));
    g.fillRect(0, headerH - 1, getWidth(), 1);

    // Title with glow (left-aligned from edge)
    g.setColour(psyLnf.primary);
    g.setFont(psyLnf.titleFont);
    g.drawText(PsyMelody::tr(currentLang, PsyMelody::Str::Title),
               14, 4, 180, 22, juce::Justification::centredLeft);
    // Version badge
    g.setColour(psyLnf.surfaceContainerHigh);
    g.fillRect(155, 8, 42, 14);
    g.setColour(psyLnf.primary.withAlpha(0.6f));
    g.setFont(psyLnf.uiFontBold.withHeight(11.0f));
    g.drawText("v0.1.0", 155, 8, 42, 14, juce::Justification::centred);
    // Subtitle
    g.setColour(psyLnf.onSurfaceVariant);
    g.setFont(psyLnf.uiFontRegular.withHeight(12.0f));
    g.drawText(PsyMelody::tr(currentLang, PsyMelody::Str::Subtitle),
               14, 27, 250, 14, juce::Justification::centredLeft);

    // ---- Sidebar (below header, above footer) ----
    g.setColour(psyLnf.surfaceContainerLow);
    g.fillRect(0, headerH, sidebarW, getHeight() - headerH - footerH);
    // Active nav indicator (left border)
    // Active nav indicator moved to paintOverChildren

    if (showSettings) return;

    // ---- Footer bar (full width) ----
    int footerY = getHeight() - footerH;
    g.setColour(psyLnf.surface);
    g.fillRect(0, footerY, getWidth(), footerH);
    g.setColour(psyLnf.primary.withAlpha(0.10f));
    g.fillRect(0, footerY, getWidth(), 1);

    // ---- Control panel border ----
    int controlY = headerH;
    int controlH = 36;
    g.setColour(psyLnf.surfaceContainerLow);
    g.fillRect(sidebarW, controlY, getWidth() - sidebarW, controlH);

    // ---- Parameter knob + right panel background ----
    // Match resized() calculations exactly:
    //   contentW = getWidth() - sidebarW - 16 (pad=8 each side)
    //   ctrlW = (contentW - 8) / 3
    //   rightPanelW = ctrlW, knobAreaW = contentW - ctrlW
    //   knobW = (knobAreaW - 15) / 6
    int bentoY = headerH + controlH + 4;
    int bentoH = 80;
    int bentoX = sidebarW + 8;
    int contentW2 = getWidth() - sidebarW - 16;
    int ctrlW2 = (contentW2 - 12) / 4;
    int rightPanelW = ctrlW2;
    int knobAreaW = contentW2 - rightPanelW;
    int cardW2 = (knobAreaW - 15) / 6;
    // Knob card backgrounds
    for (int i = 0; i < 6; ++i) {
        g.setColour(psyLnf.surfaceContainerHigh);
        g.fillRect(bentoX + i * (cardW2 + 3), bentoY, cardW2, bentoH);
    }
    // Right panel background
    int rightPanelX = bentoX + 6 * (cardW2 + 3);
    g.setColour(psyLnf.surfaceContainerHigh);
    g.fillRect(rightPanelX, bentoY, getWidth() - 8 - rightPanelX, bentoH);

}

void PsyMelodyEditor::paintOverChildren(juce::Graphics& g)
{
    // ---- Active nav indicator (left border) ----
    if (activeNavIndex < 3 && !showSettings) {
        int navBtnH2 = 56;
        int navY2 = headerH + activeNavIndex * navBtnH2;
        g.setColour(psyLnf.primary);
        g.fillRect(0, navY2, 3, navBtnH2);
    }
    if (showSettings) {
        int gearY = headerH + 56 * 3;
        g.setColour(psyLnf.primary);
        g.fillRect(0, gearY, 3, 56);
    }

    // ---- Settings nav: gear icon + SETTINGS label (same layout as MELODY etc) ----
    {
        auto b = navManualBtn.getBounds().toFloat();
        float cx = b.getCentreX();
        float iconY = b.getY() + 18.0f;
        float textY = b.getY() + 32.0f;
        auto col = showSettings ? psyLnf.primary : psyLnf.onSurfaceVariant.withAlpha(0.4f);
        g.setColour(col);

        // Gear icon (same size as other nav icons)
        float outerR = 7.0f;
        float innerR = 3.5f;
        int teeth = 8;
        float toothDepth = 2.5f;

        juce::Path gear;
        for (int i = 0; i < teeth * 2; ++i) {
            float angle = (float)i * juce::MathConstants<float>::pi / (float)teeth;
            float r = (i % 2 == 0) ? outerR : (outerR - toothDepth);
            float px = cx + std::cos(angle) * r;
            float py = iconY + std::sin(angle) * r;
            if (i == 0) gear.startNewSubPath(px, py);
            else gear.lineTo(px, py);
        }
        gear.closeSubPath();
        g.strokePath(gear, juce::PathStrokeType(1.3f));
        g.drawEllipse(cx - innerR * 0.5f, iconY - innerR * 0.5f, innerR, innerR, 1.1f);

        // Label
        g.setFont(psyLnf.uiFontBold.withHeight(11.0f));
        g.drawText("SETTINGS", (int)b.getX(), (int)textY, (int)b.getWidth(), 12,
                   juce::Justification::centred);
    }

    // ---- Save icon: down arrow + tray ----
    {
        auto sb = savePresetBtn.getBounds().toFloat();
        float cx = sb.getCentreX();
        float cy = sb.getCentreY();
        bool hover = savePresetBtn.isMouseOver();
        auto col = hover ? psyLnf.primary : psyLnf.onSurfaceVariant.withAlpha(0.5f);
        g.setColour(col);

        // Down arrow (shaft + head)
        g.fillRect(cx - 1.0f, cy - 7.0f, 2.0f, 9.0f);  // shaft
        juce::Path arrow;
        arrow.startNewSubPath(cx - 5.0f, cy + 0.0f);
        arrow.lineTo(cx, cy + 5.0f);
        arrow.lineTo(cx + 5.0f, cy + 0.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.5f));

        // Tray (U shape at bottom)
        juce::Path tray;
        tray.startNewSubPath(cx - 7.0f, cy + 2.0f);
        tray.lineTo(cx - 7.0f, cy + 7.0f);
        tray.lineTo(cx + 7.0f, cy + 7.0f);
        tray.lineTo(cx + 7.0f, cy + 2.0f);
        g.strokePath(tray, juce::PathStrokeType(1.5f));
    }

    // ---- Sidebar nav: icon + label ----
    auto drawNavItem = [&](juce::TextButton& btn, int idx, const juce::String& label) {
        auto b = btn.getBounds().toFloat();
        float cx = b.getCentreX();
        float iconY = b.getY() + 18.0f;   // icon on top
        float textY = b.getY() + 32.0f;       // label just below icon
        bool active = (activeNavIndex == idx && !showSettings);
        auto col = active ? psyLnf.primary : psyLnf.onSurfaceVariant.withAlpha(0.4f);
        g.setColour(col);

        // Icon on top
        if (idx == 0) {
            // Eighth note (♪) - notehead + stem + flag
            // Tilted notehead (ellipse)
            juce::Path head;
            head.addEllipse(-3.5f, -2.0f, 7.0f, 4.5f);
            auto headTransform = juce::AffineTransform::rotation(-0.3f).translated(cx - 1, iconY + 3);
            g.fillPath(head, headTransform);
            // Stem
            float stemX = cx + 2.0f;
            g.drawLine(stemX, iconY - 7, stemX, iconY + 2, 1.5f);
            // Flag (curved)
            juce::Path flag;
            flag.startNewSubPath(stemX, iconY - 7);
            flag.cubicTo(stemX + 5, iconY - 5, stemX + 6, iconY - 1, stemX + 2, iconY + 0);
            g.strokePath(flag, juce::PathStrokeType(1.3f));
        } else if (idx == 1) {
            // Piano keyboard - 4 white keys + 3 black keys (C D E F + C# D# F#)
            float kw = 5.0f, kh = 14.0f;
            float kx = cx - 10.0f;
            float ky = iconY - 7.0f;
            // White keys - no fill, no separators
            float bkw = 3.0f, bkh = 8.0f;
            // Outer border
            g.setColour(col);
            g.drawRect(kx, ky, kw * 4, kh, 0.5f);
            // Black keys (bright bars)
            g.fillRect(kx + kw * 1 - bkw * 0.5f, ky, bkw, bkh);
            g.fillRect(kx + kw * 2 - bkw * 0.5f, ky, bkw, bkh);
            g.fillRect(kx + kw * 3 - bkw * 0.5f, ky, bkw, bkh);
        } else if (idx == 2) {
            // layers - 3 stacked rectangles offset
            g.drawRect(cx - 7, iconY - 4, 14.0f, 3.0f, 1.0f);
            g.drawRect(cx - 5, iconY, 14.0f, 3.0f, 1.0f);
            g.drawRect(cx - 7, iconY + 4, 14.0f, 3.0f, 1.0f);
        }

        // Label below icon
        g.setFont(psyLnf.uiFontBold.withHeight(11.0f));
        g.drawText(label, (int)b.getX(), (int)textY, (int)b.getWidth(), 12,
                   juce::Justification::centred);
    };
    drawNavItem(navMelodyBtn, 0, "MELODY");
    drawNavItem(navBasslineBtn, 1, "BASSLINE");
    drawNavItem(navChordBtn, 2, "CHORD");

    // ---- Speaker icon for volume ----
    if (previewVolLabel.isVisible())
    {
        auto lb = previewVolLabel.getBounds();
        float cx = lb.getCentreX();
        float cy = lb.getCentreY();
        g.setColour(psyLnf.onSurfaceVariant.withAlpha(0.8f));
        // Speaker body
        juce::Path speaker;
        speaker.addRectangle(cx - 6.0f, cy - 3.0f, 4.0f, 6.0f);
        // Cone
        speaker.startNewSubPath(cx - 2.0f, cy - 3.0f);
        speaker.lineTo(cx + 3.0f, cy - 6.0f);
        speaker.lineTo(cx + 3.0f, cy + 6.0f);
        speaker.lineTo(cx - 2.0f, cy + 3.0f);
        speaker.closeSubPath();
        g.fillPath(speaker);
        // Sound waves
        g.setColour(psyLnf.onSurfaceVariant.withAlpha(0.55f));
        auto drawArc = [&](float radius) {
            juce::Path arc;
            arc.addCentredArc(cx + 3.0f, cy, radius, radius,
                              juce::MathConstants<float>::halfPi,
                              -juce::MathConstants<float>::pi * 0.3f,
                              juce::MathConstants<float>::pi * 0.3f, true);
            g.strokePath(arc, juce::PathStrokeType(1.2f));
        };
        drawArc(5.0f);
        drawArc(8.0f);
    }

    // ---- Undo/Redo icons (Unicode characters) ----
    {
        auto drawIcon = [&](juce::TextButton& btn, bool isUndo) {
            auto b = btn.getBounds();
            bool hover = btn.isMouseOver();
            g.setColour(hover ? psyLnf.primary : psyLnf.onSurfaceVariant.withAlpha(0.45f));
            int cx = b.getCentreX();
            int iconY = b.getY() + 4;
            auto iconRect = juce::Rectangle<int>(cx - 20, iconY, 40, 32);
            g.setFont(juce::Font("Apple Symbols", 32.0f, juce::Font::plain));
            g.drawText(isUndo ? juce::String::charToString(0x27F2)    // ⟲
                              : juce::String::charToString(0x27F3),   // ⟳
                       iconRect, juce::Justification::centred);
            auto labelRect = juce::Rectangle<int>(cx - 20, iconY + 30, 40, 12);
            g.setFont(juce::Font(9.0f));
            g.drawText(isUndo ? "UNDO" : "REDO",
                       labelRect, juce::Justification::centredTop);
        };
        drawIcon(undoBtn, true);
        drawIcon(redoBtn, false);
    }
}

void PsyMelodyEditor::resized()
{
    // ---- Preset in header (top-right) ----
    int presetW = 200;
    int saveW = 26;
    savePresetBtn.setBounds(getWidth() - 10 - saveW, 11, saveW, saveW);
    presetSelector.setBounds(getWidth() - 10 - saveW - 4 - presetW, 12, presetW, 24);
    // Preset label
    int presetLabelX = getWidth() - 10 - saveW - 4 - presetW - 54;
    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setBounds(presetLabelX, 14, 50, 20);
    presetLabel.setFont(psyLnf.uiFontBold.withHeight(10.0f));
    presetLabel.setColour(juce::Label::textColourId, psyLnf.onSurfaceVariant);
    presetLabel.setJustificationType(juce::Justification::centredRight);
    presetLabel.setVisible(true);

    // ---- Sidebar navigation ----
    int navBtnH = 56;
    int navY = headerH;
    navMelodyBtn.setBounds(0, navY, sidebarW, navBtnH); navY += navBtnH;
    navBasslineBtn.setBounds(0, navY, sidebarW, navBtnH); navY += navBtnH;
    navChordBtn.setBounds(0, navY, sidebarW, navBtnH); navY += navBtnH;
    // Settings gear right below CHORD
    navManualBtn.setBounds(0, navY, sidebarW, navBtnH);

    // Undo/Redo at sidebar bottom (stacked vertically, above footer)
    int undoRedoSize = 48;
    int undoRedoY = getHeight() - footerH - undoRedoSize * 2 - 4;
    undoBtn.setBounds(0, undoRedoY, sidebarW, undoRedoSize);
    redoBtn.setBounds(0, undoRedoY + undoRedoSize, sidebarW, undoRedoSize);
    // Make buttons fully invisible (icons drawn in paintOverChildren)
    undoBtn.setAlpha(0.0f);
    redoBtn.setAlpha(0.0f);

    // Content area (right of sidebar, below header, above footer)
    auto area = getLocalBounds();
    area.removeFromLeft(sidebarW);
    area.removeFromTop(headerH);
    area.removeFromBottom(footerH);

    if (showSettings) {
        settingsPage.setBounds(area.reduced(8, 4));
        return;
    }

    int pad = 8;
    area = area.reduced(pad, 0);

    // ---- Control strip (Root/Scale, BPM, Phrase/Octave, Pattern/Chords) ----
    auto controlStrip = area.removeFromTop(36);
    int ctrlW = (controlStrip.getWidth() - 12) / 4;

    // Root + Scale
    auto rootArea = controlStrip.removeFromLeft(ctrlW);
    rootLabel.setText("ROOT / SCALE", juce::dontSendNotification);
    rootLabel.setBounds(rootArea.removeFromTop(10));
    rootLabel.setFont(psyLnf.uiFontBold.withHeight(11.0f));
    rootLabel.setColour(juce::Label::textColourId, psyLnf.outline);
    rootLabel.setJustificationType(juce::Justification::centredLeft);
    auto rootRow = rootArea;
    rootNoteSelector.setBounds(rootRow.removeFromLeft(rootRow.getWidth() / 3).reduced(1, 0));
    scaleSelector.setBounds(rootRow.reduced(1, 0));
    scaleLabel.setVisible(false);
    controlStrip.removeFromLeft(4);

    // BPM
    auto bpmArea = controlStrip.removeFromLeft(ctrlW / 2);
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setBounds(bpmArea.removeFromTop(10));
    bpmLabel.setFont(psyLnf.uiFontBold.withHeight(11.0f));
    bpmLabel.setColour(juce::Label::textColourId, psyLnf.outline);
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    bpmSlider.setBounds(bpmArea.reduced(1, 0));
    controlStrip.removeFromLeft(4);

    // Phrase + Octave
    auto phraseArea = controlStrip.removeFromLeft(ctrlW);
    phraseLabel.setText("PHRASE / OCTAVE", juce::dontSendNotification);
    phraseLabel.setBounds(phraseArea.removeFromTop(10));
    phraseLabel.setFont(psyLnf.uiFontBold.withHeight(11.0f));
    phraseLabel.setColour(juce::Label::textColourId, psyLnf.outline);
    phraseLabel.setJustificationType(juce::Justification::centredLeft);
    auto phraseRow = phraseArea;
    phraseLengthSlider.setBounds(phraseRow.removeFromLeft(phraseRow.getWidth() / 2).reduced(1, 0));
    octaveSlider.setBounds(phraseRow.reduced(1, 0));
    octaveLabel.setVisible(false);
    controlStrip.removeFromLeft(4);

    // Pattern + Progression
    auto patArea = controlStrip;
    patternLabel.setText("PATTERN / CHORDS", juce::dontSendNotification);
    patternLabel.setBounds(patArea.removeFromTop(10));
    patternLabel.setFont(psyLnf.uiFontBold.withHeight(11.0f));
    patternLabel.setColour(juce::Label::textColourId, psyLnf.outline);
    patternLabel.setJustificationType(juce::Justification::centredLeft);
    auto patRow = patArea;
    patternCategorySelector.setBounds(patRow.removeFromLeft(patRow.getWidth() / 2).reduced(1, 0));
    progressionSelector.setBounds(patRow.reduced(1, 0));
    progressionLabel.setVisible(false);

    // (Subgenre pills moved to right panel in knob row below)

    area.removeFromTop(4);

    // ---- Parameter knobs (3 cols wide) + right panel (Subgenre + Bass/Voicing) ----
    auto bentoArea = area.removeFromTop(80);
    // Right panel = 4th column width
    auto rightPanel = bentoArea.removeFromRight(ctrlW);
    // Knobs fill the remaining 3 columns
    int knobW = (bentoArea.getWidth() - 15) / 6;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, const juce::String& name,
                          juce::Rectangle<int> cell) {
        cell = cell.reduced(4, 4);
        l.setText(name, juce::dontSendNotification);
        l.setBounds(cell.removeFromTop(12));
        l.setFont(psyLnf.uiFontBold.withHeight(11.0f));
        l.setColour(juce::Label::textColourId, psyLnf.onSurfaceVariant);
        l.setJustificationType(juce::Justification::centred);
        s.setBounds(cell);
    };

    auto k1 = bentoArea.removeFromLeft(knobW); bentoArea.removeFromLeft(3);
    auto k2 = bentoArea.removeFromLeft(knobW); bentoArea.removeFromLeft(3);
    auto k3 = bentoArea.removeFromLeft(knobW); bentoArea.removeFromLeft(3);
    auto k4 = bentoArea.removeFromLeft(knobW); bentoArea.removeFromLeft(3);
    auto k5 = bentoArea.removeFromLeft(knobW); bentoArea.removeFromLeft(3);
    auto k6 = bentoArea;

    placeKnob(densitySlider, densityLabel, "DENSITY", k1);
    placeKnob(acidSlider, acidLabel, "ACID", k2);
    placeKnob(ornamentSlider, ornamentLabel, "ORNAMENT", k3);
    placeKnob(graceSlider, graceLabel, "GRACE", k4);
    placeKnob(rhythmVarSlider, rhythmLabel, "RHYTHM", k5);
    placeKnob(pitchRangeSlider, pitchLabel, "PITCH RNG", k6);

    // Right panel: Subgenre pills + Bass Style / Voicing Style
    rightPanel = rightPanel.reduced(4, 4);
    // Subgenre pills (2x2 grid)
    auto pillArea = rightPanel.removeFromTop(38);
    int pillHalfW = pillArea.getWidth() / 2;
    auto pillRow1 = pillArea.removeFromTop(19);
    auto pillRow2 = pillArea;
    subGoaBtn.setBounds(pillRow1.removeFromLeft(pillHalfW).reduced(1, 1));
    subFullOnBtn.setBounds(pillRow1.reduced(1, 1));
    subDarkBtn.setBounds(pillRow2.removeFromLeft(pillHalfW).reduced(1, 1));
    subProgBtn.setBounds(pillRow2.reduced(1, 1));

    rightPanel.removeFromTop(4);

    // Bass Style / Voicing Style (below subgenre, when applicable)
    if (bassStyleSelector.isVisible()) {
        bassStyleLabel.setText("BASS STYLE", juce::dontSendNotification);
        bassStyleLabel.setBounds(rightPanel.removeFromTop(12));
        bassStyleLabel.setFont(psyLnf.uiFontBold.withHeight(11.0f));
        bassStyleLabel.setColour(juce::Label::textColourId, psyLnf.onSurfaceVariant);
        bassStyleLabel.setJustificationType(juce::Justification::centredLeft);
        bassStyleLabel.setVisible(true);
        bassStyleSelector.setBounds(rightPanel.removeFromTop(22).reduced(0, 1));
        voicingStyleLabel.setVisible(false);
    } else if (voicingStyleSelector.isVisible()) {
        voicingStyleLabel.setText("VOICING STYLE", juce::dontSendNotification);
        voicingStyleLabel.setBounds(rightPanel.removeFromTop(12));
        voicingStyleLabel.setFont(psyLnf.uiFontBold.withHeight(11.0f));
        voicingStyleLabel.setColour(juce::Label::textColourId, psyLnf.onSurfaceVariant);
        voicingStyleLabel.setJustificationType(juce::Justification::centredLeft);
        voicingStyleLabel.setVisible(true);
        voicingStyleSelector.setBounds(rightPanel.removeFromTop(22).reduced(0, 1));
        bassStyleLabel.setVisible(false);
    } else {
        bassStyleLabel.setVisible(false);
        voicingStyleLabel.setVisible(false);
    }

    area.removeFromTop(4);

    // ---- Piano roll (main area) ----
    // Bottom: param lane + lane tabs
    auto laneSection = area.removeFromBottom(80);
    auto laneTabs = laneSection.removeFromTop(22);
    int tabW = 80;
    laneVelBtn.setBounds(laneTabs.removeFromLeft(tabW).reduced(0, 1));
    lanePanBtn.setBounds(laneTabs.removeFromLeft(tabW).reduced(0, 1));
    lanePitchBtn.setBounds(laneTabs.removeFromLeft(tabW).reduced(0, 1));
    paramLane.setBounds(laneSection);

    // Scrollbars
    auto hScrollArea = area.removeFromBottom(6);
    hScrollArea.removeFromRight(6);
    hScrollBar.setBounds(hScrollArea);
    auto vScrollArea = area.removeFromRight(6);
    vScrollBar.setBounds(vScrollArea);

    // Piano roll
    pianoRoll.setBounds(area);

    // ---- Footer: action buttons + preview (full width including sidebar area) ----
    auto footer = getLocalBounds().removeFromBottom(footerH);
    footer = footer.reduced(4, 0);
    footer.removeFromTop(4);

    // Footer layout: all buttons uniform height, grouped by function
    int btnH = footerH - 8;
    int btnY = 2;

    // Group 1: Generate (large) + Variation
    int genW = 110;
    int varW = 110;
    generateButton.setBounds(footer.removeFromLeft(genW).reduced(1, btnY));
    variationButton.setBounds(footer.removeFromLeft(varW).reduced(1, btnY));
    footer.removeFromLeft(6);

    // Group 2: Export/Import MIDI (same width)
    int midiW = 86;
    exportMidiBtn.setBounds(footer.removeFromLeft(midiW).reduced(1, btnY));
    importMidiBtn.setBounds(footer.removeFromLeft(midiW).reduced(1, btnY));
    footer.removeFromLeft(6);

    // Undo/Redo moved to sidebar bottom

    // Right side: Quick Save + dir
    quickSaveDirBtn.setBounds(footer.removeFromRight(24).reduced(1, btnY));
    copyMidiBtn.setBounds(footer.removeFromRight(82).reduced(1, btnY));
    footer.removeFromRight(6);

    // Preview controls
    previewVolSlider.setBounds(footer.removeFromRight(120).reduced(1, btnY + 2));
    previewVolLabel.setBounds(footer.removeFromRight(22).reduced(0, btnY));
    previewWaveSelector.setBounds(footer.removeFromRight(82).reduced(1, btnY + 3));
    footer.removeFromRight(4);
    previewWaveLabel.setBounds(footer.removeFromRight(36).reduced(0, btnY));
    previewToggle.setBounds(footer.removeFromRight(100).reduced(0, btnY));

    // Zoom buttons - overlay inside piano roll (top-right corner, local coords)
    {
        int prW = pianoRoll.getWidth();
        int zw = 24, zh = 24, zPad = 6, zGap = 1;
        int totalW = zw * 5 + zGap * 2 + 8 * 2 + 11 * 2;  // buttons + group gaps + label space
        int zx = prW - totalW - zPad;
        int zy = zPad;
        zx += 11; // skip H label space
        pianoRoll.zoomOutXBtn.setBounds(zx, zy, zw, zh); zx += zw + zGap;
        pianoRoll.zoomInXBtn.setBounds(zx, zy, zw, zh);  zx += zw + 8 + 11; // gap + V label
        pianoRoll.zoomOutYBtn.setBounds(zx, zy, zw, zh); zx += zw + zGap;
        pianoRoll.zoomInYBtn.setBounds(zx, zy, zw, zh);  zx += zw + 8;
        pianoRoll.zoomFitBtn.setBounds(zx, zy, zw, zh);
    }
}

juce::AudioProcessorEditor* PsyMelodyProcessor::createEditor()
{
    return new PsyMelodyEditor(*this);
}
