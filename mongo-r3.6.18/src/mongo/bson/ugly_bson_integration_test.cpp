
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

#include <algorithm>
#include <exception>

#include "mongo/client/connection_string.h"
#include "mongo/executor/network_interface_asio_integration_fixture.h"
#include "mongo/unittest/integration_test.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/net/op_msg.h"
#include "mongo/util/scopeguard.h"

namespace mongo {
namespace executor {
namespace {

class UglyBSONFixture : public NetworkInterfaceASIOIntegrationFixture {
    void setUp() override {
        startNet();
    }
};

TEST_F(UglyBSONFixture, DuplicateFields) {
    OpMsgBuilder::disableDupeFieldCheck_forTest.store(true);
    ON_BLOCK_EXIT([] { OpMsgBuilder::disableDupeFieldCheck_forTest.store(false); });

    assertCommandFailsOnServer("admin",
                               BSON("insert"
                                    << "test"
                                    << "documents"
                                    << BSONArray()
                                    << "documents"
                                    << BSONArray()),
                               ErrorCodes::duplicateCodeForTest(40413));
}

}  // namespace
}  // namespace executor
}  // namespace mongo
