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

#include "mongo/s/query/document_source_merge_cursors.h"

#include "mongo/db/pipeline/document_source_sort.h"
#include "mongo/db/query/find_common.h"
#include "mongo/executor/task_executor_pool.h"
#include "mongo/s/grid.h"
#include "mongo/s/query/establish_cursors.h"

namespace mongo {

REGISTER_DOCUMENT_SOURCE(mergeCursors,
                         LiteParsedDocumentSourceDefault::parse,
                         DocumentSourceMergeCursors::createFromBson);

constexpr StringData DocumentSourceMergeCursors::kStageName;

DocumentSourceMergeCursors::DocumentSourceMergeCursors(
    executor::TaskExecutor* executor,
    AsyncResultsMergerParams armParams,
    const boost::intrusive_ptr<ExpressionContext>& expCtx,
    boost::optional<BSONObj> ownedParamsSpec)
    : DocumentSource(expCtx),
      _armParamsObj(std::move(ownedParamsSpec)),
      _executor(executor),
      _armParams(std::move(armParams)) {}

std::size_t DocumentSourceMergeCursors::getNumRemotes() const {
    if (_armParams) {
        return _armParams->getRemotes().size();
    }
    return _blockingResultsMerger->getNumRemotes();
}

BSONObj DocumentSourceMergeCursors::getHighWaterMark() {
    if (!_blockingResultsMerger) {
        populateMerger();
    }
    return _blockingResultsMerger->getHighWaterMark();
}

bool DocumentSourceMergeCursors::remotesExhausted() const {
    if (_armParams) {
        // We haven't started iteration yet.
        return false;
    }
    return _blockingResultsMerger->remotesExhausted();
}

void DocumentSourceMergeCursors::populateMerger() {
    invariant(!_blockingResultsMerger);
    invariant(_armParams);

    _blockingResultsMerger.emplace(pExpCtx->opCtx,
                                   std::move(*_armParams),
                                   _executor,
                                   pExpCtx->mongoProcessInterface->getResourceYielder());
    _armParams = boost::none;
    // '_blockingResultsMerger' now owns the cursors.
    _ownCursors = false;
}

std::unique_ptr<RouterStageMerge> DocumentSourceMergeCursors::convertToRouterStage() {
    invariant(!_blockingResultsMerger, "Expected conversion to happen before execution");
    return std::make_unique<RouterStageMerge>(pExpCtx->opCtx, _executor, std::move(*_armParams));
}

DocumentSource::GetNextResult DocumentSourceMergeCursors::getNext() {
    if (!_blockingResultsMerger) {
        populateMerger();
    }

    auto next = uassertStatusOK(_blockingResultsMerger->next(pExpCtx->opCtx, _execContext));
    if (next.isEOF()) {
        return GetNextResult::makeEOF();
    }
    return Document::fromBsonWithMetaData(*next.getResult());
}

Value DocumentSourceMergeCursors::serialize(
    boost::optional<ExplainOptions::Verbosity> explain) const {
    invariant(!_blockingResultsMerger);
    invariant(_armParams);
    return Value(Document{{kStageName, _armParams->toBSON()}});
}

boost::intrusive_ptr<DocumentSource> DocumentSourceMergeCursors::createFromBson(
    BSONElement elem, const boost::intrusive_ptr<ExpressionContext>& expCtx) {
    uassert(17026,
            "$mergeCursors stage expected an object as argument",
            elem.type() == BSONType::Object);
    auto ownedObj = elem.embeddedObject().getOwned();
    auto armParams = AsyncResultsMergerParams::parse(IDLParserErrorContext(kStageName), ownedObj);
    return new DocumentSourceMergeCursors(
        Grid::get(expCtx->opCtx)->getExecutorPool()->getArbitraryExecutor(),
        std::move(armParams),
        expCtx,
        std::move(ownedObj));
}

boost::intrusive_ptr<DocumentSourceMergeCursors> DocumentSourceMergeCursors::create(
    executor::TaskExecutor* executor,
    AsyncResultsMergerParams params,
    const boost::intrusive_ptr<ExpressionContext>& expCtx) {
    return new DocumentSourceMergeCursors(executor, std::move(params), expCtx);
}

void DocumentSourceMergeCursors::detachFromOperationContext() {
    if (_blockingResultsMerger) {
        _blockingResultsMerger->detachFromOperationContext();
    }
}

void DocumentSourceMergeCursors::reattachToOperationContext(OperationContext* opCtx) {
    if (_blockingResultsMerger) {
        _blockingResultsMerger->reattachToOperationContext(opCtx);
    }
}

void DocumentSourceMergeCursors::doDispose() {
    if (_blockingResultsMerger) {
        invariant(!_ownCursors);
        _blockingResultsMerger->kill(pExpCtx->opCtx);
    } else if (_ownCursors) {
        populateMerger();
        _blockingResultsMerger->kill(pExpCtx->opCtx);
    }
}

}  // namespace mongo
