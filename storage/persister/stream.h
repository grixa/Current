/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef CURRENT_STORAGE_PERSISTER_STREAM_H
#define CURRENT_STORAGE_PERSISTER_STREAM_H

#include "common.h"
#include "../base.h"
#include "../exceptions.h"
#include "../transaction.h"
#include "../../stream/replicator.h"
#include "../../stream/stream.h"

#include "../../bricks/sync/locks.h"

namespace current {
namespace storage {
namespace persister {

template <typename MUTATIONS_VARIANT, template <typename> class UNDERLYING_PERSISTER, typename STREAM_RECORD_TYPE>
class StreamStreamPersisterImpl final {
 public:
  using variant_t = MUTATIONS_VARIANT;
  using transaction_t = Transaction<variant_t>;
  using stream_entry_t = typename std::conditional<std::is_same<STREAM_RECORD_TYPE, NoCustomPersisterParam>::value,
                                                   transaction_t,
                                                   STREAM_RECORD_TYPE>::type;
  using stream_t = stream::Stream<stream_entry_t, UNDERLYING_PERSISTER>;
  using controller_t = stream::MasterFlipController<stream_t>;
  using fields_update_function_t = std::function<void(const variant_t&)>;

  struct StreamSubscriberImpl {
    using EntryResponse = current::ss::EntryResponse;
    using TerminationResponse = current::ss::TerminationResponse;
    using replay_function_t = std::function<void(const transaction_t&, std::chrono::microseconds)>;
    replay_function_t replay_f_;
    uint64_t next_replay_index_ = 0u;

    StreamSubscriberImpl(replay_function_t f) : replay_f_(f) {}

    EntryResponse operator()(const transaction_t& transaction, idxts_t current, idxts_t) {
      replay_f_(transaction, current.us);
      std::cout << "StreamSubscriberImpl " << (void*)this << " index " << current.index << " - " << current.us.count()
                << std::endl;
      next_replay_index_ = current.index + 1u;
      return EntryResponse::More;
    }

    EntryResponse operator()(std::chrono::microseconds) const { return EntryResponse::More; }

    EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }
    TerminationResponse Terminate() const { return TerminationResponse::Terminate; }
  };
  using StreamSubscriber = current::ss::StreamSubscriber<StreamSubscriberImpl, transaction_t>;

  struct Master {};
  struct Following {};

  StreamStreamPersisterImpl(Master, fields_update_function_t f, Borrowed<stream_t> stream)
      : fields_update_f_(f),
        stream_publishing_mutex_ref_(stream->Impl()->publishing_mutex),
        stream_controller_(stream),
        publisher_used_(stream_controller_->BecomeFollowingStream()) {
    subscriber_instance_ = std::make_unique<StreamSubscriber>(
        [this](const transaction_t& transaction, std::chrono::microseconds timestamp) {
          std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
          ApplyMutationsFromLockedSectionOrConstructor(transaction, timestamp);
        });
    std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
    SyncReplayStreamFromLockedSectionOrConstructor(0u);
  }

  StreamStreamPersisterImpl(Following, fields_update_function_t f, Borrowed<stream_t> stream)
      : fields_update_f_(f),
        stream_publishing_mutex_ref_(stream->Impl()->publishing_mutex),
        stream_controller_(stream) {
    subscriber_instance_ = std::make_unique<StreamSubscriber>(
        [this](const transaction_t& transaction, std::chrono::microseconds timestamp) {
          std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
          ApplyMutationsFromLockedSectionOrConstructor(transaction, timestamp);
        });
    std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
    SubscribeToStreamFromLockedSection();
  }

  StreamStreamPersisterImpl(const std::string& url,
                            stream::SubscriptionMode mode,
                            fields_update_function_t f,
                            Borrowed<stream_t> stream)
      : fields_update_f_(f),
        stream_publishing_mutex_ref_(stream->Impl()->publishing_mutex),
        stream_controller_(stream) {
    subscriber_instance_ = std::make_unique<StreamSubscriber>(
        [this](const transaction_t& transaction, std::chrono::microseconds timestamp) {
          std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
          ApplyMutationsFromLockedSectionOrConstructor(transaction, timestamp);
        });
    stream_controller_.FollowRemoteStream(url, mode);
    std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
    SubscribeToStreamFromLockedSection();
  }

  ~StreamStreamPersisterImpl() {
    std::lock_guard<std::mutex> master_follower_change_lock(master_follower_change_mutex_);
    TerminateStreamSubscriptionFromLockedSection();
  }

  template <current::locks::MutexLockStatus MLS>
  bool IsMasterStoragePersister() const {
    locks::SmartMutexLockGuard<MLS> master_follower_change_lock(master_follower_change_mutex_);
    return Exists(publisher_used_);
  }

  template <current::locks::MutexLockStatus MLS>
  std::chrono::microseconds LastAppliedTimestampPersister() const {
    locks::SmartMutexLockGuard<MLS> master_follower_change_lock(master_follower_change_mutex_);
    return last_applied_timestamp_;
  }

  void PersistJournalFromLockedSection(MutationJournal& journal) {
    const std::chrono::microseconds timestamp = current::time::Now();
    CURRENT_ASSERT(Exists(publisher_used_));
    if (!journal.commit_log.empty()) {
#ifndef CURRENT_MOCK_TIME
      CURRENT_ASSERT(journal.transaction_meta.begin_us < journal.transaction_meta.end_us);
#else
      CURRENT_ASSERT(journal.transaction_meta.begin_us <= journal.transaction_meta.end_us);
#endif
      transaction_t transaction;
      for (auto&& entry : journal.commit_log) {
        transaction.mutations.emplace_back(BypassVariantTypeCheck(), std::move(entry));
      }
      std::swap(transaction.meta, journal.transaction_meta);
      Value(publisher_used_)
          ->template Publish<current::locks::MutexLockStatus::AlreadyLocked>(std::move(transaction), timestamp);
      SetLastAppliedTimestampFromLockedSection(timestamp);
    }
    journal.Clear();
  }

  uint64_t ExposeRawLogViaHTTP(uint16_t port,
                               const std::string& route,
                               stream::MasterFlipRestrictions restrictions,
                               std::function<void()> flip_started,
                               std::function<void()> flip_finished,
                               std::function<void()> flip_canceled) {
    return stream_controller_.ExposeViaHTTP(port,
                                            route,
                                            restrictions,
                                            [this, flip_started]() { FlipStarted(flip_started); },
                                            [this, flip_finished]() { FlipFinished(flip_finished); },
                                            [this, flip_canceled]() { FlipCanceled(flip_canceled); });
  }

  void FollowRemoteStream(const std::string& url, stream::SubscriptionMode mode) {
    stream_controller_.FollowRemoteStream(url, mode);
  }

  Borrowed<stream_t> BorrowStream() const { return stream_controller_.BorrowStream(); }
  const WeakBorrowed<stream_t>& Stream() const { return stream_controller_.Stream(); }
  WeakBorrowed<stream_t>& Stream() { return stream_controller_.Stream(); }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  Borrowed<typename stream_t::publisher_t> PublisherUsed() {
    locks::SmartMutexLockGuard<MLS> lock(master_follower_change_mutex_);
    CURRENT_ASSERT(Exists(publisher_used_));
    return Value(publisher_used_);
  }

  // Note: `BecomeMasterStorage` can not be called from a locked section, as terminating the subscriber
  // is by itself an operation that locks the publishing mutex of the stream -- in the destructor.
  void BecomeMasterStorage(uint64_t secret_flip_key) {
    std::lock_guard<std::mutex> master_follower_change_lock(master_follower_change_mutex_);
    if (Exists(publisher_used_)) {
      CURRENT_THROW(StorageIsAlreadyMasterException());
    } else {
      try {
        stream_controller_.FlipToMaster(secret_flip_key);
      } catch (const stream::StreamDoesNotFollowAnyoneException&) {
      }
      TerminateStreamSubscriptionFromLockedSection();
      std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
      publisher_used_ = nullptr;  // Why?
      publisher_used_ =
          stream_controller_->template BecomeFollowingStream<current::locks::MutexLockStatus::AlreadyLocked>();
      const uint64_t save_replay_index = subscriber_instance_->next_replay_index_;
      std::cout << (void*)subscriber_instance_.get() << "BecomeMasterStorage replay_index " << save_replay_index
                << ", applied_timestamp " << last_applied_timestamp_.count() << std::endl;
      subscriber_instance_ = nullptr;
      SyncReplayStreamFromLockedSectionOrConstructor(save_replay_index);
    }
  }

 private:
  // Invariant: both `subscriber_creator_destructor_mutex_` and `stream_publishing_mutex_ref_` are locked,
  // or the call is taking place from the constructor.
  void SyncReplayStreamFromLockedSectionOrConstructor(uint64_t from_idx) {
    std::cout << "SyncReplayStreamFromLockedSectionOrConstructor" << std::endl;
    for (const auto& stream_record :
         stream_controller_->Data()->template Iterate<current::locks::MutexLockStatus::AlreadyLocked>(from_idx)) {
      if (Exists<transaction_t>(stream_record.entry)) {
        const transaction_t& transaction = Value<transaction_t>(stream_record.entry);
        std::cout << "ApplyMutationsFromLockedSectionOrConstructor index " << stream_record.idx_ts.index << ", "
                  << stream_record.idx_ts.us.count() << std::endl;
        ApplyMutationsFromLockedSectionOrConstructor(transaction, stream_record.idx_ts.us);
      }
    }
  }

  void ApplyMutationsFromLockedSectionOrConstructor(const transaction_t& transaction,
                                                    std::chrono::microseconds timestamp) {
    for (const auto& mutation : transaction.mutations) {
      fields_update_f_(mutation);
    }
    SetLastAppliedTimestampFromLockedSection(timestamp);
  }

 private:
  // Invariant: `master_follower_change_mutex_` is locked, or the call is happening from the constructor.
  void SubscribeToStreamFromLockedSection() {
    CURRENT_ASSERT(!subscriber_scope_);
    CURRENT_ASSERT(subscriber_instance_);
    subscriber_scope_ = std::move(stream_controller_->template Subscribe<transaction_t>(*subscriber_instance_));
  }

  // Invariant: `master_follower_change_mutex_` is locked.
  // Important: The publishing mutex of the respective stream must be unlocked!
  void TerminateStreamSubscriptionFromLockedSection() { subscriber_scope_ = nullptr; }

  void SetLastAppliedTimestampFromLockedSection(std::chrono::microseconds timestamp) {
    CURRENT_ASSERT(timestamp > last_applied_timestamp_);
    if (timestamp <= last_applied_timestamp_)
      std::cout << "timestamp is " << timestamp.count() << ", but last_applied_timestamp_ is "
                << last_applied_timestamp_.count() << std::endl;
    last_applied_timestamp_ = timestamp;
  }

  void FlipStarted(std::function<void()> flip_started_callback) {
    if (flip_started_callback) {
      flip_started_callback();
    }
    // 1. Serve503
    // Release the publisher to switch storage to read-only mode
    publisher_used_ = nullptr;
  }

  void FlipFinished(std::function<void()> flip_finished_callback) {
    if (flip_finished_callback) {
      flip_finished_callback();
    }
    std::lock_guard<std::mutex> lock(stream_publishing_mutex_ref_);
    SubscribeToStreamFromLockedSection();
  }

  void FlipCanceled(std::function<void()> flip_canceled_callback) {
    if (flip_canceled_callback) {
      flip_canceled_callback();
    }
    // how to restore publisher_used_ ?
  }

 private:
  fields_update_function_t fields_update_f_;

  std::mutex& stream_publishing_mutex_ref_;  // == `stream_->Impl()->publishing_mutex`.
  controller_t stream_controller_;
  Optional<Borrowed<typename stream_t::publisher_t>> publisher_used_;  // Set iff the storage is the master storage.

  mutable std::mutex master_follower_change_mutex_;
  std::unique_ptr<StreamSubscriber> subscriber_instance_;
  current::stream::SubscriberScope subscriber_scope_;

  std::chrono::microseconds last_applied_timestamp_ = std::chrono::microseconds(-1);  // Replayed or from the master.
};

template <typename TYPELIST, typename STREAM_RECORD_TYPE = NoCustomPersisterParam>
using StreamInMemoryStreamPersister =
    StreamStreamPersisterImpl<TYPELIST, current::persistence::Memory, STREAM_RECORD_TYPE>;

template <typename TYPELIST, typename STREAM_RECORD_TYPE = NoCustomPersisterParam>
using StreamStreamPersister = StreamStreamPersisterImpl<TYPELIST, current::persistence::File, STREAM_RECORD_TYPE>;

}  // namespace persister
}  // namespace storage
}  // namespace current

using current::storage::persister::StreamInMemoryStreamPersister;
using current::storage::persister::StreamStreamPersister;

#endif  // CURRENT_STORAGE_PERSISTER_STREAM_H
