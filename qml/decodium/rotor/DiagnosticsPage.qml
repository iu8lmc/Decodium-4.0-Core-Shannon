import QtQuick
import QtQuick.Layouts

RowLayout {
    id: page

    spacing: RotorTheme.spacing

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 620
        Layout.minimumWidth: 340
        spacing: RotorTheme.spacing

        TrafficPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
        }

        HistoryStrip {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            Layout.minimumHeight: 110
        }
    }

    ColumnLayout {
        Layout.fillHeight: true
        Layout.preferredWidth: 380
        Layout.minimumWidth: 320
        spacing: RotorTheme.spacing

        StatsPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
        }

        NetworkPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 180
        }
    }
}
