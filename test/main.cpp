#include <QtTest>

#include "tst_lfqueue.h"
#include "tst_blf.h"
#include "tst_ascheader.h"
#include "tst_nativecsv.h"
#include "tst_synthetic_classic.h"


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

   return status;
}
