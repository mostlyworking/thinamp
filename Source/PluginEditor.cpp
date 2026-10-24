#include "PluginEditor.h"
#include <BinaryData.h>

using juce::Colour;
using juce::ColourGradient;
using juce::Graphics;
using juce::Rectangle;

namespace
{

namespace palette
{
    const Colour bodyTop { 0xff3c3c5e }, bodyBottom { 0xff1e1e32 };
    const Colour bevelLight { 0xff76769e }, bevelDark { 0xff0a0a14 };
    const Colour titleTop { 0xff50508a }, titleBottom { 0xff20203a }, ridgeLight { 0xff9090c0 }, ridgeDark { 0xff16162a };
    const Colour titleText { 0xffe6e6f4 };
    const Colour lcd { 0xff000000 }, lcdGrid { 0xff0c1c0c };
    const Colour green { 0xff00e000 }, greenGhost { 0xff06200a }, truncated { 0xff27402c };
    const Colour barTop { 0xffe82a10 }, barMid { 0xffe8d820 }, barBottom { 0xff18c818 };
    const Colour peakCap { 0xffb8b8cc };
    const Colour faceTop { 0xffdcdcea }, faceBottom { 0xff8c8ca8 }, faceText { 0xff1a1a30 };
    const Colour pressedTop { 0xff26263e }, pressedBottom { 0xff40406a };
}

const char* const modeShortNames[] = { "LOW", "HIGH", "ODD", "PEAK" };
const char* const modeLongNames[] = { "LOW HARMONICS", "HIGH HARMONICS", "SPARSE ODD", "STRONGEST" };

juce::Font lcdFont (float size, bool bold = true)
{
    static const auto regular = juce::Typeface::createSystemTypefaceFor (BinaryData::DejaVuSansMono_ttf,
                                                                         BinaryData::DejaVuSansMono_ttfSize);
    static const auto boldFace = juce::Typeface::createSystemTypefaceFor (BinaryData::DejaVuSansMonoBold_ttf,
                                                                          BinaryData::DejaVuSansMonoBold_ttfSize);
    return juce::Font (juce::FontOptions (bold ? boldFace : regular).withHeight (size));
}

// Raised (light top-left) or sunken (dark top-left) edge of thickness t.
void bevel (Graphics& g, Rectangle<float> r, Colour topLeft, Colour bottomRight, float t = 1.0f)
{
    g.setColour (topLeft);
    g.fillRect (r.getX(), r.getY(), r.getWidth(), t);
    g.fillRect (r.getX(), r.getY(), t, r.getHeight());
    g.setColour (bottomRight);
    g.fillRect (r.getX(), r.getBottom() - t, r.getWidth(), t);
    g.fillRect (r.getRight() - t, r.getY(), t, r.getHeight());
}

Rectangle<int> scaled (int x, int y, int w, int h)
{
    return Rectangle<int> (x, y, w, h).toFloat().transformedBy (juce::AffineTransform::scale (ft::ui::scale)).toNearestInt();
}

// Layout in base units.
const Rectangle<float> lcdBox { 9, 19, 95, 41 };
const Rectangle<float> analyzerBox { 24, 42, 76, 16 };
const Rectangle<float> marqueeBox { 110, 22, 155, 12 };
constexpr float digitX = 40, digitY = 23, digitW = 9, digitH = 13, digitPitch = 12;
constexpr float analyzerRangeDb = 60.0f;

void drawSevenSegment (Graphics& g, float x, float y, int digit)
{
    // Segment bits: a b c d e f g
    static const int masks[] = { 0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b };
    const int mask = digit < 0 ? 0 : masks[digit];
    const float t = 1.6f, w = digitW, h = digitH, half = h * 0.5f;

    const Rectangle<float> segments[] = {
        { x + 1.5f, y, w - 3, t },                       // a
        { x + w - t, y + 1.2f, t, half - 1.6f },          // b
        { x + w - t, y + half + 0.4f, t, half - 1.6f },   // c
        { x + 1.5f, y + h - t, w - 3, t },               // d
        { x, y + half + 0.4f, t, half - 1.6f },           // e
        { x, y + 1.2f, t, half - 1.6f },                  // f
        { x + 1.5f, y + half - t * 0.5f, w - 3, t },     // g
    };

    for (int s = 0; s < 7; ++s)
    {
        g.setColour ((mask >> (6 - s)) & 1 ? palette::green : palette::greenGhost);
        g.fillRect (segments[s]);
    }
}

} // namespace

//==============================================================================
void WinampLookAndFeel::drawLinearSlider (Graphics& g, int, int, int, int, float sliderPos, float, float,
                                          juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float u = ft::ui::scale;
    const auto r = slider.getLocalBounds().toFloat();

    // Sunken groove with the volume-slider gradient filled up to the thumb.
    const auto groove = r.reduced (0, r.getHeight() * 0.22f);
    g.setColour (Colour (0xff08080e));
    g.fillRect (groove);

    ColourGradient fill (palette::barBottom, groove.getX(), 0, palette::barTop, groove.getRight(), 0, false);
    fill.addColour (0.6, palette::barMid);
    g.setGradientFill (fill);
    g.fillRect (groove.reduced (u).withRight (sliderPos));
    bevel (g, groove, palette::bevelDark, palette::bevelLight, u);

    const float thumbWidth = (float) getSliderThumbRadius (slider) * 2.0f;
    const Rectangle<float> thumb (sliderPos - thumbWidth * 0.5f, r.getY(), thumbWidth, r.getHeight());
    g.setGradientFill (ColourGradient (palette::faceTop, 0, thumb.getY(), palette::faceBottom, 0, thumb.getBottom(), false));
    g.fillRect (thumb);
    bevel (g, thumb, Colour (0xfff4f4ff), Colour (0xff3a3a54), u);

    g.setColour (Colour (0xff50506c));
    for (int i = -1; i <= 1; ++i)
        g.fillRect (thumb.getCentreX() + (float) i * 2.0f * u - u * 0.5f, thumb.getY() + 2.5f * u, u, thumb.getHeight() - 5.0f * u);
}

int WinampLookAndFeel::getSliderThumbRadius (juce::Slider& slider)
{
    // The N slider gets Winamp's wide seek-bar thumb, MIX the narrow volume thumb.
    return juce::roundToInt ((slider.getName() == "n" ? 14.5f : 7.0f) * ft::ui::scale);
}

//==============================================================================
ModeButtons::ModeButtons (juce::RangedAudioParameter& modeParameter)
    : attachment (modeParameter, [this] (float value) { selected = juce::roundToInt (value); repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModeButtons::paint (Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (ft::ui::scale));
    const float h = (float) getHeight() / ft::ui::scale;

    for (int i = 0; i < numButtons; ++i)
    {
        const Rectangle<float> b ((float) (i * (buttonWidth + gap)), 0, (float) buttonWidth, h);
        const bool pressed = i == selected;

        g.setGradientFill (pressed ? ColourGradient (palette::pressedTop, 0, 0, palette::pressedBottom, 0, h, false)
                                   : ColourGradient (palette::faceTop, 0, 0, palette::faceBottom, 0, h, false));
        g.fillRect (b);
        bevel (g, b, pressed ? palette::bevelDark : Colour (0xfff4f4ff), pressed ? palette::bevelLight : Colour (0xff3a3a54));

        g.setColour (pressed ? palette::green : palette::faceText);
        g.setFont (lcdFont (6.5f));
        g.drawText (modeShortNames[i], b.translated (pressed ? 0.5f : 0.0f, pressed ? 0.5f : 0.0f),
                    juce::Justification::centred, false);
    }
}

void ModeButtons::mouseDown (const juce::MouseEvent& e)
{
    const int index = juce::jlimit (0, numButtons - 1, (int) (e.position.x / ft::ui::scale) / (buttonWidth + gap));
    attachment.setValueAsCompleteGesture ((float) index);
}

//==============================================================================
ThinampEditor::ThinampEditor (ThinampProcessor& p)
    : AudioProcessorEditor (p),
      truncator (p),
      nAttachment (p.apvts, "n", nSlider),
      mixAttachment (p.apvts, "mix", mixSlider),
      modeButtons (*p.apvts.getParameter ("mode"))
{
    for (auto* s : { &nSlider, &mixSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setLookAndFeel (&lookAndFeel);
        s->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (*s);
    }

    nSlider.setName ("n");
    nSlider.setDoubleClickReturnValue (true, (double) ft::maxN);
    mixSlider.setDoubleClickReturnValue (true, 1.0);
    addAndMakeVisible (modeButtons);

    setSize (juce::roundToInt (ft::ui::baseWidth * ft::ui::scale), juce::roundToInt (ft::ui::baseHeight * ft::ui::scale));
    startTimerHz (30);
}

ThinampEditor::~ThinampEditor()
{
    nSlider.setLookAndFeel (nullptr);
    mixSlider.setLookAndFeel (nullptr);
}

void ThinampEditor::resized()
{
    mixSlider.setBounds (scaled (107, 56, 108, 11));
    nSlider.setBounds (scaled (16, 71, 248, 10));
    modeButtons.setBounds (scaled (16, 88, ModeButtons::numButtons * (ModeButtons::buttonWidth + ModeButtons::gap)
                                                - ModeButtons::gap, 17));
}

void ThinampEditor::timerCallback()
{
    auto& spectrum = truncator.spectrum();
    const float barHeight = analyzerBox.getHeight();
    auto toHeight = [barHeight] (float linear)
    {
        const float db = juce::Decibels::gainToDecibels (linear, -analyzerRangeDb);
        return juce::jlimit (0.0f, barHeight, (db + analyzerRangeDb) / analyzerRangeDb * barHeight);
    };

    for (size_t b = 0; b < ft::numBands; ++b)
    {
        constexpr float fall = 1.2f, peakFall = 0.5f;
        barTotal[b] = juce::jmax (toHeight (spectrum.total[b].load (std::memory_order_relaxed)), barTotal[b] - fall);
        barKept[b] = juce::jmax (toHeight (spectrum.kept[b].load (std::memory_order_relaxed)), barKept[b] - fall);

        if (barKept[b] >= barPeak[b])
        {
            barPeak[b] = barKept[b];
            peakHold[b] = 12;
        }
        else if (peakHold[b] > 0)
            --peakHold[b];
        else
            barPeak[b] = juce::jmax (0.0f, barPeak[b] - peakFall);
    }

    const int n = juce::roundToInt (truncator.apvts.getRawParameterValue ("n")->load());
    const int mode = juce::roundToInt (truncator.apvts.getRawParameterValue ("mode")->load());
    const int mix = juce::roundToInt (truncator.apvts.getRawParameterValue ("mix")->load() * 100.0f);
    const int kept = mode == (int) ft::Mode::Odd ? juce::jmin (n, ft::numBins / 2) : n;

    const auto text = juce::String ("***  THINAMP  ***  ") + modeLongNames[mode] + "  ***  "
                    + juce::String (kept) + " OF " + juce::String (ft::numBins) + " BASIS FUNCTIONS  ***  DRY/WET "
                    + juce::String (mix) + "%  ";

    if (text != marqueeText)
    {
        marqueeText = text;
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (lcdFont (7.0f), marqueeText, 0, 0);
        marqueeWidth = glyphs.getBoundingBox (0, -1, true).getWidth();
    }

    marqueeOffset += 0.5f;
    if (marqueeWidth > 0.0f && marqueeOffset > marqueeWidth)
        marqueeOffset -= marqueeWidth;

    repaint();
}

void ThinampEditor::paint (Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (ft::ui::scale));

    const Rectangle<float> body (0, 0, (float) ft::ui::baseWidth, (float) ft::ui::baseHeight);
    g.setGradientFill (ColourGradient (palette::bodyTop, 0, 14, palette::bodyBottom, 0, body.getBottom(), false));
    g.fillRect (body);
    bevel (g, body, palette::bevelLight, palette::bevelDark);

    drawTitleBar (g);
    drawLcd (g);
    drawMarquee (g);
    drawReadouts (g);

    // Small captions in the empty bevel space, like Winamp's skin legends.
    g.setFont (lcdFont (5.0f));
    g.setColour (palette::bevelLight);
    g.drawText ("N  BASIS FUNCTIONS", Rectangle<float> (16, 81, 120, 6), juce::Justification::left, false);

    // Logo plaque where Winamp's lightning bolt sits.
    const Rectangle<float> logo (232, 89, 33, 16);
    g.setGradientFill (ColourGradient (palette::titleTop, 0, logo.getY(), palette::titleBottom, 0, logo.getBottom(), false));
    g.fillRect (logo);
    bevel (g, logo, palette::bevelLight, palette::bevelDark);
    g.setColour (palette::barMid);
    g.setFont (juce::Font (juce::FontOptions ("Georgia", 10.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("F(w)", logo, juce::Justification::centred, false);
}

void ThinampEditor::drawTitleBar (Graphics& g)
{
    const Rectangle<float> bar (1, 1, (float) ft::ui::baseWidth - 2, 13);
    g.setGradientFill (ColourGradient (palette::titleTop, 0, bar.getY(), palette::titleBottom, 0, bar.getBottom(), false));
    g.fillRect (bar);

    // Ridged grip lines either side of the title, like the classic skin.
    auto ridges = [&g] (float x0, float x1)
    {
        for (int i = 0; i < 4; ++i)
        {
            const float y = 3.5f + (float) i * 2.0f;
            g.setColour (palette::ridgeLight);
            g.fillRect (x0, y, x1 - x0, 1.0f);
            g.setColour (palette::ridgeDark);
            g.fillRect (x0, y + 1.0f, x1 - x0, 1.0f);
        }
    };
    ridges (16, 82);
    ridges (193, 238);

    // Menu square on the left, minimise / shade / close on the right.
    auto smallButton = [&g] (float x)
    {
        const Rectangle<float> b (x, 3.5f, 8, 8);
        g.setGradientFill (ColourGradient (palette::faceTop, 0, b.getY(), palette::faceBottom, 0, b.getBottom(), false));
        g.fillRect (b);
        bevel (g, b, Colour (0xfff4f4ff), Colour (0xff3a3a54), 0.5f);
    };
    smallButton (5);
    for (float x : { 243.0f, 253.0f, 263.0f })
        smallButton (x);

    g.setColour (palette::titleText);
    g.setFont (lcdFont (7.0f));
    g.drawText ("THINAMP", Rectangle<float> (84, 1, 107, 13), juce::Justification::centred, false);
}

void ThinampEditor::drawLcd (Graphics& g)
{
    g.setColour (palette::lcd);
    g.fillRect (lcdBox);
    bevel (g, lcdBox, palette::bevelDark, palette::bevelLight);

    // "N" indicator column where Winamp shows play / pause.
    g.setColour (palette::green);
    g.setFont (lcdFont (9.0f));
    g.drawText ("N", Rectangle<float> (13, digitY, 12, digitH), juce::Justification::centred, false);

    const int n = juce::roundToInt (truncator.apvts.getRawParameterValue ("n")->load());
    const auto digits = juce::String (n).paddedLeft (' ', 4);
    for (int i = 0; i < 4; ++i)
        drawSevenSegment (g, digitX + (float) i * digitPitch, digitY,
                          digits[i] == ' ' ? -1 : (int) (digits[i] - '0'));

    // Analyzer: dotted grid, dim bar for energy that was truncated away, lit bar for what was kept.
    g.setColour (palette::lcdGrid);
    for (float y = analyzerBox.getY(); y < analyzerBox.getBottom(); y += 2.0f)
        for (float x = analyzerBox.getX(); x < analyzerBox.getRight(); x += 2.0f)
            g.fillRect (x, y, 1.0f, 1.0f);

    const float bottom = analyzerBox.getBottom();
    ColourGradient lit (palette::barTop, 0, analyzerBox.getY(), palette::barBottom, 0, bottom, false);
    lit.addColour (0.45, palette::barMid);

    for (size_t b = 0; b < ft::numBands; ++b)
    {
        const float x = analyzerBox.getX() + (float) b * 4.0f;

        g.setColour (palette::truncated);
        g.fillRect (x, bottom - barTotal[b], 3.0f, barTotal[b]);

        g.setGradientFill (lit);
        g.fillRect (x, bottom - barKept[b], 3.0f, barKept[b]);

        if (barPeak[b] > 0.5f)
        {
            g.setColour (palette::peakCap);
            g.fillRect (x, bottom - barPeak[b] - 1.0f, 3.0f, 1.0f);
        }
    }
}

void ThinampEditor::drawMarquee (Graphics& g)
{
    g.setColour (palette::lcd);
    g.fillRect (marqueeBox);
    bevel (g, marqueeBox, palette::bevelDark, palette::bevelLight);

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (marqueeBox.reduced (1.0f).toNearestInt());
    g.setColour (palette::green);
    g.setFont (lcdFont (7.0f));

    for (float x = marqueeBox.getX() + 3.0f - marqueeOffset; x < marqueeBox.getRight(); x += juce::jmax (1.0f, marqueeWidth))
        g.drawText (marqueeText, Rectangle<float> (x, marqueeBox.getY(), marqueeWidth + 4.0f, marqueeBox.getHeight()),
                    juce::Justification::centredLeft, false);
}

void ThinampEditor::drawReadouts (Graphics& g)
{
    auto box = [&g] (Rectangle<float> r, const juce::String& value, const char* label)
    {
        g.setColour (palette::lcd);
        g.fillRect (r);
        bevel (g, r, palette::bevelDark, palette::bevelLight);
        g.setColour (palette::green);
        g.setFont (lcdFont (6.5f));
        g.drawText (value, r, juce::Justification::centred, false);
        g.setColour (palette::faceTop);
        g.setFont (lcdFont (5.5f));
        g.drawText (label, Rectangle<float> (r.getRight() + 2, r.getY(), 20, r.getHeight()), juce::Justification::left, false);
    };

    box ({ 111, 39, 22, 9 }, juce::String (ft::fftSize), "fft");
    box ({ 156, 39, 15, 9 }, juce::String (juce::roundToInt (truncator.getDisplaySampleRate() / 1000.0)), "khz");

    const bool stereo = truncator.getTotalNumInputChannels() > 1;
    g.setFont (lcdFont (6.0f));
    g.setColour (stereo ? palette::greenGhost : palette::green);
    g.drawText ("mono", Rectangle<float> (210, 39, 22, 9), juce::Justification::centred, false);
    g.setColour (stereo ? palette::green : palette::greenGhost);
    g.drawText ("stereo", Rectangle<float> (234, 39, 30, 9), juce::Justification::centred, false);

    const int mix = juce::roundToInt (truncator.apvts.getRawParameterValue ("mix")->load() * 100.0f);
    g.setColour (palette::faceTop);
    g.setFont (lcdFont (6.0f));
    g.drawText ("MIX " + juce::String (mix) + "%", Rectangle<float> (219, 56, 46, 11), juce::Justification::centredLeft, false);
}
