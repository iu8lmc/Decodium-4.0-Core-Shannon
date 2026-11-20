#include <QtCore>
class DecodiumBridge { public: double m_fontScale=1.08; int notifications=0; void fontScaleChanged(){++notifications;} void increaseFontScale(); void decreaseFontScale(); void setFontScale(double); };
#include "bridge_refactor_under_test.inc"
int main(){DecodiumBridge b;QTextStream o(stdout);b.increaseFontScale();o<<qRound(b.m_fontScale*100)<<'|'<<b.notifications<<'\n';b.decreaseFontScale();o<<qRound(b.m_fontScale*100)<<'|'<<b.notifications<<'\n';b.setFontScale(20);b.setFontScale(3);o<<qRound(b.m_fontScale*100)<<'|'<<b.notifications<<'\n';b.setFontScale(-20);b.setFontScale(-1);o<<qRound(b.m_fontScale*100)<<'|'<<b.notifications<<'\n';}
