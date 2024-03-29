
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

#include "mongo/client/remote_command_targeter_mock.h"

#include "mongo/base/status_with.h"
#include "mongo/client/read_preference.h"

namespace mongo {

RemoteCommandTargeterMock::RemoteCommandTargeterMock()
    : _findHostReturnValue(Status(ErrorCodes::InternalError, "No return value set")) {}

RemoteCommandTargeterMock::~RemoteCommandTargeterMock() = default;

std::shared_ptr<RemoteCommandTargeterMock> RemoteCommandTargeterMock::get(
    std::shared_ptr<RemoteCommandTargeter> targeter) {
    auto mock = std::dynamic_pointer_cast<RemoteCommandTargeterMock>(targeter);
    invariant(mock);

    return mock;
}

ConnectionString RemoteCommandTargeterMock::connectionString() {
    return _connectionStringReturnValue;
}

StatusWith<HostAndPort> RemoteCommandTargeterMock::findHost(OperationContext* opCtx,
                                                            const ReadPreferenceSetting& readPref) {
    return _findHostReturnValue;
}

StatusWith<HostAndPort> RemoteCommandTargeterMock::findHostWithMaxWait(
    const ReadPreferenceSetting& readPref, Milliseconds maxTime) {

    return _findHostReturnValue;
}

void RemoteCommandTargeterMock::markHostNotMaster(const HostAndPort& host, const Status& status) {}

void RemoteCommandTargeterMock::markHostUnreachable(const HostAndPort& host, const Status& status) {
}

void RemoteCommandTargeterMock::setConnectionStringReturnValue(const ConnectionString returnValue) {
    _connectionStringReturnValue = std::move(returnValue);
}

void RemoteCommandTargeterMock::setFindHostReturnValue(StatusWith<HostAndPort> returnValue) {
    _findHostReturnValue = std::move(returnValue);
}

}  // namespace mongo
