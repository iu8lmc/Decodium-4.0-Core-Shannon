static QString satelliteCodeFromDisplayText(const QString& displayText)
{
    QString const text = displayText.trimmed();
    int const separator = text.indexOf(QStringLiteral(" - "));
    return separator > 0 ? text.left(separator).trimmed() : text;
}

void DecodiumBridge::showLogQsoPromptDialog()
{
    // Ultimo controllo: se nel frattempo un pannello ha aperto la finestra
    // QML, non se ne apre una seconda per lo stesso QSO.
    if (m_logPromptShownByQml && !m_logQsoPromptDialog) {
        return;
    }
    if (m_logQsoPromptDialog) {
        m_logQsoPromptDialog->show();
        m_logQsoPromptDialog->raise();
        m_logQsoPromptDialog->activateWindow();
        return;
    }

    QVariantMap const preview = pendingLogQsoPreview();
    QString const call = preview.value(QStringLiteral("call")).toString().trimmed();
    if (call.isEmpty()) {
        rejectPromptedLogQso();
        return;
    }

    // Keep the emergency QWidget fallback in the same translation context as
    // the themed QML prompt.  The fallback used QStringLiteral throughout, so
    // a delayed/missed QML acknowledgement made the second QSO appear to
    // switch back to English even though UILanguage was still unchanged.
    auto const promptTr = [](const char* sourceText) {
        return QCoreApplication::translate("TxPanel", sourceText);
    };

    auto* dialog = new QDialog(QApplication::activeWindow());
    m_logQsoPromptDialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(promptTr("Confirm QSO logging"));
    dialog->setModal(false);
    dialog->setMinimumWidth(420);
    dialog->setProperty("decodiumAccepted", false);

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->addRow(promptTr("Call:"), new QLabel(call, dialog));
    form->addRow(promptTr("Grid:"), new QLabel(preview.value(QStringLiteral("grid")).toString().trimmed(), dialog));
    form->addRow(promptTr("Report:"), new QLabel(QStringLiteral("%1 / %2")
                                                           .arg(preview.value(QStringLiteral("sent")).toString().trimmed(),
                                                                preview.value(QStringLiteral("rcvd")).toString().trimmed()), dialog));
    form->addRow(promptTr("Mode:"), new QLabel(preview.value(QStringLiteral("mode")).toString().trimmed(), dialog));
    double const freqHz = preview.value(QStringLiteral("freq")).toDouble();
    form->addRow(promptTr("Freq:"), new QLabel(freqHz > 0.0
                                                        ? QStringLiteral("%1 Hz").arg(freqHz, 0, 'f', 0)
                                                        : QStringLiteral("-"), dialog));

    auto* timeOnEdit = new QLineEdit(preview.value(QStringLiteral("timeOn")).toString(), dialog);
    timeOnEdit->setClearButtonEnabled(false);
    timeOnEdit->setPlaceholderText(QStringLiteral("YYYY-MM-DD HH:MM:SS UTC"));
    form->addRow(promptTr("Start UTC:"), timeOnEdit);

    auto* timeOffEdit = new QLineEdit(preview.value(QStringLiteral("timeOff")).toString(), dialog);
    timeOffEdit->setClearButtonEnabled(false);
    timeOffEdit->setPlaceholderText(QStringLiteral("YYYY-MM-DD HH:MM:SS UTC"));
    form->addRow(promptTr("End UTC:"), timeOffEdit);

    auto* commentEdit = new QLineEdit(preview.value(QStringLiteral("comment")).toString().trimmed(),
                                      dialog);
    commentEdit->setClearButtonEnabled(true);
    form->addRow(promptTr("Comment:"), commentEdit);

    auto* satelliteCombo = new QComboBox(dialog);
    satelliteCombo->addItems(satelliteOptions());
    QString const savedSatellite = getSetting(QStringLiteral("Satellite"), QString()).toString().trimmed();
    if (!savedSatellite.isEmpty()) {
        for (int i = 0; i < satelliteCombo->count(); ++i) {
            if (satelliteCodeFromDisplayText(satelliteCombo->itemText(i)) == savedSatellite) {
                satelliteCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    form->addRow(promptTr("Satellite:"), satelliteCombo);

    auto* satModeCombo = new QComboBox(dialog);
    satModeCombo->addItems(satModeOptions());
    QString const savedSatMode = getSetting(QStringLiteral("SatMode"), QString()).toString().trimmed();
    int const satModeIndex = satModeCombo->findText(savedSatMode);
    if (satModeIndex >= 0) {
        satModeCombo->setCurrentIndex(satModeIndex);
    }
    satModeCombo->setEnabled(!satelliteCodeFromDisplayText(satelliteCombo->currentText()).isEmpty());
    connect(satelliteCombo, &QComboBox::currentTextChanged, dialog,
            [satModeCombo](const QString& text) {
        satModeCombo->setEnabled(!satelliteCodeFromDisplayText(text).isEmpty());
    });
    form->addRow(promptTr("Sat Mode:"), satModeCombo);

    bool const clusterAvailable = dxClusterConnected();
    auto* spotCheck = new QCheckBox(clusterAvailable
                                        ? promptTr("Spot to cluster")
                                        : promptTr("Cluster not connected"), dialog);
    spotCheck->setEnabled(clusterAvailable);
    spotCheck->setChecked(clusterAvailable && m_autoSpotEnabled);
    form->addRow(promptTr("DX Cluster:"), spotCheck);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    if (auto* okButton = buttons->button(QDialogButtonBox::Ok)) {
        okButton->setText(promptTr("Add"));
    }
    if (auto* cancelButton = buttons->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(promptTr("Skip"));
    }
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this,
            [this, dialog, satelliteCombo, satModeCombo, spotCheck, commentEdit, timeOnEdit, timeOffEdit]() {
        dialog->setProperty("decodiumAccepted", true);
        setNextLogTimes(timeOnEdit->text(), timeOffEdit->text());
        m_promptLogComment = commentEdit->text().trimmed();
        m_promptLogCommentOverrideValid = true;
        setSetting(QStringLiteral("LogComments"), m_promptLogComment);
        QString const satellite = satelliteCodeFromDisplayText(satelliteCombo->currentText());
        QString const satMode = satellite.isEmpty() ? QString() : satModeCombo->currentText().trimmed();
        if (!satellite.isEmpty()) {
            setSetting(QStringLiteral("Satellite"), satellite);
            setSetting(QStringLiteral("SatMode"), satMode);
            setSetting(QStringLiteral("PropMode"), QStringLiteral("SAT"));
            setSetting(QStringLiteral("SaveSatellite"), true);
            setSetting(QStringLiteral("SaveSatMode"), !satMode.isEmpty());
            setSetting(QStringLiteral("SavePropMode"), true);
        }
        setNextLogClusterSpotEnabled(spotCheck->isEnabled() && spotCheck->isChecked());
        dialog->accept();
        confirmLogQso();
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, this, [this, dialog](int) {
        bool const accepted = dialog->property("decodiumAccepted").toBool();
        m_logQsoPromptDialog.clear();
        if (!accepted && m_logPromptOpen) {
            rejectPromptedLogQso();
        }
    });

    // Questa finestra di riserva e' fatta di widget, non di QML: lasciata a se
    // stessa esce con lo stile nativo del sistema e stona con il resto
    // dell'interfaccia. Le si passano i colori del tema attivo, cosi' anche il
    // ripiego resta in tinta; leggendoli dalla palette segue da solo i temi
    // chiari e quelli scuri.
    if (m_themeManager) {
        auto const hex = [](const QColor& c) { return c.name(QColor::HexRgb); };
        dialog->setStyleSheet(QStringLiteral(
            "QDialog { background-color: %1; }"
            "QLabel { color: %2; background: transparent; }"
            "QCheckBox { color: %2; background: transparent; }"
            "QCheckBox:disabled { color: %3; }"
            "QLineEdit, QComboBox {"
            " background-color: %4; color: %2;"
            " border: 1px solid %5; border-radius: 4px; padding: 4px 6px;"
            " selection-background-color: %6; selection-color: %1; }"
            "QLineEdit:focus, QComboBox:focus { border: 1px solid %6; }"
            "QLineEdit:disabled, QComboBox:disabled { color: %3; }"
            "QComboBox QAbstractItemView {"
            " background-color: %4; color: %2; border: 1px solid %5;"
            " selection-background-color: %6; selection-color: %1; }"
            "QPushButton {"
            " background-color: %4; color: %2; border: 1px solid %5;"
            " border-radius: 4px; padding: 6px 18px; min-width: 84px; }"
            "QPushButton:hover, QPushButton:default { border-color: %6; color: %6; }"
            "QPushButton:pressed { background-color: %5; }")
                                  .arg(hex(m_themeManager->bgDeep()),
                                       hex(m_themeManager->textPrimary()),
                                       hex(m_themeManager->textSecondary()),
                                       hex(m_themeManager->bgMedium()),
                                       hex(m_themeManager->borderColor()),
                                       hex(m_themeManager->accentColor())));
    }

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

