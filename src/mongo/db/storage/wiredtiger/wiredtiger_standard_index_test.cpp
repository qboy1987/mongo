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

#include <memory>

#include "mongo/base/init.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/catalog/collection_mock.h"
#include "mongo/db/catalog/index_catalog_entry.h"
#include "mongo/db/index/index_descriptor.h"
#include "mongo/db/json.h"
#include "mongo/db/operation_context_noop.h"
#include "mongo/db/storage/kv/kv_prefix.h"
#include "mongo/db/storage/sorted_data_interface_test_harness.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_index.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_record_store.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_recovery_unit.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_session_cache.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_util.h"
#include "mongo/unittest/temp_dir.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/system_clock_source.h"

namespace mongo {
namespace {

using std::string;

class MyHarnessHelper final : public SortedDataInterfaceHarnessHelper {
public:
    MyHarnessHelper() : _dbpath("wt_test"), _conn(nullptr) {
        const char* config = "create,cache_size=1G,";
        int ret = wiredtiger_open(_dbpath.path().c_str(), nullptr, config, &_conn);
        invariantWTOK(ret);

        _fastClockSource = std::make_unique<SystemClockSource>();
        _sessionCache = new WiredTigerSessionCache(_conn, _fastClockSource.get());
    }

    ~MyHarnessHelper() final {
        delete _sessionCache;
        _conn->close(_conn, nullptr);
    }

    std::unique_ptr<SortedDataInterface> newSortedDataInterface(bool unique, bool partial) final {
        std::string ns = "test.wt";
        OperationContextNoop opCtx(newRecoveryUnit().release());

        BSONObj spec = BSON("key" << BSON("a" << 1) << "name"
                                  << "testIndex"
                                  << "v"
                                  << static_cast<int>(IndexDescriptor::kLatestIndexVersion)
                                  << "ns"
                                  << ns
                                  << "unique"
                                  << unique);

        if (partial) {
            auto partialBSON =
                BSON(IndexDescriptor::kPartialFilterExprFieldName.toString() << BSON(""
                                                                                     << ""));
            spec = spec.addField(partialBSON.firstElement());
        }

        auto collection = std::make_unique<CollectionMock>(NamespaceString(ns));
        IndexDescriptor desc(collection.get(), "", spec);

        KVPrefix prefix = KVPrefix::kNotPrefixed;
        StatusWith<std::string> result = WiredTigerIndex::generateCreateString(
            kWiredTigerEngineName, "", "", desc, prefix.isPrefixed());
        ASSERT_OK(result.getStatus());

        string uri = "table:" + ns;
        invariantWTOK(WiredTigerIndex::Create(&opCtx, uri, result.getValue()));

        if (unique)
            return std::make_unique<WiredTigerIndexUnique>(&opCtx, uri, &desc, prefix);
        return std::make_unique<WiredTigerIndexStandard>(&opCtx, uri, &desc, prefix);
    }

    std::unique_ptr<RecoveryUnit> newRecoveryUnit() final {
        return std::make_unique<WiredTigerRecoveryUnit>(_sessionCache, &_oplogManager);
    }

private:
    unittest::TempDir _dbpath;
    std::unique_ptr<ClockSource> _fastClockSource;
    WT_CONNECTION* _conn;
    WiredTigerSessionCache* _sessionCache;
    WiredTigerOplogManager _oplogManager;
};

std::unique_ptr<HarnessHelper> makeHarnessHelper() {
    return std::make_unique<MyHarnessHelper>();
}

MONGO_INITIALIZER(RegisterHarnessFactory)(InitializerContext* const) {
    mongo::registerHarnessHelperFactory(makeHarnessHelper);
    return Status::OK();
}
}  // namespace
}  // namespace mongo
