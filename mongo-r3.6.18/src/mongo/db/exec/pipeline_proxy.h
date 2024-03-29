
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

#include <boost/intrusive_ptr.hpp>
#include <boost/optional/optional.hpp>

#include "mongo/db/catalog/collection.h"
#include "mongo/db/exec/plan_stage.h"
#include "mongo/db/exec/plan_stats.h"
#include "mongo/db/pipeline/pipeline.h"
#include "mongo/db/query/plan_summary_stats.h"
#include "mongo/db/record_id.h"
#include "mongo/util/assert_util.h"

namespace mongo {

/**
 * Stage for pulling results out from an aggregation pipeline.
 */
class PipelineProxyStage final : public PlanStage {
public:
    PipelineProxyStage(OperationContext* opCtx,
                       std::unique_ptr<Pipeline, Pipeline::Deleter> pipeline,
                       WorkingSet* ws);

    PlanStage::StageState doWork(WorkingSetID* out) final;

    bool isEOF() final;

    //
    // Manage our OperationContext.
    //
    void doDetachFromOperationContext() final;
    void doReattachToOperationContext() final;

    // Returns empty PlanStageStats object
    std::unique_ptr<PlanStageStats> getStats() final;

    // Not used.
    SpecificStats* getSpecificStats() const final {
        MONGO_UNREACHABLE;
    }

    void doInvalidate(OperationContext* opCtx, const RecordId& rid, InvalidationType type) final {
        // A PlanExecutor with a PipelineProxyStage should be registered with the global cursor
        // manager, so should not receive invalidations.
        MONGO_UNREACHABLE;
    }

    /**
     * Pass through the last oplog timestamp from the proxied pipeline.
     */
    Timestamp getLatestOplogTimestamp() const;

    std::string getPlanSummaryStr() const;
    void getPlanSummaryStats(PlanSummaryStats* statsOut) const;

    StageType stageType() const final {
        return STAGE_PIPELINE_PROXY;
    }

    static const char* kStageType;

protected:
    void doDispose() final;

private:
    boost::optional<BSONObj> getNextBson();

    // Things in the _stash should be returned before pulling items from _pipeline.
    std::unique_ptr<Pipeline, Pipeline::Deleter> _pipeline;
    std::vector<BSONObj> _stash;
    const bool _includeMetaData;

    // When the aggregation request is from a 3.4 mongos, the merge may happen on a 3.4 shard (which
    // does not understand sort key metadata), so we should not serialize the sort key, and
    // '_includeSortKey' is set to false.
    // TODO SERVER-30924: remove this.
    const bool _includeSortKey;

    // Not owned by us.
    WorkingSet* _ws;
};

}  // namespace mongo
