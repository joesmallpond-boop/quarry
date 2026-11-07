#include "PluginEditor.h"
#include <complex>

namespace quarry
{
using namespace juce;

// ===================================================================== palette
namespace C
{
    const Colour body    { 0xff1b1b1d };
    const Colour bodyHi  { 0xff222225 };
    const Colour key     { 0xff2a2a2d };
    const Colour keyLo   { 0xff212124 };
    const Colour text    { 0xffcfcfcf };
    const Colour dim     { 0xff7b7b80 };
    const Colour lcd     { 0xff0b0b0d };
    const Colour lcdLine { 0xff2b2b2f };
    const Colour lcdText { 0xffe9e9e6 };
}

static Font uiFont (float size, bool bold = false)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}
static Font monoFont (float size, bool bold = false)
{
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), size, bold ? Font::bold : Font::plain));
}

Colour QuarryEditor::trackColour (int t)
{
    static const Colour cols[] = { Colour (0xffff9a52), Colour (0xff4fd8c6), Colour (0xffb78cff), Colour (0xffc6e35a) };
    return cols[jlimit (0, 3, t)];
}

// ===================================================================== icons
static void drawIcon (Graphics& g, Key::Icon icon, Rectangle<float> r, Colour c)
{
    g.setColour (c);
    const float cx = r.getCentreX(), cy = r.getCentreY();
    Path p;
    switch (icon)
    {
        case Key::Wave:
        {
            const float hs[] = { 0.3f, 0.7f, 0.45f, 1.0f, 0.55f, 0.8f, 0.35f };
            for (int i = 0; i < 7; ++i)
            {
                const float x = r.getX() + i * (r.getWidth() / 6.5f), h = r.getHeight() * hs[i];
                g.fillRect (x, cy - h * 0.5f, 1.6f, h);
            }
            return;
        }
        case Key::Dots:
        {
            const float pts[][2] = { { 0.05f, 0.6f }, { 0.25f, 0.3f }, { 0.35f, 0.75f }, { 0.5f, 0.45f }, { 0.62f, 0.15f }, { 0.7f, 0.7f }, { 0.85f, 0.4f }, { 0.97f, 0.65f } };
            for (auto& d : pts) g.fillEllipse (r.getX() + d[0] * r.getWidth() - 1.4f, r.getY() + d[1] * r.getHeight() - 1.4f, 2.8f, 2.8f);
            return;
        }
        case Key::Curve:
            p.startNewSubPath (r.getX(), r.getY() + r.getHeight() * 0.35f);
            p.lineTo (r.getX() + r.getWidth() * 0.55f, r.getY() + r.getHeight() * 0.35f);
            p.quadraticTo (r.getX() + r.getWidth() * 0.7f, r.getY() - r.getHeight() * 0.1f, r.getX() + r.getWidth() * 0.78f, r.getY() + r.getHeight() * 0.4f);
            p.lineTo (r.getRight(), r.getBottom());
            break;
        case Key::Rings:
            p.addEllipse (r.getX(), r.getY() + 1, r.getHeight() - 2, r.getHeight() - 2);
            p.addEllipse (r.getX() + r.getHeight() * 0.55f, r.getY() + 1, r.getHeight() - 2, r.getHeight() - 2);
            for (int i = 0; i < 3; ++i)
            {
                const float x = r.getX() + r.getHeight() * 0.6f + i * 3.0f;
                p.startNewSubPath (x, r.getBottom() - 2); p.lineTo (x + 4, r.getY() + 2);
            }
            break;
        case Key::Cube:
        {
            const float s = r.getHeight() * 0.55f;
            const Point<float> top (cx, cy - s), l (cx - s, cy - s * 0.45f), rr (cx + s, cy - s * 0.45f), mid (cx, cy + 0.1f * s);
            p.startNewSubPath (top); p.lineTo (rr); p.lineTo (mid); p.lineTo (l); p.closeSubPath();
            p.startNewSubPath (l); p.lineTo (l.x, l.y + s); p.lineTo (cx, cy + s * 1.1f); p.lineTo (rr.x, rr.y + s); p.lineTo (rr);
            p.startNewSubPath (mid); p.lineTo (cx, cy + s * 1.1f);
            break;
        }
        case Key::Play:
            p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getX() + r.getHeight() * 0.85f, cy);
            p.addRectangle (r.getX() + r.getHeight() * 1.2f, r.getY() + 0.5f, r.getHeight() - 1, r.getHeight() - 1);
            break;
        case Key::Triangle: p.addTriangle (cx, cy - 6.5f, cx - 7.5f, cy + 6, cx + 7.5f, cy + 6); break;
        case Key::Square:   p.addRectangle (cx - 6, cy - 6, 12, 12); break;
        case Key::Hexagon:  p.addPolygon ({ cx, cy }, 6, 7.5f, MathConstants<float>::pi / 6.0f); break;
        case Key::Circle:   p.addEllipse (cx - 7, cy - 7, 14, 14); break;
        default: return;
    }
    g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ===================================================================== Key
void Key::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    // drop shadow
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0, 2.5f), 7.0f);
    ColourGradient grad (down ? C::keyLo : C::key, r.getX(), r.getY(), down ? C::keyLo.darker (0.1f) : C::keyLo, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (Colours::white.withAlpha (down ? 0.02f : 0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    const Colour ink = lit ? litColour : C::text;
    if (lit)
    {
        g.setColour (litColour.withAlpha (0.12f));
        g.fillRoundedRectangle (r.reduced (3), 5.0f);
    }
    const bool centreIcon = icon >= Triangle;
    if (icon != None)
    {
        auto ir = centreIcon ? r.withSizeKeepingCentre (18, 16).translated (0, r.getHeight() * 0.18f)
                             : Rectangle<float> (r.getX() + 12, r.getBottom() - 26, 26, 12);
        drawIcon (g, icon, ir, ink);
    }
    if (label.isNotEmpty())
    {
        g.setColour (ink);
        g.setFont (uiFont (15.5f));
        g.drawText (label, r.withTrimmedLeft (11).withTrimmedBottom (10), Justification::bottomLeft);
    }
}

// ===================================================================== Encoder
void Encoder::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (10);
    const float d = jmin (r.getWidth(), r.getHeight());
    auto k = r.withSizeKeepingCentre (d, d);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (k.translated (0, 3).expanded (1));
    ColourGradient grad (Colour (0xff2e2e31), k.getX(), k.getY(), Colour (0xff121214), k.getRight(), k.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (k);
    auto inner = k.reduced (d * 0.09f);
    ColourGradient grad2 (Colour (0xff1c1c1e), inner.getX(), inner.getY(), Colour (0xff141416), inner.getRight(), inner.getBottom(), false);
    g.setGradientFill (grad2);
    g.fillEllipse (inner);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawEllipse (k.reduced (0.5f), 1.0f);

    if (! bound) return;
    const auto rp = getRotaryParameters();
    const float prop = (float) valueToProportionOfLength (getValue());
    const float ang = rp.startAngleRadians + prop * (rp.endAngleRadians - rp.startAngleRadians);
    // value arc around the knob
    Path arc;
    arc.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.5f + 5, d * 0.5f + 5, 0, rp.startAngleRadians, ang, true);
    g.setColour (accent.withAlpha (0.85f));
    g.strokePath (arc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    // indicator dot
    const float rr = d * 0.33f;
    g.setColour (C::text);
    g.fillEllipse (k.getCentreX() + std::sin (ang) * rr - 2.2f, k.getCentreY() - std::cos (ang) * rr - 2.2f, 4.4f, 4.4f);
}

// ===================================================================== Panel
void Panel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    ColourGradient grad (C::bodyHi, 0, 0, C::body, 0, r.getHeight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine (1, 6, r.getRight() - 6);

    // top strip legends
    g.setColour (C::text);
    g.setFont (uiFont (14.5f));
    g.drawText ("master", 100, 14, 80, 20, Justification::centred);
    const char* legends[] = { "in", "out", "tempo", "midi" };
    const int lx[] = { 360, 445, 860, 960 };
    for (int i = 0; i < 4; ++i) g.drawText (legends[i], lx[i], 14, 70, 20, Justification::centred);

    // in / out LEDs and tempo / midi readouts
    const float outPk = ed.proc.masterPeak.load();
    auto led = [&g] (float x, float y, float amt, Colour c)
    {
        g.setColour (Colour (0xff0e0e10)); g.fillEllipse (x - 5, y - 5, 10, 10);
        g.setColour (c.withAlpha (jlimit (0.08f, 1.0f, amt))); g.fillEllipse (x - 3.5f, y - 3.5f, 7, 7);
    };
    float inPk = 0;
    for (int t = 0; t < kNumTracks; ++t)
        if ((int) ed.proc.tracks[(size_t) t].v (MAT_INPUT) == 1) inPk = jmax (inPk, ed.proc.trackPeak[(size_t) t].load());
    led (395, 46, inPk * 2.0f, Colour (0xff7fe07f));
    led (480, 46, outPk * 1.5f, outPk > 0.97f ? Colour (0xffff5050) : Colour (0xff7fe07f));
    g.setColour (C::dim); g.setFont (monoFont (13));
    g.drawText (String (ed.proc.bpmAtomic.load(), 1), 860, 36, 70, 20, Justification::centred);
    g.drawText (noteName (ed.proc.tracks[(size_t) ed.track].lastNote), 960, 36, 70, 20, Justification::centred);

    // speaker-style grille dots → here: power LED cluster
    g.setColour (Colour (0xff0d0d0f));
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) g.fillEllipse (233.0f + i * 9, 113.0f + j * 9, 6, 6);

    // track LEDs (clickable)
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto a = ed.trackLedArea[(size_t) t].toFloat();
        const bool sel = t == ed.track;
        const float pk = ed.proc.trackPeak[(size_t) t].load();
        g.setColour (Colour (0xff0e0e10)); g.fillEllipse (a.getCentreX() - 7, a.getY() + 2, 14, 14);
        g.setColour (QuarryEditor::trackColour (t).withAlpha (sel ? 1.0f : jlimit (0.12f, 0.8f, pk * 2.0f)));
        g.fillEllipse (a.getCentreX() - 5, a.getY() + 4, 10, 10);
        g.setColour (sel ? C::text : C::dim);
        g.setFont (uiFont (13.0f));
        g.drawText ("track " + String (t + 1), a.withTrimmedTop (20), Justification::centredTop);
    }

    // soft-key legends under the screen
    auto labels = ed.softKeyLabels();
    g.setFont (monoFont (11.0f));
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (C::dim);
        g.drawText (labels[(size_t) i].toLowerCase(), 149 + i * 98 - 4, 632, 88, 14, Justification::centred);
    }

    // vertical wordmark
    {
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, 650, 720));
        g.setColour (C::text);
        g.setFont (uiFont (30.0f, true));
        g.drawText ("quarry", 650, 690, 110, 34, Justification::centredLeft);
    }
}

void Panel::mouseDown (const MouseEvent& e)
{
    for (int t = 0; t < kNumTracks; ++t)
        if (ed.trackLedArea[(size_t) t].contains (e.getPosition())) ed.selectTrack (t);
}

// ===================================================================== Screen
Rectangle<float> Screen::cellArea (int i) const
{
    const float x0 = 40, w = (getWidth() - 48.0f) / 4.0f, h = 34;
    const float y0 = getHeight() - 8.0f - 2 * h - 4;
    return { x0 + (i % 4) * w, y0 + (i / 4) * (h + 4), w - 6, h };
}
Rectangle<float> Screen::vizArea() const { return { 40, 32, getWidth() - 50.0f, cellArea (0).getY() - 38 }; }

void Screen::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff050506)); g.fillRoundedRectangle (r, 4);
    g.setColour (C::lcd); g.fillRoundedRectangle (r.reduced (3), 3);

    // vertical brand strip
    {
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, 0, 0).translated (0, r.getHeight()));
        g.setColour (C::lcdText.withAlpha (0.85f));
        g.setFont (uiFont (21.0f));
        g.drawText ("quarry", 14, 6, 70, 26, Justification::centredLeft);
        g.setFont (uiFont (13.0f));
        g.setColour (C::lcdText.withAlpha (0.55f));
        g.drawText ("granular workstation", 84, 9, 170, 22, Justification::centredLeft);
    }

    drawHeader (g, { 40, 6, r.getWidth() - 50, 22 });
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (vizArea().toNearestInt());
        drawViz (g, vizArea());
    }
    drawCells (g);

    // flash message
    if (Time::getMillisecondCounterHiRes() < ed.flashUntil && ed.flash.isNotEmpty())
    {
        auto v = vizArea();
        auto b = v.withSizeKeepingCentre (jmin (v.getWidth() - 20, 360.0f), 30);
        g.setColour (Colours::black.withAlpha (0.85f)); g.fillRoundedRectangle (b, 4);
        g.setColour (ed.accent()); g.drawRoundedRectangle (b, 4, 1);
        g.setColour (C::lcdText); g.setFont (monoFont (12.5f, true));
        g.drawText (ed.flash, b, Justification::centred);
    }
}

void Screen::drawHeader (Graphics& g, Rectangle<float> r)
{
    const auto acc = ed.accent();
    drawIcon (g, Key::Wave, { r.getX(), r.getCentreY() - 5, 18, 10 }, acc);
    g.setColour (C::lcdText); g.setFont (monoFont (13.5f));
    String left = "TRACK " + String (ed.track + 1) + "  " + ed.viewName();
    if (ed.view == View::Material || ed.view == View::Project)
        left << " / " << ed.tp (MAT_MODE)->getCurrentValueAsText().toUpperCase();
    if (ed.ctrl) left << "  [CTRL]";
    if (ed.temp) left << "  [TEMP]";
    g.drawText (left, r.withTrimmedLeft (26), Justification::centredLeft);
    // tempo with metronome glyph
    const String bpm = String (ed.proc.bpmAtomic.load(), 1);
    g.drawText (bpm, r.removeFromRight (54), Justification::centredRight);
    Path m; const float x = r.getRight() - 6, y = r.getCentreY();
    m.addTriangle (x - 5, y + 6, x + 5, y + 6, x, y - 6);
    m.startNewSubPath (x, y + 2); m.lineTo (x + 4, y - 5);
    const bool playing = ed.gp (GID::play)->getValue() > 0.5f;
    g.setColour (playing ? acc : C::lcdText.withAlpha (0.5f));
    g.strokePath (m, PathStrokeType (1.3f));
}

void Screen::drawCells (Graphics& g)
{
    if (ed.pages.empty()) return;
    const auto& pg = ed.pages[(size_t) jlimit (0, (int) ed.pages.size() - 1, ed.page)];
    const bool trackish = ed.view != View::Mix && ed.view != View::Perform && ed.view != View::Scene;
    for (int i = 0; i < 8; ++i)
    {
        auto c = cellArea (i);
        g.setColour (Colour (0xff141416)); g.fillRect (c);
        auto* p = pg.p[(size_t) i];
        if (p == nullptr) continue;
        const bool hot = ed.lastTouchedCell == i && Time::getMillisecondCounterHiRes() - ed.lastTouchTime < 1500;
        g.setColour (hot ? ed.accent() : C::lcdText.withAlpha (0.9f));
        g.setFont (monoFont (10.5f));
        auto top = c.withHeight (16).reduced (5, 0);
        g.drawText (pg.label[(size_t) i].toUpperCase(), top, Justification::centredLeft);
        g.setColour (hot ? C::lcdText : C::lcdText.withAlpha (0.55f));
        g.drawText (p->getCurrentValueAsText(), top, Justification::centredRight);

        auto bar = Rectangle<float> (c.getX() + 5, c.getY() + 19, c.getWidth() - 10, 8);
        g.setColour (C::lcdLine); g.fillRect (bar);
        const float v = p->getValue();
        const auto range = p->getNormalisableRange();
        const bool bip = range.start < 0 && range.end > 0;
        g.setColour (C::lcdText.withAlpha (0.92f));
        if (dynamic_cast<AudioParameterBool*> (p) != nullptr) { if (v > 0.5f) g.fillRect (bar); }
        else if (bip)
        {
            const float zero = range.convertTo0to1 (0.0f);
            const float a = jmin (zero, v), b = jmax (zero, v);
            g.fillRect (bar.getX() + a * bar.getWidth(), bar.getY(), jmax (2.0f, (b - a) * bar.getWidth()), bar.getHeight());
        }
        else g.fillRect (bar.withWidth (jmax (2.0f, v * bar.getWidth())));

        // modulation indicator
        if (trackish)
        {
            const String id = p->getParameterID();
            const String pre = "t" + String (ed.track + 1) + "_";
            if (id.startsWith (pre))
            {
                const auto sid = id.substring (pre.length());
                for (int k = 0; k < TP_COUNT; ++k)
                    if (trackSpecs()[(size_t) k].id == sid && trackSpecs()[(size_t) k].modulatable)
                    {
                        const float e = ed.proc.tracks[(size_t) ed.track].effectiveNorm (k);
                        if (std::abs (e - v) > 0.002f)
                        {
                            g.setColour (ed.accent());
                            g.fillRect (bar.getX() + e * bar.getWidth() - 1.5f, bar.getY() - 3, 3.0f, bar.getHeight() + 6);
                        }
                    }
            }
        }
    }
}

void Screen::drawViz (Graphics& g, Rectangle<float> r)
{
    switch (ed.view)
    {
        case View::Material:    drawMaterial (g, r); break;
        case View::Granular:    drawGranular (g, r); break;
        case View::Filter:      drawFilter (g, r); break;
        case View::Color:       drawColor (g, r); break;
        case View::Space:       drawSpace (g, r); break;
        case View::Mod:         drawMod (g, r); break;
        case View::Mix:         drawMix (g, r); break;
        case View::Perform:     drawPerform (g, r); break;
        case View::Scene:       drawScene (g, r); break;
        case View::Config:      drawConfig (g, r); break;
        case View::Project:     drawProject (g, r); break;
        case View::TrackSelect: drawTrackSelect (g, r); break;
    }
}

void Screen::drawWave (Graphics& g, Rectangle<float> r, const float* peaks, int n, int validBins)
{
    const int bins = validBins > 0 ? jmin (validBins, n) : n;
    float mx = 1.0e-4f;
    for (int b = 0; b < bins; ++b) mx = jmax (mx, peaks[b]);
    const float cy = r.getCentreY(), h = r.getHeight() * 0.47f;
    const int W = (int) r.getWidth();
    g.setGradientFill (ColourGradient (C::lcdText, 0, cy - h, C::lcdText.withAlpha (0.25f), 0, cy, false));
    for (int x = 0; x < W; ++x)
    {
        const int b0 = x * bins / W, b1 = jmax (b0 + 1, (x + 1) * bins / W);
        float m = 0;
        for (int b = b0; b < b1 && b < n; ++b) m = jmax (m, peaks[b]);
        const float hh = std::pow (m / mx, 0.8f) * h;
        g.fillRect (r.getX() + x, cy - hh, 1.0f, hh);
    }
    g.setGradientFill (ColourGradient (C::lcdText.withAlpha (0.25f), 0, cy, C::lcdText.withAlpha (0.8f), 0, cy + h, false));
    for (int x = 0; x < W; ++x)
    {
        const int b0 = x * bins / W, b1 = jmax (b0 + 1, (x + 1) * bins / W);
        float m = 0;
        for (int b = b0; b < b1 && b < n; ++b) m = jmax (m, peaks[b]);
        const float hh = std::pow (m / mx, 0.8f) * h;
        g.fillRect (r.getX() + x, cy, 1.0f, hh + 1.0f);
    }
}

void Screen::drawMaterial (Graphics& g, Rectangle<float> r)
{
    auto& tr = ed.proc.tracks[(size_t) ed.track];
    const int mode = (int) tr.v (MAT_MODE);
    const float start = tr.v (MAT_START), len = jmin (tr.v (MAT_LENGTH), 1.0f - start * 0.999f);
    float play = tr.material.playNorm.load();
    bool empty = false;
    if (mode == 0)
    {
        const int tl = tr.material.tapeLenAtomic.load(), tmax = jmax (1, tr.material.getTapeMax());
        const bool rec = tr.material.tapeRecAtomic.load();
        if (tl <= 0) empty = true;
        else
        {
            float pk[dsp::Material::kPeakBins];
            for (int i = 0; i < dsp::Material::kPeakBins; ++i) pk[i] = tr.material.tapePeaks.v[(size_t) i].load();
            const int valid = jmax (1, (int) ((int64) tl * dsp::Material::kPeakBins / tmax));
            drawWave (g, r, pk, dsp::Material::kPeakBins, valid);
            if (rec && tr.v (MAT_REC) > 0.5f && tr.material.tapeLenAtomic.load() > 0) {}
            play = play >= 0 ? play * tmax / (float) tl : play;
            if (rec) play = -1;
        }
        g.setColour (C::lcdText.withAlpha (0.6f)); g.setFont (monoFont (10.5f));
        g.drawText (String (tl / jmax (1.0, (double) ed.getAudioProcessor()->getSampleRate()), 2) + " s", r.reduced (4), Justification::bottomRight);
        if (tr.v (MAT_REC) > 0.5f)
        {
            g.setColour (Colour (0xffff4a4a)); g.fillEllipse (r.getRight() - 16, r.getY() + 4, 9, 9);
            g.setFont (monoFont (10.5f, true)); g.drawText (tl == 0 || rec ? "REC" : "DUB", r.withTrimmedRight (20).withHeight (17), Justification::centredRight);
        }
        if (empty)
        {
            g.setColour (C::lcdText.withAlpha (0.6f)); g.setFont (monoFont (12));
            g.drawText (tr.v (MAT_REC) > 0.5f ? "RECORDING INPUT..." : "TAPE EMPTY  -  ctrl+play arms REC", r, Justification::centred);
            return;
        }
    }
    else
    {
        auto s = ed.proc.getSample (ed.track);
        if (s == nullptr)
        {
            g.setColour (C::lcdText.withAlpha (0.6f)); g.setFont (monoFont (12));
            g.drawText ("DROP AUDIO HERE  -  or project > load", r, Justification::centred);
            return;
        }
        drawWave (g, r, s->peaks.data(), (int) s->peaks.size());
        g.setColour (C::lcdText.withAlpha (0.6f)); g.setFont (monoFont (10.5f));
        g.drawText (s->name.substring (0, 40), r.reduced (4), Justification::bottomLeft);
    }
    // region shading
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRect (r.withWidth (start * r.getWidth()));
    g.fillRect (r.withTrimmedLeft ((start + len) * r.getWidth()));
    g.setColour (ed.accent().withAlpha (0.7f));
    g.drawVerticalLine ((int) (r.getX() + start * r.getWidth()), r.getY(), r.getBottom());
    g.drawVerticalLine ((int) (r.getX() + (start + len) * r.getWidth()) - 1, r.getY(), r.getBottom());

    if (mode == 2)
    {
        for (auto& v : tr.material.voiceNorm)
        {
            const float p = v.load();
            if (p >= 0) { g.setColour (ed.accent()); g.drawVerticalLine ((int) (r.getX() + p * r.getWidth()), r.getY(), r.getBottom()); }
        }
    }
    else if (play >= 0 && play <= 1)
    {
        g.setColour (Colours::white);
        g.fillRect (r.getX() + play * r.getWidth() - 1, r.getY(), 2.5f, r.getHeight());
    }
}

void Screen::drawGranular (Graphics& g, Rectangle<float> r)
{
    auto& tr = ed.proc.tracks[(size_t) ed.track];
    float pk[dsp::Granular::kPeakBins];
    for (int i = 0; i < dsp::Granular::kPeakBins; ++i) pk[i] = tr.granular.peaks.v[(size_t) i].load();
    {
        Graphics::ScopedSaveState s (g);
        g.setOpacity (0.45f);
        drawWave (g, r, pk, dsp::Granular::kPeakBins);
    }
    const float w = tr.granular.writeNorm.load();
    const bool frozen = tr.granular.frozenAtomic.load();
    g.setColour (frozen ? Colour (0xff8fd3ff) : C::lcdText);
    g.fillRect (r.getX() + w * r.getWidth() - 1, r.getY(), 2.0f, r.getHeight());

    int active = 0;
    for (int i = 0; i < dsp::Granular::kMaxGrains; ++i)
    {
        const float p = tr.granular.grainPos[(size_t) i].load();
        if (p < 0) continue;
        ++active;
        const float y = r.getY() + 8 + std::fmod (i * 0.618034f, 1.0f) * (r.getHeight() - 16);
        g.setColour (ed.accent());
        g.fillEllipse (r.getX() + p * r.getWidth() - 3, y - 3, 6, 6);
    }
    g.setColour (C::lcdText.withAlpha (0.7f)); g.setFont (monoFont (10.5f));
    g.drawText (String (active) + " grains" + (frozen ? "   FROZEN" : "") + (tr.v (GR_MIX) < 0.01f ? "   (mix at 0)" : ""),
                r.reduced (4), Justification::bottomLeft);
    g.drawText ("buffer 4.0 s", r.reduced (4), Justification::bottomRight);
}

void Screen::drawFilter (Graphics& g, Rectangle<float> r)
{
    auto& tr = ed.proc.tracks[(size_t) ed.track];
    const float fc = tr.v (FL_CUTOFF), reso = tr.v (FL_RESO), type = tr.v (FL_TYPE), morph = tr.v (FL_MORPH), mix = tr.v (FL_MIX);
    const float k = 1.0f / (0.5f + reso * reso * 24.0f);
    const float lpA = jlimit (0.0f, 1.0f, 1.0f - type), bpA = 1.0f - std::abs (type - 1.0f), hpA = jlimit (0.0f, 1.0f, type - 1.0f);
    float rootNote = tr.v (FL_RPITCH);
    if (tr.v (FL_KEY) > 0.5f) rootNote = (float) tr.lastNote + (rootNote - 48.0f);
    const float root = 440.0f * std::exp2 ((rootNote - 69.0f) / 12.0f);
    const float expo = std::exp2 ((tr.v (FL_RSPREAD) - 0.5f) * 2.0f);
    const float Q = 4.0f * std::pow (100.0f, tr.v (FL_RDECAY));
    const int bands = (int) tr.v (FL_RBANDS);
    const float tilt = tr.v (FL_RTILT);

    // grid
    g.setColour (C::lcdLine);
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) (r.getX() + std::log (f / 20.0f) / std::log (1000.0f) * r.getWidth()), r.getY(), r.getBottom());
    auto yOf = [&] (float db) { return r.getY() + jmap (jlimit (-36.0f, 24.0f, db), 24.0f, -36.0f, 0.0f, r.getHeight()); };
    g.drawHorizontalLine ((int) yOf (0), r.getX(), r.getRight());

    if (morph > 0.001f)
    {
        g.setColour (ed.accent().withAlpha (0.25f));
        for (int b = 0; b < bands; ++b)
        {
            const float f = root * std::pow ((float) (b + 1), expo);
            if (f > 20000) break;
            g.drawVerticalLine ((int) (r.getX() + std::log (f / 20.0f) / std::log (1000.0f) * r.getWidth()), r.getY(), r.getBottom());
        }
    }

    Path p;
    for (int x = 0; x <= (int) r.getWidth(); x += 2)
    {
        const float f = 20.0f * std::pow (1000.0f, x / r.getWidth());
        const float w = f / fc;
        const std::complex<float> D (1.0f - w * w, w * k);
        const std::complex<float> H = (lpA * 1.0f + bpA * std::complex<float> (0, w) + hpA * (-w * w)) / D;
        float mag = std::abs (H);
        if (morph > 0.001f)
        {
            float res = 0;
            for (int b = 0; b < bands; ++b)
            {
                const float fb = root * std::pow ((float) (b + 1), expo);
                if (fb > 21000) break;
                const float d = f / fb - fb / f;
                res += std::pow ((float) (b + 1), tilt) / std::sqrt (1.0f + Q * Q * d * d);
            }
            mag = mag * (1 - morph) + jmin (4.0f, res * 1.2f) * morph;
        }
        mag = (1 - mix) + mix * mag;
        const float y = yOf (20.0f * std::log10 (jmax (1.0e-4f, mag)));
        if (x == 0) p.startNewSubPath (r.getX() + x, y); else p.lineTo (r.getX() + x, y);
    }
    Path fill (p); fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
    g.setGradientFill (ColourGradient (C::lcdText.withAlpha (0.35f), 0, r.getY(), C::lcdText.withAlpha (0.0f), 0, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (C::lcdText);
    g.strokePath (p, PathStrokeType (1.8f));
    g.setColour (C::lcdText.withAlpha (0.6f)); g.setFont (monoFont (10.5f));
    g.drawText (morph < 0.01f ? "STATE VARIABLE" : morph > 0.99f ? "RESONATOR x" + String (bands) : "MORPH " + String (roundToInt (morph * 100)) + "%",
                r.reduced (4), Justification::topRight);
}

void Screen::drawColor (Graphics& g, Rectangle<float> r)
{
    auto& tr = ed.proc.tracks[(size_t) ed.track];
    const float drive = tr.v (CO_DRIVE), bits = tr.v (CO_BITS);
    const int type = (int) tr.v (CO_TYPE);
    const float gn = 1.0f + drive * drive * 40.0f, mk = 1.0f / std::pow (gn, 0.7f), q = std::exp2 (bits - 1.0f);
    auto sq = r.withSizeKeepingCentre (r.getHeight(), r.getHeight()).reduced (4);
    g.setColour (C::lcdLine);
    g.drawRect (sq);
    g.drawLine (sq.getX(), sq.getCentreY(), sq.getRight(), sq.getCentreY());
    g.drawLine (sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom());
    Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = -1.0f + i / 100.0f;
        float y = drive > 0.001f ? dsp::shapeSample (x * gn, type) * mk : x;
        if (bits < 15.95f) y = std::round (y * q) / q;
        y = jlimit (-1.2f, 1.2f, y);
        const float px = sq.getX() + (x + 1) * 0.5f * sq.getWidth(), py = sq.getCentreY() - y * 0.5f * sq.getHeight() / 1.2f * 1.2f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (C::lcdText); g.strokePath (p, PathStrokeType (2.0f));

    // side readouts
    g.setFont (monoFont (11)); g.setColour (C::lcdText.withAlpha (0.75f));
    auto left = r.withRight (sq.getX() - 8).reduced (4);
    g.drawFittedText ("SHAPER\n" + tr.params[CO_TYPE]->getCurrentValueAsText().toUpperCase() + "\n\nDRIVE\n" + String (roundToInt (drive * 100)) + "%",
                      left.toNearestInt(), Justification::topLeft, 8);
    auto right = r.withLeft (sq.getRight() + 8).reduced (4);
    g.drawFittedText ("BITS " + String (bits, 1) + "\nRATE /" + String (tr.v (CO_DOWN), 1) + "\nGLUE " + String (roundToInt (tr.v (CO_COMP) * 100)) + "%"
                      + "\nNOISE " + String (roundToInt (tr.v (CO_NOISE) * 100)) + "%",
                      right.toNearestInt(), Justification::topLeft, 8);
}

void Screen::drawSpace (Graphics& g, Rectangle<float> r)
{
    auto& tr = ed.proc.tracks[(size_t) ed.track];
    const bool freeze = tr.v (SP_FREEZE) > 0.5f || ed.gp (GID::punch (3))->getValue() > 0.5f;
    static const float divs[] = { 0, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f };
    const int sync = (int) tr.v (SP_DSYNC);
    const float T = sync == 0 ? tr.v (SP_DTIME) : 60.0f / jmax (20.0f, ed.proc.bpmAtomic.load()) * divs[sync];
    const float fb = freeze ? 1.0f : jmin (0.97f, tr.v (SP_DFB) * 1.05f);
    const float span = 4.0f;
    auto top = r.withHeight (r.getHeight() * 0.55f).reduced (0, 4);
    auto bot = r.withTrimmedTop (r.getHeight() * 0.58f);
    // delay taps
    float a = 1.0f;
    for (int n = 1; n < 64; ++n)
    {
        const float t = T * n; if (t > span) break;
        const float x = top.getX() + t / span * top.getWidth();
        const float h = a * top.getHeight() * tr.v (SP_DLEVEL);
        g.setColour (C::lcdText.withAlpha (0.4f + 0.6f * a));
        g.fillRect (x - 1.5f, top.getBottom() - h, 3.0f, h);
        a *= fb; if (a < 0.01f) break;
    }
    // reverb decay envelope
    const float rt = freeze ? 1e9f : 0.3f + std::pow (tr.v (SP_RDECAY), 2.0f) * 18.0f;
    Path p; p.startNewSubPath (bot.getX(), bot.getBottom());
    for (int x = 0; x <= (int) bot.getWidth(); x += 3)
    {
        const float t = x / bot.getWidth() * span;
        const float env = std::pow (10.0f, -3.0f * t / rt) * jmin (1.0f, t / 0.05f) * tr.v (SP_RLEVEL);
        p.lineTo (bot.getX() + x, bot.getBottom() - env * bot.getHeight() * 0.9f);
    }
    p.lineTo (bot.getRight(), bot.getBottom()); p.closeSubPath();
    g.setGradientFill (ColourGradient (ed.accent().withAlpha (0.55f), bot.getX(), 0, ed.accent().withAlpha (0.05f), bot.getRight(), 0, false));
    g.fillPath (p);
    g.setColour (C::lcdText.withAlpha (0.6f)); g.setFont (monoFont (10.5f));
    g.drawText ("DELAY " + (sync ? tr.params[SP_DSYNC]->getCurrentValueAsText() : String (roundToInt (T * 1000)) + " ms"), top.reduced (4), Justification::topLeft);
    g.drawText ("REVERB " + (freeze ? String ("FROZEN") : String (rt, 1) + " s"), bot.reduced (4), Justification::topLeft);
    g.drawText ("4 s", r.reduced (2), Justification::bottomRight);
}

void Screen::drawMod (Graphics& g, Rectangle<float> r)
{
    auto& tr = ed.proc.tracks[(size_t) ed.track];
    const float laneH = r.getHeight() / 4.0f;
    for (int k = 0; k < kNumMods; ++k)
    {
        auto lane = Rectangle<float> (r.getX(), r.getY() + k * laneH, r.getWidth(), laneH).reduced (0, 2);
        const bool sel = k == ed.page;
        g.setColour (sel ? Colour (0xff17171a) : Colour (0xff0f0f11)); g.fillRect (lane);
        const int b = M1_TYPE + k * 5;
        const float depth = tr.baseValue (b + 3);
        Path p;
        const auto& h = modHist[(size_t) k];
        for (int i = 0; i < 120; ++i)
        {
            const float v = h[(size_t) ((modHistPos + i) % 120)];
            const float x = lane.getX() + 110 + i / 119.0f * (lane.getWidth() - 115);
            const float y = lane.getCentreY() - v * lane.getHeight() * 0.42f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (sel ? ed.accent() : C::lcdText.withAlpha (std::abs (depth) > 0.001f ? 0.8f : 0.3f));
        g.strokePath (p, PathStrokeType (1.4f));
        g.setFont (monoFont (10.5f, sel));
        g.setColour (sel ? C::lcdText : C::lcdText.withAlpha (0.6f));
        g.drawText (String (k + 1) + " " + tr.params[(size_t) b]->getCurrentValueAsText().toUpperCase(), lane.withWidth (105).reduced (4, 0), Justification::topLeft);
        g.setColour (C::lcdText.withAlpha (0.5f));
        g.drawText ("> " + tr.params[(size_t) b + 4]->getCurrentValueAsText(), lane.withWidth (105).reduced (4, 0), Justification::bottomLeft);
    }
}

void Screen::drawMix (Graphics& g, Rectangle<float> r)
{
    const float sw = r.getWidth() / 5.0f;
    for (int t = 0; t <= kNumTracks; ++t)
    {
        auto s = Rectangle<float> (r.getX() + t * sw, r.getY(), sw - 8, r.getHeight());
        const bool isM = t == kNumTracks;
        const float pk = isM ? ed.proc.masterPeak.load() : ed.proc.trackPeak[(size_t) t].load();
        const Colour col = isM ? C::lcdText : QuarryEditor::trackColour (t);
        g.setColour (t == ed.track ? col : C::lcdText.withAlpha (0.7f)); g.setFont (monoFont (11, true));
        g.drawText (isM ? "MST" : "T" + String (t + 1), s.removeFromTop (14), Justification::centredLeft);
        auto meter = s.removeFromLeft (10).withTrimmedBottom (14);
        g.setColour (C::lcdLine); g.fillRect (meter);
        const float db = Decibels::gainToDecibels (pk, -60.0f);
        const float h = jmap (db, -60.0f, 0.0f, 0.0f, meter.getHeight());
        g.setColour (pk > 0.97f ? Colour (0xffff5050) : col);
        g.fillRect (meter.withTop (meter.getBottom() - h));
        g.setFont (monoFont (10)); g.setColour (C::lcdText.withAlpha (0.75f));
        auto info = s.withTrimmedLeft (5);
        if (! isM)
        {
            auto& tr = ed.proc.tracks[(size_t) t];
            String txt = tr.params[MX_LEVEL]->getCurrentValueAsText() + "\n" + tr.params[MX_PAN]->getCurrentValueAsText();
            if (tr.v (MX_MUTE) > 0.5f) txt << "\nMUTE";
            if (tr.v (MX_SOLO) > 0.5f) txt << "\nSOLO";
            if (tr.v (MX_MASTER) < 0.5f) txt << "\nNO MST";
            txt << "\nin:" << tr.params[MAT_INPUT]->getCurrentValueAsText().replace ("Track ", "T").replace ("Ext In", "EXT");
            g.drawFittedText (txt, info.toNearestInt(), Justification::topLeft, 6);
        }
        else
            g.drawFittedText (ed.gp (GID::out)->getCurrentValueAsText() + "\nGLUE\n" + ed.gp (GID::comp)->getCurrentValueAsText()
                              + "\nDJ " + ed.gp (GID::dj)->getCurrentValueAsText(), info.toNearestInt(), Justification::topLeft, 6);
    }
}

void Screen::drawPerform (Graphics& g, Rectangle<float> r)
{
    const float w = r.getWidth() / 8.0f;
    auto punchRow = r.removeFromBottom (24);
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto c = Rectangle<float> (r.getX() + m * w, r.getY(), w - 6, r.getHeight() - 4);
        auto* p = ed.gp (GID::macro (m));
        const float v = p->getValue();
        g.setColour (C::lcdLine); g.fillRect (c.withTrimmedTop (14).withTrimmedBottom (26));
        auto bar = c.withTrimmedTop (14).withTrimmedBottom (26);
        g.setColour (ed.page == 0 ? C::lcdText : C::lcdText.withAlpha (0.6f));
        g.fillRect (bar.withTop (bar.getBottom() - v * bar.getHeight()));
        g.setFont (monoFont (10, true)); g.setColour (C::lcdText);
        g.drawText ("M" + String (m + 1), c.withHeight (13), Justification::centred);
        g.setFont (monoFont (8.5f)); g.setColour (C::lcdText.withAlpha (0.6f));
        auto ta = ed.gp (GID::macroTgt (m, 0))->getCurrentValueAsText(), tb = ed.gp (GID::macroTgt (m, 1))->getCurrentValueAsText();
        g.drawFittedText ((ta == "None" ? "-" : ta) + "\n" + (tb == "None" ? "-" : tb), c.withTop (c.getBottom() - 24).toNearestInt(), Justification::centred, 2, 0.7f);
    }
    for (int i = 0; i < 4; ++i)
    {
        auto c = Rectangle<float> (punchRow.getX() + i * punchRow.getWidth() / 4, punchRow.getY() + 2, punchRow.getWidth() / 4 - 6, punchRow.getHeight() - 4);
        const bool on = ed.gp (GID::punch (i))->getValue() > 0.5f;
        g.setColour (on ? ed.accent() : C::lcdLine); g.fillRect (c);
        g.setColour (on ? Colours::black : C::lcdText.withAlpha (0.7f)); g.setFont (monoFont (10, true));
        g.drawText (punchNames()[i], c, Justification::centred);
    }
}

int Screen::sceneSlotAt (Point<float> pt) const
{
    auto r = vizArea();
    if (ed.view != View::Scene || ! r.contains (pt)) return -1;
    auto grid = r.withTrimmedTop (16);
    const int col = (int) ((pt.x - grid.getX()) / (grid.getWidth() / 8)), row = (int) ((pt.y - grid.getY()) / (grid.getHeight() / 2));
    if (col < 0 || col > 7 || row < 0 || row > 1) return -1;
    return ed.sceneBank * 16 + row * 8 + col;
}

void Screen::drawScene (Graphics& g, Rectangle<float> r)
{
    g.setFont (monoFont (11)); g.setColour (C::lcdText.withAlpha (0.75f));
    String mode = ed.sceneStore ? "STORE: tap a slot" : ed.sceneDelete ? "DELETE: tap a slot" : "RECALL: tap a slot (ctrl = store)";
    g.drawText ("BANK " + String (ed.sceneBank + 1) + "/8   " + mode, r.withHeight (14), Justification::centredLeft);
    auto grid = r.withTrimmedTop (16);
    const float cw = grid.getWidth() / 8, ch = grid.getHeight() / 2;
    for (int i = 0; i < 16; ++i)
    {
        const int slot = ed.sceneBank * 16 + i;
        auto c = Rectangle<float> (grid.getX() + (i % 8) * cw, grid.getY() + (i / 8) * ch, cw - 5, ch - 5);
        const bool exists = ed.proc.sceneExists (slot), cur = ed.proc.currentScene() == slot;
        g.setColour (cur ? ed.accent() : exists ? C::lcdText.withAlpha (0.85f) : C::lcdLine);
        if (exists || cur) g.fillRect (c); else g.drawRect (c, 1.0f);
        g.setColour (exists || cur ? Colours::black : C::lcdText.withAlpha (0.5f));
        g.setFont (monoFont (12, true));
        g.drawText (String (slot + 1), c, Justification::centred);
    }
}

void Screen::drawConfig (Graphics& g, Rectangle<float> r)
{
    auto& tr = ed.proc.tracks[(size_t) ed.track];
    g.setFont (monoFont (10.5f)); g.setColour (C::lcdText.withAlpha (0.8f));
    String s;
    s << "TRACK " << (ed.track + 1) << "   midi " << tr.params[MX_MIDICH]->getCurrentValueAsText()
      << "   input " << tr.params[MAT_INPUT]->getCurrentValueAsText() << "\n\n"
      << "keys: 1-4 track  space play  R rec  C ctrl  T temp\n"
      << "ctrl+play  = arm record (tape)      ctrl+clear = init track\n"
      << "ctrl+device = reset that device     copy > pick track = paste\n"
      << "drop audio on the panel to load it into the current track\n"
      << "midi program change N recalls scene N+1";
    g.drawFittedText (s, r.reduced (4).toNearestInt(), Justification::topLeft, 10);
}

void Screen::drawProject (Graphics& g, Rectangle<float> r)
{
    const float rh = r.getHeight() / 4;
    const double sr = jmax (1.0, ed.getAudioProcessor()->getSampleRate());
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto row = Rectangle<float> (r.getX(), r.getY() + t * rh, r.getWidth(), rh - 3);
        auto& tr = ed.proc.tracks[(size_t) t];
        g.setColour (t == ed.track ? Colour (0xff1a1a1d) : Colour (0xff111113)); g.fillRect (row);
        g.setColour (QuarryEditor::trackColour (t)); g.fillRect (row.withWidth (3));
        g.setFont (monoFont (11, t == ed.track)); g.setColour (C::lcdText);
        String txt = "T" + String (t + 1) + "  " + tr.params[MAT_MODE]->getCurrentValueAsText().toUpperCase().paddedRight (' ', 6);
        const auto nm = ed.proc.sampleName (t);
        txt << (nm.isNotEmpty() ? nm.substring (0, 26) : String ("(no sample)"));
        txt << "   tape " << String (tr.material.tapeLenAtomic.load() / sr, 1) << "s";
        if (tr.v (MAT_REC) > 0.5f) txt << "  REC";
        g.drawText (txt, row.reduced (8, 0), Justification::centredLeft);
    }
}

void Screen::drawTrackSelect (Graphics& g, Rectangle<float> r)
{
    const float rh = r.getHeight() / 4;
    const Key::Icon icons[] = { Key::Triangle, Key::Square, Key::Hexagon, Key::Circle };
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto row = Rectangle<float> (r.getX(), r.getY() + t * rh, r.getWidth(), rh - 3);
        g.setColour (t == ed.track ? Colour (0xff1a1a1d) : Colour (0xff111113)); g.fillRect (row);
        drawIcon (g, icons[t], row.withWidth (30).reduced (4), QuarryEditor::trackColour (t));
        g.setFont (monoFont (11, true)); g.setColour (C::lcdText);
        const String what = ed.pasteMode ? "PASTE TO TRACK " : "TRACK ";
        g.drawText (what + String (t + 1), row.withTrimmedLeft (36).withWidth (150), Justification::centredLeft);
        if (auto s = ed.proc.getSample (t))
        {
            Graphics::ScopedSaveState ss (g);
            g.setOpacity (0.6f);
            drawWave (g, row.withTrimmedLeft (190).reduced (2), s->peaks.data(), (int) s->peaks.size());
        }
        const float pk = ed.proc.trackPeak[(size_t) t].load();
        g.setColour (QuarryEditor::trackColour (t)); g.fillRect (row.getX() + 36, row.getBottom() - 3, pk * 140, 2.0f);
    }
}

void Screen::mouseDown (const MouseEvent& e)
{
    dragCell = -1;
    const auto pos = e.position;
    if (ed.view == View::Scene) { const int s = sceneSlotAt (pos); if (s >= 0) { ed.sceneClicked (s); return; } }
    if (ed.view == View::TrackSelect && vizArea().contains (pos))
    {
        ed.selectTrack (jlimit (0, 3, (int) ((pos.y - vizArea().getY()) / (vizArea().getHeight() / 4))));
        return;
    }
    if (ed.view == View::Material && vizArea().contains (pos) && ed.proc.getSample (ed.track) == nullptr
        && (int) ed.proc.tracks[(size_t) ed.track].v (MAT_MODE) != 0) { ed.openLoadDialog(); return; }
    if (ed.pages.empty()) return;
    const auto& pg = ed.pages[(size_t) ed.page];
    for (int i = 0; i < 8; ++i)
        if (cellArea (i).contains (pos) && pg.p[(size_t) i] != nullptr)
        {
            dragCell = i; dragStartNorm = pg.p[(size_t) i]->getValue(); dragStartY = e.getPosition().y;
            pg.p[(size_t) i]->beginChangeGesture();
            ed.touchCell (i);
            // bools / choices step on click
            auto* p = pg.p[(size_t) i];
            if (dynamic_cast<AudioParameterBool*> (p)) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            return;
        }
}

void Screen::mouseDrag (const MouseEvent& e)
{
    if (dragCell < 0 || ed.pages.empty()) return;
    auto* p = ed.pages[(size_t) ed.page].p[(size_t) dragCell];
    if (p == nullptr || dynamic_cast<AudioParameterBool*> (p)) return;
    const float delta = (float) (dragStartY - e.getPosition().y) / 220.0f + (float) e.getDistanceFromDragStartX() / 400.0f;
    p->setValueNotifyingHost (dsp::clamp01 (dragStartNorm + delta));
    ed.touchCell (dragCell);
}

void Screen::mouseUp (const MouseEvent&)
{
    if (dragCell >= 0 && ! ed.pages.empty())
        if (auto* p = ed.pages[(size_t) ed.page].p[(size_t) dragCell]) p->endChangeGesture();
    dragCell = -1;
}

void Screen::mouseDoubleClick (const MouseEvent& e)
{
    if (ed.pages.empty()) return;
    const auto& pg = ed.pages[(size_t) ed.page];
    for (int i = 0; i < 8; ++i)
        if (cellArea (i).contains (e.position) && pg.p[(size_t) i] != nullptr && ! dynamic_cast<AudioParameterBool*> (pg.p[(size_t) i]))
            pg.p[(size_t) i]->setValueNotifyingHost (pg.p[(size_t) i]->getDefaultValue());
}

// ===================================================================== Editor
QuarryEditor::QuarryEditor (QuarryProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    addAndMakeVisible (panel);
    panel.setBounds (0, 0, kW, kH);
    panel.addAndMakeVisible (screen);
    panel.addAndMakeVisible (master);
    for (auto& e : enc) panel.addAndMakeVisible (e);
    for (auto* k : { &project, &config, &modKey, &s1, &s2, &s3, &s4, &dMat, &dGr, &dFl, &dCo, &dSp,
                     &play, &copy, &clear, &ctrlKey, &trackKey, &scene, &mix, &perform, &tempKey })
        panel.addAndMakeVisible (k);

    masterAttach = std::make_unique<SliderParameterAttachment> (*gp (GID::out), master);
    master.bound = true; master.accent = C::text;
    master.setDoubleClickReturnValue (true, 0.0);

    for (int i = 0; i < 8; ++i)
    {
        enc[(size_t) i].onDragStart = [this, i] { touchCell (i); };
        enc[(size_t) i].onValueChange = [this, i] { touchCell (i); };
    }

    // ---- layout (matches the hardware-style reference) ----
    master.setBounds (102, 84, 76, 76);
    project.setBounds (52, 208, 82, 82); config.setBounds (148, 208, 82, 82);
    screen.setBounds (50, 353, 474, 279);
    modKey.setBounds (52, 648, 81, 81);
    for (int i = 0; i < 4; ++i) soft[(size_t) i]->setBounds (149 + i * 98, 648, 81, 81);
    const int ex[] = { 727, 856, 986, 1116 };
    for (int i = 0; i < 8; ++i) enc[(size_t) i].setBounds (ex[i % 4] - 40, (i < 4 ? 122 : 248) - 40, 80, 80);
    Key* dev[] = { &dMat, &dGr, &dFl, &dCo, &dSp };
    for (int i = 0; i < 5; ++i) dev[i]->setBounds (687 + i * 98, 353, 82, 82);
    play.setBounds (687, 549, 82, 82); copy.setBounds (883, 549, 82, 82); clear.setBounds (981, 549, 82, 82); ctrlKey.setBounds (1079, 549, 82, 82);
    Key* bottom[] = { &trackKey, &scene, &mix, &perform, &tempKey };
    for (int i = 0; i < 5; ++i) bottom[i]->setBounds (687 + i * 98, 646, 82, 82);
    for (int t = 0; t < 4; ++t) trackLedArea[(size_t) t] = { 300 + t * 85, 110, 70, 40 };

    // ---- behaviour ----
    dMat.onPress = [this] { deviceKey (View::Material, "mat_"); };
    dGr.onPress  = [this] { deviceKey (View::Granular, "gr_"); };
    dFl.onPress  = [this] { deviceKey (View::Filter, "fl_"); };
    dCo.onPress  = [this] { deviceKey (View::Color, "co_"); };
    dSp.onPress  = [this] { deviceKey (View::Space, "sp_"); };
    modKey.onPress = [this]
    {
        if (ctrl) { proc.resetDevice (track, "m"); consumeCtrl(); showFlash ("T" + String (track + 1) + " MODS RESET"); updateLights(); return; }
        setView (view == View::Mod ? prevView : View::Mod);
    };
    for (int i = 0; i < 4; ++i)
    {
        soft[(size_t) i]->onPress = [this, i] { softPressed (i); };
        soft[(size_t) i]->onRelease = [this, i] { softReleased (i); };
    }
    project.onPress = [this] { setView (view == View::Project ? prevView : View::Project); };
    config.onPress  = [this] { setView (view == View::Config ? prevView : View::Config); };
    scene.onPress   = [this] { setView (view == View::Scene ? prevView : View::Scene); };
    mix.onPress     = [this] { setView (view == View::Mix ? prevView : View::Mix); };
    perform.onPress = [this] { setView (view == View::Perform ? prevView : View::Perform); };
    trackKey.onPress = [this] { pasteMode = false; setView (view == View::TrackSelect ? prevView : View::TrackSelect); };
    tempKey.onPress = [this] { temp = ! temp; updateLights(); };
    ctrlKey.onPress = [this] { ctrl = ! ctrl; updateLights(); };
    play.onPress = [this]
    {
        if (ctrl)
        {
            auto* r = tp (MAT_REC);
            const bool on = r->getValue() < 0.5f;
            if (on && (int) proc.tracks[(size_t) track].v (MAT_MODE) != 0) tp (MAT_MODE)->setValueNotifyingHost (0.0f);  // record needs Tape mode
            if (on && (int) proc.tracks[(size_t) track].v (MAT_INPUT) == 0) tp (MAT_INPUT)->setValueNotifyingHost (tp (MAT_INPUT)->convertTo0to1 (1));
            r->setValueNotifyingHost (on ? 1.0f : 0.0f);
            showFlash (on ? "T" + String (track + 1) + " RECORD ARMED" : "T" + String (track + 1) + " RECORD OFF");
            consumeCtrl();
        }
        else { auto* p = gp (GID::play); p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); }
        updateLights();
    };
    copy.onPress = [this]
    {
        proc.copyTrack (track); pasteMode = true;
        showFlash ("COPIED T" + String (track + 1) + " - PICK A TRACK");
        setView (View::TrackSelect);
    };
    clear.onPress = [this]
    {
        if (ctrl) { proc.resetTrack (track); consumeCtrl(); showFlash ("T" + String (track + 1) + " INITIALISED"); }
        else      { tp (MAT_REC)->setValueNotifyingHost (0.0f); proc.clearTrack (track); showFlash ("T" + String (track + 1) + " TAPE + SAMPLE CLEARED"); }
        updateLights();
    };

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / kH);
    setSize ((int) (kW * 0.82), (int) (kH * 0.82));

    rebuildPages();
    updateLights();
    startTimerHz (30);
}

QuarryEditor::~QuarryEditor()
{
    stopTimer();
    for (auto& a : encAttach) a.reset();
    masterAttach.reset();
}

void QuarryEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    panel.setTransform (AffineTransform::scale (s));
}

String QuarryEditor::viewName() const
{
    switch (view)
    {
        case View::Material: return "MATERIAL";
        case View::Granular: return "GRANULAR";
        case View::Filter:   return "FILTER";
        case View::Color:    return "COLOR";
        case View::Space:    return "SPACE";
        case View::Mod:      return "MOD " + String (page + 1);
        case View::Mix:      return "MIX";
        case View::Perform:  return "PERFORM";
        case View::Scene:    return "SCENE";
        case View::Config:   return "CONFIG";
        case View::Project:  return "PROJECT";
        case View::TrackSelect: return pasteMode ? "PASTE" : "SELECT";
    }
    return {};
}

std::vector<String> QuarryEditor::softKeyLabels() const
{
    if (temp) return { "stutter", "tape stop", "sweep", "freeze" };
    if (view == View::TrackSelect) return { "track 1", "track 2", "track 3", "track 4" };
    if (view == View::Scene) return { "bank -", "bank +", sceneStore ? "[store]" : "store", sceneDelete ? "[delete]" : "delete" };
    if (view == View::Project) return { "load", "rec", "init trk", "clear" };
    std::vector<String> l (4);
    for (int i = 0; i < 4 && i < (int) pages.size(); ++i) l[(size_t) i] = pages[(size_t) i].name;
    if (pages.size() > 4) l[3] = pages[(size_t) jmax (3, page)].name + " >";
    return l;
}

void QuarryEditor::setView (View v)
{
    if (v == View::TrackSelect || v == View::Scene || v == View::Mix || v == View::Perform || v == View::Config || v == View::Project || v == View::Mod)
    {
        if (view != v && (view == View::Material || view == View::Granular || view == View::Filter || view == View::Color || view == View::Space))
            prevView = view;
    }
    if (v != View::TrackSelect) pasteMode = false;
    if (v != View::Scene) sceneStore = sceneDelete = false;
    const bool changed = v != view;
    view = v;
    if (changed && v != View::TrackSelect) page = (v == View::Mod ? modSlot : 0);
    if (v != View::TrackSelect) rebuildPages();
    updateLights();
    screen.repaint();
}

void QuarryEditor::deviceKey (View v, const String& prefix)
{
    if (ctrl)
    {
        proc.resetDevice (track, prefix); consumeCtrl();
        setView (v);
        showFlash ("T" + String (track + 1) + " " + viewName() + " RESET");
        return;
    }
    if (view == v && pages.size() > 1) { page = (page + 1) % (int) pages.size(); bindEncoders(); updateLights(); return; }
    prevView = v;
    setView (v);
}

void QuarryEditor::selectTrack (int t)
{
    if (pasteMode)
    {
        proc.pasteTrack (t);
        showFlash ("PASTED T" + String (track + 1) + " > T" + String (t + 1));
        pasteMode = false;
    }
    track = jlimit (0, 3, t);
    if (view == View::TrackSelect) setView (prevView);
    else { rebuildPages(); updateLights(); }
}

void QuarryEditor::rebuildPages()
{
    pages.clear();
    auto T = [this] (int i) { return proc.trackParam (track, i); };
    auto name = [] (int i) { return trackSpecs()[(size_t) i].name; };
    auto mk = [&] (const String& nm, std::initializer_list<int> ids)
    {
        Page p; p.name = nm; int k = 0;
        for (int id : ids) { if (id >= 0) { p.p[(size_t) k] = T (id); p.label[(size_t) k] = name (id); } ++k; }
        pages.push_back (p);
    };
    auto mkG = [&] (const String& nm, std::vector<std::pair<RangedAudioParameter*, String>> items)
    {
        Page p; p.name = nm;
        for (size_t k = 0; k < items.size() && k < 8; ++k) { p.p[k] = items[k].first; p.label[k] = items[k].second; }
        pages.push_back (p);
    };

    switch (view)
    {
        case View::Material:
            mk ("main", { MAT_MODE, MAT_INPUT, MAT_SPEED, MAT_START, MAT_LENGTH, MAT_GAIN, MAT_TUNE, MAT_MONITOR });
            mk ("play", { MAT_TRIG, MAT_LOOP, MAT_ROOT, MAT_ATTACK, MAT_RELEASE, MAT_WOW, MAT_DUB, MAT_REC });
            break;
        case View::Granular:
            mk ("grain", { GR_MIX, GR_POS, GR_SIZE, GR_DENSITY, GR_SPRAY, GR_SCAN, GR_PITCH, GR_SHAPE });
            mk ("more",  { GR_PRAND, GR_QUANT, GR_SPREAD, GR_REVERSE, GR_FREEZE, GR_MIX, GR_POS, GR_SIZE });
            break;
        case View::Filter:
            mk ("filter", { FL_MIX, FL_MORPH, FL_CUTOFF, FL_RESO, FL_TYPE, FL_DRIVE, FL_RPITCH, FL_RDECAY });
            mk ("reson",  { FL_RPITCH, FL_RSPREAD, FL_RDECAY, FL_RBANDS, FL_RTILT, FL_KEY, FL_MORPH, FL_MIX });
            break;
        case View::Color:
            mk ("color", { CO_MIX, CO_DRIVE, CO_TYPE, CO_BAND, CO_TONE, CO_COMP, CO_NOISE, CO_OUT });
            mk ("crush", { CO_BITS, CO_DOWN, CO_DRIVE, CO_TYPE, CO_NOISE, CO_MIX, CO_COMP, CO_OUT });
            break;
        case View::Space:
            mk ("delay",  { SP_MIX, SP_DTIME, SP_DSYNC, SP_DFB, SP_SHIMMER, SP_SHPITCH, SP_FREEZE, SP_DLEVEL });
            mk ("reverb", { SP_MIX, SP_RLEVEL, SP_RSIZE, SP_RDECAY, SP_RDAMP, SP_WIDTH, SP_DTOR, SP_FREEZE });
            break;
        case View::Mod:
            for (int k = 0; k < kNumMods; ++k)
            {
                const int b = M1_TYPE + k * 5;
                Page p; p.name = "mod " + String (k + 1);
                const char* lbl[] = { "Type", "Rate", "Shape", "Depth", "Target" };
                for (int j = 0; j < 5; ++j) { p.p[(size_t) j] = T (b + j); p.label[(size_t) j] = lbl[j]; }
                // 6-8: depths of the other mods for quick balancing
                int slot = 5;
                for (int o = 0; o < kNumMods && slot < 8; ++o)
                    if (o != k) { p.p[(size_t) slot] = T (M1_TYPE + o * 5 + 3); p.label[(size_t) slot] = "M" + String (o + 1) + " Depth"; ++slot; }
                pages.push_back (p);
            }
            break;
        case View::Mix:
        {
            std::vector<std::pair<RangedAudioParameter*, String>> a, b, c;
            for (int t = 0; t < 4; ++t) a.push_back ({ proc.trackParam (t, MX_LEVEL), "T" + String (t + 1) + " Level" });
            for (int t = 0; t < 4; ++t) a.push_back ({ proc.trackParam (t, MX_PAN), "T" + String (t + 1) + " Pan" });
            for (int t = 0; t < 4; ++t) b.push_back ({ proc.trackParam (t, MX_MUTE), "T" + String (t + 1) + " Mute" });
            for (int t = 0; t < 4; ++t) b.push_back ({ proc.trackParam (t, MX_SOLO), "T" + String (t + 1) + " Solo" });
            c = { { gp (GID::comp), "Glue" }, { gp (GID::dj), "DJ Filter" }, { gp (GID::out), "Master" }, { gp (GID::glide), "Scene Glide" } };
            for (int t = 0; t < 4; ++t) c.push_back ({ proc.trackParam (t, MX_MASTER), "T" + String (t + 1) + " > Mst" });
            mkG ("level", a); mkG ("mute", b); mkG ("master", c);
            break;
        }
        case View::Perform:
        {
            std::vector<std::pair<RangedAudioParameter*, String>> m, ta, aa, tb, ab;
            for (int i = 0; i < kNumMacros; ++i)
            {
                m.push_back  ({ gp (GID::macro (i)), "Macro " + String (i + 1) });
                ta.push_back ({ gp (GID::macroTgt (i, 0)), "M" + String (i + 1) + " Tgt A" });
                aa.push_back ({ gp (GID::macroAmt (i, 0)), "M" + String (i + 1) + " Amt A" });
                tb.push_back ({ gp (GID::macroTgt (i, 1)), "M" + String (i + 1) + " Tgt B" });
                ab.push_back ({ gp (GID::macroAmt (i, 1)), "M" + String (i + 1) + " Amt B" });
            }
            mkG ("macros", m); mkG ("tgt a", ta); mkG ("amt a", aa); mkG ("tgt b", tb); mkG ("amt b", ab);
            break;
        }
        case View::Scene:
            mkG ("scene", { { gp (GID::glide), "Glide" }, { gp (GID::dj), "DJ Filter" }, { gp (GID::comp), "Glue" }, { gp (GID::out), "Master" },
                            { gp (GID::macro (0)), "Macro 1" }, { gp (GID::macro (1)), "Macro 2" }, { gp (GID::macro (2)), "Macro 3" }, { gp (GID::macro (3)), "Macro 4" } });
            break;
        case View::Config:
            mk ("track", { MX_MIDICH, MAT_INPUT, MX_MASTER, MAT_ROOT, MAT_TRIG, MAT_LOOP, MAT_MONITOR, MAT_DUB });
            mkG ("global", { { gp (GID::comp), "Glue" }, { gp (GID::dj), "DJ Filter" }, { gp (GID::out), "Master" }, { gp (GID::glide), "Scene Glide" }, { gp (GID::play), "Play" } });
            break;
        case View::Project:
            mk ("project", { MAT_MODE, MAT_INPUT, MAT_GAIN, MAT_START, MAT_LENGTH, MAT_SPEED, MAT_REC, MAT_MONITOR });
            break;
        case View::TrackSelect: break;
    }
    page = jlimit (0, jmax (0, (int) pages.size() - 1), page);
    bindEncoders();
}

void QuarryEditor::bindEncoders()
{
    for (int i = 0; i < 8; ++i)
    {
        auto& e = enc[(size_t) i];
        encAttach[(size_t) i].reset();
        RangedAudioParameter* p = pages.empty() ? nullptr : pages[(size_t) page].p[(size_t) i];
        e.bound = p != nullptr;
        e.accent = accent();
        if (p != nullptr)
        {
            encAttach[(size_t) i] = std::make_unique<SliderParameterAttachment> (*p, e);
            e.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            const bool stepped = p->getNumSteps() < 40 && dynamic_cast<AudioParameterFloat*> (p) == nullptr;
            e.setMouseDragSensitivity (stepped ? 60 + p->getNumSteps() * 8 : 220);
            e.setEnabled (true);
        }
        else e.setEnabled (false);
        e.repaint();
    }
    lastTouchedCell = -1;
    screen.repaint();
}

void QuarryEditor::softPressed (int i)
{
    if (temp)
    {
        gp (GID::punch (i))->setValueNotifyingHost (1.0f);
        punchHeld[(size_t) i] = 1; updateLights(); return;
    }
    if (view == View::TrackSelect) { selectTrack (i); return; }
    if (view == View::Scene)
    {
        if (i == 0) sceneBank = (sceneBank + 7) % 8;
        if (i == 1) sceneBank = (sceneBank + 1) % 8;
        if (i == 2) { sceneStore = ! sceneStore; sceneDelete = false; }
        if (i == 3) { sceneDelete = ! sceneDelete; sceneStore = false; }
        updateLights(); screen.repaint(); return;
    }
    if (view == View::Project)
    {
        if (i == 0) openLoadDialog();
        if (i == 1) { ctrl = true; play.onPress(); }
        if (i == 2) { proc.resetTrack (track); showFlash ("T" + String (track + 1) + " INITIALISED"); }
        if (i == 3) { proc.clearTrack (track); showFlash ("T" + String (track + 1) + " CLEARED"); }
        updateLights(); return;
    }
    if (pages.empty()) return;
    int np = i;
    if (i == 3 && pages.size() > 4) np = page >= 3 ? 3 + ((page - 3 + 1) % ((int) pages.size() - 3)) : 3;
    if (np < (int) pages.size())
    {
        page = np;
        if (view == View::Mod) modSlot = page;
        bindEncoders(); updateLights();
    }
}

void QuarryEditor::softReleased (int i)
{
    if (punchHeld[(size_t) i])
    {
        gp (GID::punch (i))->setValueNotifyingHost (0.0f);
        punchHeld[(size_t) i] = 0; updateLights();
    }
}

void QuarryEditor::sceneClicked (int slot)
{
    if (sceneStore || ctrl) { proc.storeScene (slot); showFlash ("STORED SCENE " + String (slot + 1)); sceneStore = false; consumeCtrl(); }
    else if (sceneDelete)   { proc.clearScene (slot); showFlash ("DELETED SCENE " + String (slot + 1)); sceneDelete = false; }
    else if (proc.sceneExists (slot)) { proc.recallScene (slot); showFlash ("SCENE " + String (slot + 1)); }
    else showFlash ("EMPTY - ctrl+tap or [store] to save");
    updateLights(); screen.repaint();
}

void QuarryEditor::openLoadDialog()
{
    chooser = std::make_unique<FileChooser> ("Load audio into track " + String (track + 1), File(), proc.formats.getWildcardForAllFormats());
    const int t = track;
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, t] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile())
            showFlash (proc.loadSample (t, f) ? "LOADED " + f.getFileNameWithoutExtension().substring (0, 24).toUpperCase() : String ("COULD NOT READ FILE"));
    });
}

void QuarryEditor::filesDropped (const StringArray& files, int, int)
{
    int t = track;
    for (auto& path : files)
    {
        if (t > 3) break;
        if (proc.loadSample (t, File (path))) { showFlash ("LOADED INTO T" + String (t + 1)); ++t; }
    }
    if (view != View::Material) setView (View::Material);
}

bool QuarryEditor::keyPressed (const KeyPress& k)
{
    const auto c = k.getTextCharacter();
    if (c >= '1' && c <= '4') { selectTrack (c - '1'); return true; }
    if (k == KeyPress::spaceKey) { play.onPress(); return true; }
    if (c == 'r' || c == 'R') { ctrl = true; play.onPress(); return true; }
    if (c == 'c' || c == 'C') { ctrl = ! ctrl; updateLights(); return true; }
    if (c == 't' || c == 'T') { temp = ! temp; updateLights(); return true; }
    return false;
}

void QuarryEditor::updateLights()
{
    const auto acc = accent();
    dMat.setLit (view == View::Material, acc); dGr.setLit (view == View::Granular, acc); dFl.setLit (view == View::Filter, acc);
    dCo.setLit (view == View::Color, acc); dSp.setLit (view == View::Space, acc);
    modKey.setLit (view == View::Mod, acc);
    project.setLit (view == View::Project, acc); config.setLit (view == View::Config, acc);
    scene.setLit (view == View::Scene, acc); mix.setLit (view == View::Mix, acc); perform.setLit (view == View::Perform, acc);
    trackKey.setLit (view == View::TrackSelect, acc);
    tempKey.setLit (temp, Colour (0xffff5a5a));
    ctrlKey.setLit (ctrl, Colours::white);
    copy.setLit (pasteMode, acc);
    play.setLit (gp (GID::play)->getValue() > 0.5f, tp (MAT_REC)->getValue() > 0.5f ? Colour (0xffff4a4a) : acc);

    for (int i = 0; i < 4; ++i)
    {
        bool lit = false; Colour c = acc;
        if (temp) { lit = gp (GID::punch (i))->getValue() > 0.5f; c = Colour (0xffff5a5a); }
        else if (view == View::TrackSelect) { lit = i == track; c = trackColour (i); }
        else if (view == View::Scene) lit = (i == 2 && sceneStore) || (i == 3 && sceneDelete);
        else if (view != View::Project) lit = i == jmin (page, 3) && (int) pages.size() > i;
        soft[(size_t) i]->setLit (lit, c);
    }
    panel.repaint();
}

void QuarryEditor::timerCallback()
{
    // mod history for the MOD screen
    auto& tr = proc.tracks[(size_t) track];
    for (int k = 0; k < kNumMods; ++k) screen.modHist[(size_t) k][(size_t) screen.modHistPos] = tr.modValue[(size_t) k].load();
    screen.modHistPos = (screen.modHistPos + 1) % 120;

    // keep play key in sync when host automation changes it
    static float lastPlay = -1, lastRec = -1;
    const float pv = gp (GID::play)->getValue(), rv = tp (MAT_REC)->getValue();
    if (pv != lastPlay || rv != lastRec) { lastPlay = pv; lastRec = rv; updateLights(); }
    if (temp) updateLights();

    screen.repaint();
    panel.repaint (0, 0, kW, 160);
    for (auto& e : enc) if (e.bound) e.repaint();
}
} // namespace quarry
