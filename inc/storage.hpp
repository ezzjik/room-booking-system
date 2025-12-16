#pragma once
#include "models.hpp"
#include <vector>
#include <mutex>
#include <optional>

struct IStorage {
    virtual ~IStorage() = default;
    virtual void saveState(const nlohmann::json& snapshot) = 0;
    virtual nlohmann::json loadState() = 0;
    virtual void appendJournal(const nlohmann::json& entry) = 0;
    virtual std::vector<nlohmann::json> loadJournal() = 0;
};

class MemoryStorage: public IStorage {
public:
    MemoryStorage() = default;

    nlohmann::json loadState() override {
        std::scoped_lock guard(mutex_);
        return snapshot_;
    }

    void saveState(const nlohmann::json& snapshot) override {
        std::scoped_lock guard(mutex_);
        snapshot_ = snapshot;
    }

    std::vector<nlohmann::json> loadJournal() override {
        std::scoped_lock guard(mutex_);
        return journal_;
    }

    void appendJournal(const nlohmann::json& entry) override {
        std::scoped_lock guard(mutex_);
        journal_.push_back(entry);
    }

private:
    std::mutex mutex_;
    nlohmann::json snapshot_ = nlohmann::json::object();
    std::vector<nlohmann::json> journal_;
};

struct IRepository {
    virtual ~IRepository() = default;
    virtual BookingId createBooking(const Booking& b) = 0;
    virtual void updateBooking(const Booking& b) = 0;
    virtual void removeBooking(BookingId id) = 0;
    virtual std::optional<Booking> getBooking(BookingId id) = 0;
    virtual std::vector<Booking> listAll() = 0;
};

class Repository: public IRepository {
public:
    explicit Repository(std::shared_ptr<IStorage> storage)
        : storage_(std::move(storage)) {
        reload();
    }

    BookingId createBooking(const Booking& b) override {
        std::lock_guard lk(mutex_);
        Booking nb = b;
        BookingId maxid = 0;
        for (auto const& ex : bookings_) {
            maxid = std::max(maxid, ex.first);
        }
        nb.id = maxid + 1;
        bookings_[nb.id] = nb;
        persist();
        nlohmann::json je = {{"op", "create"}, {"booking", bookingToJson(nb)}};
        storage_->appendJournal(je);
        return nb.id;
    }

    void updateBooking(const Booking& b) override {
        std::lock_guard lk(mutex_);
        bookings_[b.id] = b;
        persist();
        nlohmann::json je = {{"op", "update"}, {"booking", bookingToJson(b)}};
        storage_->appendJournal(je);
    }

    void removeBooking(BookingId id) override {
        std::lock_guard lk(mutex_);
        bookings_.erase(id);
        persist();
        nlohmann::json je = {{"op", "remove"}, {"id", id}};
        storage_->appendJournal(je);
    }

    std::optional<Booking> getBooking(BookingId id) override {
        std::lock_guard lk(mutex_);
        auto it = bookings_.find(id);
        if (it == bookings_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<Booking> listAll() override {
        std::lock_guard lk(mutex_);
        std::vector<Booking> out;
        out.reserve(bookings_.size());
        for (auto const& kv : bookings_) {
            out.push_back(kv.second);
        }
        return out;
    }

private:
    void reload() {
        std::lock_guard lk(mutex_);
        bookings_.clear();
        nlohmann::json snap = storage_->loadState();
        if (snap.is_object() && snap.contains("bookings") && snap["bookings"].is_array()) {
            for (auto const& jb : snap["bookings"]) {
                Booking b;
                booking::from_json(jb, b);
                bookings_[b.id] = b;
            }
        }
    }

    void persist() {
        nlohmann::json snap = nlohmann::json::object();
        snap["bookings"] = nlohmann::json::array();
        for (auto const& kv : bookings_) {
            snap["bookings"].push_back(bookingToJson(kv.second));
        }
        storage_->saveState(snap);
    }

    static nlohmann::json bookingToJson(const Booking& b) {
        nlohmann::json j;
        booking::to_json(j, b);
        nlohmann::json r;
        r["type"] = static_cast<int>(b.recurrence.type);
        if (b.recurrence.until) {
            r["until"] = std::chrono::duration_cast<std::chrono::seconds>(b.recurrence.until->time_since_epoch()).count();
        }
        j["recurrence"] = r;
        j["attendees"] = nlohmann::json::array();
        for (auto a : b.attendees) {
            j["attendees"].push_back(a);
        }
        j["resources"] = nlohmann::json::array();
        for (auto const& res : b.resources) {
            j["resources"].push_back(res.id);
        }
        return j;
    }

    static void from_json(const nlohmann::json& j, Booking& b) {
        booking::from_json(j, b);
        if (j.contains("recurrence")) {
            auto const& r = j["recurrence"];
            if (r.contains("type")) {
                b.recurrence.type = static_cast<Recurrence::Type>(r["type"].get<int>());
            }
            if (r.contains("until")) {
                long long s = r["until"].get<long long>();
                b.recurrence.until = std::chrono::system_clock::time_point(std::chrono::seconds(s));
            }
        }
        if (j.contains("attendees") && j["attendees"].is_array()) {
            b.attendees.clear();
            for (auto const& a : j["attendees"]) {
                b.attendees.push_back(a.get<UserId>());
            }
        }
        if (j.contains("resources") && j["resources"].is_array()) {
            b.resources.clear();
            for (auto const& r : j["resources"]) {
                b.resources.push_back(Resource{r.get<std::string>()});
            }
        }
    }

private:
    std::shared_ptr<IStorage> storage_;
    std::mutex mutex_;
    std::unordered_map<BookingId, Booking> bookings_;
};
