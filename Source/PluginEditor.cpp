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
        g.setColour(juce::Colour(0xff555577));
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
    g.setColour(juce::Colour(0xff888899));
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
PianoRollView::PianoRollView() { setWantsKeyboardFocus(true); startTimerHz(30); }

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

double PianoRollView::beatAtX(float x) const { return (double)((x + scrollX) / beatWidth()); }
float PianoRollView::xForBeat(double beat) const { return (float)beat * beatWidth() - scrollX; }

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

    for (int n = lowestNote; n <= highestNote; ++n) {
        float y = yForNote(n);
        if (y > bounds.getHeight() || y + nh < 0) continue;
        int nio = n % 12;
        bool bk = (nio==1||nio==3||nio==6||nio==8||nio==10);
        g.setColour(bk ? juce::Colour(0xff111122) : bgColour);
        g.fillRect(0.0f, y, bounds.getWidth(), nh);
        g.setColour(gridColour); g.drawHorizontalLine((int)y, 0, bounds.getWidth());
        if (nio == 0) {
            g.setColour(juce::Colour(0xff666688)); g.setFont(juce::Font(9.0f));
            g.drawText("C" + juce::String(n/12-1), 2, (int)y, 24, (int)nh, juce::Justification::centredLeft);
        }
    }

    int tb = numBars * 4;
    for (int b = 0; b <= tb; ++b) {
        float x = xForBeat((double)b);
        if (x < -1 || x > bounds.getWidth()+1) continue;
        if (b%4==0) {
            g.setColour(barLineColour); g.drawLine(x,0,x,bounds.getHeight(),1.5f);
            g.setColour(juce::Colour(0xff666688)); g.setFont(juce::Font(9.0f));
            g.drawText(juce::String(b/4+1),(int)x+2,0,20,12,juce::Justification::centredLeft);
        } else { g.setColour(gridColour); g.drawLine(x,0,x,bounds.getHeight(),0.5f); }
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
        else if (ev.accent) c = accentColour;
        else if (ev.slide) c = slideColour;
        else c = noteColour;
        g.setColour(c.withAlpha(0.85f)); g.fillRoundedRectangle(x,y,w,h,2.0f);
        g.setColour(c.brighter(0.3f)); g.drawRoundedRectangle(x,y,w,h,2.0f, selectedNotes.count(i) ? 1.5f : 0.8f);
        g.setColour(c.brighter(ev.velocity*0.5f)); g.fillRect(x,y,std::min(3.0f,w),h);
        if (w > 8.0f) { g.setColour(c.brighter(0.5f).withAlpha(0.5f)); g.fillRect(x+w-3.0f,y,3.0f,h); }
    }

    float ly = bounds.getHeight()-14; g.setFont(juce::Font(9.0f));
    auto dl=[&](float lx,juce::Colour c,const juce::String& t){
        g.setColour(c); g.fillRect(lx,ly,8.0f,8.0f);
        g.setColour(juce::Colour(0xff888899)); g.drawText(t,(int)lx+10,(int)ly-1,50,12,juce::Justification::centredLeft);
    };
    dl(4,noteColour,"Note"); dl(54,accentColour,"Accent"); dl(114,slideColour,"Slide"); dl(164,graceColour,"Grace");
    g.setColour(juce::Colour(0xff555566));
    g.drawText("Click:add | Drag:move | Edge:resize | RClick:del | Ctrl+Wh:zoom",
               220,(int)ly-1,(int)bounds.getWidth()-224,12,juce::Justification::centredLeft);

    // Rubber band selection rectangle
    if (isRubberBanding && !selectionRect.isEmpty()) {
        g.setColour(juce::Colour(0x3300aaff));
        g.fillRect(selectionRect);
        g.setColour(juce::Colour(0xaa00aaff));
        g.drawRect(selectionRect, 1.0f);
    }

    if (processor && processor->isCurrentlyPlaying() && processor->getPhraseLengthBeats()>0) {
        float cx = xForBeat(processor->getPlaybackPositionBeats());
        if (cx>=0 && cx<=bounds.getWidth()) {
            g.setColour(cursorColour); g.drawLine(cx,0,cx,bounds.getHeight(),2.0f);
            g.setColour(cursorColour.withAlpha(0.15f)); g.fillRect(cx-4.0f,0.0f,8.0f,bounds.getHeight());
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
PsyMelodyEditor::PsyMelodyEditor(PsyMelodyProcessor& p)
    : AudioProcessorEditor(&p), psyProcessor(p)
{
    setLookAndFeel(&psyLnf);
    setSize(780, 720);

    // Settings button
    settingsBtn.setButtonText("SET");
    settingsBtn.setColour(juce::TextButton::buttonColourId, psyLnf.panelBg);
    settingsBtn.setColour(juce::TextButton::textColourOffId, psyLnf.textDim);
    settingsBtn.onClick = [this] {
        showSettings = !showSettings;
        settingsPage.setVisible(showSettings);
        // Hide/show main UI components
        auto toggle = [&](juce::Component& c) { c.setVisible(!showSettings); };
        toggle(presetSelector); toggle(savePresetBtn);
        toggle(rootNoteSelector); toggle(scaleSelector);
        toggle(patternCategorySelector); toggle(progressionSelector);
        toggle(phraseLengthSlider); toggle(densitySlider);
        toggle(acidSlider); toggle(ornamentSlider); toggle(graceSlider);
        toggle(rhythmVarSlider); toggle(pitchRangeSlider); toggle(octaveSlider);
        toggle(subgenreSelector); toggle(genModeSelector);
        toggle(previewToggle); toggle(previewWaveSelector); toggle(previewVolSlider); toggle(previewVolLabel);
        toggle(generateButton); toggle(variationButton);
        toggle(undoBtn); toggle(redoBtn);
        toggle(exportMidiBtn); toggle(importMidiBtn); toggle(copyMidiBtn); toggle(quickSaveDirBtn);
        toggle(pianoRoll); toggle(paramLane); toggle(laneTypeSelector);
        toggle(zoomInXBtn); toggle(zoomOutXBtn);
        toggle(zoomInYBtn); toggle(zoomOutYBtn); toggle(zoomFitBtn);
        toggle(hScrollBar); toggle(vScrollBar);
        // Labels
        toggle(presetLabel); toggle(rootLabel); toggle(scaleLabel);
        toggle(patternLabel); toggle(progressionLabel); toggle(phraseLabel);
        toggle(densityLabel); toggle(acidLabel); toggle(ornamentLabel);
        toggle(graceLabel); toggle(rhythmLabel); toggle(pitchLabel);
        toggle(octaveLabel); toggle(subgenreLabel); toggle(genModeLabel);
        toggle(laneLabel);

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
        repaint();
        resized();
    };
    addAndMakeVisible(settingsBtn);

    // Settings page (hidden by default, added last so it draws on top)
    settingsPage.setVisible(false);
    settingsPage.onLanguageChanged = [this](PsyMelody::Lang lang) {
        currentLang = lang;
        generateButton.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::Generate));
        variationButton.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::Variation));
        exportMidiBtn.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::ExportMidi));
        copyMidiBtn.setButtonText(PsyMelody::tr(lang, PsyMelody::Str::QuickSave));
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

    savePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff443366));
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
    };
    addAndMakeVisible(laneTypeSelector);

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
    subgenreSelector.onChange = [this] { syncToParams(); };
    addAndMakeVisible(subgenreSelector);

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
        // Show/hide bass/voicing specific controls
        bool isBass = (id == 2);
        bool isChord = (id == 3);
        bassStyleSelector.setVisible(isBass);
        bassStyleLabel.setVisible(isBass);
        voicingStyleSelector.setVisible(isChord);
        voicingStyleLabel.setVisible(isChord);
        resized();
        repaint();
    };
    addAndMakeVisible(genModeSelector);

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

    previewVolSlider.setRange(0, 1, 0.01);
    previewVolSlider.setValue(0.15);
    previewVolSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    previewVolSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    previewVolSlider.onValueChange = [this] { psyProcessor.setPreviewVolume((float)previewVolSlider.getValue()); };
    previewVolLabel.setText("Vol", juce::dontSendNotification);
    addAndMakeVisible(previewVolSlider);
    addAndMakeVisible(previewVolLabel);

    // Sliders
    setupSlider(phraseLengthSlider, phraseLabel, "Bars", 1, 16, 4, 1);
    phraseLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    phraseLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 18);
    setupSlider(densitySlider, densityLabel, "Density", 0, 1, 0.6);
    setupSlider(acidSlider, acidLabel, "Acid", 0, 1, 0.5);
    setupSlider(ornamentSlider, ornamentLabel, "Ornament", 0, 1, 0.3);
    setupSlider(graceSlider, graceLabel, "Grace", 0, 1, 0.0);
    setupSlider(rhythmVarSlider, rhythmLabel, "Rhythm Var", 0, 1, 0.4);
    setupSlider(pitchRangeSlider, pitchLabel, "Pitch Range", 0, 1, 0.5);
    setupSlider(octaveSlider, octaveLabel, "Octave", 2, 6, 4, 1);
    octaveSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    octaveSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 18);

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
    generateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00664a));
    addAndMakeVisible(generateButton);

    variationButton.onClick = [this] { syncToParams(); pianoRoll.clearSelection(); psyProcessor.generateVariation(); updatePianoRoll(); };
    variationButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334466));
    addAndMakeVisible(variationButton);

    // Undo/Redo buttons
    undoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a50));
    undoBtn.onClick = [this] {
        psyProcessor.undo();
        updatePianoRoll();
    };
    addAndMakeVisible(undoBtn);

    redoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a50));
    redoBtn.onClick = [this] {
        psyProcessor.redo();
        updatePianoRoll();
    };
    addAndMakeVisible(redoBtn);

    // Export MIDI button
    exportMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff664422));
    exportMidiBtn.onClick = [this] {
        const auto& phrase = psyProcessor.getCurrentPhrase();
        if (phrase.empty()) return;

        auto chooser = std::make_shared<juce::FileChooser>(
            "Export MIDI", juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                              .getChildFile("PsyMelody.mid"),
            "*.mid");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, chooser](const juce::FileChooser& fc) {
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
                    exportMidiBtn.setButtonText("Exported!");
                    juce::Timer::callAfterDelay(2000, [this] { exportMidiBtn.setButtonText("Export MIDI"); });
                }
            }
        });
    };
    addAndMakeVisible(exportMidiBtn);

    // Quick save MIDI
    copyMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff445566));
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
                juce::Timer::callAfterDelay(2000, [this] { copyMidiBtn.setButtonText("Quick Save"); });
            }
        };

        if (quickSaveDir.isDirectory()) {
            doSave(quickSaveDir);
        } else {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Select Quick Save Folder",
                juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
            chooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser, doSave](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.isDirectory())
                        doSave(result);
                });
        }
    };
    addAndMakeVisible(copyMidiBtn);

    // Change quick save folder button
    quickSaveDirBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334455));
    quickSaveDirBtn.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Quick Save Folder",
            quickSaveDir.isDirectory() ? quickSaveDir
                : juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.isDirectory()) {
                    quickSaveDir = result;
                    quickSaveDirBtn.setButtonText("...");
                }
            });
    };
    addAndMakeVisible(quickSaveDirBtn);

    // Import MIDI
    importMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff335544));
    importMidiBtn.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import MIDI", juce::File(), "*.mid;*.midi");
        chooser->launchAsync(juce::FileBrowserComponent::openMode, [this, chooser](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file == juce::File()) return;

            juce::FileInputStream stream(file);
            if (!stream.openedOk()) return;

            juce::MidiFile midiFile;
            if (!midiFile.readFrom(stream)) return;

            // Convert MIDI to NoteEvents
            std::vector<PsyMelody::NoteEvent> imported;
            int ticksPerBeat = midiFile.getTimeFormat();
            if (ticksPerBeat <= 0) ticksPerBeat = 480;

            for (int track = 0; track < midiFile.getNumTracks(); ++track) {
                const auto* seq = midiFile.getTrack(track);
                if (!seq) continue;

                // Build note-on/off pairs manually
                std::map<int, double> activeNotes; // noteNumber -> startTick

                for (int i = 0; i < seq->getNumEvents(); ++i) {
                    const auto& evt = seq->getEventPointer(i)->message;
                    if (evt.isNoteOn() && evt.getVelocity() > 0) {
                        activeNotes[evt.getNoteNumber()] = evt.getTimeStamp();
                    }
                    else if (evt.isNoteOff() || (evt.isNoteOn() && evt.getVelocity() == 0)) {
                        auto it = activeNotes.find(evt.getNoteNumber());
                        if (it != activeNotes.end()) {
                            PsyMelody::NoteEvent note;
                            note.noteNumber = evt.getNoteNumber();
                            note.velocity = 0.75f;
                            note.pan = 0.0f;
                            note.startBeat = it->second / ticksPerBeat;
                            note.duration = (evt.getTimeStamp() - it->second) / ticksPerBeat;
                            if (note.duration < 0.01) note.duration = 0.25;
                            note.pitchBend = 0;
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
                // Update bars to fit imported content
                double maxBeat = 0;
                for (const auto& n : imported)
                    maxBeat = std::max(maxBeat, n.startBeat + n.duration);
                int bars = std::max(1, (int)std::ceil(maxBeat / 4.0));
                psyProcessor.getGeneratorParams().phraseLengthBars = std::min(bars, 16);
                syncFromParams();
                updatePianoRoll();
            }
        });
    };
    addAndMakeVisible(importMidiBtn);

    // Zoom
    auto setupZ = [this](juce::TextButton& b) {
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a3e));
        addAndMakeVisible(b);
    };
    setupZ(zoomInXBtn); setupZ(zoomOutXBtn); setupZ(zoomInYBtn); setupZ(zoomOutYBtn); setupZ(zoomFitBtn);
    zoomInXBtn.onClick  = [this] { pianoRoll.setZoomX(pianoRoll.getZoomX()*1.3f); };
    zoomOutXBtn.onClick = [this] { pianoRoll.setZoomX(pianoRoll.getZoomX()/1.3f); };
    zoomInYBtn.onClick  = [this] { pianoRoll.setZoomY(pianoRoll.getZoomY()*1.3f); };
    zoomOutYBtn.onClick = [this] { pianoRoll.setZoomY(pianoRoll.getZoomY()/1.3f); };
    zoomFitBtn.onClick  = [this] { pianoRoll.setZoomX(1); pianoRoll.setZoomY(1); pianoRoll.setScrollX(0); };

    syncFromParams();
    updatePianoRoll();
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
    // Background gradient
    juce::ColourGradient bgGrad(psyLnf.bg, 0, 0,
                                 psyLnf.bg.darker(0.3f), 0, (float)getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillRect(getLocalBounds());

    // Header bar with gradient
    juce::ColourGradient headerGrad(juce::Colour(0xff1a1a40), 0, 0,
                                     juce::Colour(0xff0e0e28), 0, 44, false);
    g.setGradientFill(headerGrad);
    g.fillRect(0, 0, getWidth(), 48);
    // Header bottom line
    g.setColour(psyLnf.accent.withAlpha(0.3f));
    g.fillRect(0, 47, getWidth(), 1);

    // Title
    g.setColour(psyLnf.accent);
    g.setFont(psyLnf.titleFont);
    g.drawText(PsyMelody::tr(currentLang, PsyMelody::Str::Title),
               14, 4, 200, 22, juce::Justification::centredLeft);
    g.setColour(psyLnf.textDim);
    g.setFont(psyLnf.uiFontRegular.withHeight(12.0f));
    g.drawText(PsyMelody::tr(currentLang, PsyMelody::Str::Subtitle),
               14, 25, 250, 14, juce::Justification::centredLeft);

    if (showSettings) return;

    // Section headers with gradient lines
    psyLnf.drawSectionHeader(g, 14, sectionPresetY, getWidth() - 28, "PRESET");
    psyLnf.drawSectionHeader(g, 14, sectionGenY, getWidth() - 28, "GENERATOR");
    psyLnf.drawSectionHeader(g, 14, sectionExpY, getWidth() - 28, "EXPRESSION");
}

void PsyMelodyEditor::resized()
{
    // Settings button top-right
    settingsBtn.setBounds(getWidth() - 40, 8, 30, 28);

    auto area = getLocalBounds();
    area.removeFromTop(44); // header
    area = area.reduced(10, 0);

    if (showSettings) {
        settingsPage.setBounds(area.reduced(0, 4));
        return;
    }

    int rh = 22; // row height
    int g = 2;   // gap
    int labelW = 68;
    int halfW = (area.getWidth() - 10) / 2; // each column width

    // Helper: layout a row with label on left, return control bounds
    auto rowIn = [&](juce::Rectangle<int>& col, juce::Label* lbl = nullptr) {
        auto r = col.removeFromTop(rh);
        col.removeFromTop(g);
        if (lbl) {
            lbl->setBounds(r.removeFromLeft(labelW));
            lbl->setJustificationType(juce::Justification::centredRight);
            lbl->setFont(psyLnf.uiFont.withHeight(13.0f));
            lbl->setColour(juce::Label::textColourId, juce::Colour(0xff99aabb));
        } else {
            r.removeFromLeft(labelW);
        }
        return r.reduced(1, 0);
    };

    // === PRESET section ===
    sectionPresetY = area.getY() + 2;
    area.removeFromTop(16); // section header
    auto presetRow = area.removeFromTop(rh);
    presetLabel.setBounds(presetRow.removeFromLeft(labelW));
    presetLabel.setJustificationType(juce::Justification::centredRight);
    presetLabel.setFont(psyLnf.uiFont.withHeight(13.0f));
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff99aabb));
    savePresetBtn.setBounds(presetRow.removeFromRight(50).reduced(1));
    presetSelector.setBounds(presetRow.reduced(1));
    area.removeFromTop(g);

    // === GENERATOR section ===
    sectionGenY = area.getY() + 2;
    area.removeFromTop(16); // section header

    // 4 rows always shown, Bass Style/Voicing Style only if relevant
    int genRows = 4;
    auto genArea = area.removeFromTop(rh * genRows + g * (genRows - 1));
    auto leftCol = genArea.removeFromLeft(halfW);
    genArea.removeFromLeft(10); // gutter
    auto rightCol = genArea;

    // Left column: Mode, Root, Pattern, Bars
    genModeSelector.setBounds(rowIn(leftCol, &genModeLabel));
    rootNoteSelector.setBounds(rowIn(leftCol, &rootLabel));
    patternCategorySelector.setBounds(rowIn(leftCol, &patternLabel));
    phraseLengthSlider.setBounds(rowIn(leftCol, &phraseLabel));

    // Right column: Subgenre, Scale, Chords, Octave
    subgenreSelector.setBounds(rowIn(rightCol, &subgenreLabel));
    scaleSelector.setBounds(rowIn(rightCol, &scaleLabel));
    progressionSelector.setBounds(rowIn(rightCol, &progressionLabel));
    octaveSlider.setBounds(rowIn(rightCol, &octaveLabel));

    // Bass Style / Voicing Style - only take space if visible
    bool showBass = bassStyleSelector.isVisible();
    bool showVoicing = voicingStyleSelector.isVisible();
    if (showBass || showVoicing) {
        auto extraRow = area.removeFromTop(rh);
        area.removeFromTop(g);
        auto extraLeft = extraRow.removeFromLeft(halfW);
        extraRow.removeFromLeft(10);
        auto extraRight = extraRow;
        if (showBass)
            bassStyleSelector.setBounds(rowIn(extraLeft, &bassStyleLabel));
        if (showVoicing)
            voicingStyleSelector.setBounds(rowIn(extraRight, &voicingStyleLabel));
    }

    // === EXPRESSION section (knob grid) ===
    sectionExpY = area.getY() + 2;
    area.removeFromTop(16); // section header
    int knobH = 70;  // height for rotary knob + label
    int knobW = (area.getWidth() - 10) / 6;  // 6 knobs per row
    auto knobRow = area.removeFromTop(knobH);

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int>& row) {
        auto cell = row.removeFromLeft(knobW);
        l.setBounds(cell.removeFromTop(12));
        l.setJustificationType(juce::Justification::centred);
        l.setFont(psyLnf.uiFont.withHeight(12.0f));
        l.setColour(juce::Label::textColourId, juce::Colour(0xff8888aa));
        s.setBounds(cell);
    };

    placeKnob(densitySlider, densityLabel, knobRow);
    placeKnob(acidSlider, acidLabel, knobRow);
    placeKnob(ornamentSlider, ornamentLabel, knobRow);
    placeKnob(rhythmVarSlider, rhythmLabel, knobRow);
    placeKnob(pitchRangeSlider, pitchLabel, knobRow);
    placeKnob(graceSlider, graceLabel, knobRow);

    // === Action buttons ===
    area.removeFromTop(6);
    auto btnRow = area.removeFromTop(30);
    int btnW = btnRow.getWidth() / 5;
    int btnW5 = btnRow.getWidth() / 5;
    generateButton.setBounds(btnRow.removeFromLeft(btnW5).reduced(2));
    variationButton.setBounds(btnRow.removeFromLeft(btnW5).reduced(2));
    exportMidiBtn.setBounds(btnRow.removeFromLeft(btnW5).reduced(2));
    importMidiBtn.setBounds(btnRow.removeFromLeft(btnW5).reduced(2));
    quickSaveDirBtn.setBounds(btnRow.removeFromRight(24).reduced(2));
    copyMidiBtn.setBounds(btnRow.reduced(2));

    // === Zoom + Preview row ===
    area.removeFromTop(4);
    auto zRow = area.removeFromTop(22);
    int zw = 32;
    zoomFitBtn.setBounds(zRow.removeFromLeft(zw).reduced(1)); zRow.removeFromLeft(4);
    zoomOutXBtn.setBounds(zRow.removeFromLeft(zw).reduced(1));
    zoomInXBtn.setBounds(zRow.removeFromLeft(zw).reduced(1)); zRow.removeFromLeft(4);
    zoomOutYBtn.setBounds(zRow.removeFromLeft(zw).reduced(1));
    zoomInYBtn.setBounds(zRow.removeFromLeft(zw).reduced(1)); zRow.removeFromLeft(8);
    undoBtn.setBounds(zRow.removeFromLeft(40).reduced(1));
    redoBtn.setBounds(zRow.removeFromLeft(40).reduced(1));
    zRow.removeFromLeft(8);
    previewToggle.setBounds(zRow.removeFromLeft(64));
    previewWaveSelector.setBounds(zRow.removeFromLeft(95).reduced(1));
    previewVolLabel.setBounds(zRow.removeFromLeft(28));
    previewVolSlider.setBounds(zRow.removeFromLeft(60).reduced(1));

    area.removeFromTop(4);

    // === Parameter lane at bottom ===
    auto laneArea = area.removeFromBottom(70);
    auto laneHeader = laneArea.removeFromTop(18);
    laneTypeSelector.setBounds(laneHeader.removeFromLeft(130).withTrimmedLeft(36));
    paramLane.setBounds(laneArea);

    // === Scrollbars ===
    auto hScrollArea = area.removeFromBottom(7);
    hScrollArea.removeFromRight(7);
    hScrollBar.setBounds(hScrollArea);
    auto vScrollArea = area.removeFromRight(7);
    vScrollBar.setBounds(vScrollArea);

    // === Piano roll ===
    pianoRoll.setBounds(area);
}

juce::AudioProcessorEditor* PsyMelodyProcessor::createEditor()
{
    return new PsyMelodyEditor(*this);
}
