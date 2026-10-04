#include "FlpAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

FlpAnalyzerSettings::FlpAnalyzerSettings()
    : mInputChannel(UNDEFINED_CHANNEL),
      mPolarity(0),
      mDecodeNlps(false),
      mInputChannelInterface(),
      mPolarityInterface(),
      mDecodeNlpsInterface() {
    mInputChannelInterface.SetTitleAndTooltip("Input", "Pair the probe is clipped to.");
    mInputChannelInterface.SetChannel(mInputChannel);
    mInputChannelInterface.SetSelectionOfNoneIsAllowed(false);

    mPolarityInterface.SetTitleAndTooltip("Polarity", "Active level of a link pulse on this channel.");
    mPolarityInterface.AddNumber(0.0, "Active high", "Pulses are high. Idle is low.");
    mPolarityInterface.AddNumber(1.0, "Active low", "Pulses are low. Idle is high.");
    mPolarityInterface.SetNumber(mPolarity);

    mDecodeNlpsInterface.SetTitleAndTooltip(
        "NLPs", "A Normal Link Pulse is one pulse about every 16 ms once the link is up.");
    mDecodeNlpsInterface.SetCheckBoxText("Decode NLPs");
    mDecodeNlpsInterface.SetValue(mDecodeNlps);

    AddInterface(&mInputChannelInterface);
    AddInterface(&mPolarityInterface);
    AddInterface(&mDecodeNlpsInterface);

    AddExportOption(0, "Export as text/csv file");
    AddExportExtension(0, "csv", "csv");
    AddExportExtension(0, "text", "txt");

    ClearChannels();
    AddChannel(mInputChannel, "FLP", false);
}

FlpAnalyzerSettings::~FlpAnalyzerSettings() {
}

bool FlpAnalyzerSettings::SetSettingsFromInterfaces() {
    mInputChannel = mInputChannelInterface.GetChannel();
    mPolarity = static_cast<U32>(mPolarityInterface.GetNumber());
    if (mPolarity != 0 && mPolarity != 1)
        mPolarity = 0;
    mDecodeNlps = mDecodeNlpsInterface.GetValue();

    if (mInputChannel == UNDEFINED_CHANNEL) {
        SetErrorText("Select the channel the pair is connected to.");
        return false;
    }

    ClearChannels();
    AddChannel(mInputChannel, "FLP", true);
    return true;
}

void FlpAnalyzerSettings::UpdateInterfacesFromSettings() {
    mInputChannelInterface.SetChannel(mInputChannel);
    mPolarityInterface.SetNumber(mPolarity);
    mDecodeNlpsInterface.SetValue(mDecodeNlps);
}

void FlpAnalyzerSettings::LoadSettings(const char* settings) {
    SimpleArchive text_archive;
    text_archive.SetString(settings);

    text_archive >> mInputChannel;
    if (!(text_archive >> mPolarity) || (mPolarity != 0 && mPolarity != 1))
        mPolarity = 0;
    // Older saved settings have no checkbox. Leave NLPs off.
    if (!(text_archive >> mDecodeNlps))
        mDecodeNlps = false;

    ClearChannels();
    AddChannel(mInputChannel, "FLP", true);

    UpdateInterfacesFromSettings();
}

const char* FlpAnalyzerSettings::SaveSettings() {
    SimpleArchive text_archive;

    text_archive << mInputChannel;
    text_archive << mPolarity;
    text_archive << mDecodeNlps;

    return SetReturnString(text_archive.GetString());
}
