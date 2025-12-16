#pragma once
#include <memory>
#include <mutex>
#include <deque>
#include <vector>

#include "models.hpp"
#include "storage.hpp"
#include "strategies.hpp"
#include "commands.hpp"
#include "calendar.hpp"

namespace booking {

    class BookingManager {
    public:
        BookingManager(std::shared_ptr<IRepository> repository,
                       std::shared_ptr<IStorage> storage,
                       std::shared_ptr<IConflictStrategy> conflictStrategy);

        // Основные операции бронирования
        std::optional<BookingId> createBooking(const CreateRequest& request);
        std::optional<BookingId> createBooking(const Booking& bookingData, const User& actorUser);
        bool modifyBooking(const ChangeRequest& request);
        bool cancelBooking(BookingId bookingId, const User& actorUser);

        // Получение информации
        std::optional<Booking> getBooking(BookingId bookingId);
        std::vector<Booking> listBookings(RoomId roomId,
                                          std::chrono::system_clock::time_point fromTime,
                                          std::chrono::system_clock::time_point toTime);

        // История операций
        std::optional<std::string> undo();
        std::optional<std::string> redo();

        // Импорт из внешних систем
        std::vector<BookingId> importFromCalendar(
            ICalendarAdapter& calendarAdapter,
            std::chrono::system_clock::time_point startTime,
            std::chrono::system_clock::time_point endTime,
            const User& actorUser);

        void setStrategy(std::shared_ptr<IConflictStrategy> newStrategy);

        // Внутренние методы (для тестирования)
        void internalCreate(Booking bookingData);
        void internalRemove(BookingId bookingId);
        void internalRestore(Booking bookingData);

    private:
        bool canCreate(const User& actorUser) const;
        bool canModify(const User& actorUser, const Booking& targetBooking) const;
        bool canCancel(const User& actorUser, const Booking& targetBooking) const;

        void pushUndo(std::unique_ptr<ICommand> command);

    private:
        std::shared_ptr<IRepository> repository_;
        std::shared_ptr<IStorage> storage_;
        std::shared_ptr<IConflictStrategy> conflictStrategy_;

        std::mutex operationMutex_;
        std::mutex historyLock_;

        std::deque<std::unique_ptr<ICommand>> undoHistory_;
        std::deque<std::unique_ptr<ICommand>> redoHistory_;
        const size_t maxUndoCount_ = 300;
    };

} // namespace booking
