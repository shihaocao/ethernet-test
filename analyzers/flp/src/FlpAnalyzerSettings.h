#ifndef FLP_ANALYZER_SETTINGS_H
#define FLP_ANALYZER_SETTINGS_H

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

class FlpAnalyzerSettings : public AnalyzerSettings {
  public:
    FlpAnalyzerSettings();
    virtual ~FlpAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    void UpdateInterfacesFromSettings();
    virtual void LoadSettings(const char* settings);
    virtual const char* SaveSettings();

    Channel mInputChannel;
    U32 mPolarity; // 0 = active high, 1 = active low
    bool mDecodeNlps;

  protected:
    AnalyzerSettingInterfaceChannel mInputChannelInterface;
    AnalyzerSettingInterfaceNumberList mPolarityInterface;
    AnalyzerSettingInterfaceBool mDecodeNlpsInterface;
};

#endif
