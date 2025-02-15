/**
 *    Copyright (C) 2019-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include "mongo/base/status.h"
#include "mongo/client/replica_set_change_notifier.h"
#include "mongo/executor/connection_pool.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/stdx/mutex.h"
#include "mongo/stdx/unordered_map.h"

namespace mongo {

/**
 * A special Controller for the sharding ConnectionPool
 *
 * This class has two special members:
 * * A global set of synchronized Parameters for the ShardingTaskExecutorPool server parameters
 * * A ReplicaSetChangeListener to inform it of changes to replica set membership
 *
 * When the MatchingStrategy from its Parameters is kDisabled, this class operates much like the
 * LimitController but with its limits allowed to shift at runtime (via Parameters).
 *
 * When the MatchingStrategy is kMatchPrimaryNode, the limits are obeyed but, when the pool for a
 * primary member calls updateHost, it can increase the targetConnections for the pool of each other
 * member of its replica set. Note that this will, at time of writing, follow the "hosts" field
 * from the primary isMaster combined with the seed list for the replica set. If the seed list were
 * to include arbiters or hidden members, then they would also be subject to these constraints.
 *
 * When the MatchingStrategy is kMatchBusiestNode, it operates like kMatchPrimaryNode, but any pool
 * can be responsible for increasing the targetConnections of each member of its set.
 *
 * Note that, in essence, there are three outside elements that can mutate the state of this class:
 * * The ReplicaSetChangeNotifier can notify the listener which updates the host groups
 * * The ServerParameters can update the Parameters which will used in the next update
 * * The SpecificPools for its ConnectionPool can updateHost with their individual States
 */
class ShardingTaskExecutorPoolController final
    : public executor::ConnectionPool::ControllerInterface {
    class ReplicaSetChangeListener;

public:
    using ConnectionPool = executor::ConnectionPool;

    enum class MatchingStrategy {
        kDisabled,
        kMatchPrimaryNode,
        kMatchBusiestNode,
    };

    class Parameters {
    public:
        AtomicWord<int> minConnections;
        AtomicWord<int> maxConnections;
        AtomicWord<int> maxConnecting;

        AtomicWord<int> hostTimeoutMS;
        AtomicWord<int> pendingTimeoutMS;
        AtomicWord<int> toRefreshTimeoutMS;

        synchronized_value<std::string> matchingStrategyString;
        AtomicWord<MatchingStrategy> matchingStrategy;
    };

    static inline Parameters gParameters;

    /**
     * Validate that hostTimeoutMS is greater than the sum of pendingTimeoutMS and
     * toRefreshTimeoutMS
     */
    static Status validateHostTimeout(const int& hostTimeoutMS);

    /**
     * Validate that pendingTimeoutMS is less than toRefreshTimeoutMS
     */
    static Status validatePendingTimeout(const int& pendingTimeoutMS);

    /**
     *  Matches the matching strategy string against a set of literals
     *  and either sets gParameters.matchingStrategy or returns !Status::isOK().
     */
    static Status onUpdateMatchingStrategy(const std::string& str);

    ShardingTaskExecutorPoolController() = default;
    ShardingTaskExecutorPoolController& operator=(ShardingTaskExecutorPoolController&&) = delete;

    void init(ConnectionPool* parent) override;

    HostGroup updateHost(const SpecificPool* pool,
                         const HostAndPort& host,
                         const HostState& stats) override;
    void removeHost(const SpecificPool* pool) override;

    ConnectionControls getControls(const SpecificPool* pool) override;

    Milliseconds hostTimeout() const override;
    Milliseconds pendingTimeout() const override;
    Milliseconds toRefreshTimeout() const override;

    StringData name() const override {
        return "ShardingTaskExecutorPoolController"_sd;
    }

private:
    void _addGroup(WithLock, const ReplicaSetChangeNotifier::State& state);
    void _removeGroup(WithLock, const std::string& key);

    /**
     * HostGroup is a shared state for a set of hosts (a replica set).
     *
     * When the ReplicaSetChangeListener is informed of a change to a replica set,
     * it creates a new HostGroup and fills it into _hostGroups[setName] and
     * _hostGroupsByHost[memberHost]. This does not immediately affect the results of getControls.
     *
     * When a SpecificPool calls updateHost, it checks _hostGroupsByHost to see if it belongs to
     * any group and pushes itself into hostData for that group. It then will update target for its
     * group according to the MatchingStrategy. It will also set shouldShutdown to true if every
     * member of the group has shouldShutdown at true.
     *
     * Note that a HostData can find itself orphaned from its HostGroup during a reconfig.
     */
    struct HostGroupData {
        // The ReplicaSet state for this set
        ReplicaSetChangeNotifier::State state;

        // Pointer index for each pool in the set
        stdx::unordered_set<const SpecificPool*> pools;

        // The number of connections that all hosts in the group should maintain
        size_t target = 0;
    };

    /**
     * HostData represents the current state for a specific HostAndPort/SpecificPool.
     *
     * It is mutated by updateHost/removeHost and used along with Parameters to form Controls
     * for getControls.
     */
    struct HostData {
        // The HostGroup associated with this pool.
        // Note that this will be invalid if there was a replica set change
        std::weak_ptr<HostGroupData> hostGroup;

        // The number of connections the host should maintain
        size_t target = 0;

        // This host is able to shutdown
        bool isAbleToShutdown = false;
    };

    ReplicaSetChangeListenerHandle _listener;

    stdx::mutex _mutex;

    stdx::unordered_map<const SpecificPool*, HostData> _poolData;
    stdx::unordered_map<std::string, std::shared_ptr<HostGroupData>> _hostGroups;
    stdx::unordered_map<HostAndPort, std::shared_ptr<HostGroupData>> _hostGroupsByHost;
};
}  // namespace mongo
