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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kQuery

#include "mongo/db/query/planner_analysis.h"

#include <set>
#include <vector>

#include "mongo/bson/simple_bsonelement_comparator.h"
#include "mongo/db/bson/dotted_path_support.h"
#include "mongo/db/index/expression_params.h"
#include "mongo/db/index/s2_common.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/matcher/expression_geo.h"
#include "mongo/db/query/query_planner.h"
#include "mongo/db/query/query_planner_common.h"
#include "mongo/util/log.h"

namespace mongo {

using std::unique_ptr;
using std::endl;
using std::string;
using std::vector;

namespace dps = ::mongo::dotted_path_support;

//
// Helpers for bounds explosion AKA quick-and-dirty SERVER-1205.
//

namespace {

/**
 * Walk the tree 'root' and output all leaf nodes into 'leafNodes'.
 */
void getLeafNodes(QuerySolutionNode* root, vector<QuerySolutionNode*>* leafNodes) {
    if (0 == root->children.size()) {
        leafNodes->push_back(root);
    } else {
        for (size_t i = 0; i < root->children.size(); ++i) {
            getLeafNodes(root->children[i], leafNodes);
        }
    }
}

/**
 * Returns true if every interval in 'oil' is a point, false otherwise.
 */
bool isUnionOfPoints(const OrderedIntervalList& oil) {
    // We can't explode if there are empty bounds. Don't consider the
    // oil a union of points if there are no intervals.
    if (0 == oil.intervals.size()) {
        return false;
    }

    for (size_t i = 0; i < oil.intervals.size(); ++i) {
        if (!oil.intervals[i].isPoint()) {
            return false;
        }
    }

    return true;
}

/**
 * Should we try to expand the index scan(s) in 'solnRoot' to pull out an indexed sort?
 *
 * Returns the node which should be replaced by the merge sort of exploded scans
 * in the out-parameter 'toReplace'.
 */
bool structureOKForExplode(QuerySolutionNode* solnRoot, QuerySolutionNode** toReplace) {
    // For now we only explode if we *know* we will pull the sort out.  We can look at
    // more structure (or just explode and recalculate properties and see what happens)
    // but for now we just explode if it's a sure bet.
    //
    // TODO: Can also try exploding if root is AND_HASH (last child dictates order.),
    // or other less obvious cases...

    // Skip over a sharding filter stage.
    if (STAGE_SHARDING_FILTER == solnRoot->getType()) {
        solnRoot = solnRoot->children[0];
    }

    if (STAGE_IXSCAN == solnRoot->getType()) {
        *toReplace = solnRoot;
        return true;
    }

    if (STAGE_FETCH == solnRoot->getType()) {
        if (STAGE_IXSCAN == solnRoot->children[0]->getType()) {
            *toReplace = solnRoot->children[0];
            return true;
        }
    }

    if (STAGE_OR == solnRoot->getType()) {
        for (size_t i = 0; i < solnRoot->children.size(); ++i) {
            if (STAGE_IXSCAN != solnRoot->children[i]->getType()) {
                return false;
            }
        }
        *toReplace = solnRoot;
        return true;
    }

    return false;
}

// vectors of vectors can be > > annoying.
typedef vector<Interval> PointPrefix;

/**
 * The first 'fieldsToExplode' fields of 'bounds' are points.  Compute the Cartesian product
 * of those fields and place it in 'prefixOut'.
 */
void makeCartesianProduct(const IndexBounds& bounds,
                          size_t fieldsToExplode,
                          vector<PointPrefix>* prefixOut) {
    vector<PointPrefix> prefixForScans;

    // We dump the Cartesian product of bounds into prefixForScans, starting w/the first
    // field's points.
    verify(fieldsToExplode >= 1);
    const OrderedIntervalList& firstOil = bounds.fields[0];
    verify(firstOil.intervals.size() >= 1);
    for (size_t i = 0; i < firstOil.intervals.size(); ++i) {
        const Interval& ival = firstOil.intervals[i];
        verify(ival.isPoint());
        PointPrefix pfix;
        pfix.push_back(ival);
        prefixForScans.push_back(pfix);
    }

    // For each subsequent field...
    for (size_t i = 1; i < fieldsToExplode; ++i) {
        vector<PointPrefix> newPrefixForScans;
        const OrderedIntervalList& oil = bounds.fields[i];
        verify(oil.intervals.size() >= 1);
        // For each point interval in that field (all ivals must be points)...
        for (size_t j = 0; j < oil.intervals.size(); ++j) {
            const Interval& ival = oil.intervals[j];
            verify(ival.isPoint());
            // Make a new scan by appending it to all scans in prefixForScans.
            for (size_t k = 0; k < prefixForScans.size(); ++k) {
                PointPrefix pfix = prefixForScans[k];
                pfix.push_back(ival);
                newPrefixForScans.push_back(pfix);
            }
        }
        // And update prefixForScans.
        newPrefixForScans.swap(prefixForScans);
    }

    prefixOut->swap(prefixForScans);
}

/**
 * Take the provided index scan node 'isn'. Returns a list of index scans which are
 * logically equivalent to 'isn' if joined by a MergeSort through the out-parameter
 * 'explosionResult'. These index scan instances are owned by the caller.
 *
 * fieldsToExplode is a count of how many fields in the scan's bounds are the union of point
 * intervals.  This is computed beforehand and provided as a small optimization.
 *
 * Example:
 *
 * For the query find({a: {$in: [1,2]}}).sort({b: 1}) using the index {a:1, b:1}:
 * 'isn' will be scan with bounds a:[[1,1],[2,2]] & b: [MinKey, MaxKey]
 * 'sort' will be {b: 1}
 * 'fieldsToExplode' will be 1 (as only one field isUnionOfPoints).
 *
 * On return, 'explosionResult' will contain the following two scans:
 * a:[[1,1]], b:[MinKey, MaxKey]
 * a:[[2,2]], b:[MinKey, MaxKey]
 */
void explodeScan(const IndexScanNode* isn,
                 const BSONObj& sort,
                 size_t fieldsToExplode,
                 vector<QuerySolutionNode*>* explosionResult) {
    // Turn the compact bounds in 'isn' into a bunch of points...
    vector<PointPrefix> prefixForScans;
    makeCartesianProduct(isn->bounds, fieldsToExplode, &prefixForScans);

    for (size_t i = 0; i < prefixForScans.size(); ++i) {
        const PointPrefix& prefix = prefixForScans[i];
        verify(prefix.size() == fieldsToExplode);

        // Copy boring fields into new child.
        IndexScanNode* child = new IndexScanNode(isn->index);
        child->direction = isn->direction;
        child->addKeyMetadata = isn->addKeyMetadata;
        child->queryCollator = isn->queryCollator;

        // Copy the filter, if there is one.
        if (isn->filter.get()) {
            child->filter = isn->filter->shallowClone();
        }

        // Create child bounds.
        child->bounds.fields.resize(isn->bounds.fields.size());
        for (size_t j = 0; j < fieldsToExplode; ++j) {
            child->bounds.fields[j].intervals.push_back(prefix[j]);
            child->bounds.fields[j].name = isn->bounds.fields[j].name;
        }
        for (size_t j = fieldsToExplode; j < isn->bounds.fields.size(); ++j) {
            child->bounds.fields[j] = isn->bounds.fields[j];
        }
        explosionResult->push_back(child);
    }
}

/**
 * In the tree '*root', replace 'oldNode' with 'newNode'.
 */
void replaceNodeInTree(QuerySolutionNode** root,
                       QuerySolutionNode* oldNode,
                       QuerySolutionNode* newNode) {
    if (*root == oldNode) {
        *root = newNode;
    } else {
        for (size_t i = 0; i < (*root)->children.size(); ++i) {
            replaceNodeInTree(&(*root)->children[i], oldNode, newNode);
        }
    }
}

bool hasNode(QuerySolutionNode* root, StageType type) {
    if (type == root->getType()) {
        return true;
    }

    for (size_t i = 0; i < root->children.size(); ++i) {
        if (hasNode(root->children[i], type)) {
            return true;
        }
    }

    return false;
}

void geoSkipValidationOn(const std::set<StringData>& twoDSphereFields,
                         QuerySolutionNode* solnRoot) {
    // If there is a GeoMatchExpression in the tree on a field with a 2dsphere index,
    // we can skip validation since it was validated on insertion. This only applies to
    // 2dsphere index version >= 3.
    //
    // This does not mean that there is necessarily an IXSCAN using this 2dsphere index,
    // only that there exists a 2dsphere index on this field.
    MatchExpression* expr = solnRoot->filter.get();
    if (expr) {
        StringData nodeField = expr->path();
        if (expr->matchType() == MatchExpression::GEO &&
            twoDSphereFields.find(nodeField) != twoDSphereFields.end()) {
            GeoMatchExpression* gme = static_cast<GeoMatchExpression*>(expr);
            gme->setCanSkipValidation(true);
        }
    }

    for (QuerySolutionNode* child : solnRoot->children) {
        geoSkipValidationOn(twoDSphereFields, child);
    }
}

/**
 * If any field is missing from the list of fields the projection wants, we are not covered.
 */
auto isCoveredOrAlreadyFetched(const vector<StringData>& fields,
                               const QuerySolutionNode& solnRoot) {
    for (size_t i = 0; i < fields.size(); ++i) {
        if (!solnRoot.hasField(fields[i].toString()))
            return false;
    }
    return true;
}

/**
 * Checks all properties that exclude a projection from being simple.
 */
auto isSimpleProjection(const CanonicalQuery& query) {
    return !query.getProj()->wantIndexKey() && !query.getProj()->wantSortKey() &&
        !query.getProj()->hasDottedFieldPath() && !query.getProj()->requiresDocument();
}

/**
 * If 'solnRoot' is returning index key data from a single index, returns the associated index key
 * pattern. Otherwise, returns an empty object.
 */
auto produceCoveredKeyObj(QuerySolutionNode* solnRoot) {
    vector<QuerySolutionNode*> leafNodes;
    getLeafNodes(solnRoot, &leafNodes);

    // Both the IXSCAN and DISTINCT stages provide covered key data.
    if (1 == leafNodes.size()) {
        if (STAGE_IXSCAN == leafNodes[0]->getType()) {
            IndexScanNode* ixn = static_cast<IndexScanNode*>(leafNodes[0]);
            return ixn->index.keyPattern;
        } else if (STAGE_DISTINCT_SCAN == leafNodes[0]->getType()) {
            DistinctNode* dn = static_cast<DistinctNode*>(leafNodes[0]);
            return dn->index.keyPattern;
        }
    }
    return BSONObj();
}

/**
 * When projection needs to be added to the solution tree, this function chooses between the default
 * implementation and one of the fast paths.
 */
std::unique_ptr<ProjectionNode> analyzeProjection(const CanonicalQuery& query,
                                                  std::unique_ptr<QuerySolutionNode> solnRoot,
                                                  const bool hasSortStage) {
    const QueryRequest& qr = query.getQueryRequest();

    // If there's no sort stage but we have a sortKey meta-projection, we need to add a stage to
    // generate the sort key computed data.
    auto addSortKeyGeneratorStageIfNeeded = [&]() {
        if (!hasSortStage && query.getProj()->wantSortKey()) {
            auto keyGenNode = std::make_unique<SortKeyGeneratorNode>();
            keyGenNode->sortSpec = qr.getSort();
            keyGenNode->children.push_back(solnRoot.release());
            solnRoot = std::move(keyGenNode);
        }
    };

    LOG(5) << "PROJECTION: Current plan is:\n" << redact(solnRoot->toString());

    // If the projection requires the entire document we add a fetch stage if not present. Otherwise
    // we add a fetch stage if we are not covered and not returnKey.
    if ((query.getProj()->requiresDocument() && !solnRoot->fetched()) ||
        (!isCoveredOrAlreadyFetched(query.getProj()->getRequiredFields(), *solnRoot) &&
         !query.getProj()->wantIndexKey())) {
        auto fetch = std::make_unique<FetchNode>();
        fetch->children.push_back(solnRoot.release());
        solnRoot = std::move(fetch);
    }

    // There are two projection fast paths available for simple inclusion projections that don't
    // need an index key or sort key, don't have any dotted-path inclusions, and don't have the
    // 'requiresDocument' property: the ProjectionNodeSimple fast-path for plans that have a fetch
    // stage and the ProjectionNodeCovered for plans with an index scan that the projection can
    // cover. Plans that don't meet all the requirements for these fast path projections will all
    // use ProjectionNodeDefault, which is able to handle all projections, covered or otherwise.
    if (isSimpleProjection(query)) {
        // If the projection is simple, but not covered, use 'ProjectionNodeSimple'.
        if (solnRoot->fetched()) {
            addSortKeyGeneratorStageIfNeeded();
            return std::make_unique<ProjectionNodeSimple>(
                std::move(solnRoot), *query.root(), qr.getProj(), *query.getProj());
        } else {
            // If we're here we're not fetched so we're covered. Let's see if we can get out of
            // using the default projType. If 'solnRoot' is an index scan we can use the faster
            // covered impl.
            BSONObj coveredKeyObj = produceCoveredKeyObj(solnRoot.get());
            if (!coveredKeyObj.isEmpty()) {
                addSortKeyGeneratorStageIfNeeded();
                return std::make_unique<ProjectionNodeCovered>(std::move(solnRoot),
                                                               *query.root(),
                                                               qr.getProj(),
                                                               *query.getProj(),
                                                               std::move(coveredKeyObj));
            }
        }
    }

    addSortKeyGeneratorStageIfNeeded();
    return std::make_unique<ProjectionNodeDefault>(
        std::move(solnRoot), *query.root(), qr.getProj(), *query.getProj());
}

}  // namespace

// static
void QueryPlannerAnalysis::analyzeGeo(const QueryPlannerParams& params,
                                      QuerySolutionNode* solnRoot) {
    // Get field names of all 2dsphere indexes with version >= 3.
    std::set<StringData> twoDSphereFields;
    for (const IndexEntry& indexEntry : params.indices) {
        if (indexEntry.type != IndexType::INDEX_2DSPHERE) {
            continue;
        }

        S2IndexingParams params;
        ExpressionParams::initialize2dsphereParams(
            indexEntry.infoObj, indexEntry.collator, &params);

        if (params.indexVersion < S2_INDEX_VERSION_3) {
            continue;
        }

        for (auto elt : indexEntry.keyPattern) {
            if (elt.type() == BSONType::String && elt.String() == "2dsphere") {
                twoDSphereFields.insert(elt.fieldName());
            }
        }
    }
    if (twoDSphereFields.size() > 0) {
        geoSkipValidationOn(twoDSphereFields, solnRoot);
    }
}

BSONObj QueryPlannerAnalysis::getSortPattern(const BSONObj& indexKeyPattern) {
    BSONObjBuilder sortBob;
    BSONObjIterator kpIt(indexKeyPattern);
    while (kpIt.more()) {
        BSONElement elt = kpIt.next();
        if (elt.type() == mongo::String) {
            break;
        }
        // The canonical check as to whether a key pattern element is "ascending" or "descending" is
        // (elt.number() >= 0). This is defined by the Ordering class.
        int sortOrder = (elt.number() >= 0) ? 1 : -1;
        sortBob.append(elt.fieldName(), sortOrder);
    }
    return sortBob.obj();
}

// static
bool QueryPlannerAnalysis::explodeForSort(const CanonicalQuery& query,
                                          const QueryPlannerParams& params,
                                          QuerySolutionNode** solnRoot) {
    vector<QuerySolutionNode*> leafNodes;

    QuerySolutionNode* toReplace;
    if (!structureOKForExplode(*solnRoot, &toReplace)) {
        return false;
    }

    getLeafNodes(*solnRoot, &leafNodes);

    const BSONObj& desiredSort = query.getQueryRequest().getSort();

    // How many scan leaves will result from our expansion?
    size_t totalNumScans = 0;

    // The value of entry i is how many scans we want to blow up for leafNodes[i].
    // We calculate this in the loop below and might as well reuse it if we blow up
    // that scan.
    vector<size_t> fieldsToExplode;

    // The sort order we're looking for has to possibly be provided by each of the index scans
    // upon explosion.
    for (size_t i = 0; i < leafNodes.size(); ++i) {
        // We can do this because structureOKForExplode is only true if the leaves are index
        // scans.
        IndexScanNode* isn = static_cast<IndexScanNode*>(leafNodes[i]);
        const IndexBounds& bounds = isn->bounds;

        // Not a point interval prefix, can't try to rewrite.
        if (bounds.isSimpleRange) {
            return false;
        }

        if (isn->index.multikey && isn->index.multikeyPaths.empty()) {
            // The index is multikey but has no path-level multikeyness metadata. In this case, the
            // index can never provide a sort.
            return false;
        }

        // How many scans will we create if we blow up this ixscan?
        size_t numScans = 1;

        // Skip every field that is a union of point intervals and build the resulting sort
        // order from the remaining fields.
        BSONObjIterator kpIt(isn->index.keyPattern);
        size_t boundsIdx = 0;
        while (kpIt.more()) {
            const OrderedIntervalList& oil = bounds.fields[boundsIdx];
            if (!isUnionOfPoints(oil)) {
                break;
            }
            numScans *= oil.intervals.size();
            kpIt.next();
            ++boundsIdx;
        }

        // There's no sort order left to gain by exploding.  Just go home.  TODO: verify nothing
        // clever we can do here.
        if (!kpIt.more()) {
            return false;
        }

        // Only explode if there's at least one field to explode for this scan.
        if (0 == boundsIdx) {
            return false;
        }

        // The rest of the fields define the sort order we could obtain by exploding
        // the bounds.
        BSONObjBuilder resultingSortBob;
        while (kpIt.more()) {
            auto elem = kpIt.next();
            if (isn->multikeyFields.find(elem.fieldNameStringData()) != isn->multikeyFields.end()) {
                // One of the indexed fields providing the sort is multikey. It is not correct for a
                // field with multikey components to provide a sort, so bail out.
                return false;
            }
            resultingSortBob.append(elem);
        }

        // See if it's the order we're looking for.
        BSONObj possibleSort = resultingSortBob.obj();
        if (!desiredSort.isPrefixOf(possibleSort, SimpleBSONElementComparator::kInstance)) {
            // We can't get the sort order from the index scan. See if we can
            // get the sort by reversing the scan.
            BSONObj reversePossibleSort = QueryPlannerCommon::reverseSortObj(possibleSort);
            if (!desiredSort.isPrefixOf(reversePossibleSort,
                                        SimpleBSONElementComparator::kInstance)) {
                // Can't get the sort order from the reversed index scan either. Give up.
                return false;
            } else {
                // We can get the sort order we need if we reverse the scan.
                QueryPlannerCommon::reverseScans(isn);
            }
        }

        // Do some bookkeeping to see how many ixscans we'll create total.
        totalNumScans += numScans;

        // And for this scan how many fields we expand.
        fieldsToExplode.push_back(boundsIdx);
    }

    // Too many ixscans spoil the performance.
    if (totalNumScans > (size_t)internalQueryMaxScansToExplode.load()) {
        LOG(5) << "Could expand ixscans to pull out sort order but resulting scan count"
               << "(" << totalNumScans << ") is too high.";
        return false;
    }

    // If we're here, we can (probably?  depends on how restrictive the structure check is)
    // get our sort order via ixscan blow-up.
    MergeSortNode* merge = new MergeSortNode();
    merge->sort = desiredSort;
    for (size_t i = 0; i < leafNodes.size(); ++i) {
        IndexScanNode* isn = static_cast<IndexScanNode*>(leafNodes[i]);
        explodeScan(isn, desiredSort, fieldsToExplode[i], &merge->children);
    }

    merge->computeProperties();

    // Replace 'toReplace' with the new merge sort node.
    replaceNodeInTree(solnRoot, toReplace, merge);
    // And get rid of the node that got replaced.
    delete toReplace;

    return true;
}

// static
QuerySolutionNode* QueryPlannerAnalysis::analyzeSort(const CanonicalQuery& query,
                                                     const QueryPlannerParams& params,
                                                     QuerySolutionNode* solnRoot,
                                                     bool* blockingSortOut) {
    *blockingSortOut = false;

    const QueryRequest& qr = query.getQueryRequest();
    const BSONObj& sortObj = qr.getSort();

    if (sortObj.isEmpty()) {
        return solnRoot;
    }

    // TODO: We could check sortObj for any projections other than :1 and :-1
    // and short-cut some of this.

    // If the sort is $natural, we ignore it, assuming that the caller has detected that and
    // outputted a collscan to satisfy the desired order.
    BSONElement natural = dps::extractElementAtPath(sortObj, "$natural");
    if (!natural.eoo()) {
        return solnRoot;
    }

    // See if solnRoot gives us the sort.  If so, we're done.
    BSONObjSet sorts = solnRoot->getSort();

    // If the sort we want is in the set of sort orders provided already, bail out.
    if (sorts.end() != sorts.find(sortObj)) {
        return solnRoot;
    }

    // Sort is not provided.  See if we provide the reverse of our sort pattern.
    // If so, we can reverse the scan direction(s).
    BSONObj reverseSort = QueryPlannerCommon::reverseSortObj(sortObj);
    if (sorts.end() != sorts.find(reverseSort)) {
        QueryPlannerCommon::reverseScans(solnRoot);
        LOG(5) << "Reversing ixscan to provide sort. Result: " << redact(solnRoot->toString());
        return solnRoot;
    }

    // Sort not provided, can't reverse scans to get the sort.  One last trick: We can "explode"
    // index scans over point intervals to an OR of sub-scans in order to pull out a sort.
    // Let's try this.
    if (explodeForSort(query, params, &solnRoot)) {
        return solnRoot;
    }

    // If we're here, we need to add a sort stage.

    // If we're not allowed to put a blocking sort in, bail out.
    if (params.options & QueryPlannerParams::NO_BLOCKING_SORT) {
        delete solnRoot;
        return nullptr;
    }

    if (!solnRoot->fetched()) {
        const bool sortIsCovered =
            std::all_of(sortObj.begin(), sortObj.end(), [solnRoot](BSONElement e) {
                // Note that hasField() will return 'false' in the case that this field is a string
                // and there is a non-simple collation on the index. This will lead to encoding of
                // the field from the document on fetch, despite having read the encoded value from
                // the index.
                return solnRoot->hasField(e.fieldName());
            });

        if (!sortIsCovered) {
            FetchNode* fetch = new FetchNode();
            fetch->children.push_back(solnRoot);
            solnRoot = fetch;
        }
    }

    // And build the full sort stage. The sort stage has to have a sort key generating stage
    // as its child, supplying it with the appropriate sort keys.
    SortKeyGeneratorNode* keyGenNode = new SortKeyGeneratorNode();
    keyGenNode->sortSpec = sortObj;
    keyGenNode->children.push_back(solnRoot);
    solnRoot = keyGenNode;

    SortNode* sort = new SortNode();
    sort->pattern = sortObj;
    sort->children.push_back(solnRoot);
    solnRoot = sort;
    // When setting the limit on the sort, we need to consider both
    // the limit N and skip count M. The sort should return an ordered list
    // N + M items so that the skip stage can discard the first M results.
    if (qr.getLimit()) {
        // We have a true limit. The limit can be combined with the SORT stage.
        sort->limit =
            static_cast<size_t>(*qr.getLimit()) + static_cast<size_t>(qr.getSkip().value_or(0));
    } else if (qr.getNToReturn()) {
        // We have an ntoreturn specified by an OP_QUERY style find. This is used
        // by clients to mean both batchSize and limit.
        //
        // Overflow here would be bad and could cause a nonsense limit. Cast
        // skip and limit values to unsigned ints to make sure that the
        // sum is never stored as signed. (See SERVER-13537).
        sort->limit =
            static_cast<size_t>(*qr.getNToReturn()) + static_cast<size_t>(qr.getSkip().value_or(0));

        // This is a SORT with a limit. The wire protocol has a single quantity
        // called "numToReturn" which could mean either limit or batchSize.
        // We have no idea what the client intended. One way to handle the ambiguity
        // of a limited OR stage is to use the SPLIT_LIMITED_SORT hack.
        //
        // If wantMore is false (meaning that 'ntoreturn' was initially passed to
        // the server as a negative value), then we treat numToReturn as a limit.
        // Since there is no limit-batchSize ambiguity in this case, we do not use the
        // SPLIT_LIMITED_SORT hack.
        //
        // If numToReturn is really a limit, then we want to add a limit to this
        // SORT stage, and hence perform a topK.
        //
        // If numToReturn is really a batchSize, then we want to perform a regular
        // blocking sort.
        //
        // Since we don't know which to use, just join the two options with an OR,
        // with the topK first. If the client wants a limit, they'll get the efficiency
        // of topK. If they want a batchSize, the other OR branch will deliver the missing
        // results. The OR stage handles deduping.
        //
        // We must also add an ENSURE_SORTED node above the OR to ensure that the final results are
        // in correct sorted order, which may not be true if the data is concurrently modified.
        if (qr.wantMore() && params.options & QueryPlannerParams::SPLIT_LIMITED_SORT &&
            !QueryPlannerCommon::hasNode(query.root(), MatchExpression::TEXT) &&
            !QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO) &&
            !QueryPlannerCommon::hasNode(query.root(), MatchExpression::GEO_NEAR)) {
            // If we're here then the SPLIT_LIMITED_SORT hack is turned on,
            // and the query is of a type that allows the hack.
            //
            // Not allowed for geo or text, because we assume elsewhere that those
            // stages appear just once.
            OrNode* orn = new OrNode();
            orn->children.push_back(sort);
            SortNode* sortClone = static_cast<SortNode*>(sort->clone());
            sortClone->limit = 0;
            orn->children.push_back(sortClone);

            // Add ENSURE_SORTED above the OR.
            EnsureSortedNode* esn = new EnsureSortedNode();
            esn->pattern = sort->pattern;
            esn->children.push_back(orn);
            solnRoot = esn;
        }
    } else {
        sort->limit = 0;
    }

    *blockingSortOut = true;

    return solnRoot;
}

std::unique_ptr<QuerySolution> QueryPlannerAnalysis::analyzeDataAccess(
    const CanonicalQuery& query,
    const QueryPlannerParams& params,
    std::unique_ptr<QuerySolutionNode> solnRoot) {
    auto soln = std::make_unique<QuerySolution>();
    soln->filterData = query.getQueryObj();
    soln->indexFilterApplied = params.indexFiltersApplied;

    solnRoot->computeProperties();

    analyzeGeo(params, solnRoot.get());

    // solnRoot finds all our results.  Let's see what transformations we must perform to the
    // data.

    // If we're answering a query on a sharded system, we need to drop documents that aren't
    // logically part of our shard.
    if (params.options & QueryPlannerParams::INCLUDE_SHARD_FILTER) {
        if (!solnRoot->fetched()) {
            // See if we need to fetch information for our shard key.
            // NOTE: Solution nodes only list ordinary, non-transformed index keys for now

            bool fetch = false;
            BSONObjIterator it(params.shardKey);
            while (it.more()) {
                BSONElement nextEl = it.next();
                if (!solnRoot->hasField(nextEl.fieldName())) {
                    fetch = true;
                    break;
                }
            }

            if (fetch) {
                FetchNode* fetchNode = new FetchNode();
                fetchNode->children.push_back(solnRoot.release());
                solnRoot.reset(fetchNode);
            }
        }

        ShardingFilterNode* sfn = new ShardingFilterNode();
        sfn->children.push_back(solnRoot.release());
        solnRoot.reset(sfn);
    }

    bool hasSortStage = false;
    solnRoot.reset(analyzeSort(query, params, solnRoot.release(), &hasSortStage));

    // This can happen if we need to create a blocking sort stage and we're not allowed to.
    if (!solnRoot) {
        return nullptr;
    }

    // A solution can be blocking if it has a blocking sort stage or
    // a hashed AND stage.
    bool hasAndHashStage = hasNode(solnRoot.get(), STAGE_AND_HASH);
    soln->hasBlockingStage = hasSortStage || hasAndHashStage;

    const QueryRequest& qr = query.getQueryRequest();

    if (qr.getSkip()) {
        auto skip = std::make_unique<SkipNode>();
        skip->skip = *qr.getSkip();
        skip->children.push_back(solnRoot.release());
        solnRoot = std::move(skip);
    }

    // Project the results.
    if (query.getProj()) {
        solnRoot = analyzeProjection(query, std::move(solnRoot), hasSortStage);
        // If we don't have a covered project, and we're not allowed to put an uncovered one in,
        // bail out.
        if (solnRoot->fetched() && params.options & QueryPlannerParams::NO_UNCOVERED_PROJECTIONS)
            return nullptr;
    } else {
        // If there's no projection, we must fetch, as the user wants the entire doc.
        if (!solnRoot->fetched() && !(params.options & QueryPlannerParams::IS_COUNT)) {
            FetchNode* fetch = new FetchNode();
            fetch->children.push_back(solnRoot.release());
            solnRoot.reset(fetch);
        }
    }

    // When there is both a blocking sort and a limit, the limit will
    // be enforced by the blocking sort.
    // Otherwise, we need to limit the results in the case of a hard limit
    // (ie. limit in raw query is negative)
    if (!hasSortStage) {
        // We don't have a sort stage. This means that, if there is a limit, we will have
        // to enforce it ourselves since it's not handled inside SORT.
        if (qr.getLimit()) {
            LimitNode* limit = new LimitNode();
            limit->limit = *qr.getLimit();
            limit->children.push_back(solnRoot.release());
            solnRoot.reset(limit);
        } else if (qr.getNToReturn() && !qr.wantMore()) {
            // We have a "legacy limit", i.e. a negative ntoreturn value from an OP_QUERY style
            // find.
            LimitNode* limit = new LimitNode();
            limit->limit = *qr.getNToReturn();
            limit->children.push_back(solnRoot.release());
            solnRoot.reset(limit);
        }
    }

    soln->root = std::move(solnRoot);
    return soln;
}

}  // namespace mongo
