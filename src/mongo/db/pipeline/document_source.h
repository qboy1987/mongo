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

#pragma once

#include "mongo/platform/basic.h"

#include <boost/intrusive_ptr.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "mongo/base/init.h"
#include "mongo/bson/simple_bsonobj_comparator.h"
#include "mongo/db/collection_index_usage_tracker.h"
#include "mongo/db/commands.h"
#include "mongo/db/generic_cursor.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/pipeline/dependencies.h"
#include "mongo/db/pipeline/document.h"
#include "mongo/db/pipeline/expression_context.h"
#include "mongo/db/pipeline/field_path.h"
#include "mongo/db/pipeline/lite_parsed_document_source.h"
#include "mongo/db/pipeline/pipeline.h"
#include "mongo/db/pipeline/stage_constraints.h"
#include "mongo/db/pipeline/value.h"
#include "mongo/db/query/explain_options.h"
#include "mongo/util/intrusive_counter.h"

namespace mongo {

class AggregationRequest;
class Document;

/**
 * Registers a DocumentSource to have the name 'key'.
 *
 * 'liteParser' takes an AggregationRequest and a BSONElement and returns a
 * LiteParsedDocumentSource. This is used for checks that need to happen before a full parse,
 * such as checks about which namespaces are referenced by this aggregation.
 *
 * 'fullParser' takes a BSONElement and an ExpressionContext and returns a fully-executable
 * DocumentSource. This will be used for optimization and execution.
 *
 * Stages that do not require any special pre-parse checks can use
 * LiteParsedDocumentSourceDefault::parse as their 'liteParser'.
 *
 * As an example, if your stage DocumentSourceFoo looks like {$foo: <args>} and does *not* require
 * any special pre-parse checks, you should implement a static parser like
 * DocumentSourceFoo::createFromBson(), and register it like so:
 * REGISTER_DOCUMENT_SOURCE(foo,
 *                          LiteParsedDocumentSourceDefault::parse,
 *                          DocumentSourceFoo::createFromBson);
 *
 * If your stage is actually an alias which needs to return more than one stage (such as
 * $sortByCount), you should use the REGISTER_MULTI_STAGE_ALIAS macro instead.
 */
#define REGISTER_DOCUMENT_SOURCE_CONDITIONALLY(key, liteParser, fullParser, ...)             \
    MONGO_INITIALIZER(addToDocSourceParserMap_##key)(InitializerContext*) {                  \
        if (!__VA_ARGS__) {                                                                  \
            return Status::OK();                                                             \
        }                                                                                    \
        auto fullParserWrapper = [](BSONElement stageSpec,                                   \
                                    const boost::intrusive_ptr<ExpressionContext>& expCtx) { \
            return std::list<boost::intrusive_ptr<DocumentSource>>{                          \
                (fullParser)(stageSpec, expCtx)};                                            \
        };                                                                                   \
        LiteParsedDocumentSource::registerParser("$" #key, liteParser);                      \
        DocumentSource::registerParser("$" #key, fullParserWrapper);                         \
        return Status::OK();                                                                 \
    }

#define REGISTER_DOCUMENT_SOURCE(key, liteParser, fullParser) \
    REGISTER_DOCUMENT_SOURCE_CONDITIONALLY(key, liteParser, fullParser, true)

#define REGISTER_TEST_DOCUMENT_SOURCE(key, liteParser, fullParser) \
    REGISTER_DOCUMENT_SOURCE_CONDITIONALLY(                        \
        key, liteParser, fullParser, ::mongo::getTestCommandsEnabled())

/**
 * Registers a multi-stage alias (such as $sortByCount) to have the single name 'key'. When a stage
 * with name '$key' is found, 'liteParser' will be used to produce a LiteParsedDocumentSource,
 * while 'fullParser' will be called to construct a vector of DocumentSources. See the comments on
 * REGISTER_DOCUMENT_SOURCE for more information.
 *
 * As an example, if your stage alias looks like {$foo: <args>} and does *not* require any special
 * pre-parse checks, you should implement a static parser like DocumentSourceFoo::createFromBson(),
 * and register it like so:
 * REGISTER_MULTI_STAGE_ALIAS(foo,
 *                            LiteParsedDocumentSourceDefault::parse,
 *                            DocumentSourceFoo::createFromBson);
 */
#define REGISTER_MULTI_STAGE_ALIAS(key, liteParser, fullParser)                  \
    MONGO_INITIALIZER(addAliasToDocSourceParserMap_##key)(InitializerContext*) { \
        LiteParsedDocumentSource::registerParser("$" #key, (liteParser));        \
        DocumentSource::registerParser("$" #key, (fullParser));                  \
        return Status::OK();                                                     \
    }

class DocumentSource : public RefCountable {
public:
    using Parser = std::function<std::list<boost::intrusive_ptr<DocumentSource>>(
        BSONElement, const boost::intrusive_ptr<ExpressionContext>&)>;

    using ChangeStreamRequirement = StageConstraints::ChangeStreamRequirement;
    using HostTypeRequirement = StageConstraints::HostTypeRequirement;
    using PositionRequirement = StageConstraints::PositionRequirement;
    using DiskUseRequirement = StageConstraints::DiskUseRequirement;
    using FacetRequirement = StageConstraints::FacetRequirement;
    using StreamType = StageConstraints::StreamType;
    using TransactionRequirement = StageConstraints::TransactionRequirement;
    using LookupRequirement = StageConstraints::LookupRequirement;

    /**
     * This is what is returned from the main DocumentSource API: getNext(). It is essentially a
     * (ReturnStatus, Document) pair, with the first entry being used to communicate information
     * about the execution of the DocumentSource, such as whether or not it has been exhausted.
     */
    class GetNextResult {
    public:
        enum class ReturnStatus {
            // There is a result to be processed.
            kAdvanced,
            // There will be no further results.
            kEOF,
            // There is not a result to be processed yet, but there may be more results in the
            // future. If a DocumentSource retrieves this status from its child, it must propagate
            // it without doing any further work.
            kPauseExecution,
        };

        static GetNextResult makeEOF() {
            return GetNextResult(ReturnStatus::kEOF);
        }

        static GetNextResult makePauseExecution() {
            return GetNextResult(ReturnStatus::kPauseExecution);
        }

        /**
         * Shortcut constructor for the common case of creating an 'advanced' GetNextResult from the
         * given 'result'. Accepts only an rvalue reference as an argument, since DocumentSources
         * will want to move 'result' into this GetNextResult, and should have to opt in to making a
         * copy.
         */
        /* implicit */ GetNextResult(Document&& result)
            : _status(ReturnStatus::kAdvanced), _result(std::move(result)) {}

        /**
         * Gets the result document. It is an error to call this if isAdvanced() returns false.
         */
        const Document& getDocument() const {
            dassert(isAdvanced());
            return _result;
        }

        /**
         * Releases the result document, transferring ownership to the caller. It is an error to
         * call this if isAdvanced() returns false.
         */
        Document releaseDocument() {
            dassert(isAdvanced());
            return std::move(_result);
        }

        ReturnStatus getStatus() const {
            return _status;
        }

        bool isAdvanced() const {
            return _status == ReturnStatus::kAdvanced;
        }

        bool isEOF() const {
            return _status == ReturnStatus::kEOF;
        }

        bool isPaused() const {
            return _status == ReturnStatus::kPauseExecution;
        }

    private:
        GetNextResult(ReturnStatus status) : _status(status) {}

        ReturnStatus _status;
        Document _result;
    };

    /**
     * A struct representing the information needed to execute this stage on a distributed
     * collection. Describes how a pipeline should be split for sharded execution.
     */
    struct DistributedPlanLogic {
        // A stage which executes on each shard in parallel, or nullptr if nothing can be done in
        // parallel. For example, a partial $group before a subsequent global $group.
        boost::intrusive_ptr<DocumentSource> shardsStage = nullptr;

        // A stage which executes after merging all the results together, or nullptr if nothing is
        // necessary after merging. For example, a $limit stage.
        boost::intrusive_ptr<DocumentSource> mergingStage = nullptr;

        // If set, each document is expected to have sort key metadata which will be serialized in
        // the '$sortKey' field. 'inputSortPattern' will then be used to describe which fields are
        // ascending and which fields are descending when merging the streams together.
        boost::optional<BSONObj> inputSortPattern = boost::none;
    };

    virtual ~DocumentSource() {}

    /**
     * The main execution API of a DocumentSource. Returns an intermediate query result generated by
     * this DocumentSource.
     *
     * All implementers must call pExpCtx->checkForInterrupt().
     *
     * For performance reasons, a streaming stage must not keep references to documents across calls
     * to getNext(). Such stages must retrieve a result from their child and then release it (or
     * return it) before asking for another result. Failing to do so can result in extra work, since
     * the Document/Value library must copy data on write when that data has a refcount above one.
     */
    virtual GetNextResult getNext() = 0;

    /**
     * Returns a struct containing information about any special constraints imposed on using this
     * stage. Input parameter Pipeline::SplitState is used by stages whose requirements change
     * depending on whether they are in a split or unsplit pipeline.
     */
    virtual StageConstraints constraints(
        Pipeline::SplitState = Pipeline::SplitState::kUnsplit) const = 0;

    /**
     * Informs the stage that it is no longer needed and can release its resources. After dispose()
     * is called the stage must still be able to handle calls to getNext(), but can return kEOF.
     *
     * This is a non-virtual public interface to ensure dispose() is threaded through the entire
     * pipeline. Subclasses should override doDispose() to implement their disposal.
     */
    void dispose() {
        doDispose();
        if (pSource) {
            pSource->dispose();
        }
    }

    /**
     * Get the stage's name.
     */
    virtual const char* getSourceName() const;

    /**
     * Set the underlying source this source should use to get Documents from. Must not throw
     * exceptions.
     */
    virtual void setSource(DocumentSource* source) {
        pSource = source;
    }

    /**
     * In the default case, serializes the DocumentSource and adds it to the std::vector<Value>.
     *
     * A subclass may choose to overwrite this, rather than serialize, if it should output multiple
     * stages (eg, $sort sometimes also outputs a $limit).
     *
     * The 'explain' parameter indicates the explain verbosity mode, or is equal boost::none if no
     * explain is requested.
     */
    virtual void serializeToArray(
        std::vector<Value>& array,
        boost::optional<ExplainOptions::Verbosity> explain = boost::none) const;

    /**
     * If this stage uses additional namespaces, adds them to 'collectionNames'. These namespaces
     * should all be names of collections, not views.
     */
    virtual void addInvolvedCollections(
        stdx::unordered_set<NamespaceString>* collectionNames) const {}


    virtual void detachFromOperationContext() {}

    virtual void reattachToOperationContext(OperationContext* opCtx) {}

    virtual bool usedDisk() {
        return false;
    };

    /**
     * Create a DocumentSource pipeline stage from 'stageObj'.
     */
    static std::list<boost::intrusive_ptr<DocumentSource>> parse(
        const boost::intrusive_ptr<ExpressionContext>& expCtx, BSONObj stageObj);

    /**
     * Registers a DocumentSource with a parsing function, so that when a stage with the given name
     * is encountered, it will call 'parser' to construct that stage.
     *
     * DO NOT call this method directly. Instead, use the REGISTER_DOCUMENT_SOURCE macro defined in
     * this file.
     */
    static void registerParser(std::string name, Parser parser);

private:
    /**
     * Attempt to push a match stage from directly ahead of the current stage given by itr to before
     * the current stage. Returns whether the optimization was performed.
     */
    bool pushMatchBefore(Pipeline::SourceContainer::iterator itr,
                         Pipeline::SourceContainer* container);

    /**
     * Attempt to push a sample stage from directly ahead of the current stage given by itr to
     * before the current stage. Returns whether the optimization was performed.
     */
    bool pushSampleBefore(Pipeline::SourceContainer::iterator itr,
                          Pipeline::SourceContainer* container);

public:
    /**
     * The non-virtual public interface for optimization. Attempts to do some generic optimizations
     * such as pushing $matches as early in the pipeline as possible, then calls out to
     * doOptimizeAt() for stage-specific optimizations.
     *
     * Subclasses should override doOptimizeAt() if they can apply some optimization(s) based on
     * subsequent stages in the pipeline.
     */
    Pipeline::SourceContainer::iterator optimizeAt(Pipeline::SourceContainer::iterator itr,
                                                   Pipeline::SourceContainer* container);

    /**
     * Returns an optimized DocumentSource that is semantically equivalent to this one, or
     * nullptr if this stage is a no-op. Implementations are allowed to modify themselves
     * in-place and return a pointer to themselves. For best results, first optimize the pipeline
     * with the optimizePipeline() method defined in pipeline.cpp.
     *
     * This is intended for any operations that include expressions, and provides a hook for
     * those to optimize those operations.
     *
     * The default implementation is to do nothing and return yourself.
     */
    virtual boost::intrusive_ptr<DocumentSource> optimize();

    //
    // Property Analysis - These methods allow a DocumentSource to expose information about
    // properties of themselves, such as which fields they need to apply their transformations, and
    // whether or not they produce or preserve a sort order.
    //
    // Property analysis can be useful during optimization (e.g. analysis of sort orders determines
    // whether or not a blocking group can be upgraded to a streaming group).
    //

    struct GetModPathsReturn {
        enum class Type {
            // No information is available about which paths are modified.
            kNotSupported,

            // All fields will be modified. This should be used by stages like $replaceRoot which
            // modify the entire document.
            kAllPaths,

            // A finite set of paths will be modified by this stage. This is true for something like
            // {$project: {a: 0, b: 0}}, which will only modify 'a' and 'b', and leave all other
            // paths unmodified.
            kFiniteSet,

            // This stage will modify an infinite set of paths, but we know which paths it will not
            // modify. For example, the stage {$project: {_id: 1, a: 1}} will leave only the fields
            // '_id' and 'a' unmodified, but all other fields will be projected out.
            kAllExcept,
        };

        GetModPathsReturn(Type type,
                          std::set<std::string>&& paths,
                          StringMap<std::string>&& renames)
            : type(type), paths(std::move(paths)), renames(std::move(renames)) {}

        Type type;
        std::set<std::string> paths;

        // Stages may fill out 'renames' to contain information about path renames. Each entry in
        // 'renames' maps from the new name of the path (valid in documents flowing *out* of this
        // stage) to the old name of the path (valid in documents flowing *into* this stage).
        //
        // For example, consider the stage
        //
        //   {$project: {_id: 0, a: 1, b: "$c"}}
        //
        // This stage should return kAllExcept, since it modifies all paths other than "a". It can
        // also fill out 'renames' with the mapping "b" => "c".
        StringMap<std::string> renames;
    };

    /**
     * Returns information about which paths are added, removed, or updated by this stage. The
     * default implementation uses kNotSupported to indicate that the set of modified paths for this
     * stage is not known.
     *
     * See GetModPathsReturn above for the possible return values and what they mean.
     */
    virtual GetModPathsReturn getModifiedPaths() const {
        return {GetModPathsReturn::Type::kNotSupported, std::set<std::string>{}, {}};
    }

    /**
     * Returns the expression context from the stage's context.
     */
    const boost::intrusive_ptr<ExpressionContext>& getContext() const {
        return pExpCtx;
    }

    /**
     * Given 'currentNames' which describes a set of paths which the caller is interested in,
     * returns boost::none if any of those paths are modified by this stage, or a mapping from
     * their old name to their new name if they are preserved but possibly renamed by this stage.
     */
    boost::optional<StringMap<std::string>> renamedPaths(
        const std::set<std::string>& currentNames) const;

    /**
     * Get the dependencies this operation needs to do its job. If overridden, subclasses must add
     * all paths needed to apply their transformation to 'deps->fields', and call
     * 'deps->setNeedsMetadata()' to indicate what metadata (e.g. text score), if any, is required.
     *
     * See DepsTracker::State for the possible return values and what they mean.
     */
    virtual DepsTracker::State getDependencies(DepsTracker* deps) const {
        return DepsTracker::State::NOT_SUPPORTED;
    }

    /**
     * If this stage can be run in parallel across a distributed collection, returns boost::none.
     * Otherwise, returns a struct representing what needs to be done to merge each shard's pipeline
     * into a single stream of results. Must not mutate the existing source object; if different
     * behaviour is required, a new source should be created and configured appropriately. It is an
     * error for the returned DistributedPlanLogic to have identical pointers for 'shardsStage' and
     * 'mergingStage'.
     */
    virtual boost::optional<DistributedPlanLogic> distributedPlanLogic() = 0;

    /**
     * Returns true if it would be correct to execute this stage in parallel across the shards in
     * cases where the final stage is a stage which can perform a write operation, such as $merge.
     * For example, a $group stage which is just merging the groups from the shards can be run in
     * parallel since it will preserve the shard key.
     */
    virtual bool canRunInParallelBeforeWriteStage(
        const std::set<std::string>& nameOfShardKeyFieldsUponEntryToStage) const {
        return false;
    }

protected:
    explicit DocumentSource(const boost::intrusive_ptr<ExpressionContext>& pExpCtx);

    /**
     * Attempt to perform an optimization with the following source in the pipeline. 'container'
     * refers to the entire pipeline, and 'itr' points to this stage within the pipeline.
     *
     * The return value is an iterator over the same container which points to the first location
     * in the container at which an optimization may be possible, or the end of the container().
     *
     * For example, if a swap takes place, the returned iterator should just be the position
     * directly preceding 'itr', if such a position exists, since the stage at that position may be
     * able to perform further optimizations with its new neighbor.
     */
    virtual Pipeline::SourceContainer::iterator doOptimizeAt(
        Pipeline::SourceContainer::iterator itr, Pipeline::SourceContainer* container) {
        return std::next(itr);
    };

    /**
     * Release any resources held by this stage. After doDispose() is called the stage must still be
     * able to handle calls to getNext(), but can return kEOF.
     */
    virtual void doDispose() {}

    /*
      Most DocumentSources have an underlying source they get their data
      from.  This is a convenience for them.

      The default implementation of setSource() sets this; if you don't
      need a source, override that to verify().  The default is to
      verify() if this has already been set.
    */
    DocumentSource* pSource;

    boost::intrusive_ptr<ExpressionContext> pExpCtx;

private:
    /**
     * Create a Value that represents the document source.
     *
     * This is used by the default implementation of serializeToArray() to add this object
     * to a pipeline being serialized. Returning a missing() Value results in no entry
     * being added to the array for this stage (DocumentSource).
     *
     * The 'explain' parameter indicates the explain verbosity mode, or is equal boost::none if no
     * explain is requested.
     */
    virtual Value serialize(
        boost::optional<ExplainOptions::Verbosity> explain = boost::none) const = 0;
};

}  // namespace mongo
