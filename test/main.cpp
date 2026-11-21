#include <QtTest>

#include "tst_lfqueue.h"
#include "tst_blf.h"
#include "tst_ascheader.h"
#include "tst_nativecsv.h"
#include "tst_synthetic_classic.h"
#include "tst_virtualbus.h"
#include "tst_logformat.h"
#include "tst_candumpparser.h"
#include "tst_ascparser.h"
#include "tst_trcparser.h"
#include "tst_csvparser.h"


int main(int argc, char** argv)
{
   QCoreApplication app(argc, argv);

   int status = 0;
   auto ASSERT_TEST = [&status, argc, argv](QObject* obj) {
     status |= QTest::qExec(obj, argc, argv);
     delete obj;
   };

   ASSERT_TEST(new TestLFQueue());
   ASSERT_TEST(new TestBLF());
   ASSERT_TEST(new TestASCHeader());
   ASSERT_TEST(new TestNativeCSV());
   ASSERT_TEST(new TestSyntheticClassic());
   ASSERT_TEST(new TestVirtualBus());
   ASSERT_TEST(new TestLogFormat());
   ASSERT_TEST(new TestCandumpParser());
   ASSERT_TEST(new TestAscParser());
   ASSERT_TEST(new TestTrcParser());
   ASSERT_TEST(new TestCsvParser());

   return status;
}
