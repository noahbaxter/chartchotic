#ifdef DEBUG

#include "DebugTuningPanel.h"

DebugTuningPanel::DebugTuningPanel(juce::ValueTree& state)
    : state(state)
{
    // --- Curvature section ---
    setupSectionHeader(curvatureHeader, "Curvature");

    setupScrollLabel(guitarCurvLabel);
    guitarCurvLabel.onScroll = [this](int delta) {
        guitarCurvature = juce::jlimit(-0.20f, 0.20f, guitarCurvature + delta * 0.002f);
        guitarCurvLabel.setText("Guitar: " + juce::String(guitarCurvature, 3), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(drumCurvLabel);
    drumCurvLabel.onScroll = [this](int delta) {
        drumCurvature = juce::jlimit(-0.20f, 0.20f, drumCurvature + delta * 0.002f);
        drumCurvLabel.setText("Drums: " + juce::String(drumCurvature, 3), juce::dontSendNotification);
        fireChanged();
    };

    // --- Gem scale section ---
    setupSectionHeader(gemScaleHeader, "Gem Scale");

    setupScrollLabel(gemWLabel);
    gemWLabel.onScroll = [this](int delta) {
        gemW = juce::jlimit(0.50f, 2.00f, gemW + delta * 0.01f);
        gemWLabel.setText("Gem W: " + juce::String(gemW, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemHLabel);
    gemHLabel.onScroll = [this](int delta) {
        gemH = juce::jlimit(0.50f, 2.00f, gemH + delta * 0.01f);
        gemHLabel.setText("Gem H: " + juce::String(gemH, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(barWLabel);
    barWLabel.onScroll = [this](int delta) {
        barW = juce::jlimit(0.50f, 2.00f, barW + delta * 0.01f);
        barWLabel.setText("Bar W: " + juce::String(barW, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(barHLabel);
    barHLabel.onScroll = [this](int delta) {
        barH = juce::jlimit(0.50f, 2.00f, barH + delta * 0.01f);
        barHLabel.setText("Bar H: " + juce::String(barH, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemGhostScaleLabel);
    gemGhostScaleLabel.onScroll = [this](int delta) {
        gemGhostScale = juce::jlimit(0.30f, 1.50f, gemGhostScale + delta * 0.01f);
        gemGhostScaleLabel.setText("Ghost: " + juce::String(gemGhostScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemAccentScaleLabel);
    gemAccentScaleLabel.onScroll = [this](int delta) {
        gemAccentScale = juce::jlimit(0.50f, 2.00f, gemAccentScale + delta * 0.01f);
        gemAccentScaleLabel.setText("Accent: " + juce::String(gemAccentScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemHopoScaleLabel);
    gemHopoScaleLabel.onScroll = [this](int delta) {
        gemHopoScale = juce::jlimit(0.30f, 2.00f, gemHopoScale + delta * 0.01f);
        gemHopoScaleLabel.setText("HOPO: " + juce::String(gemHopoScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemTapScaleLabel);
    gemTapScaleLabel.onScroll = [this](int delta) {
        gemTapScale = juce::jlimit(0.30f, 1.50f, gemTapScale + delta * 0.01f);
        gemTapScaleLabel.setText("Tap: " + juce::String(gemTapScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemSpScaleLabel);
    gemSpScaleLabel.onScroll = [this](int delta) {
        gemSpScale = juce::jlimit(0.50f, 2.00f, gemSpScale + delta * 0.01f);
        gemSpScaleLabel.setText("SP: " + juce::String(gemSpScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    // --- Hit animation scale section ---
    setupSectionHeader(hitScaleHeader, "Hit Scale");

    setupScrollLabel(hitGemScaleLabel);
    hitGemScaleLabel.onScroll = [this](int delta) {
        hitGemScale = juce::jlimit(0.50f, 3.00f, hitGemScale + delta * 0.02f);
        hitGemScaleLabel.setText("Note: " + juce::String(hitGemScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitBarScaleLabel);
    hitBarScaleLabel.onScroll = [this](int delta) {
        hitBarScale = juce::jlimit(0.50f, 3.00f, hitBarScale + delta * 0.02f);
        hitBarScaleLabel.setText("Bar: " + juce::String(hitBarScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitGemWLabel);
    hitGemWLabel.onScroll = [this](int delta) {
        hitGemW = juce::jlimit(0.50f, 3.00f, hitGemW + delta * 0.01f);
        hitGemWLabel.setText("Gem W: " + juce::String(hitGemW, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitGemHLabel);
    hitGemHLabel.onScroll = [this](int delta) {
        hitGemH = juce::jlimit(0.50f, 3.00f, hitGemH + delta * 0.01f);
        hitGemHLabel.setText("Gem H: " + juce::String(hitGemH, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitBarWLabel);
    hitBarWLabel.onScroll = [this](int delta) {
        hitBarW = juce::jlimit(0.50f, 3.00f, hitBarW + delta * 0.01f);
        hitBarWLabel.setText("Bar W: " + juce::String(hitBarW, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitBarHLabel);
    hitBarHLabel.onScroll = [this](int delta) {
        hitBarH = juce::jlimit(0.50f, 3.00f, hitBarH + delta * 0.01f);
        hitBarHLabel.setText("Bar H: " + juce::String(hitBarH, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitGhostScaleLabel);
    hitGhostScaleLabel.onScroll = [this](int delta) {
        hitGhostScale = juce::jlimit(0.30f, 1.50f, hitGhostScale + delta * 0.01f);
        hitGhostScaleLabel.setText("Ghost: " + juce::String(hitGhostScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitAccentScaleLabel);
    hitAccentScaleLabel.onScroll = [this](int delta) {
        hitAccentScale = juce::jlimit(0.50f, 2.00f, hitAccentScale + delta * 0.01f);
        hitAccentScaleLabel.setText("Accent: " + juce::String(hitAccentScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitHopoScaleLabel);
    hitHopoScaleLabel.onScroll = [this](int delta) {
        hitHopoScale = juce::jlimit(0.30f, 2.00f, hitHopoScale + delta * 0.01f);
        hitHopoScaleLabel.setText("HOPO: " + juce::String(hitHopoScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitTapScaleLabel);
    hitTapScaleLabel.onScroll = [this](int delta) {
        hitTapScale = juce::jlimit(0.30f, 1.50f, hitTapScale + delta * 0.01f);
        hitTapScaleLabel.setText("Tap: " + juce::String(hitTapScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(hitSpScaleLabel);
    hitSpScaleLabel.onScroll = [this](int delta) {
        hitSpScale = juce::jlimit(0.50f, 2.00f, hitSpScale + delta * 0.01f);
        hitSpScaleLabel.setText("SP: " + juce::String(hitSpScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(spWhiteFlareLabel);
    spWhiteFlareLabel.onScroll = [this](int) {
        spWhiteFlare = !spWhiteFlare;
        spWhiteFlareLabel.setText(juce::String("SP White: ") + (spWhiteFlare ? "ON" : "OFF"), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(tapPurpleFlareLabel);
    tapPurpleFlareLabel.onScroll = [this](int) {
        tapPurpleFlare = !tapPurpleFlare;
        tapPurpleFlareLabel.setText(juce::String("Tap Purple:") + (tapPurpleFlare ? "ON" : "OFF"), juce::dontSendNotification);
        fireChanged();
    };

    // --- Guitar Z offsets ---
    setupSectionHeader(guitarHeader, "Guitar Z");

    auto makeZScroll = [this](ScrollableLabel& label, float& val, const juce::String& name) {
        setupScrollLabel(label);
        label.onScroll = [this, &val, &label, name](int delta) {
            val = juce::jlimit(-50.0f, 50.0f, val + delta * 0.5f);
            label.setText(name + ": " + juce::String(val, 1), juce::dontSendNotification);
            fireChanged();
        };
    };

    auto makeStrikeScroll = [this](ScrollableLabel& label, float& val, const juce::String& name) {
        setupScrollLabel(label);
        label.onScroll = [this, &val, &label, name](int delta) {
            val = juce::jlimit(-0.10f, 0.10f, val + delta * 0.002f);
            label.setText(name + ": " + juce::String(val, 3), juce::dontSendNotification);
            fireChanged();
        };
    };

    makeZScroll(gGridZLabel, gGridZ, "Grid");
    makeZScroll(gGemZLabel, gGemZ, "Note");
    makeZScroll(gBarZLabel, gBarZ, "Bar");
    makeZScroll(gHitGemZLabel, gHitGemZ, "Hit Note");
    makeZScroll(gHitBarZLabel, gHitBarZ, "Hit Bar");
    makeStrikeScroll(gStrikePosGemLabel, gStrikePosGem, "Strike Note");
    makeStrikeScroll(gStrikePosBarLabel, gStrikePosBar, "Strike Bar");

    // --- Drum Z offsets ---
    setupSectionHeader(drumHeader, "Drum Z");

    makeZScroll(dGridZLabel, dGridZ, "Grid");
    makeZScroll(dGemZLabel, dGemZ, "Note");
    makeZScroll(dBarZLabel, dBarZ, "Bar");
    makeZScroll(dHitGemZLabel, dHitGemZ, "Hit Note");
    makeZScroll(dHitBarZLabel, dHitBarZ, "Hit Bar");
    makeStrikeScroll(dStrikePosGemLabel, dStrikePosGem, "Strike Note");
    makeStrikeScroll(dStrikePosBarLabel, dStrikePosBar, "Strike Bar");

    const char* drumColNames[] = {"Red", "Yellow", "Blue", "Green", "Kick"};
    for (int i = 0; i < 5; i++)
    {
        setupScrollLabel(drumColLabels[i]);
        drumColLabels[i].onScroll = [this, i, drumColNames](int delta) {
            drumZ[i] = juce::jlimit(-30.0f, 30.0f, drumZ[i] + delta * 0.5f);
            drumColLabels[i].setText(juce::String(drumColNames[i]) + ": " + juce::String(drumZ[i], 1), juce::dontSendNotification);
            fireChanged();
        };
    }

    refreshLabels();

    // Register panel children
    tuningButton.addPanelChild(&curvatureHeader);
    tuningButton.addPanelChild(&guitarCurvLabel);
    tuningButton.addPanelChild(&drumCurvLabel);

    tuningButton.addPanelChild(&gemScaleHeader);
    tuningButton.addPanelChild(&gemWLabel);
    tuningButton.addPanelChild(&gemHLabel);
    tuningButton.addPanelChild(&barWLabel);
    tuningButton.addPanelChild(&barHLabel);
    tuningButton.addPanelChild(&gemGhostScaleLabel);
    tuningButton.addPanelChild(&gemAccentScaleLabel);
    tuningButton.addPanelChild(&gemHopoScaleLabel);
    tuningButton.addPanelChild(&gemTapScaleLabel);
    tuningButton.addPanelChild(&gemSpScaleLabel);

    tuningButton.addPanelChild(&hitScaleHeader);
    tuningButton.addPanelChild(&hitGemScaleLabel);
    tuningButton.addPanelChild(&hitBarScaleLabel);
    tuningButton.addPanelChild(&hitGemWLabel);
    tuningButton.addPanelChild(&hitGemHLabel);
    tuningButton.addPanelChild(&hitBarWLabel);
    tuningButton.addPanelChild(&hitBarHLabel);
    tuningButton.addPanelChild(&hitGhostScaleLabel);
    tuningButton.addPanelChild(&hitAccentScaleLabel);
    tuningButton.addPanelChild(&hitHopoScaleLabel);
    tuningButton.addPanelChild(&hitTapScaleLabel);
    tuningButton.addPanelChild(&hitSpScaleLabel);
    tuningButton.addPanelChild(&spWhiteFlareLabel);
    tuningButton.addPanelChild(&tapPurpleFlareLabel);

    tuningButton.addPanelChild(&guitarHeader);
    tuningButton.addPanelChild(&gGridZLabel);
    tuningButton.addPanelChild(&gGemZLabel);
    tuningButton.addPanelChild(&gBarZLabel);
    tuningButton.addPanelChild(&gHitGemZLabel);
    tuningButton.addPanelChild(&gHitBarZLabel);
    tuningButton.addPanelChild(&gStrikePosGemLabel);
    tuningButton.addPanelChild(&gStrikePosBarLabel);

    tuningButton.addPanelChild(&drumHeader);
    tuningButton.addPanelChild(&dGridZLabel);
    tuningButton.addPanelChild(&dGemZLabel);
    tuningButton.addPanelChild(&dBarZLabel);
    tuningButton.addPanelChild(&dHitGemZLabel);
    tuningButton.addPanelChild(&dHitBarZLabel);
    tuningButton.addPanelChild(&dStrikePosGemLabel);
    tuningButton.addPanelChild(&dStrikePosBarLabel);
    for (int i = 0; i < 5; i++)
        tuningButton.addPanelChild(&drumColLabels[i]);

    tuningButton.setPanelSize(170, 200);
    tuningButton.onLayoutPanel = [this](juce::Component* panel) { layoutPanel(panel); };
}

DebugTuningPanel::~DebugTuningPanel()
{
    tuningButton.dismissPanel();
}

void DebugTuningPanel::fireChanged()
{
    if (onChanged) onChanged();
}

void DebugTuningPanel::refreshLabels()
{
    guitarCurvLabel.setText("Guitar: " + juce::String(guitarCurvature, 3), juce::dontSendNotification);
    drumCurvLabel.setText("Drums: " + juce::String(drumCurvature, 3), juce::dontSendNotification);

    gemWLabel.setText("Gem W: " + juce::String(gemW, 2), juce::dontSendNotification);
    gemHLabel.setText("Gem H: " + juce::String(gemH, 2), juce::dontSendNotification);
    barWLabel.setText("Bar W: " + juce::String(barW, 2), juce::dontSendNotification);
    barHLabel.setText("Bar H: " + juce::String(barH, 2), juce::dontSendNotification);
    hitGemScaleLabel.setText("Note: " + juce::String(hitGemScale, 2), juce::dontSendNotification);
    hitBarScaleLabel.setText("Bar: " + juce::String(hitBarScale, 2), juce::dontSendNotification);
    hitGemWLabel.setText("Gem W: " + juce::String(hitGemW, 2), juce::dontSendNotification);
    hitGemHLabel.setText("Gem H: " + juce::String(hitGemH, 2), juce::dontSendNotification);
    hitBarWLabel.setText("Bar W: " + juce::String(hitBarW, 2), juce::dontSendNotification);
    hitBarHLabel.setText("Bar H: " + juce::String(hitBarH, 2), juce::dontSendNotification);
    hitGhostScaleLabel.setText("Ghost: " + juce::String(hitGhostScale, 2), juce::dontSendNotification);
    hitAccentScaleLabel.setText("Accent: " + juce::String(hitAccentScale, 2), juce::dontSendNotification);
    hitHopoScaleLabel.setText("HOPO: " + juce::String(hitHopoScale, 2), juce::dontSendNotification);
    hitTapScaleLabel.setText("Tap: " + juce::String(hitTapScale, 2), juce::dontSendNotification);
    hitSpScaleLabel.setText("SP: " + juce::String(hitSpScale, 2), juce::dontSendNotification);
    spWhiteFlareLabel.setText(juce::String("SP White: ") + (spWhiteFlare ? "ON" : "OFF"), juce::dontSendNotification);
    tapPurpleFlareLabel.setText(juce::String("Tap Purple:") + (tapPurpleFlare ? "ON" : "OFF"), juce::dontSendNotification);
    gemGhostScaleLabel.setText("Ghost: " + juce::String(gemGhostScale, 2), juce::dontSendNotification);
    gemAccentScaleLabel.setText("Accent: " + juce::String(gemAccentScale, 2), juce::dontSendNotification);
    gemHopoScaleLabel.setText("HOPO: " + juce::String(gemHopoScale, 2), juce::dontSendNotification);
    gemTapScaleLabel.setText("Tap: " + juce::String(gemTapScale, 2), juce::dontSendNotification);
    gemSpScaleLabel.setText("SP: " + juce::String(gemSpScale, 2), juce::dontSendNotification);

    gGridZLabel.setText("Grid: " + juce::String(gGridZ, 1), juce::dontSendNotification);
    gGemZLabel.setText("Note: " + juce::String(gGemZ, 1), juce::dontSendNotification);
    gBarZLabel.setText("Bar: " + juce::String(gBarZ, 1), juce::dontSendNotification);
    gHitGemZLabel.setText("Hit Note: " + juce::String(gHitGemZ, 1), juce::dontSendNotification);
    gHitBarZLabel.setText("Hit Bar: " + juce::String(gHitBarZ, 1), juce::dontSendNotification);
    gStrikePosGemLabel.setText("Strike Note: " + juce::String(gStrikePosGem, 3), juce::dontSendNotification);
    gStrikePosBarLabel.setText("Strike Bar: " + juce::String(gStrikePosBar, 3), juce::dontSendNotification);

    dGridZLabel.setText("Grid: " + juce::String(dGridZ, 1), juce::dontSendNotification);
    dGemZLabel.setText("Note: " + juce::String(dGemZ, 1), juce::dontSendNotification);
    dBarZLabel.setText("Bar: " + juce::String(dBarZ, 1), juce::dontSendNotification);
    dHitGemZLabel.setText("Hit Note: " + juce::String(dHitGemZ, 1), juce::dontSendNotification);
    dHitBarZLabel.setText("Hit Bar: " + juce::String(dHitBarZ, 1), juce::dontSendNotification);
    dStrikePosGemLabel.setText("Strike Note: " + juce::String(dStrikePosGem, 3), juce::dontSendNotification);
    dStrikePosBarLabel.setText("Strike Bar: " + juce::String(dStrikePosBar, 3), juce::dontSendNotification);

    const char* drumColNames[] = {"Red", "Yellow", "Blue", "Green", "Kick"};
    for (int i = 0; i < 5; i++)
        drumColLabels[i].setText(juce::String(drumColNames[i]) + ": " + juce::String(drumZ[i], 1), juce::dontSendNotification);
}

void DebugTuningPanel::setupSectionHeader(SectionHeader& header, const juce::String& text)
{
    header.setTitle(text);
    header.setJustificationType(juce::Justification::centred);
    header.setColour(juce::Label::textColourId, juce::Colours::grey);
    header.setFont(juce::Font(11.0f));
    header.setInterceptsMouseClicks(true, true);
    header.onToggle = [this]() {
        if (tuningButton.isPanelVisible())
        {
            tuningButton.dismissPanel();
            tuningButton.showPanel();
        }
    };
}

void DebugTuningPanel::setupScrollLabel(ScrollableLabel& label)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setInterceptsMouseClicks(true, true);
}

void DebugTuningPanel::layoutPanel(juce::Component* panel)
{
    const int margin = 8;
    const int rowHeight = 22;
    const int gap = 3;
    const int headerGap = 6;
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    auto layoutRow = [&](juce::Component& comp, bool visible) {
        comp.setVisible(visible);
        if (visible)
        {
            comp.setBounds(margin, y, w, rowHeight);
            y += rowHeight + gap;
        }
    };

    // Curvature
    curvatureHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    layoutRow(guitarCurvLabel, curvatureHeader.expanded);
    layoutRow(drumCurvLabel, curvatureHeader.expanded);
    y += headerGap;

    // Gem Scale
    gemScaleHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    layoutRow(gemWLabel, gemScaleHeader.expanded);
    layoutRow(gemHLabel, gemScaleHeader.expanded);
    layoutRow(barWLabel, gemScaleHeader.expanded);
    layoutRow(barHLabel, gemScaleHeader.expanded);
    layoutRow(gemGhostScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemAccentScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemHopoScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemTapScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemSpScaleLabel, gemScaleHeader.expanded);
    y += headerGap;

    // Hit Scale
    hitScaleHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    layoutRow(hitGemScaleLabel, hitScaleHeader.expanded);
    layoutRow(hitBarScaleLabel, hitScaleHeader.expanded);
    layoutRow(hitGemWLabel, hitScaleHeader.expanded);
    layoutRow(hitGemHLabel, hitScaleHeader.expanded);
    layoutRow(hitBarWLabel, hitScaleHeader.expanded);
    layoutRow(hitBarHLabel, hitScaleHeader.expanded);
    layoutRow(hitGhostScaleLabel, hitScaleHeader.expanded);
    layoutRow(hitAccentScaleLabel, hitScaleHeader.expanded);
    layoutRow(hitHopoScaleLabel, hitScaleHeader.expanded);
    layoutRow(hitTapScaleLabel, hitScaleHeader.expanded);
    layoutRow(hitSpScaleLabel, hitScaleHeader.expanded);
    layoutRow(spWhiteFlareLabel, hitScaleHeader.expanded);
    layoutRow(tapPurpleFlareLabel, hitScaleHeader.expanded);
    y += headerGap;

    // Guitar Z
    guitarHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    layoutRow(gGridZLabel, guitarHeader.expanded);
    layoutRow(gGemZLabel, guitarHeader.expanded);
    layoutRow(gBarZLabel, guitarHeader.expanded);
    layoutRow(gHitGemZLabel, guitarHeader.expanded);
    layoutRow(gHitBarZLabel, guitarHeader.expanded);
    layoutRow(gStrikePosGemLabel, guitarHeader.expanded);
    layoutRow(gStrikePosBarLabel, guitarHeader.expanded);
    y += headerGap;

    // Drum Z
    drumHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    layoutRow(dGridZLabel, drumHeader.expanded);
    layoutRow(dGemZLabel, drumHeader.expanded);
    layoutRow(dBarZLabel, drumHeader.expanded);
    layoutRow(dHitGemZLabel, drumHeader.expanded);
    layoutRow(dHitBarZLabel, drumHeader.expanded);
    layoutRow(dStrikePosGemLabel, drumHeader.expanded);
    layoutRow(dStrikePosBarLabel, drumHeader.expanded);
    for (int i = 0; i < 5; i++)
        layoutRow(drumColLabels[i], drumHeader.expanded);

    panel->setSize(panel->getWidth(), y + margin);
}

#endif
