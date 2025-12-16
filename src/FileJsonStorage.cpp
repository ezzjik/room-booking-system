#include <FileJsonStorage.hpp>
#include <fstream>
#include <system_error>

namespace booking {

    FileJsonStorage::FileJsonStorage(std::filesystem::path snapshot_path, std::filesystem::path journal_path)
        : snapshot_path_(std::move(snapshot_path))
        , journal_path_(std::move(journal_path)) {
    }

    void FileJsonStorage::atomicWrite(const std::filesystem::path& file_path, const nlohmann::json& json_data) {
        std::error_code error;
        auto temporary_path = file_path;
        temporary_path += ".tmp";

        std::ofstream output_stream(temporary_path, std::ios::trunc);
        if (!output_stream) {
            throw std::runtime_error("Cannot open temporary file for writing: " + temporary_path.string());
        }
        output_stream << json_data.dump(2);
        output_stream.close();

        std::filesystem::rename(temporary_path, file_path, error);
        if (error) {
            std::filesystem::remove(temporary_path);
            throw std::runtime_error("Atomic rename failed: " + error.message());
        }
    }

    nlohmann::json FileJsonStorage::loadState() {
        std::scoped_lock guard(mutex_);
        if (!std::filesystem::exists(snapshot_path_)) {
            return nlohmann::json::object();
        }
        std::ifstream input_stream(snapshot_path_);
        if (!input_stream) {
            return nlohmann::json::object();
        }
        nlohmann::json json_data;
        input_stream >> json_data;
        return json_data;
    }

    void FileJsonStorage::saveState(const nlohmann::json& snapshot) {
        std::scoped_lock guard(mutex_);
        if (!snapshot_path_.parent_path().empty()) {
            std::filesystem::create_directories(snapshot_path_.parent_path());
        }
        atomicWrite(snapshot_path_, snapshot);
    }

    std::vector<nlohmann::json> FileJsonStorage::loadJournal() {
        std::scoped_lock guard(mutex_);
        std::vector<nlohmann::json> journal_entries;
        if (!std::filesystem::exists(journal_path_)) {
            return journal_entries;
        }
        std::ifstream input_stream(journal_path_);
        std::string line;
        while (std::getline(input_stream, line)) {
            if (line.empty()) {
                continue;
            }
            try {
                journal_entries.push_back(nlohmann::json::parse(line));
            } catch (...) {
            }
        }
        return journal_entries;
    }

    void FileJsonStorage::appendJournal(const nlohmann::json& entry) {
        std::scoped_lock guard(mutex_);
        if (!journal_path_.parent_path().empty()) {
            std::filesystem::create_directories(journal_path_.parent_path());
        }
        std::ofstream output_stream(journal_path_, std::ios::app);
        if (!output_stream) {
            throw std::runtime_error("Cannot open journal file for append: " + journal_path_.string());
        }
        output_stream << entry.dump() << '\n';
    }

} // namespace booking
