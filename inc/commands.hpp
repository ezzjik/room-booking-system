#pragma once
#include <memory>
#include <string>

#include "models.hpp"
#include "storage.hpp"

namespace booking {

    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void execute() = 0;
        virtual void undo() = 0;
        virtual std::string describe() const = 0;
        virtual BookingId id() const {
            return 0;
        }
    };

    class CreateBookingCommand: public ICommand {
    public:
        CreateBookingCommand(IRepository& repo, const Booking& booking)
            : repo_(repo)
            , booking_(booking) {
        }

        void execute() override {
            if (!executed_) {
                booking_.id = repo_.createBooking(booking_);
                executed_ = true;
            } else {
                repo_.updateBooking(booking_);
            }
        }

        void undo() override {
            if (executed_) {
                repo_.removeBooking(booking_.id);
            }
        }

        std::string describe() const override {
            return "Create booking id=" + std::to_string(booking_.id) + " title=\"" + booking_.title + "\"";
        }

        BookingId id() const override {
            return booking_.id;
        }

    private:
        IRepository& repo_;
        Booking booking_;
        bool executed_ = false;
    };

    class UpdateBookingCommand: public ICommand {
    public:
        UpdateBookingCommand(IRepository& repo, const Booking& old, const Booking& updated)
            : repo_(repo)
            , old_(old)
            , updated_(updated) {
        }

        void execute() override {
            repo_.updateBooking(updated_);
        }

        void undo() override {
            repo_.updateBooking(old_);
        }

        std::string describe() const override {
            return "Update booking id=" + std::to_string(old_.id) + " title=\"" + old_.title + "\"";
        }

    private:
        IRepository& repo_;
        Booking old_;
        Booking updated_;
    };

    class RemoveBookingCommand: public ICommand {
    public:
        RemoveBookingCommand(IRepository& repo, BookingId id)
            : repo_(repo)
            , id_(id) {
        }

        void execute() override {
            old_ = repo_.getBooking(id_);
            if (old_) {
                repo_.removeBooking(id_);
            }
        }

        void undo() override {
            if (old_) {
                repo_.createBooking(*old_);
            }
        }

        std::string describe() const override {
            return "Cancel booking id=" + std::to_string(id_);
        }

    private:
        IRepository& repo_;
        BookingId id_;
        std::optional<Booking> old_;
    };

    class CompositeCommand: public ICommand {
    public:
        CompositeCommand(std::string desc)
            : description_(std::move(desc)) {
        }

        void addCommand(std::unique_ptr<ICommand> cmd) {
            commands_.push_back(std::move(cmd));
        }

        void execute() override {
            for (auto& cmd : commands_) {
                cmd->execute();
            }
        }

        void undo() override {
            for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
                (*it)->undo();
            }
        }

        std::string describe() const override {
            return description_;
        }

    private:
        std::vector<std::unique_ptr<ICommand>> commands_;
        std::string description_;
    };

} // namespace booking
