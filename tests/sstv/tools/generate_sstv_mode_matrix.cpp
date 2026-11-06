// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvModeMatrixFormat.h"
#include "src/sstv/core/SstvModeRegistry.h"

#include <QCoreApplication>
#include <QTextStream>

using namespace decodium::sstv;

namespace {

QString markdownCell(QString value)
{
    value.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value.trimmed();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)
    QTextStream output(stdout);
    output << "| Mode | Registry ID | Family/class | Dimensions | "
              "Nominal duration (s) | VIS | RX | TX | Auto detect | QSSTV | "
              "Robot36 / SlowRX | Registry interoperability | Evidence status | "
              "Implementation evidence | Current limitation |\n"
              "|---|---|---|---:|---:|---|---:|---:|---:|---|---|---|---|---|---|\n";
    const SstvModeRegistry registry = SstvModeRegistry::canonical();
    for (const SstvModeSpec& mode : registry.modes()) {
        const QString appStatus = mode_matrix::externalApplicationStatus(mode);
        output << "| " << QString::fromStdString(mode.longName)
               << " | `" << QString::fromStdString(mode.id)
               << "` | " << QString::fromStdString(mode.family)
               << (mode.classification == ModeClassification::RelatedFax
                       ? "/related FAX" : "/analog")
               << " | " << mode_matrix::dimensions(mode)
               << " | " << mode_matrix::nominalDurationSeconds(mode)
               << " | " << mode_matrix::vis(mode)
               << " | " << mode_matrix::capability(mode.rxStatus)
               << " | " << mode_matrix::capability(mode.txStatus)
               << " | " << mode_matrix::capability(mode.autoDetectStatus)
               << " | " << appStatus
               << " | " << appStatus
               << " | " << mode_matrix::interoperability(
                              mode.interoperabilityStatus)
               << " | " << mode_matrix::evidence(mode.evidenceStatus)
               << " | " << mode.implementationEvidenceRefs.size()
               << " registry refs"
               << " | " << markdownCell(QString::fromStdString(
                              mode.statusNote))
               << " |\n";
    }
    return registry.isValid() ? 0 : 1;
}
