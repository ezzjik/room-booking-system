#include <BookingManager.hpp>

namespace booking {

    BookingManager::BookingManager(std::shared_ptr<IRepository> repository,
                                   std::shared_ptr<IStorage> storage,
                                   std::shared_ptr<IConflictStrategy> conflictStrategy)
        : repository_(std::move(repository))
        , storage_(std::move(storage))
        , conflictStrategy_(std::move(conflictStrategy)) {
    }

    void BookingManager::pushUndo(std::unique_ptr<ICommand> command) {
        std::lock_guard lk(historyLock_);
        undoHistory_.push_back(std::move(command));
        if (undoHistory_.size() > maxUndoCount_) {
            undoHistory_.pop_front();
        }
        redoHistory_.clear();
    }

    std::optional<std::string> BookingManager::undo() {
        std::unique_lock lk(historyLock_);
        if (undoHistory_.empty()) {
            return std::nullopt;
        }
        auto cmd = std::move(undoHistory_.back());
        undoHistory_.pop_back();
        std::string desc = cmd->describe();
        cmd->undo();
        redoHistory_.push_back(std::move(cmd));
        return std::optional<std::string>(std::string("Undid: ") + desc);
    }

    std::optional<std::string> BookingManager::redo() {
        std::unique_lock lk(historyLock_);
        if (redoHistory_.empty()) {
            return std::nullopt;
        }
        auto cmd = std::move(redoHistory_.back());
        redoHistory_.pop_back();
        std::string desc = cmd->describe();
        cmd->execute();
        undoHistory_.push_back(std::move(cmd));
        return std::optional<std::string>(std::string("Redid: ") + desc);
    }

    std::optional<Booking> BookingManager::getBooking(BookingId bookingId) {
        return repository_->getBooking(bookingId);
    }

    std::vector<Booking> BookingManager::listBookings(RoomId roomId,
                                                      std::chrono::system_clock::time_point fromTime,
                                                      std::chrono::system_clock::time_point toTime) {
        std::vector<Booking> out;
        auto all = repository_->listAll();
        for (auto& b : all) {
            if (b.room_id != roomId) {
                continue;
            }
            auto inst = generateInstances(b, fromTime, toTime);
            out.insert(out.end(), inst.begin(), inst.end());
        }
        return out;
    }

    bool BookingManager::canModify(const User& actorUser, const Booking& targetBooking) const {
        if (actorUser.role == Role::Admin) {
            return true;
        }
        if (actorUser.role == Role::Manager) {
            return true;
        }
        if (actorUser.role == Role::User) {
            return actorUser.id == targetBooking.user_id;
        }
        return false;
    }

    bool BookingManager::canCreate(const User& actorUser) const {
        return actorUser.role == Role::Admin ||
               actorUser.role == Role::Manager ||
               actorUser.role == Role::User;
    }

    bool BookingManager::canCancel(const User& actorUser, const Booking& targetBooking) const {
        if (actorUser.role == Role::Admin) {
            return true;
        }
        if (actorUser.role == Role::Manager) {
            return true;
        }
        if (actorUser.role == Role::User) {
            return actorUser.id == targetBooking.user_id;
        }
        return false;
    }

    std::optional<BookingId> BookingManager::createBooking(const Booking& bookingData, const User& actorUser) {
        if (!canCreate(actorUser)) {
            throw std::runtime_error("Access denied: create");
        }

        std::lock_guard lk(operationMutex_);

        using namespace std::chrono;

        auto from = bookingData.start - hours(24);
        auto to = bookingData.recurrence.until
                      ? (*bookingData.recurrence.until + hours(1))
                      : (bookingData.start + hours(24 * 365));

        auto requestedInst = generateInstances(bookingData, from, to);

        if (requestedInst.empty()) {
            requestedInst.push_back(bookingData);
        }

        std::vector<Booking> existingInst;
        for (auto& ex : repository_->listAll()) {
            bool related = (ex.room_id == bookingData.room_id);
            if (!related) {
                for (auto& r : ex.resources) {
                    for (auto& rr : bookingData.resources) {
                        if (r.id == rr.id) {
                            related = true;
                        }
                    }
                }
            }
            if (!related) {
                continue;
            }

            auto insts = generateInstances(ex, from, to);
            existingInst.insert(existingInst.end(), insts.begin(), insts.end());
        }

        for (auto& inst : requestedInst) {
            auto res = conflictStrategy_->resolve(inst, existingInst, actorUser);
            if (!res.ok) {
                return std::nullopt;
            }

            if (res.suggested_start) {
                Booking adjusted = bookingData;
                auto dur = adjusted.end - adjusted.start;
                adjusted.start = *res.suggested_start;
                adjusted.end = adjusted.start + dur;

                auto cmd = std::make_unique<CreateBookingCommand>(*repository_, adjusted);
                cmd->execute();
                auto id = cmd->id();
                pushUndo(std::move(cmd));
                return id;
            }
        }

        auto cmd = std::make_unique<CreateBookingCommand>(*repository_, bookingData);
        cmd->execute();
        auto id = cmd->id();
        pushUndo(std::move(cmd));
        return id;
    }

    std::optional<BookingId> BookingManager::createBooking(const CreateRequest& request) {
        return createBooking(request.booking, request.actor);
    }

    bool BookingManager::modifyBooking(const ChangeRequest& request) {
        std::lock_guard lk(operationMutex_);

        auto old = repository_->getBooking(request.id);
        if (!old) {
            return false;
        }

        if (!canModify(request.actor, *old)) {
            throw std::runtime_error("Access denied: modify");
        }

        Booking updated = *old;
        if (request.title) {
            updated.title = *request.title;
        }
        if (request.description) {
            updated.description = *request.description;
        }
        if (request.start) {
            updated.start = *request.start;
        }
        if (request.end) {
            updated.end = *request.end;
        }

        auto cmd = std::make_unique<UpdateBookingCommand>(*repository_, *old, updated);
        cmd->execute();
        pushUndo(std::move(cmd));
        return true;
    }

    bool BookingManager::cancelBooking(BookingId bookingId, const User& actorUser) {
        std::lock_guard lk(operationMutex_);
        auto ob = repository_->getBooking(bookingId);
        if (!ob) {
            return false;
        }
        if (!canCancel(actorUser, *ob)) {
            throw std::runtime_error("Access denied: cancel");
        }

        auto cmd = std::make_unique<RemoveBookingCommand>(*repository_, bookingId);
        cmd->execute();
        pushUndo(std::move(cmd));
        return true;
    }

    void BookingManager::setStrategy(std::shared_ptr<IConflictStrategy> newStrategy) {
        std::lock_guard lk(operationMutex_);
        conflictStrategy_ = newStrategy;
    }

    std::vector<BookingId> BookingManager::importFromCalendar(
        ICalendarAdapter& calendarAdapter,
        std::chrono::system_clock::time_point startTime,
        std::chrono::system_clock::time_point endTime,
        const User& actorUser) {
        if (actorUser.role != Role::Admin && actorUser.role != Role::Manager) {
            throw std::runtime_error("Access denied: import");
        }

        std::vector<BookingId> imported;
        auto events = calendarAdapter.fetch(startTime, endTime);

        for (auto const& ev : events) {
            Booking b;
            b.room_id = ev.room_id;
            b.user_id = ev.user_id;
            b.start = ev.start;
            b.end = ev.end;
            b.title = ev.title;
            b.description = ev.description;

            if (auto id = createBooking(b, actorUser)) {
                imported.push_back(*id);
            }
        }

        return imported;
    }

} // namespace booking
