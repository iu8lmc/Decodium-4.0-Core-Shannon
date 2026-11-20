#include <QtCore>
#include <cstdio>
namespace decodium { static void beginActiveSettingsProfile(QSettings& s) {s.beginGroup("test-profile");} }
class DecodiumBridge { public:
#define COLOR(x) QString m_##x=QStringLiteral(#x)
 COLOR(colorTxMessage); COLOR(colorMyCall); COLOR(colorNewDxccBand); COLOR(colorNewDxcc);
 COLOR(colorNewContinentBand); COLOR(colorNewContinent); COLOR(colorNewCqZoneBand); COLOR(colorNewCqZone);
 COLOR(colorNewItuZoneBand); COLOR(colorNewItuZone); COLOR(colorNewGridBand); COLOR(colorNewGrid);
 COLOR(colorNewCallBand); COLOR(colorNewCall); COLOR(colorLotwUser); COLOR(colorCQ); COLOR(colorDXEntity);
 COLOR(color73); COLOR(colorB4); COLOR(colorDecodeText);
#undef COLOR
 QMap<QString,bool> m_decodeColorEnabled,m_decodeColorBold,m_decodeColorBgEnabled;
 QMap<QString,QString> m_decodeColorBg; QMap<QString,QVariant> legacy;
 bool m_b4Strikethrough=false; int changes=0,bgChanges=0;
 void syncSettingToLegacyIni(const QString& key,const QVariant& v){legacy[key]=v;}
 void b4StrikethroughChanged(){++changes;} void settingValueChanged(const QString&,const QVariant&){++changes;}
 void decodeColorEnabledChanged(const QString&,bool){++changes;} void decodeColorBoldChanged(const QString&,bool){++changes;}
 void decodeColorBgChanged(){++bgChanges;}
 QString decodeColorFallback()const{return QStringLiteral("fallback");}
 QString decodeColorValue(const QString&)const; bool decodeColorEnabled(const QString&)const;
 void setB4Strikethrough(bool); void setDecodeColorEnabled(const QString&,bool);
 bool decodeColorBold(const QString&)const;void setDecodeColorBold(const QString&,bool);
 QString effectiveDecodeColor(const QString&)const;QString decodeHighlightBg(const QVariantMap&)const;
 QString decodeColorBgValue(const QString&)const;bool decodeColorBgEnabled(const QString&)const;
 void setDecodeColorBg(const QString&,const QString&);void setDecodeColorBgEnabled(const QString&,bool);
 QString decodeHighlightUserBg(const QVariantMap&)const;
};
#include "bridge_refactor_decode_colors.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;check(tmp.isValid(),"isolated");QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());DecodiumBridge b;
 check(decodeColorPropertyNames().size()==20,"all-properties");for(const QString& p:decodeColorPropertyNames())check(b.decodeColorValue(p)==p,"property-value");
 check(b.decodeColorEnabled("invalid")&&!b.decodeColorBold("invalid")&&b.effectiveDecodeColor("invalid").isEmpty(),"invalid-property");
 check(b.decodeColorBold("colorCQ")&&!b.decodeColorBold("color73")&&b.decodeColorBold("colorNewGrid"),"bold-defaults");
 b.setDecodeColorEnabled("colorCQ",false);check(!b.decodeColorEnabled("colorCQ")&&b.effectiveDecodeColor("colorCQ")=="fallback"&&b.legacy.value("colorCQEnabled")==false,"disable-color");int n=b.changes;b.setDecodeColorEnabled("colorCQ",false);check(b.changes==n,"unchanged-no-signal");
 b.setDecodeColorBold("color73",true);check(b.decodeColorBold("color73")&&b.legacy.value("bold_color73")==true,"set-bold");b.setB4Strikethrough(true);check(b.m_b4Strikethrough&&b.legacy.value("b4Strikethrough")==true,"strikethrough");
 QVariantMap e{{"isTx",true},{"isCQ",true},{"dxIsNewGrid",true}};check(b.decodeHighlightBg(e)=="colorTxMessage","tx-precedence");e.remove("isTx");check(b.decodeHighlightBg(e).isEmpty(),"disabled-cq-precedence");e.remove("isCQ");check(b.decodeHighlightBg(e)=="colorNewGrid","new-grid");
 b.setDecodeColorBg("colorCQ","#123456");b.setDecodeColorBgEnabled("colorCQ",true);check(b.decodeColorBgValue("colorCQ")=="#123456"&&b.decodeColorBgEnabled("colorCQ"),"background-settings");e.insert("isCQ",true);check(b.decodeHighlightUserBg(e)=="#123456","background-cq");b.setDecodeColorBg("color73","#ffffff");b.setDecodeColorBgEnabled("color73",true);e.insert("message","CQ 73");check(b.decodeHighlightUserBg(e)=="#ffffff","signoff-before-cq");e.insert("isTx",true);check(b.decodeHighlightUserBg(e).isEmpty(),"tx-before-signoff");
 {QSettings s(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(s);check(s.value("colorCQEnabled")==false&&s.value("bold_color73")==true&&s.value("bg_colorCQ")=="#123456"&&s.value("bgEnabled_colorCQ")==true,"persisted");}
}
