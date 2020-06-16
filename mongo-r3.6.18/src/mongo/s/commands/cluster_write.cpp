
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/s/commands/cluster_write.h"

#include <algorithm>

#include "mongo/base/status.h"
#include "mongo/client/connpool.h"
#include "mongo/db/lasterror.h"
#include "mongo/db/write_concern_options.h"
#include "mongo/s/balancer_configuration.h"
#include "mongo/s/catalog/sharding_catalog_client.h"
#include "mongo/s/catalog/type_collection.h"
#include "mongo/s/catalog_cache.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/s/commands/chunk_manager_targeter.h"
#include "mongo/s/config_server_client.h"
#include "mongo/s/grid.h"
#include "mongo/s/shard_util.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

#include "mongo/bson/mutable/document.h"

#include "mongo/bson/bsonobj.h"
#include "mongo/bson/mutable/algorithm.h"
#include "mongo/bson/mutable/damage_vector.h"
#include "mongo/bson/mutable/mutable_bson_test_utils.h"
#include "mongo/db/json.h"
#include "mongo/db/query/collation/collator_interface_mock.h"
#include "mongo/platform/decimal128.h"
#include "mongo/unittest/death_test.h"
#include "mongo/unittest/unittest.h"


namespace mongo {
namespace {

const uint64_t kTooManySplitPoints = 4;
int global_update=0;
int global_split=0;

void toBatchError(const Status& status, BatchedCommandResponse* response) {
    response->clear();
    response->setErrCode(status.code());
    response->setErrMessage(status.reason());
    response->setOk(false);
    dassert(response->isValid(NULL));
}

/**
 * Returns the split point that will result in one of the chunk having exactly one document. Also
 * returns an empty document if the split point cannot be determined.
 *
 * doSplitAtLower - determines which side of the split will have exactly one document. True means
 * that the split point chosen will be closer to the lower bound.
 *
 * NOTE: this assumes that the shard key is not "special"- that is, the shardKeyPattern is simply an
 * ordered list of ascending/descending field names. For example {a : 1, b : -1} is not special, but
 * {a : "hashed"} is.
 */
BSONObj findExtremeKeyForShard(OperationContext* opCtx,
                               const NamespaceString& nss,
                               const ShardId& shardId,
                               const ShardKeyPattern& shardKeyPattern,
                               bool doSplitAtLower) {
    Query q;

    if (doSplitAtLower) {
        q.sort(shardKeyPattern.toBSON());
    } else {
        // need to invert shard key pattern to sort backwards
        // TODO: make a helper in ShardKeyPattern?
        BSONObjBuilder r;

        BSONObjIterator i(shardKeyPattern.toBSON());
        while (i.more()) {
            BSONElement e = i.next();
            uassert(10163, "can only handle numbers here - which i think is correct", e.isNumber());
            r.append(e.fieldName(), -1 * e.number());
        }

        q.sort(r.obj());
    }

    // Find the extreme key
    const auto shardConnStr = [&]() {
        const auto shard =
            uassertStatusOK(Grid::get(opCtx)->shardRegistry()->getShard(opCtx, shardId));
        return shard->getConnString();
    }();

    ScopedDbConnection conn(shardConnStr);

    BSONObj end;

    if (doSplitAtLower) {
        // Splitting close to the lower bound means that the split point will be the
        // upper bound. Chunk range upper bounds are exclusive so skip a document to
        // make the lower half of the split end up with a single document.
        std::unique_ptr<DBClientCursor> cursor = conn->query(nss.ns(),
                                                             q,
                                                             1, /* nToReturn */
                                                             1 /* nToSkip */);

        uassert(28736,
                str::stream() << "failed to initialize cursor during auto split due to "
                              << "connection problem with "
                              << conn->getServerAddress(),
                cursor.get() != nullptr);

        if (cursor->more()) {
            end = cursor->next().getOwned();
        }
    } else {
        end = conn->findOne(nss.ns(), q);
    }

    conn.done();

    if (end.isEmpty()) {
        return BSONObj();
    }

    return shardKeyPattern.extractShardKeyFromDoc(end);
}

/**
 * Splits the chunks touched based from the targeter stats if needed.
 */
void splitIfNeeded(OperationContext* opCtx,
                   const NamespaceString& nss,
                   const TargeterStats& stats,
		   double double_key) {
    auto routingInfoStatus = Grid::get(opCtx)->catalogCache()->getCollectionRoutingInfo(opCtx, nss);
    if (!routingInfoStatus.isOK()) {
        log() << "failed to get collection information for " << nss
              << " while checking for auto-split" << causedBy(routingInfoStatus.getStatus());
        return;
    }

    auto& routingInfo = routingInfoStatus.getValue();

    if (!routingInfo.cm()) {
        return;
    }

    for (auto it = stats.chunkSizeDelta.cbegin(); it != stats.chunkSizeDelta.cend(); ++it) {
        std::shared_ptr<Chunk> chunk;
        try {
            chunk = routingInfo.cm()->findIntersectingChunkWithSimpleCollation(it->first);
        } catch (const AssertionException& ex) {
            warning() << "could not find chunk while checking for auto-split: "
                      << causedBy(redact(ex));
            return;
        }

    //log() << "heejjin split IFNEED: " << double_key;
    // heejin added)
    // sum of chunk element 
   	chunk.get()->add_element(double_key); 
	chunk.get()->add_cnt();
//	log() << "heejjin get split sum : " << chunk.get()->get_split_sum();
        updateChunkWriteStatsAndSplitIfNeeded(
            opCtx, routingInfo.cm().get(), chunk.get(), it->second);
    }
}

}  // namespace

void ClusterWriter::write(OperationContext* opCtx,
                          const BatchedCommandRequest& request,
                          BatchWriteExecStats* stats,
                          BatchedCommandResponse* response) {
    const NamespaceString& nss = request.getNS();
   // log() << "jinnnn ClusterWriter::write "  << nss;
	double double_key=0.0;
    LastError::Disabled disableLastError(&LastError::get(opCtx->getClient()));

    // Config writes and shard writes are done differently
    if (nss.db() == NamespaceString::kAdminDb) {
    	log() << "jin conjin config write";
        Grid::get(opCtx)->catalogClient()->writeConfigServerDirect(opCtx, request, response);
    } else { // jin) shard writes
        TargeterStats targeterStats;

        {
            ChunkManagerTargeter targeter(request.getTargetingNS(), &targeterStats);

            Status targetInitStatus = targeter.init(opCtx);
            if (!targetInitStatus.isOK()) {
                toBatchError({targetInitStatus.code(),
                              str::stream() << "unable to initialize targeter for"
                                            << (request.isInsertIndexRequest() ? " index" : "")
                                            << " write op for collection "
                                            << request.getTargetingNS().ns()
                                            << causedBy(targetInitStatus)},
                             response);
                return;
            }

            auto swEndpoints = targeter.targetCollection();
            if (!swEndpoints.isOK()) {
                toBatchError({swEndpoints.getStatus().code(),
                              str::stream() << "unable to target"
                                            << (request.isInsertIndexRequest() ? " index" : "")
                                            << " write op for collection "
                                            << request.getTargetingNS().ns()
                                            << causedBy(swEndpoints.getStatus())},
                             response);
                return;
            }

            const auto& endpoints = swEndpoints.getValue();
  //  	log() << "jin endpoints during shard request: " << request.toString();
//	log() << "jin endpoints during shard response: " << request.toBSON();
//	log() << "jin endpoints during shard response nField: " << request.toBSON().nFields();

	if(request.toBSON().hasElement("documents"))
	{
//		log() << "jin element in!!!!!";
//		log() << "jin endpoints during shard response getOwned: " << request.toBSON().getObjectField("documents").getOwned();

		mongo::mutablebson::Document doc(request.toBSON().getObjectField("documents").getOwned());
		mongo::mutablebson::Element zero =doc.root()["0"];
//		log() << "jin endpoints during shard response getObject(zero): " << zero;
		mongo::mutablebson::Element key =zero[1];
		if(zero.toString() != "INVALID-MUTABLE-ELEMENT"){
//			log() << "jin endpoints during shard response getObject(key): " << key.getValueDouble();
			double_key = key.getValueDouble();
		}
		else
			log() << "jin endpoints INVALID" ;
	
	}		

            // Handle sharded config server writes differently.
            if (std::any_of(endpoints.begin(), endpoints.end(), [](const auto& it) {
                    return it.shardName == ShardRegistry::kConfigServerShardId;
                })) {
                // There should be no namespaces that partially target config servers.
                invariant(endpoints.size() == 1);

                // For config servers, we do direct writes.
                Grid::get(opCtx)->catalogClient()->writeConfigServerDirect(
                    opCtx, request, response);
                return;
            }

            BatchWriteExec::executeBatch(opCtx, targeter, request, response, stats);
        }

        splitIfNeeded(opCtx, request.getNS(), targeterStats, double_key);
    }
}
//heejin_found split
void updateChunkWriteStatsAndSplitIfNeeded(OperationContext* opCtx,
                                           ChunkManager* manager,
                                           Chunk* chunk,
                                           long dataWritten) {

	global_update++;
    // Disable lastError tracking so that any errors, which occur during auto-split do not get
    // bubbled up on the client connection doing a write
    LastError::Disabled disableLastError(&LastError::get(opCtx->getClient()));
    //log() << "jin!!! updateChunkWriteStatsAndSplitIfNeeded: " << global_update;

    const auto balancerConfig = Grid::get(opCtx)->getBalancerConfiguration();

    const bool minIsInf =
        (0 == manager->getShardKeyPattern().getKeyPattern().globalMin().woCompare(chunk->getMin()));
    const bool maxIsInf =
        (0 == manager->getShardKeyPattern().getKeyPattern().globalMax().woCompare(chunk->getMax()));

    const uint64_t chunkBytesWritten = chunk->addBytesWritten(dataWritten);
  //  log() << "jin!!! addBytesWritten(dataWritten) " << dataWritten;
    //log() << "jin!!! addBytesWritten(chunkBytesWritten) " << chunkBytesWritten;
    const uint64_t desiredChunkSize = balancerConfig->getMaxChunkSizeBytes();
/*	if(!chunk->shouldSplit(desiredChunkSize, minIsInf, maxIsInf))
	{
		log() << "heejjin error 1: " << desiredChunkSize;
		if(minIsInf)
			log() << "heejjin error 1: minIsInf";;
		if(maxIsInf)
			log() << "heejjin error 1 : maxIsInf";

		
	}
	if(!balancerConfig->getShouldAutoSplit())
	{
		log() << "heejjin error 2";	
	}*/
    if (!chunk->shouldSplit(desiredChunkSize, minIsInf, maxIsInf) ||
        !balancerConfig->getShouldAutoSplit()) {
	//log() << "heejin_ return: " << global_update;
        return;
    }

    const NamespaceString nss(manager->getns());

    if (!manager->_autoSplitThrottle._splitTickets.tryAcquire()) {
        LOG(1) << "won't auto split because not enough tickets: " << nss;
        return;
    }

    TicketHolderReleaser releaser(&(manager->_autoSplitThrottle._splitTickets));

    const ChunkRange chunkRange(chunk->getMin(), chunk->getMax());

    try {
        // Ensure we have the most up-to-date balancer configuration
        uassertStatusOK(balancerConfig->refreshAndCheck(opCtx));

        if (!balancerConfig->getShouldAutoSplit()) {
            return;
        }

        LOG(1) << "about to initiate autosplit: " << redact(chunk->toString())
               << " dataWritten: " << chunkBytesWritten
               << " desiredChunkSize: " << desiredChunkSize;

        const uint64_t chunkSizeToUse = [&]() {
            const uint64_t estNumSplitPoints = chunkBytesWritten / desiredChunkSize * 2;

            if (estNumSplitPoints >= kTooManySplitPoints) {
                // The current desired chunk size will split the chunk into lots of small chunk and
                // at the worst case this can result into thousands of chunks. So check and see if a
                // bigger value can be used.
                return std::min(chunkBytesWritten, balancerConfig->getMaxChunkSizeBytes());
            } else {
                return desiredChunkSize;
            }
        }();
//heejin) splitpoints call selectChunkSplitPoints
	int split_average = chunk->get_split_sum() / chunk->get_cnt();
	log() << "heejjin update split_average: " << split_average;
	log() << "jin!! yamae global split " << global_split;
	log() << "jin!! yanae key is " << global_update;

        auto splitPoints =
            uassertStatusOK(shardutil::selectChunkSplitPoints(opCtx,
                                                              chunk->getShardId(),
                                                              nss,
                                                              manager->getShardKeyPattern(),
                                                              chunkRange,
                                                              chunkSizeToUse,
                                                              boost::none));
/*
        auto splitPoints =
            uassertStatusOK(shardutil::selectChunkSplitPoints(opCtx,
                                                              chunk->getShardId(),
                                                              nss,
                                                              manager->getShardKeyPattern(),
                                                              chunkRange,
                                                              chunkSizeToUse,
                                                              boost::none,
								split_average));
*/
	//BSONObjBuilder current_key;
	//current_key.append("splitKeys", global_update+49800);
	//splitPoints.push_back(current_key.obj());
        if (splitPoints.size() <= 1) {
		log() << "splitpoints.size() <= 1 " << global_update;
            // No split points means there isn't enough data to split on; 1 split point means we
            // have
            // between half the chunk size to full chunk size so there is no need to split yet
            chunk->clearBytesWritten();
            return;
        }
	else {
		int target = split_average;
		BSONObjBuilder current_key;
		current_key.append("splitKeys", split_average);
		//BSONObjIterator it(splitPoints);
		std::vector<BSONObj>::iterator it = splitPoints.begin();
		int n=-1;
		while(it != splitPoints.end()) {
		//for(int i=0; i<splitPoints.size(); i++) { 
			BSONElement e = it->getField("key");
			//int k = e.getValue().numberInt();
			int k = (int)e.Number();
			if(k < split_average) {
				target = k;
				n++;
			}
			it++;

		}
		if(target==split_average) { // every split point is bigger than split average
                    splitPoints.front() = current_key.obj().getOwned();
		}
		else {
			splitPoints[n] = current_key.obj().getOwned();
		}
	}

//	log() << "heejin*** found-front : " << splitPoints.front();
//	log() << "heejin*** found-back : " << splitPoints.back();
        if (minIsInf || maxIsInf) {
            // We don't want to reset _dataWritten since we want to check the other side right away
        } else {
            // We're splitting, so should wait a bit
//	log() << "heejin** found-front : " << splitPoints.front();
//	log() << "heejin** found-back : " << splitPoints.back();
            chunk->clearBytesWritten();
//	log() << "heejin* found-front : " << splitPoints.front();
//	log() << "heejin* found-back : " << splitPoints.back();
        }

//	log() << "heejin_ found-front : " << splitPoints.front();
//	log() << "heejin_ found-back : " << splitPoints.back();
        // We assume that if the chunk being split is the first (or last) one on the collection,
        // this chunk is likely to see more insertions. Instead of splitting mid-chunk, we use the
        // very first (or last) key as a split point.
        //
        // This heuristic is skipped for "special" shard key patterns that are not likely to produce
        // monotonically increasing or decreasing values (e.g. hashed shard keys).
        
//	auto tmp_splitPoints 


	if (KeyPattern::isOrderedKeyPattern(manager->getShardKeyPattern().toBSON())) {
//	log() << "heejin ) key pattern if statement in";
            if (minIsInf) {
                BSONObj key = findExtremeKeyForShard(
                    opCtx, nss, chunk->getShardId(), manager->getShardKeyPattern(), true);
                if (!key.isEmpty()) {
                    splitPoints.front() = key.getOwned();
		//heejin debug
//		log() << "heejin) minIsInf" << splitPoints.front() ;
                }
            } else if (maxIsInf) {
                BSONObj key = findExtremeKeyForShard(
                    opCtx, nss, chunk->getShardId(), manager->getShardKeyPattern(), false);
                if (!key.isEmpty()) {
			//toBSON().getObjectField("documents").getOwned();
		//	splitPoints.push_back(current_key.obj());

                    splitPoints.back() = key.getOwned();
		//heejin debug
//		log() << "heejin) maxIsInf" << splitPoints.back() ;
                }
            }
        }
//	log() << "heejin__ found-front : " << splitPoints.front();
//	log() << "heejin__ found-back : " << splitPoints.back();
//heejin) this part call splitChunkAtMultiplePoints
        const auto suggestedMigrateChunk =
            uassertStatusOK(shardutil::splitChunkAtMultiplePoints(opCtx,
                                                                  chunk->getShardId(),
                                                                  nss,
                                                                  manager->getShardKeyPattern(),
                                                                  manager->getVersion(),
                                                                  chunkRange,
                                                                  splitPoints));

	global_split++;
	log() << "jin!! real global split " << global_split;
        // Balance the resulting chunks if the option is enabled and if the shard suggested a chunk
        // to balance
        const bool shouldBalance = [&]() {
            if (!balancerConfig->shouldBalanceForAutoSplit())
                return false;

            auto collStatus =
                Grid::get(opCtx)->catalogClient()->getCollection(opCtx, manager->getns());
            if (!collStatus.isOK()) {
                log() << "Auto-split for " << nss << " failed to load collection metadata"
                      << causedBy(redact(collStatus.getStatus()));
                return false;
            }

            return collStatus.getValue().value.getAllowBalance();
        }();

//	log() << "heejin found-front : " << splitPoints.front();
//	log() << "heejin found-back : " << splitPoints.back();
        log() << "autosplitted " << nss << " chunk: " << redact(chunk->toString()) << " into "
              << (splitPoints.size() + 1) << " parts (desiredChunkSize " << desiredChunkSize << ")"
              << (suggestedMigrateChunk ? "" : (std::string) " (migrate suggested" +
                          (shouldBalance ? ")" : ", but no migrations allowed)"));

        // Reload the chunk manager after the split
        auto routingInfo = uassertStatusOK(
            Grid::get(opCtx)->catalogCache()->getShardedCollectionRoutingInfoWithRefresh(opCtx,
                                                                                         nss));

        if (!shouldBalance || !suggestedMigrateChunk) {
            return;
        }

        // Top chunk optimization - try to move the top chunk out of this shard to prevent the hot
        // spot from staying on a single shard. This is based on the assumption that succeeding
        // inserts will fall on the top chunk.

        // We need to use the latest chunk manager (after the split) in order to have the most
        // up-to-date view of the chunk we are about to move
        auto suggestedChunk = routingInfo.cm()->findIntersectingChunkWithSimpleCollation(
            suggestedMigrateChunk->getMin());

        ChunkType chunkToMove;
        chunkToMove.setNS(nss.ns());
        chunkToMove.setShard(suggestedChunk->getShardId());
        chunkToMove.setMin(suggestedChunk->getMin());
        chunkToMove.setMax(suggestedChunk->getMax());
        chunkToMove.setVersion(suggestedChunk->getLastmod());

        uassertStatusOK(configsvr_client::rebalanceChunk(opCtx, chunkToMove));

        // Ensure the collection gets reloaded because of the move
        Grid::get(opCtx)->catalogCache()->invalidateShardedCollection(nss);
    } catch (const DBException& ex) {
        chunk->clearBytesWritten();

        if (ErrorCodes::isStaleShardingError(ex.code())) {
            log() << "Unable to auto-split chunk " << redact(chunkRange.toString()) << causedBy(ex)
                  << ", going to invalidate routing table entry for " << nss;
            Grid::get(opCtx)->catalogCache()->invalidateShardedCollection(nss);
        }
    }
}

}  // namespace mongo
