/* Decodium 4.0 - lazy Settings tab */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

SettingsPageScroll {
    property var dialog
    readonly property var bridge: dialog ? dialog.appBridge : null
    readonly property bool compactSettingsLayout: dialog ? dialog.compactSettingsLayout : false
    readonly property bool narrowSettingsLayout: dialog ? dialog.narrowSettingsLayout : false
    readonly property int pageColumns: compactSettingsLayout ? 2 : 4
    readonly property int labelWidth: dialog ? dialog.labelWidth : 120
    readonly property int fieldMinWidth: dialog ? dialog.fieldMinWidth : 180
    readonly property int wideFieldMinWidth: dialog ? dialog.wideFieldMinWidth : 260
    readonly property int portFieldMinWidth: dialog ? dialog.portFieldMinWidth : 140
    readonly property int numericFieldMinWidth: dialog ? dialog.numericFieldMinWidth : 120
    readonly property int comboFieldMinWidth: dialog ? dialog.comboFieldMinWidth : 180
    readonly property int frequencyPageMinWidth: dialog ? dialog.frequencyPageMinWidth : 760
    readonly property int scrollLeftMargin: dialog ? dialog.scrollLeftMargin : 10
    readonly property int scrollTopMargin: dialog ? dialog.scrollTopMargin : 10
    readonly property int scrollRightMargin: dialog ? dialog.scrollRightMargin : 12
    readonly property int scrollBottomMargin: dialog ? dialog.scrollBottomMargin : 96
    pageLeftMargin: scrollLeftMargin
    pageTopMargin: scrollTopMargin
    pageRightMargin: scrollRightMargin
    pageBottomMargin: scrollBottomMargin
    minimumContentWidth: dialog ? dialog.settingsPageMinimumContentWidth(pageColumns) : 0
    readonly property color bgDeep: dialog ? dialog.bgDeep : "#080b12"
    readonly property color bgMedium: dialog ? dialog.bgMedium : "#101722"
    readonly property color bgLight: dialog ? dialog.bgLight : "#1a2433"
    readonly property color bgDark: dialog ? dialog.bgDark : "#080b12"
    readonly property color primaryBlue: dialog ? dialog.primaryBlue : "#3ba7ff"
    readonly property color secondaryCyan: dialog ? dialog.secondaryCyan : "#00d9ff"
    readonly property color accentGreen: dialog ? dialog.accentGreen : "#00f08b"
    readonly property color textPrimary: dialog ? dialog.textPrimary : "#f2f5f7"
    readonly property color textSecondary: dialog ? dialog.textSecondary : "#a7b2c0"
    readonly property color textDim: dialog ? dialog.textDim : "#667180"
    readonly property color glassBorder: dialog ? dialog.glassBorder : "#334455"
    readonly property int controlHeight: dialog ? dialog.controlHeight : 32
    readonly property int controlFontSize: dialog ? dialog.controlFontSize : 12
    readonly property int controlVerticalPadding: dialog ? dialog.controlVerticalPadding : 0
    readonly property int spinTextSidePadding: dialog ? dialog.spinTextSidePadding : 52

    function boolSetting(key, fallback) {
        return dialog ? dialog.boolSetting(key, fallback) : !!fallback
    }

    function setBoolSettingIfChanged(key, value, fallback) {
        if (dialog)
            dialog.setBoolSettingIfChanged(key, value, fallback)
    }
    id: displaySettingsScroll
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    GridLayout {
        id: displaySettingsGrid
        width: Math.max(0, parent.width - dialog.scrollLeftMargin - dialog.scrollRightMargin)
        columns: pageColumns; columnSpacing: 10; rowSpacing: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: dialog.scrollLeftMargin
        anchors.rightMargin: dialog.scrollRightMargin
        anchors.topMargin: dialog.scrollTopMargin

        // ── Aspetto / Tema ──
        Text { text: qsTr("ASPETTO / TEMA"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 4 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Theme:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: themeCombo
            Layout.fillWidth: true
            implicitHeight: controlHeight
            model: bridge.themeManager.availableThemes
            currentIndex: Math.max(0, model.indexOf(bridge.themeManager.currentTheme))
            onActivated: bridge.themeManager.applyThemeByName(currentText)
            Connections {
                target: bridge.themeManager
                function onCurrentThemeChanged() {
                    var i = themeCombo.model.indexOf(bridge.themeManager.currentTheme)
                    if (i >= 0 && themeCombo.currentIndex !== i)
                        themeCombo.currentIndex = i
                }
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate {
                contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium }
            }
        }
        // riga vuota per riempire le 4 colonne
        Item { Layout.columnSpan: 2; Layout.preferredHeight: controlHeight }

        // DX-Pedition Fase 1 — Accent + Density (visibili solo col tema DX-Pedition)
        Text {
            text: qsTr("Accent:")
            color: textSecondary; font.pixelSize: 12
            Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
            visible: bridge.themeManager.currentTheme === "Darkcodium"
        }
        RowLayout {
            id: dxpAccentRow
            Layout.columnSpan: Math.max(1, pageColumns - 1); Layout.fillWidth: true
            spacing: 8
            visible: bridge.themeManager.currentTheme === "Darkcodium"
            readonly property var accents: [
                { key: "phosphor", color: "#19ff88" },
                { key: "cyan",     color: "#66e6ff" },
                { key: "amber",    color: "#ffb820" },
                { key: "red",      color: "#ff5466" }
            ]
            Repeater {
                model: dxpAccentRow.accents
                delegate: Rectangle {
                    Layout.preferredWidth: 34; Layout.preferredHeight: controlHeight
                    radius: 6
                    color: modelData.color
                    readonly property bool sel: bridge.themeManager.accentVariant === modelData.key
                    border.width: sel ? 2 : 1
                    border.color: sel ? textPrimary : glassBorder
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bridge.themeManager.accentVariant = modelData.key
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        Text {
            text: qsTr("Density:")
            color: textSecondary; font.pixelSize: 12
            Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
            visible: bridge.themeManager.currentTheme === "Darkcodium"
        }
        RowLayout {
            id: dxpDensityRow
            Layout.columnSpan: Math.max(1, pageColumns - 1); Layout.fillWidth: true
            spacing: 0
            visible: bridge.themeManager.currentTheme === "Darkcodium"
            readonly property var densities: ["compact", "regular", "comfy"]
            Repeater {
                model: dxpDensityRow.densities
                delegate: Rectangle {
                    Layout.preferredWidth: 86; Layout.preferredHeight: controlHeight
                    readonly property bool sel: bridge.themeManager.density === modelData
                    color: sel ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.30) : bgMedium
                    border.color: glassBorder
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                        color: parent.sel ? textPrimary : textSecondary
                        font.pixelSize: controlFontSize
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bridge.themeManager.density = modelData
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // DX-Pedition Fase 2a — opt-in 3-column tactical workspace toggle.
        CheckBox {
            id: dxPeditionWorkspaceCheck
            Layout.columnSpan: pageColumns
            Layout.fillWidth: true
            text: qsTr("DX-Pedition Workspace (3-column tactical layout)")
            checked: mainWindow.dxPeditionMode
            onToggled: {
                mainWindow.dxPeditionMode = checked
                bridge.setSetting("uiDxPeditionMode", checked)
            }
            contentItem: Text {
                text: dxPeditionWorkspaceCheck.text
                color: textPrimary
                font.pixelSize: 12
                leftPadding: dxPeditionWorkspaceCheck.indicator.width + 8
                verticalAlignment: Text.AlignVCenter
            }
            ToolTip.visible: hovered
            ToolTip.delay: 600
            ToolTip.text: qsTr("Alternative single-panel operator view optimized for DX pile-ups: a tactical 3-column dashboard (Cluster / Waterfall / TX) instead of the classic workspace. Opt-in, default OFF: the standard layout is unchanged when disabled.")
        }

        // 1.0.307 (#2) — Scala interfaccia globale (icone+font+layout). Applica al riavvio.
        Text { text: qsTr("UI Scale:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
        DecoComboBox {
            id: uiScaleCombo
            Layout.fillWidth: true
            implicitHeight: controlHeight
            readonly property var scaleValues: [1.0, 1.1, 1.25, 1.5, 1.75]
            model: ["100%", "110%", "125%", "150%", "175%"]
            currentIndex: {
                var f = bridge ? Number(bridge.getSetting("uiScaleFactor", 1.0)) : 1.0
                var best = 0; var bestd = 99
                for (var i = 0; i < scaleValues.length; ++i) {
                    var d = Math.abs(scaleValues[i] - f)
                    if (d < bestd) { bestd = d; best = i }
                }
                return best
            }
            onActivated: {
                if (bridge) bridge.setSetting("uiScaleFactor", scaleValues[currentIndex])
                uiScaleRestartNote.visible = true
            }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
            delegate: ItemDelegate {
                contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium }
            }
        }
        Text {
            id: uiScaleRestartNote
            Layout.columnSpan: 2
            Layout.preferredHeight: controlHeight
            verticalAlignment: Text.AlignVCenter
            text: qsTr("↻ restart to apply")
            color: bridge.themeManager.warningColor
            font.pixelSize: 11
            visible: false
        }

        // ── Bande Operative (#4) — quali bande mostrare nel selettore ──
        Text { text: qsTr("OPERATING BANDS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }
        Text {
            text: qsTr("Click to show/hide bands in the selector. Deselected bands disappear from the HF / V-U / SHF bar.")
            color: textSecondary; font.pixelSize: 10; wrapMode: Text.WordWrap
            Layout.columnSpan: pageColumns; Layout.fillWidth: true; Layout.bottomMargin: 2
        }
        Flow {
            Layout.columnSpan: pageColumns; Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: dialog.allBandsForConfig
                delegate: Rectangle {
                    width: 64; height: 26; radius: 4
                    property bool bandOn: dialog.bandEnabledCfg(modelData.l)
                    color: bandOn ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.28)
                                  : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
                    border.color: bandOn ? primaryBlue : glassBorder
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: modelData.l
                        color: bandOn ? textPrimary : textSecondary
                        font.pixelSize: 10; font.bold: bandOn
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: dialog.toggleBandCfg(modelData.l, !parent.bandOn)
                        ToolTip.visible: containsMouse
                        ToolTip.delay: 500
                        ToolTip.text: modelData.n + " MHz — " + (parent.bandOn ? qsTr("visible (click to hide)") : qsTr("hidden (click to show)"))
                    }
                }
            }
        }

        // 1.0.189 — Riorganizzato in 2 sub-section per UX migliore:
        // PERFORMANCE (gates anti-stall) + STYLE (estetica).
        // ── UI — PERFORMANCE ──
        Text { text: qsTr("UI — PERFORMANCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        // 1.0.180 — Quality preset: gate per effetti visivi pesanti.
        Text { text: qsTr("UI Quality preset:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        DecoComboBox {
            id: uiQualityCombo
            Layout.preferredWidth: 180
            Layout.columnSpan: 1
            model: ["Low", "Medium", "High"]
            currentIndex: {
                if (!bridge) return 1
                const q = bridge.uiQuality
                return q === "Low" ? 0 : (q === "High" ? 2 : 1)
            }
            onActivated: {
                if (bridge) bridge.setUiQuality(model[currentIndex])
            }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Low = no effects (modest PCs).\nMedium = light animations.\nHigh = all available animations.\n\nDefault: Medium.")
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // 1.0.388 — Priorità processo Windows (scheduling CPU)
        Text { text: qsTr("Process priority:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        DecoComboBox {
            id: processPriorityCombo
            // Keep the complete "Above normal" and
            // "High (recommended)" labels visible in the
            // compact four-column display layout.
            Layout.preferredWidth: narrowSettingsLayout ? 220 : 250
            Layout.minimumWidth: narrowSettingsLayout ? 210 : 230
            Layout.columnSpan: 1
            model: [qsTr("Normal"), qsTr("Above normal"), qsTr("High (recommended)"), qsTr("Realtime ⚠️")]
            currentIndex: bridge ? bridge.processPriority : 1
            onActivated: {
                if (bridge && bridge.processPriority !== currentIndex)
                    bridge.processPriority = currentIndex
            }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("CPU scheduling priority for the Decodium process (Windows).\n\nNormal / Above normal (default) = safe.\nHigh = smoother audio/decode with low risk (recommended if you notice stutters).\nRealtime ⚠️ = maximum scheduling priority, but it can make the PC unresponsive (mouse/keyboard) and requires administrator privileges. Without admin rights Windows downgrades it to High.\n\nIf unsure, use High.")
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // 1.0.180 — Style (richiede restart)
        Text { text: qsTr("UI Style (restart):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        DecoComboBox {
            id: uiStyleCombo
            Layout.preferredWidth: 180
            Layout.columnSpan: 1
            // 1.0.185 — Whitelist 4 stili customizable. "Default" rimosso
            // dal model: era un alias confondente perche' su Windows con
            // Qt 6.11 risolveva al native style non-customizable (warning
            // massivi + UI degradata). Material e' la prima voce, baseline
            // visiva storica Decodium (default fino al 1.0.179).
            model: ["Material", "FluentWinUI3", "Universal", "Fusion"]
            currentIndex: {
                if (!bridge) return 0
                // Default e' alias per Material, mostra Material
                let idx = model.indexOf(bridge.uiStyle)
                return idx < 0 ? 0 : idx
            }
            onActivated: {
                if (bridge) bridge.setUiStyle(model[currentIndex])
            }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr(
                "QML Quick Controls style (requires restart):\n" +
                "• Material (recommended) — Google Material 3, customizable, Decodium's historical default\n" +
                "• FluentWinUI3 — native Windows 11 (Mica/acrylic). Automatic fallback for SplitView/StackView.\n" +
                "• Universal — Microsoft Universal (WinPhone-style)\n" +
                "• Fusion — neutral cross-platform desktop"
            )
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // 1.0.180 — Frameless pop-out
        Text { text: qsTr("Frameless pop-out:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        CheckBox {
            id: framelessPopoutsCheck
            Layout.leftMargin: 24
            checked: bridge ? bridge.uiFramelessPopouts : false
            onCheckedChanged: {
                if (bridge) bridge.setUiFramelessPopouts(checked)
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Pop-out windows (Waterfall, Period1, DecoSync) become frameless with drag via the border.\n\nWindows 11 aesthetic.\n\nDefault: OFF. Requires closing and reopening the window.")
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // 1.0.186 — Auto-detach Full Spectrum (Pasquale-pattern)
        Text { text: qsTr("Detach Full Spectrum:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        CheckBox {
            id: autoDetachFullSpectrumCheck
            Layout.leftMargin: 24
            checked: bridge ? bridge.autoDetachFullSpectrum : false
            onCheckedChanged: {
                if (bridge) bridge.setAutoDetachFullSpectrum(checked)
            }
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("At startup, opens Full Spectrum (Band Activity) in a separate window, isolating the Main render thread from ListView animations.\n\nReduces stalls on modest PCs.\n\nDefault: OFF. Requires restart.")
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // 1.0.412 — Schermo intero (opt-in, non persistito: al riavvio torna a finestra)
        Text { text: qsTr("Full screen:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        Button {
            Layout.leftMargin: 24
            Layout.preferredHeight: controlHeight
            text: qsTr("Enable (F11)")
            hoverEnabled: true
            onClicked: { dialog.fullScreenRequested(); dialog.close() }
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Switch Decodium to full screen. To exit: F11, Esc, or the top ✕ button. This is not saved: Decodium starts in normal window mode after restart.")
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // 1.0.186 — Spectrum FPS cap (15/20/30)
        Text { text: qsTr("Spectrum FPS cap:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        DecoComboBox {
            id: spectrumFpsCombo
            Layout.preferredWidth: 170
            model: ["15 fps", "20 fps", "30 fps"]
            currentIndex: {
                if (!bridge) return 1
                const cap = bridge.spectrumFpsCap
                if (cap <= 15) return 0
                if (cap >= 30) return 2
                return 1
            }
            onActivated: {
                if (!bridge) return
                const map = [15, 20, 30]
                bridge.setSpectrumFpsCap(map[currentIndex])
            }
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.delay: 400
            ToolTip.text: qsTr("Maximum frame rate of the embedded waterfall/panadapter.\n\n  • 15 = modest PCs\n  • 20 = balanced default\n  • 30 = modern hardware\n\nWhen Full Spectrum is detached the separate render thread holds 30 fps without affecting the decoder.")
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // 1.0.189 — Telemetria pressione CPU (sessione corrente, read-only).
        // Se i contatori sono alti, considera Low Quality / FPS cap=15 / Detach ON.
        Text { text: qsTr("CPU pressure:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
        Text {
            id: cpuPressureTelemetryText
            Layout.preferredWidth: 280
            Layout.columnSpan: 1
            color: {
                if (!bridge) return textSecondary
                const severe = bridge.cpuPressureSevereEventCount
                if (severe >= 5) return "#ff8844"
                if (severe >= 1) return secondaryCyan
                return textSecondary
            }
            font.pixelSize: 12
            text: bridge
                  ? qsTr("events: total=%1 · severe=%2 (session)")
                        .arg(bridge.cpuPressureEventCount)
                        .arg(bridge.cpuPressureSevereEventCount)
                  : qsTr("events: total=0 · severe=0")
            // 1.0.190 hotfix — hoverEnabled / ToolTip.* non sono
            // proprieta' di Text. Tooltip e' attached property
            // gestita da MouseArea con .text dedicato.
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                ToolTip.visible: containsMouse
                ToolTip.delay: 400
                ToolTip.text: qsTr("cpuPressure event counters for the current session.\n\nSevere ones (≥1100ms or burst of 4+ short stalls) are the strongest signal: if you see ≥5 after an hour of use, lower UI Quality to Low or Spectrum FPS cap to 15.")
            }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── Font ──
        Text { text: qsTr("FONT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Font:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: controlHeight
                radius: 4
                color: bgMedium
                border.color: glassBorder
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    text: dialog.uiFontLabel
                    color: textPrimary
                    font.pixelSize: controlFontSize
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                width: 78; height: controlHeight; radius: 4
                color: fontChooseMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                border.color: primaryBlue
                Text { anchors.centerIn: parent; text: qsTr("Choose"); color: primaryBlue; font.pixelSize: 11 }
                MouseArea { id: fontChooseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: dialog.openFontPicker("Font", "", 0, false) }
            }
            Rectangle {
                width: 64; height: controlHeight; radius: 4
                color: fontResetMA.containsMouse ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.18) : bgMedium
                border.color: glassBorder
                Text { anchors.centerIn: parent; text: qsTr("Reset"); color: textSecondary; font.pixelSize: 11 }
                MouseArea { id: fontResetMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.resetFontSetting("Font", "", 0) }
            }
        }
        Text { text: qsTr("Decoded Font:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: controlHeight
                radius: 4
                color: bgMedium
                border.color: glassBorder
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    text: dialog.decodedFontLabel
                    color: textPrimary
                    font.pixelSize: controlFontSize
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                width: 78; height: controlHeight; radius: 4
                color: decodedFontChooseMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                border.color: primaryBlue
                Text { anchors.centerIn: parent; text: qsTr("Choose"); color: primaryBlue; font.pixelSize: 11 }
                MouseArea { id: decodedFontChooseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: dialog.openFontPicker("DecodedTextFont", "Courier", 10, true) }
            }
            Rectangle {
                width: 64; height: controlHeight; radius: 4
                color: decodedFontResetMA.containsMouse ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.18) : bgMedium
                border.color: glassBorder
                Text { anchors.centerIn: parent; text: qsTr("Reset"); color: textSecondary; font.pixelSize: 11 }
                MouseArea { id: decodedFontResetMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.resetFontSetting("DecodedTextFont", "Courier", 10) }
            }
        }

        // ── Decodifiche ──
        Text { text: qsTr("DECODES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Show DXCC:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("ShowDXCC", true)
            onCheckedChanged: bridge.setSetting("ShowDXCC", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("US State:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: controlHeight
            spacing: 8
            CheckBox {
                checked: bridge.showUsState
                onCheckedChanged: bridge.showUsState = checked
                Layout.preferredWidth: 28
                Layout.preferredHeight: controlHeight
                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                contentItem: Text { text: ""; leftPadding: 24 }
            }
            Text {
                text: bridge.usStateDataUpdating ? qsTr("Updating...")
                      : (bridge.usStateDataReady ? qsTr("%1 calls").arg(bridge.usStateGridCount)
                                                 : qsTr("Not loaded"))
                color: bridge.usStateDataReady ? accentGreen : textSecondary
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.preferredHeight: controlHeight
            }
            Button {
                text: qsTr("Update")
                enabled: bridge.showUsState && !bridge.usStateDataUpdating
                implicitHeight: controlHeight
                Layout.preferredWidth: narrowSettingsLayout ? 104 : 110
                Layout.minimumWidth: narrowSettingsLayout ? 100 : 104
                Layout.rightMargin: 8
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? textPrimary : textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideNone
                }
                onClicked: bridge.updateUsStateData()
            }
        }

        Text { text: qsTr("TX Msg to RX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("TXMessagesToRX", true)
            onCheckedChanged: bridge.setSetting("TXMessagesToRX", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Waterfall Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("uiWaterfallShowCallsigns", true)
            onCheckedChanged: bridge.setSetting("uiWaterfallShowCallsigns", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("FS Dist:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("uiFullSpectrumShowDistColumn", true)
            onCheckedChanged: bridge.setSetting("uiFullSpectrumShowDistColumn", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("FS Az:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("uiFullSpectrumShowAzColumn", true)
            onCheckedChanged: bridge.setSetting("uiFullSpectrumShowAzColumn", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("RX Freq:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("uiSignalRxShowFreqColumn", true)
            onCheckedChanged: bridge.setSetting("uiSignalRxShowFreqColumn", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("RX Dist:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("uiSignalRxShowDistColumn", true)
            onCheckedChanged: bridge.setSetting("uiSignalRxShowDistColumn", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("RX Az:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: dialog.boolSetting("uiSignalRxShowAzColumn", true)
            onCheckedChanged: bridge.setSetting("uiSignalRxShowAzColumn", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

        // ── Mappa e Distanza ──
        Text { text: qsTr("MAP AND DISTANCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Miles:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: boolSetting("Miles", false)
            onCheckedChanged: bridge.setSetting("Miles", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Greyline:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            // Keep the settings page on the same canonical boolean path as
            // the Live Map.  Direct QVariant binding treats a stored string
            // such as "false" as true and can also write the fallback value
            // while the page is being constructed.
            checked: boolSetting("ShowGreyline", true)
            onToggled: setBoolSettingIfChanged("ShowGreyline", checked, true)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Map All Msgs:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("MapAllMessages", false)
            onCheckedChanged: bridge.setSetting("MapAllMessages", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Click TX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("MapSingleClickTX", false)
            onCheckedChanged: bridge.setSetting("MapSingleClickTX", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }

        Text { text: qsTr("Map GPU:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: boolSetting("LiveMapUseGpu", true)
            onToggled: setBoolSettingIfChanged("LiveMapUseGpu", checked, true)
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Use GPU rendering for the live map (default). Disable to use the CPU renderer if station markers are missing or the map has graphical problems.")
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text {
            text: qsTr("Disable if map markers are missing. Uses CPU rendering when off.")
            color: textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        // ── Allineamento ──
        Text { text: qsTr("ALIGNMENT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: pageColumns; Layout.topMargin: 10 }
        Rectangle { Layout.fillWidth: true; Layout.columnSpan: pageColumns; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

        Text { text: qsTr("Align:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        CheckBox {
            checked: bridge.getSetting("Align", false)
            onCheckedChanged: bridge.setSetting("Align", checked)
            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
            contentItem: Text { text: ""; leftPadding: 24 }
        }
        Text { text: qsTr("Align Steps:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: alignStepsSpin
            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps", 0)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("AlignSteps", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: alignStepsSpin.textFromValue(alignStepsSpin.value, alignStepsSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !alignStepsSpin.editable; validator: alignStepsSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }

        Text { text: qsTr("Align Steps 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
        SpinBox {
            id: alignSteps2Spin
            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps2", 0)); editable: true
            implicitHeight: controlHeight; Layout.fillWidth: true
            onValueChanged: bridge.setSetting("AlignSteps2", value)
            contentItem: TextInput { selectByMouse: true; onActiveFocusChanged: if (activeFocus) selectAll(); text: alignSteps2Spin.textFromValue(alignSteps2Spin.value, alignSteps2Spin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !alignSteps2Spin.editable; validator: alignSteps2Spin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
        }
        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
        Item { Layout.fillWidth: true; Layout.columnSpan: pageColumns; Layout.preferredHeight: 18 }
    }
}
