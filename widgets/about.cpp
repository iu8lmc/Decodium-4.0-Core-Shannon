#include "about.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>

#include "revision_utils.hpp"

#include "ui_about.h"

CAboutDlg::CAboutDlg(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::CAboutDlg)
{
  ui->setupUi(this);

  ui->labelTxt->setText(
    // Decodium FT2 logo on top
    "<p align=\"center\"><img src=\":/ft2logo.png\" width=\"180\" height=\"180\" /></p>"
    "<h2 align=\"center\">Decodium</h2>"
    "<p align=\"center\" style=\"font-size:14px;\"><b>Fork " FORK_RELEASE_VERSION " by Salvatore Raccampo 9H1SR</b></p>"
    "<hr>"
    "<p align=\"center\"><b>Mod by IU8LMC</b> - <a href=\"https://www.qrz.com/db/IU8LMC\">qrz.com/db/IU8LMC</a></p>"
    "<p>Decodium is a weak-signal digital communication client focused on FT2, "
    "with FT4 and FT8 support in this fork for monitoring and decoder parity.</p>"
    "<p><b>Fork " FORK_RELEASE_VERSION " highlights:</b> promoted native C++ runtime for FT8, FT4, FT2, and Q65, "
    "worker-based in-process decoding, hardened FT2/FT4/Fox transmit wave handling, "
    "expanded parity/build validation on macOS and Linux, and the experimental public RTTY path still hidden pending validation.</p>"
    "<hr>"
    "<p>&copy; 2001-2026 by Joe Taylor, K1JT, Bill Somerville, G4WJS, "
    "Steve Franke, K9AN, Nico Palermo, IV3NWV, "
    "Uwe Risse, DG2YCB, Brian Moran, N9ADG, "
    "and Roger Rehr, W3SZ.</p>"
    "<p>We gratefully acknowledge contributions from AC6SL, AE4JY, "
    "DF2ET, DJ0OT, DL3WDG, EA4AC, G4KLA, IW3RAB, JA7UDE, "
    "K3WYC, KA1GT, KA6MAL, KA9Q, KB1ZMX, KD6EKQ, KG4IYS, KI7MT, "
    "KK1D, ND0B, PY1ZRJ, PY2SDR, VE1SKY, VK3ACF, VK4BDJ, "
    "VK7MO, VR2UPU, W3DJS, W4TI, W4TV, and W9MDB.</p>"
    "<hr>"
    "<p><b>Bundled Tools:</b></p>"
    "<p><b>ChronoGPS v2.4.6</b> — GPS/NTP time synchronization<br>"
    "by Yoshiharu Tsukuura, <b>JP1LRT</b> — "
    "<a href=\"https://github.com/jp1lrt/ChronoGPS\">github.com/jp1lrt/ChronoGPS</a><br>"
    "MIT License &copy; 2026 JP1LRT</p>"
    "<p align=\"center\"><img src=\":/icon_128x128.png\" /></p>"
    "<p>Decodium is licensed under the terms of Version 3 "
    "of the <a href=\"https://www.gnu.org/licenses/gpl-3.0.txt\">GNU General Public License (GPL)</a></p>");
}

CAboutDlg::~CAboutDlg()
{
}
