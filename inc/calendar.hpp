#pragma once
#include <vector>
#include "models.hpp"
#include <fstream>

static std::chrono::system_clock::time_point time_point_from_seconds(long long seconds) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
}

namespace booking {

    struct CalendarEvent {
        RoomId room_id;
        UserId user_id;
        std::chrono::system_clock::time_point start;
        std::chrono::system_clock::time_point end;
        std::string title;
        std::string description;
    };

    class ICalendarAdapter {
    public:
        virtual ~ICalendarAdapter() = default;
        virtual std::vector<CalendarEvent> fetch(
            std::chrono::system_clock::time_point from,
            std::chrono::system_clock::time_point to) = 0;
    };

    class JsonCalendarAdapter final: public ICalendarAdapter {
    public:
        explicit JsonCalendarAdapter(std::string file)
            : file_(std::move(file)) {
        }

        std::vector<CalendarEvent> fetch(
            std::chrono::system_clock::time_point from,
            std::chrono::system_clock::time_point to) override {
            std::ifstream input(file_);
            if (!input) {
                throw std::runtime_error("Cannot open calendar file: " + file_);
            }

            nlohmann::json json_data;
            input >> json_data;

            std::vector<CalendarEvent> events;

            if (!json_data.is_array()) {
                return events;
            }

            for (auto const& event_json : json_data) {
                long long start_seconds = event_json.at("start").get<long long>();
                long long end_seconds = event_json.at("end").get<long long>();

                auto event_start = time_point_from_seconds(start_seconds);
                auto event_end = time_point_from_seconds(end_seconds);

                if (event_end <= from || event_start >= to) {
                    continue;
                }

                CalendarEvent event;
                event.room_id = event_json.at("room_id").get<RoomId>();
                event.user_id = event_json.at("user_id").get<UserId>();
                event.start = event_start;
                event.end = event_end;
                event.title = event_json.value("title", "");
                event.description = event_json.value("description", "");

                events.push_back(std::move(event));
            }

            return events;
        }

    private:
        std::string file_;
    };

} // namespace booking
