#pragma once
#include "storage.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <mutex>
#include <string>

namespace booking {

    class FileJsonStorage: public IStorage {
    public:
        explicit FileJsonStorage(std::filesystem::path snapshot_path, std::filesystem::path journal_path);
        nlohmann::json loadState() override;
        void saveState(const nlohmann::json& snapshot) override;
        std::vector<nlohmann::json> loadJournal() override;
        void appendJournal(const nlohmann::json& entry) override;

    private:
        std::filesystem::path snapshot_path_;
        std::filesystem::path journal_path_;
        std::mutex mutex_;
        void atomicWrite(const std::filesystem::path& path, const nlohmann::json& j);
    };
} // namespace booking
