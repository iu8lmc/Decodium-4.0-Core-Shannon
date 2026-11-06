// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/core/SstvModeRegistry.h"
#include "tools/SstvModeMatrixFormat.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <algorithm>
#include <cstddef>
#include <string>

#ifndef DECODIUM_SSTV_MODE_CATALOG_PATH
#error "DECODIUM_SSTV_MODE_CATALOG_PATH must name the canonical catalogue"
#endif

#ifndef DECODIUM_SSTV_MODE_MATRIX_PATH
#error "DECODIUM_SSTV_MODE_MATRIX_PATH must name the evidence matrix"
#endif

using namespace decodium::sstv;

namespace {

constexpr qint64 kMaximumDocumentationBytes = 4 * 1'024 * 1'024;

QString readDocumentation(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 1
        || file.size() > kMaximumDocumentationBytes) {
        return {};
    }
    const QByteArray bytes = file.read(kMaximumDocumentationBytes + 1);
    if (bytes.size() != file.size()) {
        return {};
    }
    return QString::fromUtf8(bytes);
}

int exactMarkdownRowCount(const QString& document,
                          const QString& firstCell,
                          bool codeCell)
{
    const QString value = QRegularExpression::escape(firstCell);
    const QString expression = codeCell
        ? QStringLiteral("(?m)^\\|\\s*`%1`\\s*\\|").arg(value)
        : QStringLiteral("(?m)^\\|\\s*%1\\s*\\|").arg(value);
    QRegularExpression regex(expression);
    if (!regex.isValid()) {
        return -1;
    }
    int count = 0;
    auto matches = regex.globalMatch(document);
    while (matches.hasNext()) {
        matches.next();
        ++count;
    }
    return count;
}

} // namespace

class TestSstvModeDocs final : public QObject
{
    Q_OBJECT

private slots:
    void catalogRowsExactlyMatchCanonicalRegistry();
    void implementedModesHaveExactlyOneEvidenceRow();
};

void TestSstvModeDocs::catalogRowsExactlyMatchCanonicalRegistry()
{
    const QString catalog = readDocumentation(QStringLiteral(
        DECODIUM_SSTV_MODE_CATALOG_PATH));
    QVERIFY2(!catalog.isEmpty(), "MODE_CATALOG.md is missing or oversized");
    const qsizetype digitalSection = catalog.indexOf(QStringLiteral(
        "\n## Digital protocols"));
    QVERIFY2(digitalSection > 0,
             "MODE_CATALOG.md has no separate digital-protocol section");
    // HAMDRM and KG-STV are intentionally documented after this boundary and
    // belong to their separate digital profile registry.  Only the analog
    // SSTV/related-FAX prefix must exactly match SstvModeRegistry.
    const QString analogCatalog = catalog.left(digitalSection);

    const SstvModeRegistry registry = SstvModeRegistry::canonical();
    const std::vector<ModeValidationIssue> issues =
        registry.validationIssues();
    QStringList issueMessages;
    for (const ModeValidationIssue& issue : issues) {
        issueMessages.append(QStringLiteral("%1: %2")
            .arg(QString::fromStdString(issue.modeId),
                 QString::fromStdString(issue.message)));
    }
    QVERIFY2(issues.empty(), qPrintable(issueMessages.join(
        QStringLiteral("; "))));
    QSet<QString> expected;
    for (const SstvModeSpec& mode : registry.modes()) {
        const QString id = QString::fromStdString(mode.id);
        QVERIFY2(!expected.contains(id), qPrintable(id));
        expected.insert(id);
        const int rowCount = exactMarkdownRowCount(analogCatalog, id, true);
        QVERIFY2(rowCount == 1, qPrintable(QStringLiteral(
            "MODE_CATALOG row count for '%1' is %2, expected 1")
            .arg(id).arg(rowCount)));
    }

    const QRegularExpression row(
        QStringLiteral("(?m)^\\|\\s*`([^`]+)`\\s*\\|"));
    QVERIFY(row.isValid());
    QSet<QString> documented;
    auto matches = row.globalMatch(analogCatalog);
    while (matches.hasNext()) {
        const QString id = matches.next().captured(1);
        QVERIFY2(!documented.contains(id), qPrintable(id));
        documented.insert(id);
    }
    QCOMPARE(documented, expected);
}

void TestSstvModeDocs::implementedModesHaveExactlyOneEvidenceRow()
{
    const QString matrix = readDocumentation(QStringLiteral(
        DECODIUM_SSTV_MODE_MATRIX_PATH));
    QVERIFY2(!matrix.isEmpty(), "MODE_MATRIX.md is missing or oversized");

    const QString expectedHeader = QStringLiteral(
        "| Mode | Registry ID | Family/class | Dimensions | "
        "Nominal duration (s) | VIS | RX | TX | Auto detect | QSSTV | "
        "Robot36 / SlowRX | Registry interoperability | Evidence status | "
        "Independent Decodium vector/test | Current blocker or next proof |");
    QVERIFY2(matrix.contains(expectedHeader),
             "MODE_MATRIX.md lacks the canonical explicit-column header");

    const QStringList lines = matrix.split(QLatin1Char('\n'));
    const SstvModeRegistry registry = SstvModeRegistry::canonical();
    for (const SstvModeSpec& mode : registry.modes()) {
        const QString longName = QString::fromStdString(mode.longName);
        const int rowCount = exactMarkdownRowCount(matrix, longName, false);
        QVERIFY2(rowCount == 1, qPrintable(QStringLiteral(
            "MODE_MATRIX row count for '%1' is %2, expected 1")
            .arg(longName).arg(rowCount)));

        QStringList cells;
        for (const QString& line : lines) {
            if (line.startsWith(QStringLiteral("| %1 |").arg(longName))) {
                cells = line.split(QLatin1Char('|'), Qt::KeepEmptyParts);
                break;
            }
        }
        QVERIFY2(cells.size() == 17, qPrintable(QStringLiteral(
            "MODE_MATRIX canonical row for '%1' has %2 cells, expected 15")
            .arg(longName)
            .arg(static_cast<qlonglong>(cells.size() > 2
                                            ? cells.size() - 2
                                            : 0))));
        for (QString& cell : cells) {
            cell = cell.trimmed();
        }
        QCOMPARE(cells.at(2), QStringLiteral("`%1`").arg(
            QString::fromStdString(mode.id)));
        QCOMPARE(cells.at(4), mode_matrix::dimensions(mode));
        QCOMPARE(cells.at(5), mode_matrix::nominalDurationSeconds(mode));
        QCOMPARE(cells.at(6), mode_matrix::vis(mode));
        QCOMPARE(cells.at(7), mode_matrix::capability(mode.rxStatus));
        QCOMPARE(cells.at(8), mode_matrix::capability(mode.txStatus));
        QCOMPARE(cells.at(9), mode_matrix::capability(
            mode.autoDetectStatus));
        QCOMPARE(cells.at(10),
                 mode_matrix::externalApplicationStatus(mode));
        QCOMPARE(cells.at(11),
                 mode_matrix::externalApplicationStatus(mode));
        QCOMPARE(cells.at(12), mode_matrix::interoperability(
            mode.interoperabilityStatus));
        QCOMPARE(cells.at(13), mode_matrix::evidence(mode.evidenceStatus));
    }
}

QTEST_APPLESS_MAIN(TestSstvModeDocs)

#include "test_sstv_mode_docs.moc"
