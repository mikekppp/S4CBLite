//
//  S4CBLiteTests.m
//  S4CBLiteTests
//
//  Created by Michael Papp on 8/30/25.
//  Copyright © 2025 SeaStones Software and Michael Papp.
//


#import <XCTest/XCTest.h>
#import "S4CBLiteTests.h"

@interface S4CBLiteTests : XCTestCase

@end

@implementation S4CBLiteTests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testPerformanceExample {
	// This is an example of a performance test case.
	[self measureBlock:^{
		// Put the code you want to measure the time of here.
		cbutil_test_basic_roundtrip();
	}];
}

- (void)test_basic_roundtrip {
	XCTAssertEqual(cbutil_test_basic_roundtrip(), 0);
}

- (void)test_arrays_and_bools {
	XCTAssertEqual(cbutil_test_arrays_and_bools(), 0);
}

- (void)test_transactions_commit_and_rollback {
	XCTAssertEqual(cbutil_test_transactions_commit_and_rollback(), 0);
}

- (void)test_collections_if_available {
	XCTAssertEqual(cbutil_test_collections_if_available(), 0);
}

//- (void)test_blob {
//	XCTAssertEqual(cbutil_test_blob(), 0);
//}

//- (void)test_nested_dict_array {
//	XCTAssertEqual(cbutil_test_nested_dict_array(), 0);
//}

@end
