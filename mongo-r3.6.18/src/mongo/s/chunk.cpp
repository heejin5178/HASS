
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

#include "mongo/s/chunk.h"

#include "mongo/platform/random.h"
#include "mongo/s/grid.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/log.h"

namespace mongo {

namespace {

// Test whether we should split once data * kSplitCheckInterval > chunkSize (approximately)
PseudoRandom prng(static_cast<int64_t>(time(0)));

// Assume user has 64MB chunkSize setting. It is ok if this assumption is wrong since it is only
// a heuristic.
const int64_t kMaxDataWrittenBytes = 64 * 1024 * 1024 / Chunk::kSplitTestFactor;

/**
 * Generates a random value for _dataWrittenBytes so that a mongos restart wouldn't cause delay in
 * splitting.
 */
int64_t mkDataWrittenBytes() {
    return prng.nextInt64(kMaxDataWrittenBytes);
}

}  // namespace

Chunk::Chunk(const ChunkType& from)
    : _range(from.getMin(), from.getMax()),
      _shardId(from.getShard()),
      _lastmod(from.getVersion()),
      _jumbo(from.getJumbo()),
      _dataWrittenBytes(mkDataWrittenBytes()),
	split_sum(0),split_average(0),cnt(0) {
    invariantOK(from.validate());
}

bool Chunk::containsKey(const BSONObj& shardKey) const {
    return getMin().woCompare(shardKey) <= 0 && shardKey.woCompare(getMax()) < 0;
}

uint64_t Chunk::getBytesWritten() const {
    return _dataWrittenBytes;
}

uint64_t Chunk::addBytesWritten(uint64_t bytesWrittenIncrement) {
    _dataWrittenBytes += bytesWrittenIncrement;
    return _dataWrittenBytes;
}

void Chunk::clearBytesWritten() {
    _dataWrittenBytes = 0;
}

bool Chunk::shouldSplit(uint64_t desiredChunkSize, bool minIsInf, bool maxIsInf) const {
    // If this chunk is at either end of the range, trigger auto-split at 10% less data written in
    // order to trigger the top-chunk optimization.
    const uint64_t splitThreshold = (minIsInf || maxIsInf)
        ? static_cast<uint64_t>((double)desiredChunkSize * 0.9)
        : desiredChunkSize;
	//log() << "heejjin shouldSplit datawrittenbytes: " << _dataWrittenBytes;
	//log() << "heejjin shouldSplit jumbo: " << _jumbo;
    // Check if there are enough estimated bytes written to warrant a split
    return _dataWrittenBytes >= splitThreshold / kSplitTestFactor;
}

std::string Chunk::toString() const {
    return str::stream() << ChunkType::shard() << ": " << _shardId << ", " << ChunkType::lastmod()
                         << ": " << _lastmod.toString() << ", " << _range.toString();
}

void Chunk::markAsJumbo() {
    _jumbo = true;
}

void Chunk::add_split_sum(std::string string_key)
{
	uint64_t double_key;
	std::string prefix_key = string_key.substr(0,10);
        std::istringstream iss(prefix_key);
       	iss >>double_key;
	//std::stringstream ssInt(prefix_key);	
	//ssInt >> double_key;

	split_sum += double_key;
	//log() << "string key : " << string_key << ", prefix_key : " << prefix_key <<", int key : " << double_key << ", split sum : " <<split_sum;
}

uint64_t Chunk::get_split_sum(void)
{
	return split_sum;
}

void Chunk::update_split_average(std::string string_key)
{
	int double_key;
	std::string prefix_key = string_key.substr(0,10);
	//log() << "string key : " << string_key <<", prefix key : " << prefix_key;
        std::istringstream iss(prefix_key);
       	iss >> double_key;	
	int prev_average = this->split_average;
	double old_weight = (this->cnt-1)/(double)(this->cnt);
	double new_weight = 1/(double)(this->cnt);
	this->split_average = (prev_average*old_weight)+(double_key*new_weight);
}

int Chunk::get_split_average(void)
{
	return this->split_average;
}

void Chunk::add_cnt(void)
{
	this->cnt++;
}
int Chunk::get_cnt(void)
{
	return this->cnt;
}
}  // namespace mongo
