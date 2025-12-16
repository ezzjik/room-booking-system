#pragma once
#include "models.hpp"
#include <optional>
#include <vector>

struct ConflictResolutionResult {
    bool ok;
    std::optional<std::string> message;
    std::optional<std::chrono::system_clock::time_point> suggested_start;
    std::vector<BookingId> to_preempt;
};

struct IConflictStrategy {
    virtual ~IConflictStrategy() = default;
    virtual ConflictResolutionResult resolve(const Booking& candidateBooking,
                                             const std::vector<Booking>& existingBookings,
                                             const User& actorUser) = 0;
};

struct QuorumStrategy: public IConflictStrategy {
    explicit QuorumStrategy(size_t quorum_size)
        : quorum_(quorum_size) {
    }

    ConflictResolutionResult resolve(const Booking& candidateBooking,
                                     const std::vector<Booking>& existingBookings,
                                     const User&) override {
        for (auto const& existingBooking : existingBookings) {
            if (!(candidateBooking.end <= existingBooking.start || candidateBooking.start >= existingBooking.end)) {
                if (candidateBooking.attendees.size() >= quorum_) {
                    return {true,
                            std::string("Allowed by quorum (") + std::to_string(quorum_) + ")",
                            std::nullopt,
                            {}};
                }

                return {false,
                        std::string("Conflict and quorum not satisfied (need ") + std::to_string(quorum_) + ")",
                        std::nullopt,
                        {}};
            }
        }
        return {true, std::nullopt, std::nullopt, {}};
    }

private:
    size_t quorum_;
};

struct RejectStrategy: public IConflictStrategy {
    ConflictResolutionResult resolve(const Booking& candidateBooking,
                                     const std::vector<Booking>& existingBookings,
                                     const User&) override {
        for (auto const& existingBooking : existingBookings) {
            if (!(candidateBooking.end <= existingBooking.start || candidateBooking.start >= existingBooking.end)) {
                return {false,
                        std::string("Conflict with booking id ") + std::to_string(existingBooking.id),
                        std::nullopt,
                        {}};
            }
        }
        return {true, std::nullopt, std::nullopt, {}};
    }
};

struct AutoBumpStrategy: public IConflictStrategy {
    ConflictResolutionResult resolve(
        const Booking& candidateBooking,
        const std::vector<Booking>& existingBookings,
        const User&) override {
        auto suggestedStartTime = candidateBooking.start;
        auto duration = candidateBooking.end - candidateBooking.start;

        bool moved = true;
        while (moved) {
            moved = false;
            for (auto const& existingBooking : existingBookings) {
                if (!(suggestedStartTime + duration <= existingBooking.start || suggestedStartTime >= existingBooking.end)) {
                    suggestedStartTime = existingBooking.end;
                    moved = true;
                }
            }
        }

        if (suggestedStartTime != candidateBooking.start) {
            return {true, "Auto-bumped", suggestedStartTime, {}};
        }
        return {true, std::nullopt, std::nullopt, {}};
    }
};

struct PreemptStrategy: public IConflictStrategy {
    ConflictResolutionResult resolve(
        const Booking& candidateBooking,
        const std::vector<Booking>& existingBookings,
        const User& actorUser) override {
        std::vector<BookingId> to_preempt;

        for (auto const& existingBooking : existingBookings) {
            if (candidateBooking.end <= existingBooking.start || candidateBooking.start >= existingBooking.end) {
                continue;
            }

            if (actorUser.priority > existingBooking.owner_priority) {
                to_preempt.push_back(existingBooking.id);
            } else {
                return {false, "Higher priority booking exists", std::nullopt, {}};
            }
        }

        return {true, "Preempt allowed", std::nullopt, to_preempt};
    }
};
