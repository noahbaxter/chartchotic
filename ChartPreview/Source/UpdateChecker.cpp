#include "UpdateChecker.h"

#ifdef CHARTPREVIEW_VERSION_STRING
  #define CHART_PREVIEW_VERSION CHARTPREVIEW_VERSION_STRING
#elif defined(JucePlugin_VersionString)
  #define CHART_PREVIEW_VERSION JucePlugin_VersionString
#else
  #define CHART_PREVIEW_VERSION "dev"
#endif

UpdateChecker::UpdateChecker() : Thread("UpdateChecker") {}

UpdateChecker::~UpdateChecker()
{
    cancelCallbacks();
    stopThread(5000);
}

void UpdateChecker::checkForUpdates()
{
    if (checking.load())
        return;

    checking.store(true);
    startThread(juce::Thread::Priority::low);
}

UpdateChecker::UpdateInfo UpdateChecker::getLatestUpdateInfo() const
{
    juce::ScopedLock sl(lock);
    return lastResult;
}

bool UpdateChecker::isChecking() const
{
    return checking.load();
}

void UpdateChecker::run()
{
    UpdateInfo result;

    juce::String channel(CHARTPREVIEW_BUILD_CHANNEL);
    if (channel == "DEV")
        result = checkDevChannel();
    else
        result = checkReleaseChannel();

    {
        juce::ScopedLock sl(lock);
        lastResult = result;
    }

    checking.store(false);

    if (callbacksEnabled->load() && onUpdateCheckComplete)
    {
        auto callback = onUpdateCheckComplete;
        auto enabled = callbacksEnabled;  // shared_ptr copy, outlives this
        juce::MessageManager::callAsync([callback, result, enabled]()
        {
            if (enabled->load() && callback)
                callback(result);
        });
    }
}

UpdateChecker::UpdateInfo UpdateChecker::checkReleaseChannel()
{
    UpdateInfo info;
    juce::URL url(juce::String(GITHUB_API_BASE) + "/releases/latest");

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withHttpRequestCmd("GET")
                       .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: ChartPreview-UpdateChecker");

    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return info;

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse(response);

    if (!json.isObject())
        return info;

    auto tagName = json.getProperty("tag_name", "").toString();
    if (tagName.isEmpty())
        return info;

    // Strip leading 'v' for comparison
    juce::String remoteVersion = tagName.startsWith("v") ? tagName.substring(1) : tagName;
    juce::String localVersion(CHART_PREVIEW_VERSION);

    if (isNewerVersion(remoteVersion, localVersion))
    {
        info.available = true;
        info.version = tagName;
        info.releaseNotes = json.getProperty("body", "").toString();
        info.downloadUrl = getInstallerUrl(json.getProperty("assets", juce::var()));

        // Fallback to HTML release page if no matching asset
        if (info.downloadUrl.isEmpty())
            info.downloadUrl = json.getProperty("html_url", "").toString();
    }

    return info;
}

UpdateChecker::UpdateInfo UpdateChecker::checkDevChannel()
{
    UpdateInfo info;
    juce::URL url(juce::String(GITHUB_API_BASE) + "/releases/tags/dev-latest");

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withHttpRequestCmd("GET")
                       .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: ChartPreview-UpdateChecker");

    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return info;

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse(response);

    if (!json.isObject())
        return info;

    // For dev builds, check if the release title contains a newer version string
    auto title = json.getProperty("name", "").toString();

    // Extract version from title like "Dev Build (0.9.5-dev.20260226123456.abc1234)"
    auto openParen = title.indexOfChar('(');
    auto closeParen = title.indexOfChar(')');
    if (openParen >= 0 && closeParen > openParen)
    {
        auto remoteDevVersion = title.substring(openParen + 1, closeParen);
        juce::String localVersion(CHART_PREVIEW_VERSION);

        // Dev builds: any different version string means an update is available
        if (remoteDevVersion != localVersion)
        {
            info.available = true;
            info.version = remoteDevVersion;
            info.downloadUrl = getInstallerUrl(json.getProperty("assets", juce::var()));

            if (info.downloadUrl.isEmpty())
                info.downloadUrl = json.getProperty("html_url", "").toString();
        }
    }

    return info;
}

bool UpdateChecker::isNewerVersion(const juce::String& remote, const juce::String& local) const
{
    auto remoteParts = juce::StringArray::fromTokens(remote, ".", "");
    auto localParts = juce::StringArray::fromTokens(local, ".", "");

    for (int i = 0; i < std::max(remoteParts.size(), localParts.size()); ++i)
    {
        int r = (i < remoteParts.size()) ? remoteParts[i].getIntValue() : 0;
        int l = (i < localParts.size()) ? localParts[i].getIntValue() : 0;

        if (r > l) return true;
        if (r < l) return false;
    }

    return false;
}

juce::String UpdateChecker::getInstallerUrl(const juce::var& assets) const
{
    if (!assets.isArray())
        return {};

    auto* assetsArray = assets.getArray();
    if (assetsArray == nullptr)
        return {};

#if JUCE_MAC
    juce::String pattern = ".pkg";
#elif JUCE_WINDOWS
    juce::String pattern = "Installer.exe";
#else
    juce::String pattern = "Linux.zip";
#endif

    // Prefer installer, fall back to zip
    for (const auto& asset : *assetsArray)
    {
        auto name = asset.getProperty("name", "").toString();
        if (name.contains(pattern))
            return asset.getProperty("browser_download_url", "").toString();
    }

    // Fallback: any platform-matching zip
#if JUCE_MAC
    juce::String fallbackPattern = "macOS";
#elif JUCE_WINDOWS
    juce::String fallbackPattern = "Windows";
#else
    juce::String fallbackPattern = "Linux";
#endif

    for (const auto& asset : *assetsArray)
    {
        auto name = asset.getProperty("name", "").toString();
        if (name.contains(fallbackPattern))
            return asset.getProperty("browser_download_url", "").toString();
    }

    return {};
}
