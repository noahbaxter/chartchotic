#ifdef DEBUG

#include "DebugTuningPanel.h"
#include "../Visual/Renderers/SceneRenderer.h"

DebugTuningPanel::DebugTuningPanel(juce::ValueTree& state)
    : state(state)
{
    // --- Layers section ---
    setupSectionHeader(layersHeader, "Layers");

    for (int i = 0; i < NUM_LAYERS; i++)
    {
        juce::String name(layerNames[i]);

        setupScrollLabel(layerScaleLabels[i]);
        layerScaleLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].scale = juce::jlimit(0.10f, 3.00f, layerStates[i].scale + delta * 0.005f);
            layerScaleLabels[i].setText(name + " S: " + juce::String(layerStates[i].scale, 3), juce::dontSendNotification);
            fireLayer(i);
        };

        setupScrollLabel(layerXLabels[i]);
        layerXLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].xOffset = juce::jlimit(-1.0f, 1.0f, layerStates[i].xOffset + delta * 0.0005f);
            layerXLabels[i].setText(name + " X: " + juce::String(layerStates[i].xOffset, 4), juce::dontSendNotification);
            fireLayer(i);
        };

        setupScrollLabel(layerYLabels[i]);
        layerYLabels[i].onScroll = [this, i, name](int delta) {
            layerStates[i].yOffset = juce::jlimit(-1.0f, 1.0f, layerStates[i].yOffset + delta * 0.0005f);
            layerYLabels[i].setText(name + " Y: " + juce::String(layerStates[i].yOffset, 4), juce::dontSendNotification);
            fireLayer(i);
        };
    }

    // --- Tiling section ---
    setupSectionHeader(tilingHeader, "Tiling");

    setupScrollLabel(tileStepLabel);
    tileStepLabel.onScroll = [this](int delta) {
        tileStepValue = juce::jlimit(0.30f, 1.00f, tileStepValue + delta * 0.005f);
        tileStepLabel.setText("Step: " + juce::String(tileStepValue, 3), juce::dontSendNotification);
        if (onTileStepChanged) onTileStepChanged(tileStepValue);
    };

    setupScrollLabel(tileScaleStepLabel);
    tileScaleStepLabel.onScroll = [this](int delta) {
        tileScaleStepValue = juce::jlimit(0.30f, 1.00f, tileScaleStepValue + delta * 0.005f);
        tileScaleStepLabel.setText("Scale: " + juce::String(tileScaleStepValue, 3), juce::dontSendNotification);
        if (onTileScaleStepChanged) onTileScaleStepChanged(tileScaleStepValue);
    };

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

    setupScrollLabel(gemNoteScaleLabel);
    gemNoteScaleLabel.onScroll = [this](int delta) {
        gemNoteScale = juce::jlimit(0.30f, 2.00f, gemNoteScale + delta * 0.01f);
        gemNoteScaleLabel.setText("Note: " + juce::String(gemNoteScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemHopoScaleLabel);
    gemHopoScaleLabel.onScroll = [this](int delta) {
        gemHopoScale = juce::jlimit(0.30f, 2.00f, gemHopoScale + delta * 0.01f);
        gemHopoScaleLabel.setText("HOPO: " + juce::String(gemHopoScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemHopoBaseScaleLabel);
    gemHopoBaseScaleLabel.onScroll = [this](int delta) {
        gemHopoBaseScale = juce::jlimit(0.30f, 2.00f, gemHopoBaseScale + delta * 0.01f);
        gemHopoBaseScaleLabel.setText("HOPO Base: " + juce::String(gemHopoBaseScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemTapOverlayScaleLabel);
    gemTapOverlayScaleLabel.onScroll = [this](int delta) {
        gemTapOverlayScale = juce::jlimit(0.30f, 2.00f, gemTapOverlayScale + delta * 0.01f);
        gemTapOverlayScaleLabel.setText("Tap Ovr: " + juce::String(gemTapOverlayScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemGhostOverlayScaleLabel);
    gemGhostOverlayScaleLabel.onScroll = [this](int delta) {
        gemGhostOverlayScale = juce::jlimit(0.30f, 2.00f, gemGhostOverlayScale + delta * 0.01f);
        gemGhostOverlayScaleLabel.setText("Ghost Ovr: " + juce::String(gemGhostOverlayScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemAccentOverlayScaleLabel);
    gemAccentOverlayScaleLabel.onScroll = [this](int delta) {
        gemAccentOverlayScale = juce::jlimit(0.30f, 2.00f, gemAccentOverlayScale + delta * 0.01f);
        gemAccentOverlayScaleLabel.setText("Accent Ovr: " + juce::String(gemAccentOverlayScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemNoteBaseScaleLabel);
    gemNoteBaseScaleLabel.onScroll = [this](int delta) {
        gemNoteBaseScale = juce::jlimit(0.30f, 2.00f, gemNoteBaseScale + delta * 0.01f);
        gemNoteBaseScaleLabel.setText("Note Base: " + juce::String(gemNoteBaseScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemCymScaleLabel);
    gemCymScaleLabel.onScroll = [this](int delta) {
        gemCymScale = juce::jlimit(0.30f, 2.00f, gemCymScale + delta * 0.01f);
        gemCymScaleLabel.setText("Cym: " + juce::String(gemCymScale, 2), juce::dontSendNotification);
        fireChanged();
    };

    setupScrollLabel(gemCymBaseScaleLabel);
    gemCymBaseScaleLabel.onScroll = [this](int delta) {
        gemCymBaseScale = juce::jlimit(0.30f, 2.00f, gemCymBaseScale + delta * 0.01f);
        gemCymBaseScaleLabel.setText("Cym Base: " + juce::String(gemCymBaseScale, 2), juce::dontSendNotification);
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

    // --- Guitar Note X Offsets section ---
    setupSectionHeader(guitarXOffHeader, "Note X Offset");

    for (int i = 0; i < 6; i++)
    {
        juce::String name(guitarXOffNames[i]);
        setupScrollLabel(guitarXOffLabels[i]);
        guitarXOffLabels[i].onScroll = [this, i, name](int delta) {
            guitarXOff[i] = juce::jlimit(-30.0f, 30.0f, guitarXOff[i] + delta * 0.5f);
            guitarXOffLabels[i].setText(name + " X1:" + juce::String(guitarXOff[i], 1), juce::dontSendNotification);
            fireChanged();
        };
        setupScrollLabel(guitarXOff2Labels[i]);
        guitarXOff2Labels[i].onScroll = [this, i, name](int delta) {
            guitarXOff2[i] = juce::jlimit(-30.0f, 30.0f, guitarXOff2[i] + delta * 0.5f);
            guitarXOff2Labels[i].setText(name + " X2:" + juce::String(guitarXOff2[i], 1), juce::dontSendNotification);
            fireChanged();
        };
    }

    // --- Drum Note X Offsets section ---
    setupSectionHeader(drumXOffHeader, "Drum X Offset");

    for (int i = 0; i < 5; i++)
    {
        juce::String name(drumXOffNames[i]);
        setupScrollLabel(drumXOffLabels[i]);
        drumXOffLabels[i].onScroll = [this, i, name](int delta) {
            drumXOff[i] = juce::jlimit(-30.0f, 30.0f, drumXOff[i] + delta * 0.5f);
            drumXOffLabels[i].setText(name + " X1:" + juce::String(drumXOff[i], 1), juce::dontSendNotification);
            fireChanged();
        };
        setupScrollLabel(drumXOff2Labels[i]);
        drumXOff2Labels[i].onScroll = [this, i, name](int delta) {
            drumXOff2[i] = juce::jlimit(-30.0f, 30.0f, drumXOff2[i] + delta * 0.5f);
            drumXOff2Labels[i].setText(name + " X2:" + juce::String(drumXOff2[i], 1), juce::dontSendNotification);
            fireChanged();
        };
    }

    // --- Guitar Lanes section ---
    setupSectionHeader(guitarLanesHeader, "Guitar Lanes");
    std::copy_n(PositionConstants::guitarBezierLaneCoords, GUITAR_LANES, guitarLaneCoords);

    for (int i = 0; i < GUITAR_LANES; i++)
    {
        juce::String name(guitarLaneNames[i]);

        setupScrollLabel(gLaneXLabels[i]);
        gLaneXLabels[i].onScroll = [this, i, name](int delta) {
            guitarLaneCoords[i].normX1 = juce::jlimit(0.0f, 1.0f, guitarLaneCoords[i].normX1 + delta * 0.001f);
            gLaneXLabels[i].setText(name + " X1:" + juce::String(guitarLaneCoords[i].normX1, 3), juce::dontSendNotification);
            fireLaneChanged();
        };

        setupScrollLabel(gLaneX2Labels[i]);
        gLaneX2Labels[i].onScroll = [this, i, name](int delta) {
            guitarLaneCoords[i].normX2 = juce::jlimit(0.0f, 1.0f, guitarLaneCoords[i].normX2 + delta * 0.001f);
            gLaneX2Labels[i].setText(name + " X2:" + juce::String(guitarLaneCoords[i].normX2, 3), juce::dontSendNotification);
            fireLaneChanged();
        };

        setupScrollLabel(gLaneWLabels[i]);
        gLaneWLabels[i].onScroll = [this, i, name](int delta) {
            guitarLaneCoords[i].normWidth1 = juce::jlimit(0.0f, 1.0f, guitarLaneCoords[i].normWidth1 + delta * 0.001f);
            gLaneWLabels[i].setText(name + " W1:" + juce::String(guitarLaneCoords[i].normWidth1, 3), juce::dontSendNotification);
            fireLaneChanged();
        };

        setupScrollLabel(gLaneW2Labels[i]);
        gLaneW2Labels[i].onScroll = [this, i, name](int delta) {
            guitarLaneCoords[i].normWidth2 = juce::jlimit(0.0f, 1.0f, guitarLaneCoords[i].normWidth2 + delta * 0.001f);
            gLaneW2Labels[i].setText(name + " W2:" + juce::String(guitarLaneCoords[i].normWidth2, 3), juce::dontSendNotification);
            fireLaneChanged();
        };
    }

    // --- Drum Lanes section ---
    setupSectionHeader(drumLanesHeader, "Drum Lanes");
    std::copy_n(PositionConstants::drumBezierLaneCoords, DRUM_LANES, drumLaneCoords);

    for (int i = 0; i < DRUM_LANES; i++)
    {
        juce::String name(drumLaneNames[i]);

        setupScrollLabel(dLaneXLabels[i]);
        dLaneXLabels[i].onScroll = [this, i, name](int delta) {
            drumLaneCoords[i].normX1 = juce::jlimit(0.0f, 1.0f, drumLaneCoords[i].normX1 + delta * 0.001f);
            dLaneXLabels[i].setText(name + " X1:" + juce::String(drumLaneCoords[i].normX1, 3), juce::dontSendNotification);
            fireLaneChanged();
        };

        setupScrollLabel(dLaneX2Labels[i]);
        dLaneX2Labels[i].onScroll = [this, i, name](int delta) {
            drumLaneCoords[i].normX2 = juce::jlimit(0.0f, 1.0f, drumLaneCoords[i].normX2 + delta * 0.001f);
            dLaneX2Labels[i].setText(name + " X2:" + juce::String(drumLaneCoords[i].normX2, 3), juce::dontSendNotification);
            fireLaneChanged();
        };

        setupScrollLabel(dLaneWLabels[i]);
        dLaneWLabels[i].onScroll = [this, i, name](int delta) {
            drumLaneCoords[i].normWidth1 = juce::jlimit(0.0f, 1.0f, drumLaneCoords[i].normWidth1 + delta * 0.001f);
            dLaneWLabels[i].setText(name + " W1:" + juce::String(drumLaneCoords[i].normWidth1, 3), juce::dontSendNotification);
            fireLaneChanged();
        };

        setupScrollLabel(dLaneW2Labels[i]);
        dLaneW2Labels[i].onScroll = [this, i, name](int delta) {
            drumLaneCoords[i].normWidth2 = juce::jlimit(0.0f, 1.0f, drumLaneCoords[i].normWidth2 + delta * 0.001f);
            dLaneW2Labels[i].setText(name + " W2:" + juce::String(drumLaneCoords[i].normWidth2, 3), juce::dontSendNotification);
            fireLaneChanged();
        };
    }

    refreshLabels();
    refreshLayerLabels();
    refreshLaneLabels();

    // Register panel children — layers + tiling first
    tuningButton.addPanelChild(&layersHeader);
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        tuningButton.addPanelChild(&layerScaleLabels[i]);
        tuningButton.addPanelChild(&layerXLabels[i]);
        tuningButton.addPanelChild(&layerYLabels[i]);
    }

    tuningButton.addPanelChild(&tilingHeader);
    tuningButton.addPanelChild(&tileStepLabel);
    tuningButton.addPanelChild(&tileScaleStepLabel);

    // Then existing tuning sections
    tuningButton.addPanelChild(&curvatureHeader);
    tuningButton.addPanelChild(&guitarCurvLabel);
    tuningButton.addPanelChild(&drumCurvLabel);

    tuningButton.addPanelChild(&gemScaleHeader);
    tuningButton.addPanelChild(&gemWLabel);
    tuningButton.addPanelChild(&gemHLabel);
    tuningButton.addPanelChild(&barWLabel);
    tuningButton.addPanelChild(&barHLabel);
    tuningButton.addPanelChild(&gemNoteScaleLabel);
    tuningButton.addPanelChild(&gemHopoScaleLabel);
    tuningButton.addPanelChild(&gemHopoBaseScaleLabel);
    tuningButton.addPanelChild(&gemTapOverlayScaleLabel);
    tuningButton.addPanelChild(&gemGhostOverlayScaleLabel);
    tuningButton.addPanelChild(&gemAccentOverlayScaleLabel);
    tuningButton.addPanelChild(&gemNoteBaseScaleLabel);
    tuningButton.addPanelChild(&gemCymScaleLabel);
    tuningButton.addPanelChild(&gemCymBaseScaleLabel);
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

    tuningButton.addPanelChild(&guitarXOffHeader);
    for (int i = 0; i < 6; i++) {
        tuningButton.addPanelChild(&guitarXOffLabels[i]);
        tuningButton.addPanelChild(&guitarXOff2Labels[i]);
    }

    tuningButton.addPanelChild(&drumXOffHeader);
    for (int i = 0; i < 5; i++) {
        tuningButton.addPanelChild(&drumXOffLabels[i]);
        tuningButton.addPanelChild(&drumXOff2Labels[i]);
    }

    tuningButton.addPanelChild(&guitarLanesHeader);
    for (int i = 0; i < GUITAR_LANES; i++)
    {
        tuningButton.addPanelChild(&gLaneXLabels[i]);
        tuningButton.addPanelChild(&gLaneX2Labels[i]);
        tuningButton.addPanelChild(&gLaneWLabels[i]);
        tuningButton.addPanelChild(&gLaneW2Labels[i]);
    }

    tuningButton.addPanelChild(&drumLanesHeader);
    for (int i = 0; i < DRUM_LANES; i++)
    {
        tuningButton.addPanelChild(&dLaneXLabels[i]);
        tuningButton.addPanelChild(&dLaneX2Labels[i]);
        tuningButton.addPanelChild(&dLaneWLabels[i]);
        tuningButton.addPanelChild(&dLaneW2Labels[i]);
    }

    tuningButton.setPanelSize(170, 200);
    tuningButton.onLayoutPanel = [this](juce::Component* panel) { layoutPanel(panel); };
}

DebugTuningPanel::~DebugTuningPanel()
{
    tuningButton.dismissPanel();
}

void DebugTuningPanel::applyTo(SceneRenderer& sr) const
{
    sr.noteCurvatureGuitar = guitarCurvature;
    sr.noteCurvatureDrums = drumCurvature;
    sr.gemWidthScale = gemW;
    sr.gemHeightScale = gemH;
    sr.barWidthScale = barW;
    sr.barHeightScale = barH;
    sr.hitGemScale = hitGemScale;
    sr.hitBarScale = hitBarScale;
    sr.hitGemWidthScale = hitGemW;
    sr.hitGemHeightScale = hitGemH;
    sr.hitBarWidthScale = hitBarW;
    sr.hitBarHeightScale = hitBarH;
    sr.hitGhostScale = hitGhostScale;
    sr.hitAccentScale = hitAccentScale;
    sr.hitHopoScale = hitHopoScale;
    sr.hitTapScale = hitTapScale;
    sr.hitSpScale = hitSpScale;
    sr.spWhiteFlare = spWhiteFlare;
    sr.tapPurpleFlare = tapPurpleFlare;
    sr.gemNoteScale = gemNoteScale;
    sr.gemHopoScale = gemHopoScale;
    sr.gemHopoBaseScale = gemHopoBaseScale;
    sr.gemTapOverlayScale = gemTapOverlayScale;
    sr.gemGhostOverlayScale = gemGhostOverlayScale;
    sr.gemAccentOverlayScale = gemAccentOverlayScale;
    sr.gemNoteBaseScale = gemNoteBaseScale;
    sr.gemCymScale = gemCymScale;
    sr.gemCymBaseScale = gemCymBaseScale;
    sr.gemSpScale = gemSpScale;

    sr.gridZOffsetGuitar = gGridZ;
    sr.gemZOffsetGuitar = gGemZ;
    sr.barZOffsetGuitar = gBarZ;
    sr.hitGemZOffsetGuitar = gHitGemZ;
    sr.hitBarZOffsetGuitar = gHitBarZ;
    sr.strikePosGemGuitar = gStrikePosGem;
    sr.strikePosBarGuitar = gStrikePosBar;

    sr.gridZOffsetDrums = dGridZ;
    sr.gemZOffsetDrums = dGemZ;
    sr.barZOffsetDrums = dBarZ;
    sr.hitGemZOffsetDrums = dHitGemZ;
    sr.hitBarZOffsetDrums = dHitBarZ;
    sr.strikePosGemDrums = dStrikePosGem;
    sr.strikePosBarDrums = dStrikePosBar;

    std::copy(std::begin(drumZ), std::end(drumZ), sr.drumColZOffsets);
    std::copy(std::begin(guitarXOff), std::end(guitarXOff), sr.guitarGemXOffsets);
    std::copy(std::begin(guitarXOff2), std::end(guitarXOff2), sr.guitarGemXOffsets2);
    std::copy(std::begin(drumXOff), std::end(drumXOff), sr.drumGemXOffsets);
    std::copy(std::begin(drumXOff2), std::end(drumXOff2), sr.drumGemXOffsets2);

    std::copy_n(guitarLaneCoords, GUITAR_LANES, sr.guitarLaneCoordsLocal);
    std::copy_n(drumLaneCoords, DRUM_LANES, sr.drumLaneCoordsLocal);
}

void DebugTuningPanel::initDefaults(const TrackRenderer& trackRenderer)
{
    std::copy_n(trackRenderer.layersGuitar, NUM_LAYERS, guitarStates);
    std::copy_n(trackRenderer.layersDrums, NUM_LAYERS, drumStates);
    tileStepValue = trackRenderer.tileStep;
    tileScaleStepValue = trackRenderer.tileScaleStep;
    refreshLayerLabels();
}

void DebugTuningPanel::setDrums(bool isDrums)
{
    layerStates = isDrums ? drumStates : guitarStates;
    refreshLayerLabels();
}

void DebugTuningPanel::fireLayer(int idx)
{
    if (onLayerChanged)
        onLayerChanged(idx, layerStates[idx].scale, layerStates[idx].xOffset, layerStates[idx].yOffset);
}

void DebugTuningPanel::refreshLayerLabels()
{
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        juce::String name(layerNames[i]);
        layerScaleLabels[i].setText(name + " S: " + juce::String(layerStates[i].scale, 3), juce::dontSendNotification);
        layerXLabels[i].setText(name + " X: " + juce::String(layerStates[i].xOffset, 4), juce::dontSendNotification);
        layerYLabels[i].setText(name + " Y: " + juce::String(layerStates[i].yOffset, 4), juce::dontSendNotification);
    }
    tileStepLabel.setText("Step: " + juce::String(tileStepValue, 3), juce::dontSendNotification);
    tileScaleStepLabel.setText("Scale: " + juce::String(tileScaleStepValue, 3), juce::dontSendNotification);
}

void DebugTuningPanel::refreshLaneLabels()
{
    for (int i = 0; i < GUITAR_LANES; i++)
    {
        juce::String name(guitarLaneNames[i]);
        gLaneXLabels[i].setText(name + " X1:" + juce::String(guitarLaneCoords[i].normX1, 3), juce::dontSendNotification);
        gLaneX2Labels[i].setText(name + " X2:" + juce::String(guitarLaneCoords[i].normX2, 3), juce::dontSendNotification);
        gLaneWLabels[i].setText(name + " W1:" + juce::String(guitarLaneCoords[i].normWidth1, 3), juce::dontSendNotification);
        gLaneW2Labels[i].setText(name + " W2:" + juce::String(guitarLaneCoords[i].normWidth2, 3), juce::dontSendNotification);
    }
    for (int i = 0; i < DRUM_LANES; i++)
    {
        juce::String name(drumLaneNames[i]);
        dLaneXLabels[i].setText(name + " X1:" + juce::String(drumLaneCoords[i].normX1, 3), juce::dontSendNotification);
        dLaneX2Labels[i].setText(name + " X2:" + juce::String(drumLaneCoords[i].normX2, 3), juce::dontSendNotification);
        dLaneWLabels[i].setText(name + " W1:" + juce::String(drumLaneCoords[i].normWidth1, 3), juce::dontSendNotification);
        dLaneW2Labels[i].setText(name + " W2:" + juce::String(drumLaneCoords[i].normWidth2, 3), juce::dontSendNotification);
    }
}

void DebugTuningPanel::fireLaneChanged()
{
    if (onLaneCoordsChanged) onLaneCoordsChanged();
}

void DebugTuningPanel::fireChanged()
{
    if (onTuningChanged) onTuningChanged();
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
    gemNoteScaleLabel.setText("Note: " + juce::String(gemNoteScale, 2), juce::dontSendNotification);
    gemHopoScaleLabel.setText("HOPO: " + juce::String(gemHopoScale, 2), juce::dontSendNotification);
    gemHopoBaseScaleLabel.setText("HOPO Base: " + juce::String(gemHopoBaseScale, 2), juce::dontSendNotification);
    gemTapOverlayScaleLabel.setText("Tap Ovr: " + juce::String(gemTapOverlayScale, 2), juce::dontSendNotification);
    gemGhostOverlayScaleLabel.setText("Ghost Ovr: " + juce::String(gemGhostOverlayScale, 2), juce::dontSendNotification);
    gemAccentOverlayScaleLabel.setText("Accent Ovr: " + juce::String(gemAccentOverlayScale, 2), juce::dontSendNotification);
    gemNoteBaseScaleLabel.setText("Note Base: " + juce::String(gemNoteBaseScale, 2), juce::dontSendNotification);
    gemCymScaleLabel.setText("Cym: " + juce::String(gemCymScale, 2), juce::dontSendNotification);
    gemCymBaseScaleLabel.setText("Cym Base: " + juce::String(gemCymBaseScale, 2), juce::dontSendNotification);
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

    for (int i = 0; i < 6; i++) {
        guitarXOffLabels[i].setText(juce::String(guitarXOffNames[i]) + " X1:" + juce::String(guitarXOff[i], 1), juce::dontSendNotification);
        guitarXOff2Labels[i].setText(juce::String(guitarXOffNames[i]) + " X2:" + juce::String(guitarXOff2[i], 1), juce::dontSendNotification);
    }
    for (int i = 0; i < 5; i++) {
        drumXOffLabels[i].setText(juce::String(drumXOffNames[i]) + " X1:" + juce::String(drumXOff[i], 1), juce::dontSendNotification);
        drumXOff2Labels[i].setText(juce::String(drumXOffNames[i]) + " X2:" + juce::String(drumXOff2[i], 1), juce::dontSendNotification);
    }
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

    // --- Layers section ---
    layersHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        layerScaleLabels[i].setVisible(layersHeader.expanded);
        layerXLabels[i].setVisible(layersHeader.expanded);
        layerYLabels[i].setVisible(layersHeader.expanded);
        if (layersHeader.expanded)
        {
            layerScaleLabels[i].setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
            layerXLabels[i].setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
            layerYLabels[i].setBounds(margin, y, w, rowHeight); y += rowHeight + gap + 2;
        }
    }

    // --- Tiling section ---
    y += headerGap;
    tilingHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    tileStepLabel.setVisible(tilingHeader.expanded);
    tileScaleStepLabel.setVisible(tilingHeader.expanded);
    if (tilingHeader.expanded)
    {
        tileStepLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
        tileScaleStepLabel.setBounds(margin, y, w, rowHeight); y += rowHeight + gap;
    }
    y += headerGap;

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
    layoutRow(gemNoteScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemHopoScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemHopoBaseScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemTapOverlayScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemGhostOverlayScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemAccentOverlayScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemNoteBaseScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemCymScaleLabel, gemScaleHeader.expanded);
    layoutRow(gemCymBaseScaleLabel, gemScaleHeader.expanded);
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
    y += headerGap;

    // Guitar Note X Offsets
    guitarXOffHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    for (int i = 0; i < 6; i++) {
        layoutRow(guitarXOffLabels[i], guitarXOffHeader.expanded);
        layoutRow(guitarXOff2Labels[i], guitarXOffHeader.expanded);
    }
    y += headerGap;

    // Drum Note X Offsets
    drumXOffHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    for (int i = 0; i < 5; i++) {
        layoutRow(drumXOffLabels[i], drumXOffHeader.expanded);
        layoutRow(drumXOff2Labels[i], drumXOffHeader.expanded);
    }
    y += headerGap;

    // Guitar Lanes
    guitarLanesHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    for (int i = 0; i < GUITAR_LANES; i++)
    {
        layoutRow(gLaneXLabels[i], guitarLanesHeader.expanded);
        layoutRow(gLaneX2Labels[i], guitarLanesHeader.expanded);
        layoutRow(gLaneWLabels[i], guitarLanesHeader.expanded);
        layoutRow(gLaneW2Labels[i], guitarLanesHeader.expanded);
        if (guitarLanesHeader.expanded) y += headerGap;
    }
    y += headerGap;

    // Drum Lanes
    drumLanesHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    for (int i = 0; i < DRUM_LANES; i++)
    {
        layoutRow(dLaneXLabels[i], drumLanesHeader.expanded);
        layoutRow(dLaneX2Labels[i], drumLanesHeader.expanded);
        layoutRow(dLaneWLabels[i], drumLanesHeader.expanded);
        layoutRow(dLaneW2Labels[i], drumLanesHeader.expanded);
        if (drumLanesHeader.expanded) y += headerGap;
    }

    panel->setSize(panel->getWidth(), y + margin);
}

#endif
