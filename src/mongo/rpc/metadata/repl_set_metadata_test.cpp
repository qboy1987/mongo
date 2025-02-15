/**
 *    Copyright (C) 2018-present MongoDB, Inc.
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

#include "mongo/platform/basic.h"

#include "mongo/db/jsobj.h"
#include "mongo/rpc/metadata/repl_set_metadata.h"
#include "mongo/unittest/unittest.h"

namespace mongo {
namespace rpc {
namespace {

using repl::OpTime;
using repl::OpTimeAndWallTime;

TEST(ReplResponseMetadataTest, ReplicaSetIdNotSet) {
    ASSERT_FALSE(
        ReplSetMetadata(3, OpTimeAndWallTime(), OpTime(), 6, OID(), 12, -1).hasReplicaSetId());
}

TEST(ReplResponseMetadataTest, Roundtrip) {
    OpTime opTime(Timestamp(1234, 100), 5);
    OpTime opTime2(Timestamp(7777, 100), 6);
    Date_t committedWallTime = Date_t() + Seconds(opTime.getSecs());
    ReplSetMetadata metadata(3, {opTime, committedWallTime}, opTime2, 6, OID::gen(), 12, -1);

    ASSERT_EQ(opTime, metadata.getLastOpCommitted().opTime);
    ASSERT_EQ(committedWallTime, metadata.getLastOpCommitted().wallTime);
    ASSERT_EQ(opTime2, metadata.getLastOpVisible());
    ASSERT_TRUE(metadata.hasReplicaSetId());

    BSONObjBuilder builder;
    metadata.writeToMetadata(&builder).transitional_ignore();

    BSONObj expectedObj(
        BSON(kReplSetMetadataFieldName
             << BSON("term" << 3 << "lastOpCommitted"
                            << BSON("ts" << opTime.getTimestamp() << "t" << opTime.getTerm())
                            << "lastCommittedWall"
                            << committedWallTime
                            << "lastOpVisible"
                            << BSON("ts" << opTime2.getTimestamp() << "t" << opTime2.getTerm())
                            << "configVersion"
                            << 6
                            << "replicaSetId"
                            << metadata.getReplicaSetId()
                            << "primaryIndex"
                            << 12
                            << "syncSourceIndex"
                            << -1)));

    BSONObj serializedObj = builder.obj();
    ASSERT_BSONOBJ_EQ(expectedObj, serializedObj);

    auto cloneStatus = ReplSetMetadata::readFromMetadata(serializedObj, /*requireWallTime*/ true);
    ASSERT_OK(cloneStatus.getStatus());

    const auto& clonedMetadata = cloneStatus.getValue();
    ASSERT_EQ(opTime, clonedMetadata.getLastOpCommitted().opTime);
    ASSERT_EQ(opTime2, clonedMetadata.getLastOpVisible());
    ASSERT_EQ(committedWallTime, clonedMetadata.getLastOpCommitted().wallTime);
    ASSERT_EQ(metadata.getConfigVersion(), clonedMetadata.getConfigVersion());
    ASSERT_EQ(metadata.getReplicaSetId(), clonedMetadata.getReplicaSetId());

    BSONObjBuilder clonedBuilder;
    clonedMetadata.writeToMetadata(&clonedBuilder).transitional_ignore();

    BSONObj clonedSerializedObj = clonedBuilder.obj();
    ASSERT_BSONOBJ_EQ(expectedObj, clonedSerializedObj);
}

TEST(ReplResponseMetadataTest, MetadataCanBeConstructedWhenMissingOplogQueryMetadataFields) {
    auto id = OID::gen();
    BSONObj obj(BSON(kReplSetMetadataFieldName
                     << BSON("term" << 3 << "configVersion" << 6 << "replicaSetId" << id)));

    auto status = ReplSetMetadata::readFromMetadata(obj, /*requireWallTime*/ true);
    ASSERT_OK(status.getStatus());

    const auto& metadata = status.getValue();
    ASSERT_EQ(metadata.getConfigVersion(), 6);
    ASSERT_EQ(metadata.getReplicaSetId(), id);
    ASSERT_EQ(metadata.getTerm(), 3);
}

}  // unnamed namespace
}  // namespace rpc
}  // namespace mongo
