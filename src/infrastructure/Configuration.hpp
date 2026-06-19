#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>

class Configuration {
public:
    explicit Configuration(const std::filesystem::path& configPath);

    void load();
    void save() const;

    std::string enginePath() const;
    void setEnginePath(const std::string& path);

    int engineThreads() const;
    void setEngineThreads(int n);

    int engineHashMb() const;
    void setEngineHashMb(int mb);

    int engineSkillLevel() const;
    void setEngineSkillLevel(int level);

    int moveTimeMs() const;
    void setMoveTimeMs(int ms);

    int multiPV() const;
    void setMultiPV(int n);

    bool autoPlay() const;
    void setAutoPlay(bool v);

    int mouseDelayMinMs() const;
    int mouseDelayMaxMs() const;
    void setMouseDelay(int minMs, int maxMs);

    std::string openingBookPath() const;
    void setOpeningBookPath(const std::string& path);

    nlohmann::json boardRect() const;
    void setBoardRect(int x, int y, int w, int h);
    void setBoardRect(const nlohmann::json& rect);

    bool playAsWhite() const;
    void setPlayAsWhite(bool v);

    const nlohmann::json& raw() const { return m_json; }

private:
    std::filesystem::path m_path;
    nlohmann::json m_json;

    template<typename T>
    T get(const std::string& key, T defaultVal) const {
        if (m_json.contains(key))
            return m_json[key].get<T>();
        return defaultVal;
    }
};
