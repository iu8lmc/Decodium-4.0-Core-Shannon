// DecoRTTY — the received text, as a model of lines.
//
// RTTY arrives one character at a time and a session can run for hours, so the
// receive window is a list of lines with a bounded history rather than one
// ever-growing string. Each line remembers the decoder's confidence, which the
// interface uses to fade uncertain text instead of presenting garbage with the
// same authority as a solid copy.
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace decortty::app {

class ReceiveTextModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int maxLines READ maxLines WRITE setMaxLines NOTIFY maxLinesChanged)
    Q_PROPERTY(int wrapColumn READ wrapColumn WRITE setWrapColumn NOTIFY wrapColumnChanged)

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        QualityRole,        // 0..1, averaged over the line
        CorrectedRole,      // the search overruled at least one character here
        TimestampRole,
        TransmittedRole,    // our own transmission, echoed back into the flow
    };

    explicit ReceiveTextModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int  maxLines() const { return m_maxLines; }
    void setMaxLines(int lines);
    int  wrapColumn() const { return m_wrapColumn; }
    void setWrapColumn(int column);

    // Append one received character. CR and LF both end the line; a line is
    // also broken at wrapColumn so a station that never sends a line ending
    // does not produce one unreadably long row.
    void appendCharacter(QChar c, double quality, bool corrected);
    // Append a character we transmitted, so the operator reads the QSO as one
    // conversation instead of two separate windows.
    void appendTransmitted(QChar c);

    Q_INVOKABLE void clear();
    // The whole buffer as plain text, for the clipboard or a log file.
    Q_INVOKABLE QString plainText() const;

signals:
    void maxLinesChanged();
    void wrapColumnChanged();
    void lineAdded();

private:
    struct Line {
        QString   text;
        double    qualitySum{0.0};
        int       qualityCount{0};
        bool      corrected{false};
        bool      transmitted{false};
        QDateTime timestamp;

        double quality() const { return qualityCount ? qualitySum / qualityCount : 1.0; }
    };

    void startLine(bool transmitted);
    void trim();
    void touchLast();

    QVector<Line> m_lines;
    int m_maxLines{2000};
    int m_wrapColumn{80};
};

} // namespace decortty::app
