// Tests that save style updates correctly change immutable fields
(function() {
    'use strict';

    var st = new ShardingTest({shards: 2, mongos: 1});

    var mongos = st.s;
    var config = mongos.getDB("config");
    var coll = mongos.getCollection(jsTestName() + ".coll1");
    var shard0 = st.shard0;

    assert.commandWorked(config.adminCommand({enableSharding: coll.getDB() + ""}));
    st.ensurePrimaryShard(coll.getDB().getName(), st.shard0.shardName);
    assert.commandWorked(config.adminCommand({shardCollection: "" + coll, key: {a: 1}}));

    var getDirectShardedConn = function(st, collName) {

        var shardConnWithVersion = new Mongo(st.shard0.host);

        var configConnStr = st._configDB;

        var maxChunk =
            st.s0.getCollection("config.chunks").find({ns: collName}).sort({lastmod: -1}).next();

        var ssvInitCmd = {
            setShardVersion: collName,
            authoritative: true,
            configdb: configConnStr,
            version: maxChunk.lastmod,
            shard: st.shard0.shardName,
            versionEpoch: maxChunk.lastmodEpoch
        };

        printjson(ssvInitCmd);
        assert.commandWorked(shardConnWithVersion.getDB("admin").runCommand(ssvInitCmd));

        return shardConnWithVersion;
    };

    var shard0Coll = getDirectShardedConn(st, coll.getFullName()).getCollection(coll.getFullName());

    // No shard key
    shard0Coll.remove({});
    assert.writeError(shard0Coll.save({_id: 3}));

    // Full shard key in save
    assert.writeOK(shard0Coll.save({_id: 1, a: 1}));

    // Full shard key on replacement (basically the same as above)
    shard0Coll.remove({});
    assert.writeOK(shard0Coll.update({_id: 1}, {a: 1}, true));

    // Full shard key after $set
    shard0Coll.remove({});
    assert.writeOK(shard0Coll.update({_id: 1}, {$set: {a: 1}}, true));

    // Update existing doc (replacement), same shard key value
    assert.writeOK(shard0Coll.update({_id: 1}, {a: 1}));

    // Update existing doc ($set), same shard key value
    assert.commandWorked(shard0Coll.update({_id: 1}, {$set: {a: 1}}));

    // Error when trying to update a shard key outside of a transaction.
    assert.commandFailedWithCode(shard0Coll.update({_id: 1, a: 1}, {_id: 1, a: 2}),
                                 ErrorCodes.IllegalOperation);
    assert.commandFailedWithCode(shard0Coll.update({_id: 1, a: 1}, {"$set": {a: 2}}),
                                 ErrorCodes.IllegalOperation);

    // Error when unsetting shard key.
    assert.writeError(shard0Coll.update({_id: 1}, {b: 3}));

    // Error when unsetting shard key ($set).
    assert.writeError(shard0Coll.update({_id: 1}, {$unset: {a: 1}}));

    // Error due to removing all the embedded fields.
    shard0Coll.remove({});

    assert.writeOK(shard0Coll.save({_id: 2, a: {c: 1, b: 1}}));

    assert.writeError(shard0Coll.update({}, {$unset: {"a.c": 1}}));
    assert.writeError(shard0Coll.update({}, {$unset: {"a.b": 1, "a.c": 1}}));

    st.stop();

})();
