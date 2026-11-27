// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumTransceiverManager.h"
#include "DecoPortLink.h"
#include "DecolinkLink.h"
#include "src/rtl/RtlSdrAudioOutput.h"
#include <QAudioDevice>
#include <QPointer>
#include <QSettings>
#include <algorithm>
#include <cmath>

bool decodiumBridgeSharedCatCanPttExplicit(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*, DecodiumLegacyBackend*, DecodiumCat4OmManager*);
#include "SsbBridge.inc"
