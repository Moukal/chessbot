#include "Configuration.hpp"
#include "Logger.hpp"
#include <fstream>

Configuration::Configuration(const std::filesystem::path& configPath)
    : m_path(configPath)
{
    m_json = {
        {"enginePath", "stockfish"},
        {"engineThreads", 4},
        {"engineHashMb", 256},
        {"engineSkillLevel", 20},
        {"moveTimeMs", 1000},
        {"multiPV", 3},
        {"autoPlay", false},
        {"mouseDelayMinMs", 300},
        {"mouseDelayMaxMs", 900},
        {"openingBookPath", ""},
        {"boardRect", {{"x",0},{"y",0},{"w",0},{"h",0}}},
        {"playAsWhite", true}
    };
}

void Configuration::load() {
    if (!std::filesystem::exists(m_path)) {
        LOG_INFO("No config file found at {}, using defaults", m_path.string());
        return;
    }
    try {
        std::ifstream f(m_path);
        nlohmann::json loaded = nlohmann::json::parse(f);
        m_json.merge_patch(loaded);
        LOG_INFO("Config loaded from {}", m_path.string());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load config: {}", e.what());
    }
}

void Configuration::save() const {
    try {
        std::filesystem::create_directories(m_path.parent_path());
        std::ofstream f(m_path);
        f << m_json.dump(2);
        LOG_INFO("Config saved to {}", m_path.string());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save config: {}", e.what());
    }
}

std::string Configuration::enginePath() const { return get<std::string>("enginePath", "stockfish"); }
void Configuration::setEnginePath(const std::string& p) { m_json["enginePath"] = p; }

int Configuration::engineThreads() const { return get<int>("engineThreads", 4); }
void Configuration::setEngineThreads(int n) { m_json["engineThreads"] = n; }

int Configuration::engineHashMb() const { return get<int>("engineHashMb", 256); }
void Configuration::setEngineHashMb(int mb) { m_json["engineHashMb"] = mb; }

int Configuration::engineSkillLevel() const { return get<int>("engineSkillLevel", 20); }
void Configuration::setEngineSkillLevel(int l) { m_json["engineSkillLevel"] = l; }

int Configuration::moveTimeMs() const { return get<int>("moveTimeMs", 1000); }
void Configuration::setMoveTimeMs(int ms) { m_json["moveTimeMs"] = ms; }

int Configuration::multiPV() const { return get<int>("multiPV", 3); }
void Configuration::setMultiPV(int n) { m_json["multiPV"] = n; }

bool Configuration::autoPlay() const { return get<bool>("autoPlay", false); }
void Configuration::setAutoPlay(bool v) { m_json["autoPlay"] = v; }

int Configuration::mouseDelayMinMs() const { return get<int>("mouseDelayMinMs", 300); }
int Configuration::mouseDelayMaxMs() const { return get<int>("mouseDelayMaxMs", 900); }
void Configuration::setMouseDelay(int minMs, int maxMs) {
    m_json["mouseDelayMinMs"] = minMs;
    m_json["mouseDelayMaxMs"] = maxMs;
}

std::string Configuration::openingBookPath() const { return get<std::string>("openingBookPath", ""); }
void Configuration::setOpeningBookPath(const std::string& p) { m_json["openingBookPath"] = p; }

nlohmann::json Configuration::boardRect() const {
    return m_json.value("boardRect", nlohmann::json({{"x",0},{"y",0},{"w",0},{"h",0}}));
}
void Configuration::setBoardRect(int x, int y, int w, int h) {
    m_json["boardRect"] = {{"x",x},{"y",y},{"w",w},{"h",h}};
}
void Configuration::setBoardRect(const nlohmann::json& rect) {
    m_json["boardRect"] = rect;
}

bool Configuration::playAsWhite() const { return get<bool>("playAsWhite", true); }
void Configuration::setPlayAsWhite(bool v) { m_json["playAsWhite"] = v; }
