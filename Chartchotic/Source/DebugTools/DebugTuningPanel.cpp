#ifdef DEBUG

#include "DebugTuningPanel.h"
#include "../Visual/Renderers/SceneRenderer.h"

DebugTuningPanel::DebugTuningPanel(juce::ValueTree& state)
    : state(state)
{
    // --- Perspective section ---
    setupSectionHeader(perspectiveHeader, "Perspective");
    {
        static constexpr const char* names[PERSP_COUNT] = {"VP Depth", "VP Y", "Near Width", "Exp Curve", "Hwy Depth", "Player Dist", "Persp Str"};
        static constexpr float steps[PERSP_COUNT] = {0.1f, 0.001f, 0.005f, 0.01f, 5.0f, 5.0f, 0.01f};
        static constexpr float lo[PERSP_COUNT]    = {0.5f, -0.500f, 0.30f, 0.05f, 10.0f, 10.0f, 0.0f};
        static constexpr float hi[PERSP_COUNT]    = {20.0f, 0.500f, 2.00f, 2.0f, 500.0f, 500.0f, 2.0f};
        static constexpr int   dec[PERSP_COUNT]   = {1, 3, 3, 2, 0, 0, 2};

        auto getPerspPtr = [](int i) -> float* {
            switch (i) {
            case 0: return &PositionMath::debugPerspParamsGuitar.vanishingPointDepth;
            case 1: return &PositionMath::debugPerspParamsGuitar.vanishingPointY;
            case 2: return &PositionMath::debugPerspParamsGuitar.nearWidth;
            case 3: return &PositionMath::debugPerspParamsGuitar.exponentialCurve;
            case 4: return &PositionMath::debugPerspParamsGuitar.highwayDepth;
            case 5: return &PositionMath::debugPerspParamsGuitar.playerDistance;
            default: return &PositionMath::debugPerspParamsGuitar.perspectiveStrength;
            }
        };

        for (int i = 0; i < PERSP_COUNT; i++)
        {
            setupScrollLabel(perspLabels[i]);
            perspLabels[i].onScroll = [this, i, getPerspPtr](int delta) {
                static constexpr const char* names_l[PERSP_COUNT] = {"VP Depth", "VP Y", "Near Width", "Exp Curve", "Hwy Depth", "Player Dist", "Persp Str"};
                static constexpr float steps_l[PERSP_COUNT] = {0.1f, 0.001f, 0.005f, 0.01f, 5.0f, 5.0f, 0.01f};
                static constexpr float lo_l[PERSP_COUNT]    = {0.5f, -0.500f, 0.30f, 0.05f, 10.0f, 10.0f, 0.0f};
                static constexpr float hi_l[PERSP_COUNT]    = {20.0f, 0.500f, 2.00f, 2.0f, 500.0f, 500.0f, 2.0f};
                static constexpr int   dec_l[PERSP_COUNT]   = {1, 3, 3, 2, 0, 0, 2};
                float* val = getPerspPtr(i);
                *val = juce::jlimit(lo_l[i], hi_l[i], *val + delta * steps_l[i]);
                perspLabels[i].setText(juce::String(names_l[i]) + ": " + juce::String(*val, dec_l[i]), juce::dontSendNotification);
                if (onPerspectiveChanged) onPerspectiveChanged();
            };
        }
    }

    // --- Track section (layers table + tiling) ---
    setupSectionHeader(trackHeader, "Track");

    for (int c = 0; c < LAYER_COLS; c++)
        setupTableHeader(layerColHdrLabels[c]);

    auto getLayerPtr = [this](int displayRow, int c) -> float* {
        int storageIdx = displayLayerMap[displayRow];
        switch (c) {
        case 0: return &layerStates[storageIdx].scale;
        case 1: return &layerStates[storageIdx].xOffset;
        default: return &layerStates[storageIdx].yOffset;
        }
    };

    static constexpr float layerStep[LAYER_COLS] = {0.005f, 0.0005f, 0.0005f};
    static constexpr float layerLo[LAYER_COLS]   = {0.10f, -1.0f, -1.0f};
    static constexpr float layerHi[LAYER_COLS]   = {3.00f,  1.0f,  1.0f};
    static constexpr int   layerDec[LAYER_COLS]   = {3, 4, 4};

    for (int r = 0; r < NUM_DISPLAY_LAYERS; r++)
    {
        layerRowLabels[r].setText(layerNames[r], juce::dontSendNotification);
        layerRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        layerRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        layerRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < LAYER_COLS; c++)
        {
            setupScrollLabel(layerParams[r][c]);
            layerParams[r][c].setFont(juce::Font(10.0f));
            layerParams[r][c].setJustificationType(juce::Justification::centred);
            layerParams[r][c].onScroll = [this, r, c, getLayerPtr](int delta) {
                float* val = getLayerPtr(r, c);
                *val = juce::jlimit(layerLo[c], layerHi[c], *val + delta * layerStep[c]);
                layerParams[r][c].setText(juce::String(*val, layerDec[c]), juce::dontSendNotification);
                fireLayer(r);
            };
        }
    }

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

    setupScrollLabel(textureScaleLabel);
    textureScaleLabel.onScroll = [this](int delta) {
        textureScaleValue = juce::jlimit(0.10f, 5.00f, textureScaleValue + delta * 0.05f);
        textureScaleLabel.setText("Tex S: " + juce::String(textureScaleValue, 2), juce::dontSendNotification);
        if (onTextureScaleChanged) onTextureScaleChanged(textureScaleValue);
    };

    setupScrollLabel(textureOpacityLabel);
    textureOpacityLabel.onScroll = [this](int delta) {
        textureOpacityValue = juce::jlimit(0.05f, 1.00f, textureOpacityValue + delta * 0.02f);
        textureOpacityLabel.setText("Tex O: " + juce::String(textureOpacityValue, 2), juce::dontSendNotification);
        if (onTextureOpacityChanged) onTextureOpacityChanged(textureOpacityValue);
    };

    polyShadeToggle.setButtonText("Poly Shade");
    polyShadeToggle.onClick = [this]() {
        PositionMath::debugPolyShade = polyShadeToggle.getToggleState();
        if (onPolyShadeChanged) onPolyShadeChanged();
    };

    setupScrollLabel(gridPosLabel);
    gridPosLabel.onScroll = [this](int delta) {
        gridlinePosOffset = juce::jlimit(-0.10f, 0.10f, gridlinePosOffset + delta * 0.002f);
        gridPosLabel.setText("Grid: " + juce::String(gridlinePosOffset, 3), juce::dontSendNotification);
        fireChanged();
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

    setupScrollLabel(depthForeshortenLabel);
    depthForeshortenLabel.onScroll = [this](int delta) {
        depthForeshorten = juce::jlimit(0.0f, 1.0f, depthForeshorten + delta * 0.02f);
        depthForeshortenLabel.setText("Depth: " + juce::String(depthForeshorten, 2), juce::dontSendNotification);
        fireChanged();
    };

    // --- Base Scale table (Gem/Bar × W/H) ---
    setupSectionHeader(baseScaleHeader, "Base Scale");

    // Column headers
    for (int c = 0; c < BASE_SCALE_COLS; c++)
        setupTableHeader(baseScaleColHdrLabels[c]);

    // Access helpers: row 0 = gemScale, row 1 = barScale
    auto getBaseScalePtr = [this](int r, int c) -> float* {
        auto* s = (r == 0) ? &gemScale : &barScale;
        return (c == 0) ? &s->width : &s->height;
    };

    for (int r = 0; r < BASE_SCALE_ROWS; r++)
    {
        baseScaleRowLabels[r].setText(baseScaleRowNames[r], juce::dontSendNotification);
        baseScaleRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        baseScaleRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        baseScaleRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < BASE_SCALE_COLS; c++)
        {
            setupScrollLabel(baseScaleParams[r][c]);
            baseScaleParams[r][c].setFont(juce::Font(10.0f));
            baseScaleParams[r][c].setJustificationType(juce::Justification::centred);
            baseScaleParams[r][c].onScroll = [this, r, c, getBaseScalePtr](int delta) {
                float* val = getBaseScalePtr(r, c);
                *val = juce::jlimit(0.50f, 2.00f, *val + delta * 0.01f);
                baseScaleParams[r][c].setText(juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // Gem type scales (list, backed by gemTypeScales struct)
    {
        static constexpr const char* names[GEM_TYPE_COUNT] = {"Normal", "Cymbal", "HOPO", "GTap", "DGho", "DAcc", "CGho", "CAcc", "SPGem", "SPBar"};
        float* ptrs[GEM_TYPE_COUNT] = {
            &gemTypeScales.normal, &gemTypeScales.cymbal, &gemTypeScales.hopo, &gemTypeScales.gTap,
            &gemTypeScales.dGhost, &gemTypeScales.dAccent, &gemTypeScales.cGhost,
            &gemTypeScales.cAccent, &gemTypeScales.spGem, &gemTypeScales.spBar
        };
        for (int i = 0; i < GEM_TYPE_COUNT; i++)
        {
            setupScrollLabel(gemTypeScaleLabels[i]);
            gemTypeScaleLabels[i].onScroll = [this, i, ptrs](int delta) {
                float* val = ptrs[i];
                *val = juce::jlimit(0.30f, 2.00f, *val + delta * 0.01f);
                gemTypeScaleLabels[i].setText(juce::String(names[i]) + ": " + juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // --- Hit Scale table (Gem/Bar × S/W/H) ---
    setupSectionHeader(hitScaleHeader, "Hit Scale");

    for (int c = 0; c < HIT_SCALE_COLS; c++)
        setupTableHeader(hitScaleColHdrLabels[c]);

    auto getHitScalePtr = [this](int r, int c) -> float* {
        auto* s = (r == 0) ? &hitGemScale : &hitBarScale;
        switch (c) {
        case 0: return &s->scale;
        case 1: return &s->width;
        default: return &s->height;
        }
    };

    for (int r = 0; r < HIT_SCALE_ROWS; r++)
    {
        hitScaleRowLabels[r].setText(hitScaleRowNames[r], juce::dontSendNotification);
        hitScaleRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        hitScaleRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        hitScaleRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < HIT_SCALE_COLS; c++)
        {
            setupScrollLabel(hitScaleParams[r][c]);
            hitScaleParams[r][c].setFont(juce::Font(10.0f));
            hitScaleParams[r][c].setJustificationType(juce::Justification::centred);
            hitScaleParams[r][c].onScroll = [this, r, c, getHitScalePtr](int delta) {
                float* val = getHitScalePtr(r, c);
                *val = juce::jlimit(0.50f, 3.00f, *val + delta * 0.01f);
                hitScaleParams[r][c].setText(juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // Hit type scales (list, backed by hitTypeConfig struct)
    {
        static constexpr const char* names[HIT_TYPE_FLOAT_COUNT] = {"Ghost", "Accent", "HOPO", "Tap", "SP"};
        static constexpr float lo[HIT_TYPE_FLOAT_COUNT] = {0.30f, 0.50f, 0.30f, 0.30f, 0.50f};
        static constexpr float hi[HIT_TYPE_FLOAT_COUNT] = {1.50f, 2.00f, 2.00f, 1.50f, 2.00f};
        float* ptrs[HIT_TYPE_FLOAT_COUNT] = {
            &hitTypeConfig.ghost, &hitTypeConfig.accent, &hitTypeConfig.hopo,
            &hitTypeConfig.tap, &hitTypeConfig.sp
        };
        for (int i = 0; i < HIT_TYPE_FLOAT_COUNT; i++)
        {
            setupScrollLabel(hitTypeScaleLabels[i]);
            hitTypeScaleLabels[i].onScroll = [this, i, ptrs](int delta) {
                float* val = ptrs[i];
                *val = juce::jlimit(lo[i], hi[i], *val + delta * 0.01f);
                hitTypeScaleLabels[i].setText(juce::String(names[i]) + ": " + juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // Hit type bools (backed by hitTypeConfig struct)
    {
        static constexpr const char* boolNames[HIT_TYPE_BOOL_COUNT] = {"SP White: ", "Tap Purple:"};
        bool* boolPtrs[HIT_TYPE_BOOL_COUNT] = {&hitTypeConfig.spWhiteFlare, &hitTypeConfig.tapPurpleFlare};
        for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
        {
            setupScrollLabel(hitTypeBoolLabels[i]);
            hitTypeBoolLabels[i].onScroll = [this, i, boolPtrs](int) {
                *boolPtrs[i] = !*boolPtrs[i];
                hitTypeBoolLabels[i].setText(juce::String(boolNames[i]) + (*boolPtrs[i] ? "ON" : "OFF"), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // --- Z Offsets table (5 rows × 2 cols: Guitar/Drums) ---
    setupSectionHeader(zOffsetsHeader, "Z Offsets");

    for (int c = 0; c < Z_COLS; c++)
        setupTableHeader(zColHdrLabels[c]);

    auto getZPtr = [this](int r, int c) -> float* {
        auto* o = (c == 0) ? &guitarOffsets : &drumOffsets;
        switch (r) {
        case 0: return &o->gridZ;
        case 1: return &o->gemZ;
        case 2: return &o->barZ;
        case 3: return &o->hitGemZ;
        default: return &o->hitBarZ;
        }
    };

    for (int r = 0; r < Z_ROWS; r++)
    {
        zRowLabels[r].setText(zRowNames[r], juce::dontSendNotification);
        zRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        zRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        zRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < Z_COLS; c++)
        {
            setupScrollLabel(zParams[r][c]);
            zParams[r][c].setFont(juce::Font(10.0f));
            zParams[r][c].setJustificationType(juce::Justification::centred);
            zParams[r][c].onScroll = [this, r, c, getZPtr](int delta) {
                float* val = getZPtr(r, c);
                *val = juce::jlimit(-50.0f, 50.0f, *val + delta * 0.5f);
                zParams[r][c].setText(juce::String(*val, 1), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // --- Strike Position table (2 rows × 2 cols: Guitar/Drums) ---
    setupSectionHeader(strikeHeader, "Strike Pos");

    for (int c = 0; c < STRIKE_COLS; c++)
        setupTableHeader(strikeColHdrLabels[c]);

    auto getStrikePtr = [this](int r, int c) -> float* {
        auto* o = (c == 0) ? &guitarOffsets : &drumOffsets;
        return (r == 0) ? &o->strikePosGem : &o->strikePosBar;
    };

    for (int r = 0; r < STRIKE_ROWS; r++)
    {
        strikeRowLabels[r].setText(strikeRowNames[r], juce::dontSendNotification);
        strikeRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        strikeRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        strikeRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < STRIKE_COLS; c++)
        {
            setupScrollLabel(strikeParams[r][c]);
            strikeParams[r][c].setFont(juce::Font(10.0f));
            strikeParams[r][c].setJustificationType(juce::Justification::centred);
            strikeParams[r][c].onScroll = [this, r, c, getStrikePtr](int delta) {
                float* val = getStrikePtr(r, c);
                *val = juce::jlimit(-0.10f, 0.10f, *val + delta * 0.002f);
                strikeParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // Helper: setup a sub-section label ("Notes" / "Lanes")
    auto setupSubLabel = [](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colours::grey);
        label.setFont(juce::Font(10.0f));
    };

    // Helper: get ColumnAdjust field by column index (shared by guitar/drums note tables)
    // Columns: 0=Z, 1=S1, 2=S2, 3=W, 4=H
    auto getColAdjustField = [](PositionConstants::ColumnAdjust& ca, int c) -> float* {
        switch (c) {
        case 0: return &ca.z;
        case 1: return &ca.sNear;
        case 2: return &ca.sFar;
        case 3: return &ca.w;
        default: return &ca.h;
        }
    };

    // Note table scroll params per column: {step, lo, hi, decimals}
    // Z is pixel offset, S1/S2/W/H are scale multipliers
    static constexpr float noteStep[COL_NOTE_COLS] = {0.5f, 0.01f, 0.01f, 0.01f, 0.01f};
    static constexpr float noteLo[COL_NOTE_COLS]   = {-30.f, 0.30f, 0.30f, 0.30f, 0.30f};
    static constexpr float noteHi[COL_NOTE_COLS]   = {30.f, 3.00f, 3.00f, 3.00f, 3.00f};
    static constexpr int   noteDec[COL_NOTE_COLS]   = {1, 2, 2, 2, 2};

    // --- Guitar Cols section ---
    setupSectionHeader(guitarColsHeader, "Guitar Cols");
    setupSubLabel(gcolSubNoteLabel, "Notes");
    setupSubLabel(gcolSubLaneLabel, "Lanes");

    // Guitar notes sub-table (6 rows × 7 cols)
    for (int c = 0; c < COL_NOTE_COLS; c++)
        setupTableHeader(gcolNoteHdrLabels[c]);

    for (int r = 0; r < GUITAR_LANES; r++)
    {
        gcolNoteRowLabels[r].setText(guitarColNames[r], juce::dontSendNotification);
        gcolNoteRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        gcolNoteRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        gcolNoteRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_NOTE_COLS; c++)
        {
            setupScrollLabel(gcolNoteParams[r][c]);
            gcolNoteParams[r][c].setFont(juce::Font(10.0f));
            gcolNoteParams[r][c].setJustificationType(juce::Justification::centred);
            gcolNoteParams[r][c].onScroll = [this, r, c, getColAdjustField](int delta) {
                float* val = getColAdjustField(guitarColAdjust[r], c);
                *val = juce::jlimit(noteLo[c], noteHi[c], *val + delta * noteStep[c]);
                gcolNoteParams[r][c].setText(juce::String(*val, noteDec[c]), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // Guitar lanes sub-table (6 rows × 4 cols)
    std::copy_n(PositionConstants::guitarBezierLaneCoords, GUITAR_LANES, guitarLaneCoords);

    for (int c = 0; c < COL_LANE_COLS; c++)
        setupTableHeader(gcolLaneHdrLabels[c]);

    auto getGcolLanePtr = [this](int r, int c) -> float* {
        switch (c) {
        case 0: return &guitarLaneCoords[r].normX1;
        default: return &guitarLaneCoords[r].normWidth1;
        }
    };

    for (int r = 0; r < GUITAR_LANES; r++)
    {
        gcolLaneRowLabels[r].setText(guitarColNames[r], juce::dontSendNotification);
        gcolLaneRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        gcolLaneRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        gcolLaneRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_LANE_COLS; c++)
        {
            setupScrollLabel(gcolLaneParams[r][c]);
            gcolLaneParams[r][c].setFont(juce::Font(10.0f));
            gcolLaneParams[r][c].setJustificationType(juce::Justification::centred);
            gcolLaneParams[r][c].onScroll = [this, r, c, getGcolLanePtr](int delta) {
                float* val = getGcolLanePtr(r, c);
                *val = juce::jlimit(0.0f, 1.0f, *val + delta * 0.001f);
                gcolLaneParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireLaneChanged();
            };
        }
    }

    // --- Drum Cols section ---
    setupSectionHeader(drumColsHeader, "Drum Cols");
    setupSubLabel(dcolSubNoteLabel, "Notes");
    setupSubLabel(dcolSubLaneLabel, "Lanes");

    // Drum notes sub-table (5 rows × 7 cols)
    for (int c = 0; c < COL_NOTE_COLS; c++)
        setupTableHeader(dcolNoteHdrLabels[c]);

    for (int r = 0; r < DRUM_LANES; r++)
    {
        dcolNoteRowLabels[r].setText(drumColNames[r], juce::dontSendNotification);
        dcolNoteRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        dcolNoteRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        dcolNoteRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_NOTE_COLS; c++)
        {
            setupScrollLabel(dcolNoteParams[r][c]);
            dcolNoteParams[r][c].setFont(juce::Font(10.0f));
            dcolNoteParams[r][c].setJustificationType(juce::Justification::centred);
            dcolNoteParams[r][c].onScroll = [this, r, c, getColAdjustField](int delta) {
                float* val = getColAdjustField(drumColAdjust[r], c);
                *val = juce::jlimit(noteLo[c], noteHi[c], *val + delta * noteStep[c]);
                dcolNoteParams[r][c].setText(juce::String(*val, noteDec[c]), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // Drum lanes sub-table (5 rows × 4 cols)
    std::copy_n(PositionConstants::drumBezierLaneCoords, DRUM_LANES, drumLaneCoords);

    for (int c = 0; c < COL_LANE_COLS; c++)
        setupTableHeader(dcolLaneHdrLabels[c]);

    auto getDcolLanePtr = [this](int r, int c) -> float* {
        switch (c) {
        case 0: return &drumLaneCoords[r].normX1;
        default: return &drumLaneCoords[r].normWidth1;
        }
    };

    for (int r = 0; r < DRUM_LANES; r++)
    {
        dcolLaneRowLabels[r].setText(drumColNames[r], juce::dontSendNotification);
        dcolLaneRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        dcolLaneRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        dcolLaneRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_LANE_COLS; c++)
        {
            setupScrollLabel(dcolLaneParams[r][c]);
            dcolLaneParams[r][c].setFont(juce::Font(10.0f));
            dcolLaneParams[r][c].setJustificationType(juce::Justification::centred);
            dcolLaneParams[r][c].onScroll = [this, r, c, getDcolLanePtr](int delta) {
                float* val = getDcolLanePtr(r, c);
                *val = juce::jlimit(0.0f, 1.0f, *val + delta * 0.001f);
                dcolLaneParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireLaneChanged();
            };
        }
    }

    // --- Lane Shape section ---
    setupSectionHeader(laneShapeHeader, "Lane Shape");
    for (int c = 0; c < LANE_SHAPE_COLS; c++)
        setupTableHeader(laneShapeColHdrLabels[c]);

    auto getLaneShapePtr = [this](int r, int c) -> float* {
        if (r == 0) {
            switch (c) {
            case 0: return &laneShape.startOffset;
            case 1: return &laneShape.innerStartArc;
            default: return &laneShape.outerStartArc;
            }
        } else {
            switch (c) {
            case 0: return &laneShape.endOffset;
            case 1: return &laneShape.innerEndArc;
            default: return &laneShape.outerEndArc;
            }
        }
    };

    for (int r = 0; r < LANE_SHAPE_ROWS; r++)
    {
        laneShapeRowLabels[r].setText(laneShapeRowNames[r], juce::dontSendNotification);
        laneShapeRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        laneShapeRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        laneShapeRowLabels[r].setFont(juce::Font(10.0f));
        for (int c = 0; c < LANE_SHAPE_COLS; c++)
        {
            setupScrollLabel(laneShapeParams[r][c]);
            laneShapeParams[r][c].setFont(juce::Font(10.0f));
            laneShapeParams[r][c].setJustificationType(juce::Justification::centred);
            laneShapeParams[r][c].onScroll = [this, r, c, getLaneShapePtr](int delta) {
                float step = (c == 0) ? 0.002f : 0.005f;
                float lo = (c == 0) ? -0.10f : -0.20f;
                float hi = (c == 0) ?  0.10f :  0.20f;
                float* val = getLaneShapePtr(r, c);
                *val = juce::jlimit(lo, hi, *val + delta * step);
                laneShapeParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // --- Overlay Adjust section ---
    setupSectionHeader(overlayAdjustHeader, "Overlay Adjust");
    std::copy_n(PositionConstants::OVERLAY_DEFAULTS, NUM_OVERLAY_TYPES, overlayAdjusts);

    for (int c = 0; c < OVERLAY_PARAMS; c++)
        setupTableHeader(overlayColHeaderLabels[c]);

    for (int r = 0; r < NUM_OVERLAY_TYPES; r++)
    {
        overlayRowNameLabels[r].setText(overlayRowNames[r], juce::dontSendNotification);
        overlayRowNameLabels[r].setJustificationType(juce::Justification::centredLeft);
        overlayRowNameLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        overlayRowNameLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < OVERLAY_PARAMS; c++)
        {
            setupScrollLabel(overlayParamLabels[r][c]);
            overlayParamLabels[r][c].setFont(juce::Font(10.0f));
            overlayParamLabels[r][c].setJustificationType(juce::Justification::centred);
            overlayParamLabels[r][c].onScroll = [this, r, c](int delta) {
                float step = (c < 2) ? 0.01f : 0.01f;
                float lo = (c < 2) ? -1.0f : 0.10f;
                float hi = (c < 2) ?  1.0f : 3.00f;
                float* val = nullptr;
                switch (c) {
                case 0: val = &overlayAdjusts[r].offsetX; break;
                case 1: val = &overlayAdjusts[r].offsetY; break;
                case 2: val = &overlayAdjusts[r].scaleX; break;
                case 3: val = &overlayAdjusts[r].scaleY; break;
                case 4: val = &overlayAdjusts[r].scale; break;
                }
                *val = juce::jlimit(lo, hi, *val + delta * step);
                overlayParamLabels[r][c].setText(juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    refreshLabels();
    refreshTrackLabels();
    refreshColLabels();

    // =========================================================================
    // Register panel children
    // =========================================================================

    // Perspective section
    tuningButton.addPanelChild(&perspectiveHeader);
    for (int i = 0; i < PERSP_COUNT; i++)
        tuningButton.addPanelChild(&perspLabels[i]);

    tuningButton.addPanelChild(&trackHeader);
    for (int c = 0; c < LAYER_COLS; c++)
        tuningButton.addPanelChild(&layerColHdrLabels[c]);
    for (int r = 0; r < NUM_DISPLAY_LAYERS; r++)
    {
        tuningButton.addPanelChild(&layerRowLabels[r]);
        for (int c = 0; c < LAYER_COLS; c++)
            tuningButton.addPanelChild(&layerParams[r][c]);
    }
    tuningButton.addPanelChild(&tileStepLabel);
    tuningButton.addPanelChild(&tileScaleStepLabel);
    tuningButton.addPanelChild(&textureScaleLabel);
    tuningButton.addPanelChild(&textureOpacityLabel);
    tuningButton.addPanelChild(&polyShadeToggle);
    tuningButton.addPanelChild(&gridPosLabel);

    tuningButton.addPanelChild(&curvatureHeader);
    tuningButton.addPanelChild(&guitarCurvLabel);
    tuningButton.addPanelChild(&drumCurvLabel);
    tuningButton.addPanelChild(&depthForeshortenLabel);

    // Base Scale table
    tuningButton.addPanelChild(&baseScaleHeader);
    for (int c = 0; c < BASE_SCALE_COLS; c++)
        tuningButton.addPanelChild(&baseScaleColHdrLabels[c]);
    for (int r = 0; r < BASE_SCALE_ROWS; r++)
    {
        tuningButton.addPanelChild(&baseScaleRowLabels[r]);
        for (int c = 0; c < BASE_SCALE_COLS; c++)
            tuningButton.addPanelChild(&baseScaleParams[r][c]);
    }
    for (int i = 0; i < GEM_TYPE_COUNT; i++)
        tuningButton.addPanelChild(&gemTypeScaleLabels[i]);

    // Hit Scale table
    tuningButton.addPanelChild(&hitScaleHeader);
    for (int c = 0; c < HIT_SCALE_COLS; c++)
        tuningButton.addPanelChild(&hitScaleColHdrLabels[c]);
    for (int r = 0; r < HIT_SCALE_ROWS; r++)
    {
        tuningButton.addPanelChild(&hitScaleRowLabels[r]);
        for (int c = 0; c < HIT_SCALE_COLS; c++)
            tuningButton.addPanelChild(&hitScaleParams[r][c]);
    }
    for (int i = 0; i < HIT_TYPE_FLOAT_COUNT; i++)
        tuningButton.addPanelChild(&hitTypeScaleLabels[i]);
    for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
        tuningButton.addPanelChild(&hitTypeBoolLabels[i]);

    // Z Offsets table
    tuningButton.addPanelChild(&zOffsetsHeader);
    for (int c = 0; c < Z_COLS; c++)
        tuningButton.addPanelChild(&zColHdrLabels[c]);
    for (int r = 0; r < Z_ROWS; r++)
    {
        tuningButton.addPanelChild(&zRowLabels[r]);
        for (int c = 0; c < Z_COLS; c++)
            tuningButton.addPanelChild(&zParams[r][c]);
    }

    // Strike Position table
    tuningButton.addPanelChild(&strikeHeader);
    for (int c = 0; c < STRIKE_COLS; c++)
        tuningButton.addPanelChild(&strikeColHdrLabels[c]);
    for (int r = 0; r < STRIKE_ROWS; r++)
    {
        tuningButton.addPanelChild(&strikeRowLabels[r]);
        for (int c = 0; c < STRIKE_COLS; c++)
            tuningButton.addPanelChild(&strikeParams[r][c]);
    }

    // Guitar Cols (notes + lanes)
    tuningButton.addPanelChild(&guitarColsHeader);
    tuningButton.addPanelChild(&gcolSubNoteLabel);
    for (int c = 0; c < COL_NOTE_COLS; c++)
        tuningButton.addPanelChild(&gcolNoteHdrLabels[c]);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        tuningButton.addPanelChild(&gcolNoteRowLabels[r]);
        for (int c = 0; c < COL_NOTE_COLS; c++)
            tuningButton.addPanelChild(&gcolNoteParams[r][c]);
    }
    tuningButton.addPanelChild(&gcolSubLaneLabel);
    for (int c = 0; c < COL_LANE_COLS; c++)
        tuningButton.addPanelChild(&gcolLaneHdrLabels[c]);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        tuningButton.addPanelChild(&gcolLaneRowLabels[r]);
        for (int c = 0; c < COL_LANE_COLS; c++)
            tuningButton.addPanelChild(&gcolLaneParams[r][c]);
    }

    // Drum Cols (notes + lanes)
    tuningButton.addPanelChild(&drumColsHeader);
    tuningButton.addPanelChild(&dcolSubNoteLabel);
    for (int c = 0; c < COL_NOTE_COLS; c++)
        tuningButton.addPanelChild(&dcolNoteHdrLabels[c]);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        tuningButton.addPanelChild(&dcolNoteRowLabels[r]);
        for (int c = 0; c < COL_NOTE_COLS; c++)
            tuningButton.addPanelChild(&dcolNoteParams[r][c]);
    }
    tuningButton.addPanelChild(&dcolSubLaneLabel);
    for (int c = 0; c < COL_LANE_COLS; c++)
        tuningButton.addPanelChild(&dcolLaneHdrLabels[c]);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        tuningButton.addPanelChild(&dcolLaneRowLabels[r]);
        for (int c = 0; c < COL_LANE_COLS; c++)
            tuningButton.addPanelChild(&dcolLaneParams[r][c]);
    }

    // Lane Shape table
    tuningButton.addPanelChild(&laneShapeHeader);
    for (int c = 0; c < LANE_SHAPE_COLS; c++)
        tuningButton.addPanelChild(&laneShapeColHdrLabels[c]);
    for (int r = 0; r < LANE_SHAPE_ROWS; r++)
    {
        tuningButton.addPanelChild(&laneShapeRowLabels[r]);
        for (int c = 0; c < LANE_SHAPE_COLS; c++)
            tuningButton.addPanelChild(&laneShapeParams[r][c]);
    }

    // Overlay Adjust table
    tuningButton.addPanelChild(&overlayAdjustHeader);
    for (int c = 0; c < OVERLAY_PARAMS; c++)
        tuningButton.addPanelChild(&overlayColHeaderLabels[c]);
    for (int r = 0; r < NUM_OVERLAY_TYPES; r++)
    {
        tuningButton.addPanelChild(&overlayRowNameLabels[r]);
        for (int c = 0; c < OVERLAY_PARAMS; c++)
            tuningButton.addPanelChild(&overlayParamLabels[r][c]);
    }

    tuningButton.setPanelSize(280, 200);
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
    sr.depthForeshorten = depthForeshorten;
    sr.gemScale = gemScale;
    sr.barScale = barScale;
    sr.hitGemScale = hitGemScale;
    sr.hitBarScale = hitBarScale;
    sr.hitTypeConfig = hitTypeConfig;
    sr.gemTypeScales = gemTypeScales;

    sr.guitarOffsets = guitarOffsets;
    sr.drumOffsets = drumOffsets;

    std::copy_n(guitarColAdjust, 6, sr.guitarColAdjust);
    std::copy_n(drumColAdjust, 5, sr.drumColAdjust);

    std::copy_n(guitarLaneCoords, GUITAR_LANES, sr.guitarLaneCoordsLocal);
    std::copy_n(drumLaneCoords, DRUM_LANES, sr.drumLaneCoordsLocal);
    std::copy_n(overlayAdjusts, NUM_OVERLAY_TYPES, sr.overlayAdjusts);
    sr.gridlinePosOffset = gridlinePosOffset;
    sr.laneShape = laneShape;
}

void DebugTuningPanel::initDefaults(const TrackRenderer& trackRenderer)
{
    std::copy_n(trackRenderer.layersGuitar, NUM_TRACK_LAYERS, guitarStates);
    std::copy_n(trackRenderer.layersDrums, NUM_TRACK_LAYERS, drumStates);
    tileStepValue = trackRenderer.tileStep;
    tileScaleStepValue = trackRenderer.tileScaleStep;
    textureScaleValue = trackRenderer.textureScale;
    textureOpacityValue = trackRenderer.textureOpacity;
    refreshTrackLabels();
}

void DebugTuningPanel::setDrums(bool isDrums)
{
    layerStates = isDrums ? drumStates : guitarStates;
    refreshTrackLabels();
}

void DebugTuningPanel::fireLayer(int displayIdx)
{
    int storageIdx = displayLayerMap[displayIdx];
    if (onLayerChanged)
        onLayerChanged(storageIdx, layerStates[storageIdx].scale, layerStates[storageIdx].xOffset, layerStates[storageIdx].yOffset);
}

void DebugTuningPanel::refreshTrackLabels()
{
    static constexpr int layerDec[LAYER_COLS] = {3, 4, 4};
    for (int c = 0; c < LAYER_COLS; c++)
        layerColHdrLabels[c].setText(layerColNames[c], juce::dontSendNotification);
    for (int r = 0; r < NUM_DISPLAY_LAYERS; r++)
    {
        int si = displayLayerMap[r];
        const float vals[LAYER_COLS] = {layerStates[si].scale, layerStates[si].xOffset, layerStates[si].yOffset};
        for (int c = 0; c < LAYER_COLS; c++)
            layerParams[r][c].setText(juce::String(vals[c], layerDec[c]), juce::dontSendNotification);
    }
    tileStepLabel.setText("Step: " + juce::String(tileStepValue, 3), juce::dontSendNotification);
    tileScaleStepLabel.setText("Scale: " + juce::String(tileScaleStepValue, 3), juce::dontSendNotification);
    textureScaleLabel.setText("Tex S: " + juce::String(textureScaleValue, 2), juce::dontSendNotification);
    textureOpacityLabel.setText("Tex O: " + juce::String(textureOpacityValue, 2), juce::dontSendNotification);
    polyShadeToggle.setToggleState(PositionMath::debugPolyShade, juce::dontSendNotification);
    gridPosLabel.setText("Grid: " + juce::String(gridlinePosOffset, 3), juce::dontSendNotification);
}

void DebugTuningPanel::refreshColLabels()
{
    static constexpr int noteDec[COL_NOTE_COLS] = {1, 2, 2, 2, 2};

    // Guitar notes
    for (int c = 0; c < COL_NOTE_COLS; c++)
        gcolNoteHdrLabels[c].setText(colNoteColNames[c], juce::dontSendNotification);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        const auto& ca = guitarColAdjust[r];
        const float vals[COL_NOTE_COLS] = {ca.z, ca.sNear, ca.sFar, ca.w, ca.h};
        for (int c = 0; c < COL_NOTE_COLS; c++)
            gcolNoteParams[r][c].setText(juce::String(vals[c], noteDec[c]), juce::dontSendNotification);
    }

    // Guitar lanes
    for (int c = 0; c < COL_LANE_COLS; c++)
        gcolLaneHdrLabels[c].setText(colLaneColNames[c], juce::dontSendNotification);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        gcolLaneParams[r][0].setText(juce::String(guitarLaneCoords[r].normX1, 3), juce::dontSendNotification);
        gcolLaneParams[r][1].setText(juce::String(guitarLaneCoords[r].normWidth1, 3), juce::dontSendNotification);
    }

    // Drum notes
    for (int c = 0; c < COL_NOTE_COLS; c++)
        dcolNoteHdrLabels[c].setText(colNoteColNames[c], juce::dontSendNotification);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        const auto& ca = drumColAdjust[r];
        const float vals[COL_NOTE_COLS] = {ca.z, ca.sNear, ca.sFar, ca.w, ca.h};
        for (int c = 0; c < COL_NOTE_COLS; c++)
            dcolNoteParams[r][c].setText(juce::String(vals[c], noteDec[c]), juce::dontSendNotification);
    }

    // Drum lanes
    for (int c = 0; c < COL_LANE_COLS; c++)
        dcolLaneHdrLabels[c].setText(colLaneColNames[c], juce::dontSendNotification);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        dcolLaneParams[r][0].setText(juce::String(drumLaneCoords[r].normX1, 3), juce::dontSendNotification);
        dcolLaneParams[r][1].setText(juce::String(drumLaneCoords[r].normWidth1, 3), juce::dontSendNotification);
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

void DebugTuningPanel::refreshLaneShapeLabels()
{
    for (int c = 0; c < LANE_SHAPE_COLS; c++)
        laneShapeColHdrLabels[c].setText(laneShapeColNames[c], juce::dontSendNotification);
    const float vals[LANE_SHAPE_ROWS][LANE_SHAPE_COLS] = {
        {laneShape.startOffset, laneShape.innerStartArc, laneShape.outerStartArc},
        {laneShape.endOffset, laneShape.innerEndArc, laneShape.outerEndArc}
    };
    for (int r = 0; r < LANE_SHAPE_ROWS; r++)
        for (int c = 0; c < LANE_SHAPE_COLS; c++)
            laneShapeParams[r][c].setText(juce::String(vals[r][c], 3), juce::dontSendNotification);
}

void DebugTuningPanel::refreshOverlayLabels()
{
    for (int r = 0; r < NUM_OVERLAY_TYPES; r++)
    {
        const float vals[OVERLAY_PARAMS] = {
            overlayAdjusts[r].offsetX, overlayAdjusts[r].offsetY,
            overlayAdjusts[r].scaleX, overlayAdjusts[r].scaleY, overlayAdjusts[r].scale
        };
        for (int c = 0; c < OVERLAY_PARAMS; c++)
            overlayParamLabels[r][c].setText(juce::String(vals[c], 2), juce::dontSendNotification);
    }
}

void DebugTuningPanel::refreshLabels()
{
    // Perspective labels
    {
        static constexpr const char* names[PERSP_COUNT] = {"VP Depth", "VP Y", "Near Width", "Exp Curve", "Hwy Depth", "Player Dist", "Persp Str"};
        static constexpr int dec[PERSP_COUNT] = {1, 3, 3, 2, 0, 0, 2};
        const float vals[PERSP_COUNT] = {
            PositionMath::debugPerspParamsGuitar.vanishingPointDepth,
            PositionMath::debugPerspParamsGuitar.vanishingPointY,
            PositionMath::debugPerspParamsGuitar.nearWidth,
            PositionMath::debugPerspParamsGuitar.exponentialCurve,
            PositionMath::debugPerspParamsGuitar.highwayDepth,
            PositionMath::debugPerspParamsGuitar.playerDistance,
            PositionMath::debugPerspParamsGuitar.perspectiveStrength
        };
        for (int i = 0; i < PERSP_COUNT; i++)
            perspLabels[i].setText(juce::String(names[i]) + ": " + juce::String(vals[i], dec[i]), juce::dontSendNotification);
    }

    guitarCurvLabel.setText("Guitar: " + juce::String(guitarCurvature, 3), juce::dontSendNotification);
    drumCurvLabel.setText("Drums: " + juce::String(drumCurvature, 3), juce::dontSendNotification);
    depthForeshortenLabel.setText("Depth: " + juce::String(depthForeshorten, 2), juce::dontSendNotification);

    // Base Scale table
    for (int r = 0; r < BASE_SCALE_ROWS; r++)
    {
        const auto& s = (r == 0) ? gemScale : barScale;
        baseScaleParams[r][0].setText(juce::String(s.width, 2), juce::dontSendNotification);
        baseScaleParams[r][1].setText(juce::String(s.height, 2), juce::dontSendNotification);
        baseScaleColHdrLabels[0].setText(baseScaleColNames[0], juce::dontSendNotification);
        baseScaleColHdrLabels[1].setText(baseScaleColNames[1], juce::dontSendNotification);
    }

    {
        static constexpr const char* names[GEM_TYPE_COUNT] = {"Normal", "Cymbal", "HOPO", "GTap", "DGho", "DAcc", "CGho", "CAcc", "SPGem", "SPBar"};
        const float vals[GEM_TYPE_COUNT] = {
            gemTypeScales.normal, gemTypeScales.cymbal, gemTypeScales.hopo, gemTypeScales.gTap,
            gemTypeScales.dGhost, gemTypeScales.dAccent, gemTypeScales.cGhost,
            gemTypeScales.cAccent, gemTypeScales.spGem, gemTypeScales.spBar
        };
        for (int i = 0; i < GEM_TYPE_COUNT; i++)
            gemTypeScaleLabels[i].setText(juce::String(names[i]) + ": " + juce::String(vals[i], 2), juce::dontSendNotification);
    }

    // Hit Scale table
    for (int r = 0; r < HIT_SCALE_ROWS; r++)
    {
        const auto& s = (r == 0) ? hitGemScale : hitBarScale;
        hitScaleParams[r][0].setText(juce::String(s.scale, 2), juce::dontSendNotification);
        hitScaleParams[r][1].setText(juce::String(s.width, 2), juce::dontSendNotification);
        hitScaleParams[r][2].setText(juce::String(s.height, 2), juce::dontSendNotification);
    }
    for (int c = 0; c < HIT_SCALE_COLS; c++)
        hitScaleColHdrLabels[c].setText(hitScaleColNames[c], juce::dontSendNotification);

    {
        static constexpr const char* names[HIT_TYPE_FLOAT_COUNT] = {"Ghost", "Accent", "HOPO", "Tap", "SP"};
        const float vals[HIT_TYPE_FLOAT_COUNT] = {
            hitTypeConfig.ghost, hitTypeConfig.accent, hitTypeConfig.hopo,
            hitTypeConfig.tap, hitTypeConfig.sp
        };
        for (int i = 0; i < HIT_TYPE_FLOAT_COUNT; i++)
            hitTypeScaleLabels[i].setText(juce::String(names[i]) + ": " + juce::String(vals[i], 2), juce::dontSendNotification);

        static constexpr const char* boolNames[HIT_TYPE_BOOL_COUNT] = {"SP White: ", "Tap Purple:"};
        const bool boolVals[HIT_TYPE_BOOL_COUNT] = {hitTypeConfig.spWhiteFlare, hitTypeConfig.tapPurpleFlare};
        for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
            hitTypeBoolLabels[i].setText(juce::String(boolNames[i]) + (boolVals[i] ? "ON" : "OFF"), juce::dontSendNotification);
    }

    // Z Offsets table
    for (int c = 0; c < Z_COLS; c++)
        zColHdrLabels[c].setText(zColNames[c], juce::dontSendNotification);
    for (int r = 0; r < Z_ROWS; r++)
    {
        const auto& g = guitarOffsets;
        const auto& d = drumOffsets;
        const float gVals[] = {g.gridZ, g.gemZ, g.barZ, g.hitGemZ, g.hitBarZ};
        const float dVals[] = {d.gridZ, d.gemZ, d.barZ, d.hitGemZ, d.hitBarZ};
        zParams[r][0].setText(juce::String(gVals[r], 1), juce::dontSendNotification);
        zParams[r][1].setText(juce::String(dVals[r], 1), juce::dontSendNotification);
    }

    // Strike Position table
    for (int c = 0; c < STRIKE_COLS; c++)
        strikeColHdrLabels[c].setText(strikeColNames[c], juce::dontSendNotification);
    strikeParams[0][0].setText(juce::String(guitarOffsets.strikePosGem, 3), juce::dontSendNotification);
    strikeParams[0][1].setText(juce::String(drumOffsets.strikePosGem, 3), juce::dontSendNotification);
    strikeParams[1][0].setText(juce::String(guitarOffsets.strikePosBar, 3), juce::dontSendNotification);
    strikeParams[1][1].setText(juce::String(drumOffsets.strikePosBar, 3), juce::dontSendNotification);

    refreshLaneShapeLabels();
    refreshOverlayLabels();
}

void DebugTuningPanel::setupTableHeader(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::grey);
    label.setFont(juce::Font(10.0f));
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
            tuningButton.relayoutPanel();
    };
}

void DebugTuningPanel::setupScrollLabel(ScrollableLabel& label)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setInterceptsMouseClicks(true, true);
}

int DebugTuningPanel::layoutTable(int y, int x, int w, int rowHeight, int gap,
                                   juce::Label* colHdrs, int numCols,
                                   juce::Label* rowNames, ScrollableLabel* params,
                                   int numRows, int paramStride, int nameW)
{
    const int cellW = (w - nameW) / numCols;

    // Column headers
    for (int c = 0; c < numCols; c++)
    {
        colHdrs[c].setVisible(true);
        colHdrs[c].setBounds(x + nameW + c * cellW, y, cellW, rowHeight);
    }
    y += rowHeight + gap;

    // Rows
    for (int r = 0; r < numRows; r++)
    {
        rowNames[r].setVisible(true);
        rowNames[r].setBounds(x, y, nameW, rowHeight);
        for (int c = 0; c < numCols; c++)
        {
            params[r * paramStride + c].setVisible(true);
            params[r * paramStride + c].setBounds(x + nameW + c * cellW, y, cellW, rowHeight);
        }
        y += rowHeight + gap;
    }
    return y;
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

    auto hideTable = [](juce::Label* colHdrs, int numCols, juce::Label* rowNames, ScrollableLabel* params, int numRows, int stride) {
        for (int c = 0; c < numCols; c++) colHdrs[c].setVisible(false);
        for (int r = 0; r < numRows; r++) {
            rowNames[r].setVisible(false);
            for (int c = 0; c < numCols; c++) params[r * stride + c].setVisible(false);
        }
    };

    // --- Perspective section ---
    perspectiveHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    for (int i = 0; i < PERSP_COUNT; i++)
        layoutRow(perspLabels[i], perspectiveHeader.expanded);
    y += headerGap;

    // --- Track section (layers table + tiling) ---
    trackHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (trackHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        layerColHdrLabels, LAYER_COLS,
                        layerRowLabels, &layerParams[0][0],
                        NUM_DISPLAY_LAYERS, LAYER_COLS, 40);
        layoutRow(tileStepLabel, true);
        layoutRow(tileScaleStepLabel, true);
        layoutRow(textureScaleLabel, true);
        layoutRow(textureOpacityLabel, true);
        layoutRow(polyShadeToggle, true);
        layoutRow(gridPosLabel, true);
    }
    else
    {
        hideTable(layerColHdrLabels, LAYER_COLS, layerRowLabels, &layerParams[0][0], NUM_DISPLAY_LAYERS, LAYER_COLS);
        tileStepLabel.setVisible(false);
        tileScaleStepLabel.setVisible(false);
        textureScaleLabel.setVisible(false);
        textureOpacityLabel.setVisible(false);
        polyShadeToggle.setVisible(false);
        gridPosLabel.setVisible(false);
    }
    y += headerGap;

    // Curvature
    curvatureHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    layoutRow(guitarCurvLabel, curvatureHeader.expanded);
    layoutRow(drumCurvLabel, curvatureHeader.expanded);
    layoutRow(depthForeshortenLabel, curvatureHeader.expanded);
    y += headerGap;

    // Base Scale table
    baseScaleHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (baseScaleHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        baseScaleColHdrLabels, BASE_SCALE_COLS,
                        baseScaleRowLabels, &baseScaleParams[0][0],
                        BASE_SCALE_ROWS, BASE_SCALE_COLS, 34);
        // Gem type scales
        for (int i = 0; i < GEM_TYPE_COUNT; i++)
            layoutRow(gemTypeScaleLabels[i], true);
    }
    else
    {
        hideTable(baseScaleColHdrLabels, BASE_SCALE_COLS, baseScaleRowLabels, &baseScaleParams[0][0], BASE_SCALE_ROWS, BASE_SCALE_COLS);
        for (int i = 0; i < GEM_TYPE_COUNT; i++)
            gemTypeScaleLabels[i].setVisible(false);
    }
    y += headerGap;

    // Hit Scale table
    hitScaleHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (hitScaleHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        hitScaleColHdrLabels, HIT_SCALE_COLS,
                        hitScaleRowLabels, &hitScaleParams[0][0],
                        HIT_SCALE_ROWS, HIT_SCALE_COLS, 34);
        for (int i = 0; i < HIT_TYPE_FLOAT_COUNT; i++)
            layoutRow(hitTypeScaleLabels[i], true);
        for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
            layoutRow(hitTypeBoolLabels[i], true);
    }
    else
    {
        hideTable(hitScaleColHdrLabels, HIT_SCALE_COLS, hitScaleRowLabels, &hitScaleParams[0][0], HIT_SCALE_ROWS, HIT_SCALE_COLS);
        for (int i = 0; i < HIT_TYPE_FLOAT_COUNT; i++)
            hitTypeScaleLabels[i].setVisible(false);
        for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
            hitTypeBoolLabels[i].setVisible(false);
    }
    y += headerGap;

    // Z Offsets table
    zOffsetsHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (zOffsetsHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        zColHdrLabels, Z_COLS,
                        zRowLabels, &zParams[0][0],
                        Z_ROWS, Z_COLS, 34);
    }
    else
    {
        hideTable(zColHdrLabels, Z_COLS, zRowLabels, &zParams[0][0], Z_ROWS, Z_COLS);
    }
    y += headerGap;

    // Strike Position table
    strikeHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (strikeHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        strikeColHdrLabels, STRIKE_COLS,
                        strikeRowLabels, &strikeParams[0][0],
                        STRIKE_ROWS, STRIKE_COLS, 34);
    }
    else
    {
        hideTable(strikeColHdrLabels, STRIKE_COLS, strikeRowLabels, &strikeParams[0][0], STRIKE_ROWS, STRIKE_COLS);
    }
    y += headerGap;

    // Guitar Cols (notes table + lanes table under one header)
    guitarColsHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (guitarColsHeader.expanded)
    {
        layoutRow(gcolSubNoteLabel, true);
        y = layoutTable(y, margin, w, rowHeight, gap,
                        gcolNoteHdrLabels, COL_NOTE_COLS,
                        gcolNoteRowLabels, &gcolNoteParams[0][0],
                        GUITAR_LANES, COL_NOTE_COLS, 28);
        y += headerGap;
        layoutRow(gcolSubLaneLabel, true);
        y = layoutTable(y, margin, w, rowHeight, gap,
                        gcolLaneHdrLabels, COL_LANE_COLS,
                        gcolLaneRowLabels, &gcolLaneParams[0][0],
                        GUITAR_LANES, COL_LANE_COLS, 34);
    }
    else
    {
        gcolSubNoteLabel.setVisible(false);
        gcolSubLaneLabel.setVisible(false);
        hideTable(gcolNoteHdrLabels, COL_NOTE_COLS, gcolNoteRowLabels, &gcolNoteParams[0][0], GUITAR_LANES, COL_NOTE_COLS);
        hideTable(gcolLaneHdrLabels, COL_LANE_COLS, gcolLaneRowLabels, &gcolLaneParams[0][0], GUITAR_LANES, COL_LANE_COLS);
    }
    y += headerGap;

    // Drum Cols (notes table + lanes table under one header)
    drumColsHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (drumColsHeader.expanded)
    {
        layoutRow(dcolSubNoteLabel, true);
        y = layoutTable(y, margin, w, rowHeight, gap,
                        dcolNoteHdrLabels, COL_NOTE_COLS,
                        dcolNoteRowLabels, &dcolNoteParams[0][0],
                        DRUM_LANES, COL_NOTE_COLS, 28);
        y += headerGap;
        layoutRow(dcolSubLaneLabel, true);
        y = layoutTable(y, margin, w, rowHeight, gap,
                        dcolLaneHdrLabels, COL_LANE_COLS,
                        dcolLaneRowLabels, &dcolLaneParams[0][0],
                        DRUM_LANES, COL_LANE_COLS, 34);
    }
    else
    {
        dcolSubNoteLabel.setVisible(false);
        dcolSubLaneLabel.setVisible(false);
        hideTable(dcolNoteHdrLabels, COL_NOTE_COLS, dcolNoteRowLabels, &dcolNoteParams[0][0], DRUM_LANES, COL_NOTE_COLS);
        hideTable(dcolLaneHdrLabels, COL_LANE_COLS, dcolLaneRowLabels, &dcolLaneParams[0][0], DRUM_LANES, COL_LANE_COLS);
    }
    y += headerGap;

    // Lane Shape (table layout)
    laneShapeHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (laneShapeHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        laneShapeColHdrLabels, LANE_SHAPE_COLS,
                        laneShapeRowLabels, &laneShapeParams[0][0],
                        LANE_SHAPE_ROWS, LANE_SHAPE_COLS, 40);
    }
    else
    {
        hideTable(laneShapeColHdrLabels, LANE_SHAPE_COLS, laneShapeRowLabels, &laneShapeParams[0][0], LANE_SHAPE_ROWS, LANE_SHAPE_COLS);
    }
    y += headerGap;

    // Overlay Adjust (table layout)
    overlayAdjustHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    if (overlayAdjustHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        overlayColHeaderLabels, OVERLAY_PARAMS,
                        overlayRowNameLabels, &overlayParamLabels[0][0],
                        NUM_OVERLAY_TYPES, OVERLAY_PARAMS, 34);
    }
    else
    {
        hideTable(overlayColHeaderLabels, OVERLAY_PARAMS, overlayRowNameLabels, &overlayParamLabels[0][0], NUM_OVERLAY_TYPES, OVERLAY_PARAMS);
    }

    panel->setSize(panel->getWidth(), y + margin);
}

#endif
